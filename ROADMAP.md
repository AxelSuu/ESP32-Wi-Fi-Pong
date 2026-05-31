# 04 — Roadmap & Backlog

Feature tracking for the ESP32 Wi-Fi GameBox. Source of truth lives here, in-repo, beside
the code. Pair this with [`TEST_PLAN.md`](TEST_PLAN.md) for verification.

**Status legend:** `[ ]` todo · `[~]` in progress · `[x]` done

**How to scope new work:** jot any idea under **M3 — Backlog**. When you commit to it, move
the line up into a milestone, break it into concrete checks in `TEST_PLAN.md`, and only then
start coding.

**Workflow (solo):**
- Branch per feature off `main` (e.g. `feat/neon-menu`); short, conventional commit subjects.
- In the same commit that lands a feature: tick its box here **and** add/extend its check in
  `TEST_PLAN.md`.
- Tag playtested checkpoints `v0.x`; record them under **Changelog** below.
- Building/flashing/monitoring is always done by hand on real hardware (see `TEST_PLAN.md`).

---

## M0 — Multi-game engine ✅ (shipped)

- [x] Game-agnostic `gfx_*` display API; SSD1327 driver knows nothing about games
- [x] `engine.c` state machine: `MENU` / `PLAYING` / `GAMEOVER` + game registry + one mutex
- [x] `game_module_t` vtable; each game owns its state file-static
- [x] Pong, Snake, Racer (tilt), Tron (2-player) as modules
- [x] WebSocket protocol + `ws_broadcast`; fd→player mapping
- [x] Morphing web controller (`index.html`) with reconnect, tilt, vibrate

## M1 — Neon Polish 🎯 (current)

Goal: the OLED menu and the phone controller should look like a product, not a debug screen.
Direction: **neon arcade**.

- [ ] **Game-over scores** — add `score()` to the vtable + `score` to the `over` message;
      Snake/Racer show `GAME OVER / SCORE n` (not "DRAW"), Pong/Tron show the real result
- [ ] **`gfx_bitmap` primitive** — 1-bpp sprite blit on top of `gfx_pixel`
- [ ] **Per-game icons** — `icons.h` sprites for Pong/Snake/Racer/Tron
- [ ] **OLED menu redesign** — header band, divider, highlight bar, per-game icons, `2P` tag,
      connected-phone count in the footer (`net_player_count()`)
- [ ] **Idle / attract screen** — animated splash when no phone is connected; names the AP
- [ ] **Controller redesign** — neon theme (glow, gradients, per-game button art), offline-safe
- [ ] **Menu mirror (`screen` message)** — phone shows the live game list + highlight instead
      of blind up/down/select

## M2 — Productionize (deferred; CLAUDE.md §7–8 step 8)

- [ ] Unique SoftAP SSID per unit (NVS `factory` id, fall back to efuse MAC) + show on screen
- [ ] OTA dual-slot partition table; update path (menu action or `/update`)
- [ ] Brownout verification under Wi-Fi TX burst

## M3 — Backlog (unscheduled ideas)

- [ ] Persist high scores in NVS; show them on the menu / attract screen
- [ ] Sound/haptics via a buzzer (score, collision, game-over jingle)
- [ ] More games (Breakout, Simon, Flappy-style)
- [ ] On-device settings (difficulty, brightness) from the controller
- [ ] Spectator view / scoreboard for 2-player rounds

---

## Changelog

- _unreleased_ — M1 in progress.
- `v0.1` — M0: multi-game engine + morphing controller (Pong/Snake/Racer/Tron).
