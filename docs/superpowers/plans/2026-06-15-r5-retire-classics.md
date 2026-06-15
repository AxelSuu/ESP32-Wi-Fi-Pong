# R5 — Retire the Classics (Snake / Racer / Breakout)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development or superpowers:executing-plans. Steps use `- [ ]` checkboxes.

> **PROJECT RULE:** Per `CLAUDE.md`, the **agent never commits, builds, flashes, or monitors
> hardware.** Loop: edit → `make -C test/host run` → hand off. "Checkpoint" steps are **user** actions.

> **GATE — do not start until the user confirms the new trio (Descender/Blocks/Runner) plays well
> on hardware (TEST_PLAN §13–§16).** This phase **deletes working games**; it is intentionally
> held until the replacements are proven. If the user wants to keep one classic, adjust the final
> lineup before running.

**Goal:** Remove Snake, Racer, and Breakout now that Survivor + Descender + Blocks + Runner cover
single-player depth. Final lineup: **Survivor, Descender, Blocks, Runner, Pong, Tron** (6 games).
Pong and Tron stay (local 2-player).

**Architecture:** Pure subtraction. The decoupled vtable design means removal is: drop the
registry entries + includes, the icons, the build sources, the host tests, then delete the files,
then update docs. No remaining code depends on the three games (verified: references live only in
`engine.c`, `icons.h`, `CMakeLists.txt`, and the host harness).

---

## File structure (what changes)
- `main/engine.c` — remove 3 includes + 3 registry entries.
- `main/CMakeLists.txt` — remove `"snake.c" "racer.c" "breakout.c"` from `SRCS`.
- `main/icons.h` — remove `ICON_SNAKE`/`ICON_RACER`/`ICON_BREAKOUT` + their 3 `icon_for_id` branches.
- `test/host/Makefile` — remove `snake.c`/`breakout.c` from `GAME_SRCS` (racer was never host-built).
- `test/host/test_games.c` — remove the SNAKE/BREAKOUT externs, their 5 tests, and `main()` calls.
- **Delete:** `main/snake.c` `main/snake.h` `main/racer.c` `main/racer.h` `main/breakout.c` `main/breakout.h`.
- `README.md` / `ROADMAP.md` / `TEST_PLAN.md` / `CLAUDE.md` / `main/game_module.h` — doc cleanup.

---

## Task 1: De-reference from the firmware build

**Files:** `main/engine.c`, `main/CMakeLists.txt`, `main/icons.h`

- [ ] **Step 1:** In `main/engine.c`, delete these three include lines:

```c
#include "snake.h"
#include "racer.h"
#include "breakout.h"
```

- [ ] **Step 2:** In `main/engine.c`, replace the registry line:

```c
static const game_module_t *const s_games[] = { &SURVIVOR, &DESCENDER, &PUZZLER, &RUNNER, &PONG, &TRON };
```

- [ ] **Step 3:** In `main/CMakeLists.txt`, remove `"snake.c"`, `"racer.c"`, `"breakout.c"` from
  the `SRCS` list. Result:

```cmake
    SRCS "fx.c" "gfx.c" "survivor.c" "descender.c" "puzzler.c" "runner.c" "persist.c" "proto.c" "main.c" "engine.c" "pong.c" "tron.c" "display.c" "network.c"
```

- [ ] **Step 4:** In `main/icons.h`, delete the three icon definitions `ICON_SNAKE[32]`,
  `ICON_RACER[32]`, `ICON_BREAKOUT[32]` (each its comment + 5 lines), **and** their three
  `icon_for_id` branches:

```c
    if (strcmp(id, "snake")    == 0) return ICON_SNAKE;
    if (strcmp(id, "racer")    == 0) return ICON_RACER;
    if (strcmp(id, "breakout") == 0) return ICON_BREAKOUT;
```

- [ ] **Step 5:** Run `make -C test/host run` → still PASS. (Host tests compile `snake.c`/
  `breakout.c` directly and don't include `engine.c`/`icons.h`, so this step doesn't affect them
  yet — it just confirms nothing else broke.)

