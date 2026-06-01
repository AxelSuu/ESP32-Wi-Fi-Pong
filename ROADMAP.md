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

## M1 — Neon Polish ✅ (code-complete; awaiting hardware playtest)

Goal: the OLED menu and the phone controller should look like a product, not a debug screen.
Direction: **neon arcade**. All items below are **implemented and in-tree**; tick the
release once they pass a playtest pass against `TEST_PLAN.md`, then tag `v0.2`.

- [x] **Game-over scores** — `score()` on the vtable + `score` in the `over` message;
      Snake/Racer show `GAME OVER / SCORE n` (not "DRAW"), Pong/Tron show the real result
- [x] **`gfx_bitmap` primitive** — 1-bpp sprite blit on top of `gfx_pixel` (`display.c`)
- [x] **Per-game icons** — `icons.h` 16×16 sprites for Pong/Snake/Racer/Tron
- [x] **OLED menu redesign** — header band, divider, highlight bar, per-game icons, `2P` tag,
      connected-phone count in the footer (`net_player_count()`)
- [x] **Idle / attract screen** — animated splash (`render_attract`, free-running `s_anim_ms`)
      when no phone is connected; names the AP and IP
- [x] **Controller redesign** — neon theme (glow, gradients, per-game art), offline-safe inline CSS
- [x] **Menu mirror (`screen` message)** — phone shows the live game list + highlight instead
      of blind up/down/select

## M2 — Productionize 🎯 (code-complete; awaiting hardware playtest; CLAUDE.md §7 + §8 step 8)

All code below is **implemented and in-tree**; tick the release once it passes a hardware
playtest against `TEST_PLAN.md`, then tag `v0.3`. (Brownout is a hardware-only check.)

- [x] Unique SoftAP SSID per unit (NVS `factory` id, fall back to efuse MAC) + show on screen
      — `net_derive_ssid()` / `net_ssid()` in `network.c`; `WIFI_SSID_PREFIX` in `net_config.h`;
      the menu footer + attract screen render the live SSID
- [x] OTA dual-slot partition table (`partitions.csv` → `ota_0`/`ota_1`/`otadata`); update path
      — `POST /update` streams a `.bin` into the inactive slot and reboots (`update_handler` in
      `network.c`); the controller has a ⚙ firmware-update overlay. `app_update` added to
      `PRIV_REQUIRES`.
- [ ] Brownout verification under Wi-Fi TX burst — **hardware-only** (detector stays enabled at
      the ESP-IDF default); see `TEST_PLAN.md`. Tick after a playtest with no brownout resets.
- [x] **Robust disconnect cleanup** — httpd `config.close_fn` (`ws_close_fn`) now clears the fd
      and fires `engine_on_player_disconnect` on *any* socket teardown, not just an explicit WS
      CLOSE frame, so a hard drop can't leave a stale fd (`net_player_count()` overcount / Tron
      round that won't end). Both paths funnel through the idempotent `ws_cleanup_fd()`.

## M3 — Backlog (unscheduled ideas)

- [ ] Persist high scores in NVS; show them on the menu / attract screen
- [ ] Sound/haptics via a buzzer (score, collision, game-over jingle)
- [ ] More games (Breakout, Simon, Flappy-style)
- [ ] On-device settings (difficulty, brightness) from the controller
- [ ] Spectator view / scoreboard for 2-player rounds

---

## Changelog

- _unreleased_ — M2 (productionize) code-complete: per-unit SoftAP SSID (`GameBox-XXXX` /
  NVS `factory`), dual-slot OTA partition table + `POST /update` upload path & controller
  overlay, robust `close_fn` disconnect cleanup. Awaiting a hardware playtest (incl. brownout
  under Wi-Fi TX) → tag `v0.3`.
- _unreleased_ — M1 (neon polish) code-complete: game-over scores, `gfx_bitmap` + per-game
  icons, redesigned OLED menu, idle/attract screen, neon controller, `screen` menu mirror.
  Awaiting a hardware playtest pass → tag `v0.2`.
- `v0.1` — M0: multi-game engine + morphing controller (Pong/Snake/Racer/Tron).
