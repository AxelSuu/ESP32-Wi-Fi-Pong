# 05 — Test Plan

Testing is **manual playtesting on real hardware** (build/flash/monitor by hand). This file
is the checklist to run before tagging a release and the acceptance criteria for new work.
Track features in [`ROADMAP.md`](ROADMAP.md).

## Setup

1. `idf.py build flash monitor`
2. On a phone/laptop, join Wi-Fi **`ESP32-Pong`**, password **`12345678`**.
3. Open **`http://192.168.4.1`**.

For 2-player tests (Tron, menu mirror sync) you need **two phones**.

---

## Smoke test (run first, every time)

- [ ] Device boots; OLED shows the menu (or attract screen if no phone connected).
- [ ] Phone connects; status goes green; `welcome` assigns a player slot.
- [ ] Serial monitor shows `WS client connected` and no resets/brownouts.

## Per-game acceptance

### Pong
- [ ] Up/Down move the paddle; ball bounces; AI tracks the ball.
- [ ] First to 3 ends the round; game-over names the winner.

### Snake
- [ ] Arrows steer; eating food grows the snake and the score increments.
- [ ] Dies on wall and on self; 180° instant reversal is blocked.
- [ ] Game-over shows `GAME OVER / SCORE n` (not "DRAW"); phone shows `Score: n`.

### Racer
- [ ] "ENABLE TILT" arms motion (iOS: permission prompt); tilting steers the car.
- [ ] Left/Right buttons steer as a fallback.
- [ ] Obstacles scroll, speed ramps, collision ends the run; score shown on game-over.

### Tron (2 phones)
- [ ] Selecting Tron with one phone shows `waiting 1/2`; second phone joining starts it.
- [ ] Left/Right turn each player; trails render in two shades.
- [ ] Crash into wall/trail ends the round; the survivor is named the winner.
- [ ] Mid-round disconnect awards the round to the remaining rider.

## Networking

- [ ] Kill Wi-Fi / background the phone → controller auto-reconnects (back-off) and re-syncs.
- [ ] `navigator.vibrate` buzzes on round end.
- [ ] **Menu mirror:** phone shows the live game list; the highlight matches the OLED as you
      nav; nav from one phone updates the other.

## Regression checklist (before each commit/tag)

- [ ] Smoke test passes.
- [ ] All four games launch from the menu and return to it (BACK / game-over → menu).
- [ ] No serial errors, asserts, or brownouts during a full play loop.
- [ ] Controller morphs correctly for each game and on reconnect.

---

## Results log

| Date | Firmware | Result | Notes |
|------|----------|--------|-------|
| | | | |
