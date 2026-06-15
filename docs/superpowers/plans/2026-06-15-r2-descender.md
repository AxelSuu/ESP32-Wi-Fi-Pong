# R2 — Descender Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.

> **PROJECT RULE (overrides the skill's commit/build steps):** Per `CLAUDE.md`, the **agent
> never commits, builds, flashes, or monitors hardware.** Loop: edit → run **host** tests
> (`make -C test/host run`, host gcc, allowed) → hand off. Every "Checkpoint" is a **user**
> action. Do not run `git commit`, `idf.py`, or any hardware command.

**Goal:** Add **Descender**, a Downwell-like single-player faller — the second modern game,
shipping as pure firmware on top of the R1 controller-descriptor system.

**Architecture:** A new `game_module_t DESCENDER` in `main/descender.c`, file-static state, drawn
via `gfx_*`, juiced via `fx_*`, RNG via `esp_random()` (host-mockable). The player falls down an
endless well (camera follows so the player sits at a fixed screen row); **FIRE shoots downward**
(recoil brakes the fall), **stomping** non-spiky enemies from above bounces and kills, **spiky**
enemies must be shot, a **combo** counts kills without taking a hit, and crossing each depth
**segment** opens a 1-of-3 **upgrade** pick (reusing Survivor's nav/select overlay pattern).

**Tech Stack:** C (ESP-IDF v6.0, `-Wall -Werror -Wextra`), host test harness (`test/host/`, gcc
with a mocked `esp_random`/clock), `gfx_*`/`fx_*` toolkits.

**Design note (deviation from spec):** the spec listed a seedable `rng.h`. Descender uses
`esp_random()` directly (as Survivor does) — it is already host-mockable (deterministic LCG
fallback in `test/host/stubs.c`), so `rng.h`'s only unique remaining value is *daily-seed
challenges*, which is an M11 concern. Deferring `rng.h` to M11 is YAGNI-correct and matches the
proven Survivor pattern. Combo "resets on landing" (no platforms in v1) is realized as **combo
resets when you take a hit**; platforms/landing are a playtest/stretch item.

---

## File structure (what changes)

- `main/descender.h` — **new**: the vtable extern.
- `main/descender.c` — **new**: the whole game.
- `main/engine.c` — `#include "descender.h"` + add `&DESCENDER` to `s_games[]`.
- `main/icons.h` — add `ICON_DESCENDER` + an `icon_for_id` branch.
- `main/CMakeLists.txt` — add `"descender.c"` to `SRCS`.
- `test/host/test_games.c` — extern + 3 tests + `main()` calls.
- `test/host/Makefile` — add `descender.c` to `GAME_SRCS`.
- `README.md`, `ROADMAP.md`, `TEST_PLAN.md` — Descender row / milestone / §14.

---

## Task 1: Failing host tests for Descender (RED)

**Files:**
- Modify: `test/host/test_games.c`
- Modify: `test/host/Makefile:13`

- [ ] **Step 1: Add `descender.c` to the host game sources**

In `test/host/Makefile` line 13, append `$(MAIN)/descender.c` to `GAME_SRCS`:

```make
GAME_SRCS  := $(MAIN)/snake.c $(MAIN)/tron.c $(MAIN)/pong.c $(MAIN)/breakout.c $(MAIN)/survivor.c $(MAIN)/descender.c $(MAIN)/gfx.c stubs.c test_games.c
```

- [ ] **Step 2: Declare the module and add the tests**

In `test/host/test_games.c`, add the extern after the existing `extern const game_module_t SURVIVOR;` (line 15):

```c
extern const game_module_t DESCENDER;
```

Then add these three tests immediately after `test_survivor_survival_scores` (after line 178):

```c
// --- Descender (Downwell-like faller; RNG mocked, deterministic LCG fallback) ---

static void test_descender_reset(void)
{
    printf("descender: fresh run starts clean\n");
    mock_random_reset();
    DESCENDER.reset();
    CHECK(!DESCENDER.is_over(), "not over at start");
    CHECK(DESCENDER.score() == 0, "score starts at 0");
    CHECK(DESCENDER.winner() == -1, "single-player: no winner");
}

static void test_descender_descent_scores(void)
{
    printf("descender: falling accrues depth score\n");
    mock_random_reset();
    DESCENDER.reset();
    for (int i = 0; i < 30; i++) DESCENDER.tick(30);   // ~0.9 s of falling, no input
    CHECK(DESCENDER.score() >= 1, "depth contributes score while falling");
    CHECK(!DESCENDER.is_over(), "still alive after a short fall");
}

static void test_descender_attrition_ends(void)
{
    printf("descender: passive play eventually ends the run\n");
    mock_random_reset();
    DESCENDER.reset();
    int ticks = 0;
    while (!DESCENDER.is_over() && ticks < 20000) {
        send(&DESCENDER, INPUT_SELECT, 0);   // clear any upgrade pick so the sim keeps running
        DESCENDER.tick(30);
        ticks++;
    }
    CHECK(DESCENDER.is_over(), "run ends within the tick budget (spiky enemies damage a passive player)");
    CHECK(DESCENDER.winner() == -1, "single-player: no winner");
}
```

Register them in `main()` after the `test_survivor_survival_scores();` call (after line 184):

```c
    test_descender_reset();
    test_descender_descent_scores();
    test_descender_attrition_ends();
```

- [ ] **Step 3: Run host tests to verify they fail (RED)**

Run: `make -C test/host run`
Expected: **build fails** — `descender.c` does not exist yet (`No such file or directory` /
`No rule to make target '…/descender.c'`). This confirms the tests are wired to the new module.

- [ ] **Step 4: Checkpoint (user)**

```bash
git add test/host/test_games.c test/host/Makefile
git commit -m "test: failing host tests for Descender"
```

---

## Task 2: Implement Descender (GREEN)

**Files:**
- Create: `main/descender.h`
- Create: `main/descender.c`

- [ ] **Step 1: Create the header**

Create `main/descender.h`:

```c
#pragma once

#include "game_module.h"

// Downwell-like single-player faller. See descender.c.
extern const game_module_t DESCENDER;
```

- [ ] **Step 2: Create the game**

Create `main/descender.c` with exactly this content. Constants marked TUNABLE are expected to be
adjusted during the hardware playtest; they compile and pass host tests as written.

```c
#include "descender.h"
#include "display.h"
#include "fx.h"
#include "hw_config.h"
#include "esp_random.h"
#include <stdio.h>
#include <math.h>

// --- play area (below an 11px HUD band); the well scrolls past a fixed player row ---
#define PX0 3
#define PX1 (SCREEN_WIDTH - 4)
#define PY0 12
#define PY1 (SCREEN_HEIGHT - 2)
#define PLAYER_ROW 34            // player's fixed screen row; the world scrolls past it

#define MAX_ENEMIES 16
#define MAX_BULLETS 12
#define START_HP    4
#define IFRAME_MS   700          // invulnerability after a hit
#define SEGMENT     220.0f       // world depth between upgrade picks

// TUNABLE feel constants (adjust on hardware).
#define GRAV        0.020f       // downward accel per ms
#define VTERM       2.6f         // terminal fall speed (world units / 30 ms)
#define RECOIL      1.5f         // upward kick per shot
#define VUP_MAX     2.2f         // cap on upward (recoil/bounce) speed
#define BOUNCE_0    2.4f         // base stomp rebound
#define HSPEED      1.7f         // horizontal speed at full steer
#define BULLET_SPD  3.4f         // downward bullet speed

enum { UPG_FIRERATE, UPG_DAMAGE, UPG_MULTISHOT, UPG_HP, UPG_BOUNCE, UPG_COUNT };
static const char *UPG_NAME[UPG_COUNT] = { "FIRE RATE", "DAMAGE", "MULTISHOT", "MAX HP", "STOMP+" };

typedef struct { float x, wy; int hp; bool alive; bool boss; bool spiky; } enemy_t;
typedef struct { float x, wy; bool alive; } bullet_t;

// --- player + run state ---
static float    s_px;            // player screen x (horizontal does not scroll)
static float    s_wy;            // player world depth (down +)
static float    s_vy;            // vertical world velocity (down +)
static float    s_steer;         // -1..1 horizontal intent (from tilt)
static int      s_hp, s_maxhp;
static int      s_invuln;        // ms of remaining i-frames
static int      s_combo, s_best_combo, s_kills;
static int      s_depth;         // max depth reached, in "metres" (= wy / 4)
static uint32_t s_run_ms;
static bool     s_over;

// weapon
static int      s_fire_cd, s_fire_interval, s_damage, s_multishot;
static float    s_bounce;

// spawning
static int      s_spawn_acc;
static uint32_t s_last_boss;

// upgrade choice
static bool     s_leveling;
static float    s_next_seg;
static int      s_choices[3], s_choice_idx;

static enemy_t  s_enemy[MAX_ENEMIES];
static bullet_t s_bullet[MAX_BULLETS];

static float frnd(void) { return (float)(esp_random() & 0xFFFF) / 65535.0f; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static void descender_reset(void)
{
    s_px = (PX0 + PX1) / 2.0f;
    s_wy = 0;
    s_vy = 0.6f;
    s_steer = 0;
    s_hp = s_maxhp = START_HP;
    s_invuln = 0;
    s_combo = s_best_combo = s_kills = 0;
    s_depth = 0;
    s_run_ms = 0;
    s_over = false;

    s_fire_cd = 0;
    s_fire_interval = 150;
    s_damage = 1;
    s_multishot = 1;
    s_bounce = BOUNCE_0;

    s_spawn_acc = 0;
    s_last_boss = 0;

    s_leveling = false;
    s_next_seg = SEGMENT;
    s_choice_idx = 0;

    for (int i = 0; i < MAX_ENEMIES; i++) s_enemy[i].alive = false;
    for (int i = 0; i < MAX_BULLETS; i++) s_bullet[i].alive = false;
}

static void apply_upgrade(int u)
{
    switch (u) {
    case UPG_FIRERATE:
        s_fire_interval = s_fire_interval * 80 / 100;
        if (s_fire_interval < 60) s_fire_interval = 60;
        break;
    case UPG_DAMAGE:    s_damage++;                          break;
    case UPG_MULTISHOT: if (s_multishot < 4) s_multishot++;  break;
    case UPG_HP:        s_maxhp++; s_hp++;                   break;
    case UPG_BOUNCE:    s_bounce += 0.6f;                    break;
    }
}

static void start_levelup(void)
{
    s_leveling   = true;
    s_choice_idx = 0;
    for (int i = 0; i < 3; i++) {
        int u;
        bool dup;
        do {
            u = (int)(frnd() * UPG_COUNT);
            if (u >= UPG_COUNT) u = UPG_COUNT - 1;
            dup = (i > 0 && (u == s_choices[0] || (i > 1 && u == s_choices[1])));
        } while (dup);
        s_choices[i] = u;
    }
    fx_flash();
}

static void fire(void)
{
    if (s_fire_cd > 0) return;
    s_fire_cd = s_fire_interval;
    for (int k = 0; k < s_multishot; k++) {
        int slot = -1;
        for (int i = 0; i < MAX_BULLETS; i++) if (!s_bullet[i].alive) { slot = i; break; }
        if (slot < 0) break;
        s_bullet[slot].x  = s_px + (k - (s_multishot - 1) / 2.0f) * 3.0f;
        s_bullet[slot].wy = s_wy + 2.0f;
        s_bullet[slot].alive = true;
    }
    s_vy -= RECOIL;
    if (s_vy < -VUP_MAX) s_vy = -VUP_MAX;
}

static void descender_on_input(const input_event_t *ev)
{
    if (ev->player != 0) return;
    if (s_leveling) {
        if (ev->kind == INPUT_NAV) {
            int dir = (ev->analog < 0) ? -1 : 1;
            s_choice_idx = (s_choice_idx + dir + 3) % 3;
        } else if (ev->kind == INPUT_SELECT || ev->kind == INPUT_PRIMARY) {
            apply_upgrade(s_choices[s_choice_idx]);
            s_leveling = false;
        }
        return;
    }
    switch (ev->kind) {
    case INPUT_TILT:    s_steer = clampf(ev->analog / 22.0f, -1.0f, 1.0f);  break;
    case INPUT_LEFT:    s_px = clampf(s_px - 6.0f, PX0, PX1);               break;
    case INPUT_RIGHT:   s_px = clampf(s_px + 6.0f, PX0, PX1);               break;
    case INPUT_PRIMARY: fire();                                            break;
    default: break;
    }
}

static void spawn_enemy(void)
{
    int slot = -1;
    for (int i = 0; i < MAX_ENEMIES; i++) if (!s_enemy[i].alive) { slot = i; break; }
    if (slot < 0) return;

    bool boss = (s_run_ms - s_last_boss >= 30000u);
    if (boss) s_last_boss = s_run_ms;

    s_enemy[slot].x     = PX0 + 3 + frnd() * (PX1 - PX0 - 6);
    s_enemy[slot].wy    = s_wy + (PY1 - PLAYER_ROW) + 6 + frnd() * 26.0f;  // just below the view
    s_enemy[slot].boss  = boss;
    s_enemy[slot].spiky = !boss && (frnd() < 0.4f);                        // spiky = must be shot
    s_enemy[slot].hp    = boss ? (8 + s_depth / 40) : (1 + s_depth / 120);
    s_enemy[slot].alive = true;
}

static void kill_enemy(int e)
{
    s_enemy[e].alive = false;
    s_kills++;
    if (++s_combo > s_best_combo) s_best_combo = s_combo;
    fx_spark((int)s_px, PLAYER_ROW);
}

static void descender_tick(uint32_t dt_ms)
{
    if (s_over || s_leveling) return;     // the upgrade pick pauses the world
    s_run_ms += dt_ms;
    if (s_invuln > 0) s_invuln -= (int)dt_ms;
    if (s_fire_cd > 0) s_fire_cd -= (int)dt_ms;

    float step = (float)dt_ms / 30.0f;

    // Horizontal (tilt) + vertical (gravity) motion.
    s_px = clampf(s_px + s_steer * HSPEED * step, PX0, PX1);
    s_vy += GRAV * (float)dt_ms;
    if (s_vy > VTERM) s_vy = VTERM;
    s_wy += s_vy * step;
    if (s_wy < 0) s_wy = 0;

    int d = (int)(s_wy / 4.0f);
    if (d > s_depth) s_depth = d;

    // Cross a depth segment -> offer an upgrade.
    if (s_wy >= s_next_seg) { s_next_seg += SEGMENT; start_levelup(); return; }

    // Spawn cadence ramps with depth.
    int interval = 900 - s_depth * 3;
    if (interval < 240) interval = 240;
    s_spawn_acc += (int)dt_ms;
    if (s_spawn_acc >= interval) { s_spawn_acc = 0; spawn_enemy(); }

    // Bullets travel downward; hit enemies.
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!s_bullet[b].alive) continue;
        s_bullet[b].wy += BULLET_SPD * step;
        if (s_bullet[b].wy - s_wy > (PY1 - PLAYER_ROW) + 30) { s_bullet[b].alive = false; continue; }
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!s_enemy[e].alive) continue;
            float dx = s_enemy[e].x - s_bullet[b].x, dy = s_enemy[e].wy - s_bullet[b].wy;
            if (dx * dx + dy * dy < 16.0f) {
                s_enemy[e].hp -= s_damage;
                if (s_enemy[e].hp <= 0) kill_enemy(e);
                s_bullet[b].alive = false;
                break;
            }
        }
    }

    // Enemies vs player: stomp non-spiky from above (kill + bounce); else contact damages.
    for (int e = 0; e < MAX_ENEMIES; e++) {
        if (!s_enemy[e].alive) continue;
        if (s_enemy[e].wy < s_wy - (PLAYER_ROW - PY0) - 6) { s_enemy[e].alive = false; continue; }
        float dx = s_enemy[e].x - s_px, dy = s_enemy[e].wy - s_wy;
        float hitr = s_enemy[e].boss ? 6.0f : 4.5f;
        if (dx * dx + dy * dy < hitr * hitr) {
            if (s_vy > 0.4f && dy > 0 && !s_enemy[e].boss && !s_enemy[e].spiky) {
                kill_enemy(e);
                s_vy = -s_bounce;
                if (s_vy < -VUP_MAX) s_vy = -VUP_MAX;
            } else if (s_invuln <= 0) {
                s_hp--;
                s_combo = 0;
                s_invuln = IFRAME_MS;
                fx_shake(2, 6);
                if (s_hp <= 0) { s_over = true; return; }
            }
        }
    }
}

static int screen_y(float wy) { return (int)(PLAYER_ROW + (wy - s_wy)); }

static void descender_render(void)
{
    // HUD band: HP pips, depth, combo.
    gfx_rect(0, 0, SCREEN_WIDTH, 11, 0x2);
    for (int i = 0; i < s_maxhp && i < 16; i++)
        gfx_rect(2 + i * 5, 3, 3, 5, i < s_hp ? 0xF : 0x4);
    char hud[20];
    snprintf(hud, sizeof hud, "%dm", s_depth);
    gfx_text(SCREEN_WIDTH / 2 - 10, 2, hud, 0xC);
    if (s_combo > 1) {
        snprintf(hud, sizeof hud, "x%d", s_combo);
        gfx_text(SCREEN_WIDTH - 22, 2, hud, 0xF);
    }

    // Scrolling well walls (dashes convey speed/depth).
    int phase = (int)s_wy & 7;
    for (int y = PY0; y <= PY1; y++) {
        if (((y + phase) & 7) < 4) { gfx_pixel(PX0 - 1, y, 0x6); gfx_pixel(PX1 + 1, y, 0x6); }
    }

    // Enemies + bullets.
    for (int e = 0; e < MAX_ENEMIES; e++) {
        if (!s_enemy[e].alive) continue;
        int ey = screen_y(s_enemy[e].wy);
        if (ey < PY0 - 4 || ey > PY1 + 4) continue;
        int ex = (int)s_enemy[e].x;
        if (s_enemy[e].boss)       gfx_circle(ex, ey, 4, 0xF);
        else if (s_enemy[e].spiky) gfx_frame(ex - 2, ey - 2, 5, 5, 0xD);   // hollow = shoot it
        else                       gfx_rect(ex - 2, ey - 2, 4, 4, 0xA);     // solid = stompable
    }
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!s_bullet[b].alive) continue;
        gfx_vline((int)s_bullet[b].x, screen_y(s_bullet[b].wy), 2, 0xF);
    }

    // Player ship (downward chevron), blinking while invulnerable.
    if (s_invuln <= 0 || (s_invuln / 80) & 1) {
        int px = (int)s_px;
        gfx_hline(px - 2, PLAYER_ROW - 2, 5, 0xF);
        gfx_hline(px - 1, PLAYER_ROW - 1, 3, 0xF);
        gfx_pixel(px, PLAYER_ROW, 0xF);
    }

    // Upgrade overlay.
    if (s_leveling) {
        gfx_rect(8, 24, SCREEN_WIDTH - 16, 56, 0x0);
        gfx_frame(8, 24, SCREEN_WIDTH - 16, 56, 0xF);
        int x = (SCREEN_WIDTH - gfx_text_width("UPGRADE", 1)) / 2;
        gfx_text(x, 28, "UPGRADE", 0xF);
        for (int i = 0; i < 3; i++) {
            int y = 40 + i * 12;
            bool sel = (i == s_choice_idx);
            if (sel) gfx_rect(10, y - 1, SCREEN_WIDTH - 20, 11, 0x4);
            gfx_text(14, y, UPG_NAME[s_choices[i]], sel ? 0xF : 0xA);
        }
    }
}

static bool descender_is_over(void) { return s_over; }
static int  descender_winner(void)  { return -1; }
static int  descender_score(void)   { return s_depth + s_kills * 2 + s_best_combo * 2; }

const game_module_t DESCENDER = {
    .id          = "descender",
    .title       = "DESCENDER",
    .min_players = 1,
    .scored      = true,
    .controls    = "[{\"w\":\"tilt\"},{\"w\":\"dpad\",\"dirs\":[\"left\",\"right\"]},"
                   "{\"w\":\"btn\",\"label\":\"FIRE\",\"ev\":\"primary\"}]",
    .reset       = descender_reset,
    .on_input    = descender_on_input,
    .tick        = descender_tick,
    .render      = descender_render,
    .is_over     = descender_is_over,
    .winner      = descender_winner,
    .score       = descender_score,
};
```

- [ ] **Step 3: Run host tests to verify they pass (GREEN)**

Run: `make -C test/host run`
Expected: PASS — `game_tests` includes the three descender checks; `proto_tests` and `gfx_tests`
still green. (No `-Werror` on the host build, but the code is also clean under
`-Wall -Werror -Wextra` for the ESP build: explicit float→int casts, no misleading indentation,
generously-sized `snprintf` buffers, all statics used.)

- [ ] **Step 4: Checkpoint (user)**

```bash
git add main/descender.h main/descender.c
git commit -m "feat: Descender — Downwell-like single-player faller"
```

---

## Task 3: Register Descender in the firmware

**Files:**
- Modify: `main/engine.c:11,26`
- Modify: `main/icons.h`
- Modify: `main/CMakeLists.txt`

- [ ] **Step 1: Include + register in the menu**

In `main/engine.c`, add the include after `#include "survivor.h"` (line 11):

```c
#include "descender.h"
```

And add `&DESCENDER` to the registry (line 26) — place it second so the two modern games lead the
menu:

```c
static const game_module_t *const s_games[] = { &SURVIVOR, &DESCENDER, &PONG, &SNAKE, &RACER, &TRON, &BREAKOUT };
```

- [ ] **Step 2: Add the menu icon**

In `main/icons.h`, add this block after the `ICON_SURVIVOR[32]` definition (a 16×16 downward
arrow):

```c
// A downward arrow — descend the well.
static const uint8_t ICON_DESCENDER[32] = {
    0x00,0x00, 0x01,0x80, 0x01,0x80, 0x01,0x80,
    0x01,0x80, 0x01,0x80, 0x01,0x80, 0x7F,0xFE,
    0x3F,0xFC, 0x1F,0xF8, 0x0F,0xF0, 0x07,0xE0,
    0x03,0xC0, 0x01,0x80, 0x00,0x00, 0x00,0x00,
};
```

And add its lookup branch in `icon_for_id`, right after the `"survivor"` line:

```c
    if (strcmp(id, "descender") == 0) return ICON_DESCENDER;
```

- [ ] **Step 3: Add to the build**

In `main/CMakeLists.txt`, add `"descender.c"` to the `SRCS` list (e.g. right after
`"survivor.c"`):

```cmake
    SRCS "fx.c" "gfx.c" "breakout.c" "survivor.c" "descender.c" "persist.c" "proto.c" "main.c" "engine.c" "pong.c" "snake.c" "racer.c" "tron.c" "display.c" "network.c"
```

- [ ] **Step 4: Verify host tests still pass**

Run: `make -C test/host run`
Expected: PASS (this task doesn't touch host-linked logic; it confirms nothing regressed).

- [ ] **Step 5: Checkpoint (user)**

```bash
git add main/engine.c main/icons.h main/CMakeLists.txt
git commit -m "engine: register Descender (menu + icon + build)"
```

---

## Task 4: Docs + hardware playtest handoff

**Files:** `README.md`, `ROADMAP.md`, `TEST_PLAN.md`

- [ ] **Step 1: README games table**

In `README.md`, add a row under the **Survivor** row of the Games table:

```markdown
| **Descender** | 1 | Tilt / ◀▶ + FIRE | Downwell-like faller: drop down an endless well, shoot downward to brake and kill, stomp enemies, chain combos, pick gun upgrades each depth |
```

- [ ] **Step 2: ROADMAP**

In `ROADMAP.md`, under the "Modernization program (M10+)" section, add after the M10 block:

```markdown
### R2 — Descender (Downwell-like faller)  *(code-complete; awaiting hardware playtest)*
- [x] **`descender.c`** — endless vertical faller: gravity + camera follow, **FIRE shoots
      downward** (recoil brakes the fall), **stomp** non-spiky enemies / **shoot** spiky ones,
      **combo** (kills without a hit), depth-segment **upgrade picks**, HP + i-frames, mini-boss.
      `score = depth + kills + best-combo`. Ships as pure firmware on the R1 descriptor system.
- [x] Host tests (mocked RNG): fresh-run state, depth scoring, passive-play attrition death.
- [ ] Hardware playtest (TEST_PLAN §14): joystick/tilt steer, FIRE + recoil, stomp vs spiky,
      combo, upgrade picks, death→score, and that it *feels* like a modern run.
```

- [ ] **Step 3: TEST_PLAN section**

Append to `TEST_PLAN.md`:

```markdown
## 14. Descender flagship (R2)

| # | Step | Expect |
|---|------|--------|
| 14.1 | Pick DESCENDER; tilt the phone (and ◀▶) | Ship steers left/right; it falls continuously down the scrolling well; depth (m) climbs in the HUD |
| 14.2 | Tap FIRE repeatedly | Bullets shoot **downward**; each shot gives a small upward **recoil** (you can brake/hover by firing) |
| 14.3 | Fall onto a **solid** enemy vs a **hollow/spiky** one | Solid = **stomp** (kill + bounce, combo++); hollow = must be **shot** (touching it costs HP) |
| 14.4 | Chain kills without being hit | Combo `xN` rises in the HUD; taking a hit resets it; brief i-frame blink + shake on damage |
| 14.5 | Descend past a depth segment | **UPGRADE** overlay with 3 picks; ◀ ▶ choose, FIRE/PICK applies; world pauses during the choice |
| 14.6 | Lose all HP | Run ends → GAME OVER with a score; high score saved (NVS) and shown |
| 14.7 | Overall feel | Reads as a modern "one more run": fast, juicy, escalating, readable at speed |
```

- [ ] **Step 4: Hand off to the user (agent stops here)**

Descender is firmware-only **and** the controller already renders its descriptor generically
(R1), so the game is fully playable. But its control descriptor lives in SPIFFS-independent
firmware while R1's renderer is in `index.html` (SPIFFS) — if R1 has **not** yet been flashed
over USB, the phone won't know how to draw the descriptor. Tell the user:

```bash
source $IDF_PATH/export.sh
idf.py build flash monitor     # USB flash (ensures the R1 controller page is on the device too)
```

Then playtest TEST_PLAN §14. Tuning is expected — the `TUNABLE` constants at the top of
`descender.c` (GRAV/VTERM/RECOIL/BOUNCE_0/HSPEED/BULLET_SPD, fire interval, spawn cadence,
spiky fraction) are the dials for game feel.

- [ ] **Step 5: Checkpoint (user)**

```bash
git add README.md ROADMAP.md TEST_PLAN.md
git commit -m "docs: Descender (R2) — README row, roadmap, TEST_PLAN §14"
```

---

## Self-review notes

- **Spec coverage (R2):** endless procedural well + camera follow ✅; fire downward + recoil
  brake ✅; head-stomp bounce ✅ (non-spiky); combo = kills without a hit ✅; spiky enemies that
  must be shot ✅; depth-segment 1-of-3 upgrade picks (gun mods) ✅; HP + i-frames + mini-boss ✅;
  `score = depth + kills + combo` ✅; tilt/◀▶ + FIRE descriptor ✅; host tests (reset, depth
  scoring, death) ✅; registry/icon/build/docs ✅.
- **Deviations (documented above):** `rng.h` deferred to M11 (esp_random is host-mockable);
  "combo resets on landing" → "resets on taking a hit" (no platforms in v1).
- **Type/name consistency:** `DESCENDER` extern matches `descender.h`/`descender.c`/`engine.c`/
  `test_games.c`; `frnd`/`clampf`/`kill_enemy`/`fire`/`screen_y`/`spawn_enemy`/`apply_upgrade`/
  `start_levelup` all defined before use and file-static. `controls` string is valid JSON for the
  R1 widget vocabulary (`tilt`, `dpad` left/right, `btn` FIRE→primary).
- **Host-test robustness:** depth test runs only 30 ticks (well under the 220 SEGMENT and before
  the first spawned enemy can reach the player) so it can't flake to over/level-up; attrition
  test clears upgrade picks each loop (so the sim never stalls in a paused level-up) and relies on
  spiky enemies (40% of non-boss spawns) to guarantee unavoidable damage to a passive player.
- **Warning cleanliness for `-Werror`:** explicit casts on every float→int into `gfx_*`; no
  implicit-fallthrough (every `switch` case breaks); no misleading indentation; `snprintf` buffers
  (`hud[20]`) far exceed the longest output.
```