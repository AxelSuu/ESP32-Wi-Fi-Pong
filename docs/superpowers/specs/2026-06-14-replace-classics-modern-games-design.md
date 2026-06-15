# Replace the classics — modern game lineup

**Date:** 2026-06-14
**Status:** approved (design); awaiting spec review → implementation plan
**Supersedes:** ROADMAP M13 ("evolve the classics"); pulls M14 forward as an enabler

## Problem

Survivor (M10) set a modern bar — continuous analog control, escalating swarms, XP/upgrade
progression, juice, a "one more run" loop. Against it the remaining classics (Pong, Snake,
Racer, Tron, Breakout) feel decades older: shallow, one-note, no progression. The goal is to
bring the whole lineup up to Survivor's quality by **replacing the shallow single-player
classics with deep, fast, interactive modern games**, while staying crisp at the hardware's
real limits.

**Hard constraints (the achievable bar):** 128×96 px, 16-grey, ~33 fps, phone-as-controller
over WebSocket. We cannot chase modern *fidelity*; we target **Game Boy / indie-demake**
quality where modern *design* lives — depth, juice, progression, escalation. "Modern" =
design + feel, not pixel count.

## Decisions (locked with the user)

1. **Keep the two-player games** (Pong, Tron) as quick local-versus staples — lightly juiced
   only if cheap. **Replace the three single-player classics** (Snake, Racer, Breakout) with
   three deep single-player "one more run" games at Survivor's bar.