- [ ] **Step 6 (user):** `git add main/engine.c main/CMakeLists.txt main/icons.h && git commit -m "engine: drop Snake/Racer/Breakout from the lineup"`

---

## Task 2: Remove the classics from the host test harness

**Files:** `test/host/Makefile`, `test/host/test_games.c`

- [ ] **Step 1:** In `test/host/Makefile` line 13, remove `$(MAIN)/snake.c` and
  `$(MAIN)/breakout.c` from `GAME_SRCS`. Result:

```make
GAME_SRCS  := $(MAIN)/tron.c $(MAIN)/pong.c $(MAIN)/survivor.c $(MAIN)/descender.c $(MAIN)/puzzler.c $(MAIN)/runner.c $(MAIN)/gfx.c stubs.c test_games.c
```

- [ ] **Step 2:** In `test/host/test_games.c`, delete the two externs:

```c
extern const game_module_t SNAKE;
extern const game_module_t BREAKOUT;
```

- [ ] **Step 3:** In `test/host/test_games.c`, delete the five test functions in their entirety:
  `test_snake_reversal_rejected`, `test_snake_eats_food`, `test_snake_wall_death`,
  `test_breakout_breaks_a_brick`, `test_breakout_game_over_loses_lives` (and the
  `// --- Snake … ---` / `// --- Breakout … ---` section comments above them).

- [ ] **Step 4:** In `main()`, delete these five calls:

```c
    test_snake_reversal_rejected();
    test_snake_eats_food();
    test_snake_wall_death();
    test_breakout_breaks_a_brick();
    test_breakout_game_over_loses_lives();
```

(Keep the Tron and Pong tests — those games stay.)

- [ ] **Step 5:** Run `make -C test/host run` → PASS. The suite now covers survivor, descender,
  puzzler, runner, tron, pong, and the menu helper. No snake/breakout references remain.

- [ ] **Step 6 (user):** `git add test/host && git commit -m "test: drop Snake/Breakout host tests"`

---

## Task 3: Delete the orphaned source files

**Files:** delete `main/snake.{c,h}`, `main/racer.{c,h}`, `main/breakout.{c,h}`

- [ ] **Step 1:** Confirm nothing references them anymore:

Run: `grep -rniE "snake|racer|breakout" main/ test/host/ | grep -viE "/(snake|racer|breakout)\.(c|h):"`
Expected: only the incidental comment in `main/game_module.h` (the wire-id example) and
`main/persist.c` (the NVS-key-length comment). No `#include`, no `&SNAKE/&RACER/&BREAKOUT`, no
`ICON_*`, no `GAME_SRCS` entry.

- [ ] **Step 2:** Delete the six files:

```bash
rm main/snake.c main/snake.h main/racer.c main/racer.h main/breakout.c main/breakout.h
```

- [ ] **Step 3:** Run `make -C test/host run` → PASS (files are gone and nothing references them).

- [ ] **Step 4 (user):** `git add -A main/ && git commit -m "feat: retire Snake, Racer, Breakout"`

---

## Task 4: Documentation cleanup

**Files:** `README.md`, `ROADMAP.md`, `TEST_PLAN.md`, `CLAUDE.md`, `main/game_module.h`

- [ ] **Step 1:** `README.md` — delete the three games-table rows:

```markdown
| **Snake** | 1 | 4-way arrows           | Eat food, grow, don't bite yourself or the walls |
| **Racer** | 1 | Tilt (+ Left/Right)    | Top-down dodge-the-traffic; **tilt your phone** to steer |
| **Breakout** | 1 | Tilt / Left / Right | Knock out the brick wall; 3 balls; clearing the board wins |
```

Then fix the prose just below the table — replace the sentence that begins
`Single-player games end on a crash (your score is your distance/length/bricks)` with:

