# 05 — Test Plan

Testing is **manual playtesting on real hardware** (build/flash/monitor by hand). This file
is the checklist to run before tagging a release and the acceptance criteria for new work.
Track features in [`ROADMAP.md`](ROADMAP.md).

## Setup

1. `idf.py build flash monitor`
2. On a phone/laptop, join the unit's Wi-Fi — a per-unit **`GameBox-XXXX`** (the exact SSID is
   shown on the OLED attract/menu screen and logged at boot), password **`12345678`**.
3. Open **`http://192.168.4.1`**.

For 2-player tests (Tron, menu mirror sync) you need **two phones**.

---

## Smoke test (run first, every time)

- [ ] Device boots; OLED shows the menu (or attract screen if no phone connected).
- [ ] **Vertical alignment:** menu/games sit flush against the top of the OLED — no blank band
      at the top, and the bottom menu item (Tron) is fully visible, not clipped off the bottom.
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

## M1 — Neon Polish acceptance (gate before tagging `v0.2`)

- [ ] **Attract screen:** with no phone connected, the OLED runs the animated splash (bouncing
      ball) and names the AP + `192.168.4.1`; connecting the first phone switches to the menu.
- [ ] **Menu art:** each row shows its icon + title; the selected row has the highlight bar /
      bright accent; Tron shows the `2P` tag; the footer shows the SSID and one dot per phone.
- [ ] **Game-over scores:** Snake/Racer show `GAME OVER / SCORE n` (never "DRAW"); Pong/Tron
      name the winner; the phone game-over card shows the matching `Score: n` when applicable.
- [ ] **Menu mirror:** the phone lists the four games and highlights the same entry as the OLED;
      navigating on one phone updates the list on every connected phone.
- [ ] **Neon controller:** the page renders fully offline (no CDN), buttons glow, and the
      morph/reconnect/tilt/vibrate behaviours from M0 still work.

## M2 — Productionize acceptance (gate before tagging `v0.3`)

- [ ] **Per-unit SSID:** boot logs `WiFi AP started: SSID=GameBox-XXXX`; the OLED attract +
      menu footer show that same SSID; the phone joins it and reaches `192.168.4.1`. Two units
      powered together advertise **different** SSIDs (no collision).
- [ ] **OTA partition table:** `idf.py partition-table` validates; `idf.py size` shows the app
      fitting an `ota_0`/`ota_1` slot; the device boots normally from `ota_0` after a fresh flash.
- [ ] **OTA update path:** tap **⚙** on the controller, pick a freshly-built `app.bin`, **FLASH**;
      the page reports "rebooting", the device resets into the new image (serial shows boot from
      the *other* OTA slot), and the controller reconnects. A truncated/garbage file is rejected
      ("image invalid") without bricking the running slot.
- [ ] **Robust disconnect cleanup:** with a phone connected, **kill its Wi-Fi / background it**
      (no graceful close). Serial shows the slot freed; the OLED footer phone-dot count drops; a
      2-player **Tron** round in progress ends and awards the survivor (no stale fd → no hung
      round, no `net_player_count()` overcount on the next launch).
- [ ] **Brownout (hardware):** play a full loop incl. Wi-Fi TX bursts (many phones, OTA upload);
      **no brownout resets** in the serial log. Detector stays enabled at the IDF default.

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