2. **New trio** (maximizes genre + input + feel diversity from Survivor's twin-stick):
   - **Descender** — Downwell-like vertical shooter/faller.
   - **Block puzzler** — Tetris-like falling blocks.
   - **Endless runner/dodger** — Racer's true successor with depth.
3. **Controller descriptors first** (the M14 enabler): build a generic, descriptor-driven
   control surface once so each new game ships as pure firmware (no `index.html` edits).
4. **Build alongside, retire at the end**: keep the classics in the registry while building
   the new trio; delete them in a final cleanup once replacements are proven. Always shippable,
   no gap.
5. **Out of scope here:** audio (M12) and meta/coins (M11) remain separate future tracks.
   Audio is the single biggest *feel* multiplier and should follow this program.

## Final lineup

| Slot | Game | Players | Input | Status |
|------|------|---------|-------|--------|
| SP flagship | Survivor | 1 | joystick | exists (M10) |
| SP | **Descender** | 1 | tilt / ◀▶ + FIRE | new (R2) |
| SP | **Block puzzler** | 1 | ◀▶ + ROTATE + DROP | new (R3) |
| SP | **Endless runner** | 1 | tilt / ◀▶ | new (R4) |
| 2P | Pong | 1–2 | ▲▼ | keep |
| 2P | Tron | 2 | turns | keep |

Menu scrolling (M9) already handles a 6-game + `HIGH SCORES` list.

## Architecture this builds on

- **Game = one `game_module_t` vtable** (`main/game_module.h`): `id/title/min_players/scored`
  + `reset/on_input/tick/render/is_over/winner/score`; all mutable state file-static. Registry
  `s_games[]` in `engine.c`. 30 ms fixed-step loop.
- **Generic input** `input_event_t {kind, player, analog, analog2}`. Existing kinds
  (UP/DOWN/LEFT/RIGHT/PRIMARY/TILT/MOVE/NAV/SELECT/BACK) **already cover all four new games —
  no new input kinds are required.**
- **Drawing** via `gfx_*` (pure, host-tested) + transient **`fx_*`** (flash/shake/spark).
- **WS protocol** built in dependency-free `proto.c` (host-tested); `network.c` parses wire
  JSON → `input_event_t` and formats server→client messages.
- **Host test harness** (`test/host/`) links real `gfx.c`, stubs `display/fx/esp`, and has a
  mockable RNG + clock.

---

## Program: five independently-shippable phases

Each phase ends green on host tests and is handed to the user for a hardware playtest with a
new/extended `TEST_PLAN.md` section. Renumbering note: this program **redefines M13** and
**absorbs M14** (descriptors) as its first phase.

### R1 — Controller descriptors (the enabler)

Today each game hand-rolls a `<div data-mode>` + CSS/JS surface in `index.html`. Replace that
with a **widget-stack descriptor** carried by the `active` message and rendered generically.

- **New vtable field:** `const char *controls;` on `game_module_t` — a small JSON-array string
  the game author writes once. Example (Descender):
  ```json
  [{"w":"tilt"},{"w":"dpad","dirs":["left","right"]},{"w":"btn","label":"FIRE","ev":"primary"}]
  ```
- **Widget vocabulary (small, closed):**
  - `joystick` → `move` (x,y)
  - `tilt` → `tilt` (g), with a Left/Right button fallback
  - `dpad` with a `dirs` subset of `up/down/left/right` → `input`
  - `btn` with `label` + `ev` (one of `up/down/left/right/primary`) → `input`
  - MENU (`back`), connection chrome, ⚙ overlay stay global (not in the descriptor).
- **Wire:** `active` gains a `controls` field; `network.c` embeds the active game's `controls`
  string verbatim. Add/extend the `proto` formatter for `active` and host-test it (valid JSON,
  field present).
- **Frontend:** one `renderControls(spec)` builds the surface from the widget array; **all
  per-game `data-mode` branches deleted.** New game ⇒ write the `controls` string in its `.c`,
  zero `index.html` edits.
- **Proof:** convert the existing games (Survivor joystick, Pong ▲▼, Tron turns, and the
  to-be-retired Snake/Racer/Breakout) to descriptor-driven and confirm every one still controls
  correctly. This validates the system before any new game depends on it.

### Shared infrastructure (lands in R1/R2, reused after)

- **`rng.h` — seedable xorshift32** for procedural content (wells, obstacle fields, the 7-bag).
  Deterministic seeding makes the procedural games **host-testable**; `esp_random` stays for
  true entropy and is the default seed on hardware. Foundation for future daily-seed challenges.
- Reuse existing `gfx_*`, `fx_*`, and the **nav/select choice-overlay pattern** Survivor already
  uses for level-up picks (no shared module needed — each game draws its own overlay).

### R2 — Descender (Downwell-like)

Free-fall down an endless procedural well; the camera scrolls down with the player.

- **Core loop:** gravity pulls the player down; **FIRE shoots downward** to kill enemies and
  brake the fall (recoil = brief upward impulse). Head-**stomp** bounces off enemies. Side walls
  + gaps + platforms generated procedurally from the seeded RNG in vertical segments.
- **Combo:** kills while airborne increment a combo; **landing resets it**. Combo feeds score.
- **Progression:** between segments, a **1-of-3 upgrade pick** (gun mods: machinegun / shotgun /
  laser, plus stat mods) via the nav/select overlay; world pauses during the choice.
- **Survival:** HP hearts, brief i-frames + shake/flash on damage; run ends at 0 HP.
- **Score:** `depth + kills + combo bonus`.
- **Input:** tilt or ◀▶ to move horizontally; PRIMARY = fire. `controls`:
  `[{"w":"tilt"},{"w":"dpad","dirs":["left","right"]},{"w":"btn","label":"FIRE","ev":"primary"}]`
- **Juice:** muzzle flash (`fx_spark`), shake on land/hit, flash on damage.
- **Host tests (seeded well):** gravity/fall integration, a shot kills an enemy, combo
  increments airborne and resets on ground, HP→0 ends the run, depth/score monotonic.

### R3 — Block puzzler (Tetris-like)

- **Layout:** 8×16 cell grid on one side; HUD (next / hold / level / lines / score) on the
  other. Greys distinguish the 7 piece types.
- **Mechanics:** 7-bag randomizer, rotate, soft drop, **hard drop**, hold, ghost piece, line
  clears, gravity speed ramps by level, combo / back-to-back bonus, top-out = game over.
- **Score:** classic line/level/combo scoring; `scored=true`.
- **Input:** ◀▶ move, ROTATE (UP or PRIMARY), soft-drop (DOWN), HARD DROP (a `btn`). `controls`:
  a `dpad` with `left/right/down` + `btn` ROTATE + `btn` DROP.
- **Host tests (seeded bag):** bag exhaustion/refill, rotation within bounds, piece lock,
  single + tetris line clears, gravity step at a given level, top-out detection, scoring math —
  fully deterministic, the highest-coverage game.

### R4 — Endless runner / dodger

- **Core loop:** the world scrolls; procedural lane obstacles approach; the player dodges. Speed
  ramps with distance. **Near-miss** awards bonus. Pickups: coin, shield, slow-mo.
- **Progression:** **1-of-3 perk pick** at distance milestones (e.g. magnet, extra shield,
  score multiplier) via the nav/select overlay.
- **Score:** `distance` (+ near-miss / pickup bonuses).
- **Input:** tilt or ◀▶. `controls`: `[{"w":"tilt"},{"w":"dpad","dirs":["left","right"]}]`.
- **Reuse:** Racer's scrolling + speed-streak drawing as the visual base; crash shake, pickup
  spark.
- **Host tests (seeded field):** obstacle collision ends the run, pickup effects apply,
  distance/speed ramp with time, near-miss bonus triggers without collision.

### R5 — Retire the classics + docs

Once the trio is proven on hardware:

- Delete `snake.c/.h`, `racer.c/.h`, `breakout.c/.h`; remove their registry entries, `icons.h`
  sprites, and host tests (`test/host`).
- Update `README.md` (games table, protocol notes), `ROADMAP.md` (mark R-phases, drop M13
  "evolve"), `TEST_PLAN.md` (remove §4 classic rows, keep the new per-game sections), `CLAUDE.md`
  if any text references the removed games.
- Confirm `make -C test/host run` and the menu (6 games + HIGH SCORES) are clean.

---

## Testing strategy

- **Host (agent runs):** `make -C test/host run` stays green throughout. Each new game adds a
  deterministic logic suite (seeded RNG). The `active`/`controls` formatter gets a `proto` test.
- **Hardware (user runs):** per-phase `idf.py build flash monitor`; a new `TEST_PLAN.md` section
  per game; R1 re-verifies every existing game through the descriptor surface. **The agent never
  builds/flashes/monitors** — it edits, runs host tests, and hands off (per CLAUDE.md).

## Risks & notes

- **Keep the decoupled vtable pattern**: each new game owns its pools file-static; no engine
  god-objects; draw via `gfx_*` only.
- **Performance:** dozens of tiny entities + per-frame procedural stepping at 240 MHz / ~33 fps
  is fine; cap pool sizes, no per-frame allocation, watch the task watchdog (M5).
- **SPIFFS reflash:** R1 changes `index.html` (SPIFFS) → needs a USB flash, not OTA. After R1,
  new games are pure firmware and OTA-deliverable (control surface already generic).
- **Descriptor scope creep:** keep the widget vocabulary closed (joystick/tilt/dpad/btn). If a
  future game needs something exotic, extend the vocabulary deliberately, don't grow a
  mini-language in the vtable string.
- **Multi-phase program:** R1+R2 alone is a meaningful release (proves the bar + the enabler).
  Reassess priorities after Descender ships.