```markdown
Single-player games (Survivor, Descender, Blocks, Runner) end on a run-ending mistake and the
**high score per game is saved** (NVS), shown on the game-over screen, and listed on a **HIGH
SCORES** menu entry (with a device-side reset).
```

(Leave the Pong/Tron/attract/demo sentences intact.)

- [ ] **Step 2:** `ROADMAP.md` — add an R5 block after the R4 block:

```markdown
### R5 — Retire the classics  *(code-complete; awaiting hardware playtest)*
- [x] Removed **Snake / Racer / Breakout** (sources, icons, registry, host tests). Final lineup:
      **Survivor, Descender, Blocks, Runner, Pong, Tron** (6 games). The "Replace the classics"
      program (R1–R5) is complete: a generic controller, three modern single-player flagships, and
      the two local-versus staples kept.
- [ ] Hardware playtest: full menu (6 games + HIGH SCORES) cycles cleanly; no dangling references.
```

- [ ] **Step 3:** `TEST_PLAN.md` — three edits:
  - In **§3.1**, change "A list of the **four** games is drawn" to "A list of the games is drawn".
  - In **§4**, delete rows **4.2 (Snake)**, **4.3 (Racer)**, **4.5 (Breakout)**. Keep 4.1 (Pong),
    4.4 (Tron), 4.6 (Any → MENU), 4.7 (Any → PLAY AGAIN).
  - In **§10**, delete rows **10.4 (Snake)**, **10.6 (Racer)**, **10.7 (Breakout)** (they describe
    removed games). Keep the Pong/Tron/menu/readability rows.

- [ ] **Step 4:** `CLAUDE.md` — update the lineup sentence near the top (under "What this is").
  Replace:

```markdown
Six games — Survivor (single-stick roguelite), Pong (1- or 2-player), Snake,
Racer (tilt), Tron (2-player), and Breakout — run on a small
in-firmware engine, and the controller page **reshapes itself per game**.
```

  with:

```markdown
Six games — Survivor (single-stick roguelite), Descender (Downwell-like faller), Blocks
(Tetris-like), Runner (tilt dodger), Pong (1- or 2-player), and Tron (2-player) — run on a small
in-firmware engine, and the controller page **reshapes itself per game** via a generic control
descriptor.
```

- [ ] **Step 5:** `main/game_module.h` — update the incidental comment on the `id` field
  (cosmetic; keeps examples valid):

```c
    const char *id;            // stable wire id: "survivor","descender","pong","tron"
```

- [ ] **Step 6:** Final grep sweep — confirm no stale references outside intended comments:

Run: `grep -rniE "snake|racer|breakout" main/ test/host/ README.md ROADMAP.md TEST_PLAN.md CLAUDE.md`
Expected: at most the `persist.c` NVS-key comment (harmless; optionally reword to mention
`hs_descender` (12 chars) as the longest key). Anything else should be removed.

- [ ] **Step 7:** Run `make -C test/host run` → PASS.

- [ ] **Step 8 (user):** `git add -A && git commit -m "docs: retire the classics; final 6-game lineup"`

---

## Self-review notes
- **Scope:** pure subtraction; the vtable decoupling means no logic changes to surviving games.
- **Reference audit (done at plan time):** the only code references to snake/racer/breakout are
  in `engine.c`, `icons.h`, `CMakeLists.txt`, and the host harness — all removed in Tasks 1–3.
  Remaining mentions are comments (`game_module.h`, `persist.c`), handled in Task 4.
- **Host coverage after:** survivor, descender, puzzler, runner, tron, pong, menu — still green.
- **NVS:** removed games' high-score keys simply go unused; the HIGH SCORES table iterates the live
  registry, so they drop off automatically. No migration needed.
- **Menu:** 6 games + HIGH SCORES; the M9 scrolling window already handles this length.
- **Reversibility:** until the user commits, deletions are recoverable via `git checkout`/`git
  restore`; the GATE ensures the trio is hardware-proven before this runs.
```