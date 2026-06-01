## ESP32 Wi-Fi GameBox

A wireless, self-contained multi-game console running on an ESP32-S3 with an
SSD1327 grayscale LED. The ESP32 hosts its own Wi-Fi network and serves a web
controller that reshapes itself per game, pick a game from the on-device menu and
your phone turns into the right control surface. No app install required.

<table>
  <tr>
    <td><img src="imgs/pic1.jpeg"></td>
    <td><img src="imgs/pic2.jpeg"></td>
  </tr>
</table>

<table>
  <tr>
    <td><img src="imgs/wifi.png"></td>
    <td><img src="imgs/Websocket_controller.png"></td>
  </tr>
</table>

## Games

| Game  | Players | Controller            | How it plays |
|-------|---------|-----------------------|--------------|
| **Pong**  | 1 | Up / Down + Menu       | Classic paddle vs adaptive AI; first to 3 |
| **Snake** | 1 | 4-way arrows           | Eat food, grow, don't bite yourself or the walls |
| **Racer** | 1 | Tilt (+ Left/Right)    | Top-down dodge-the-traffic; **tilt your phone** to steer |
| **Tron**  | 2 | Left / Right turns     | Two light-cycles, two trail shades; last one riding wins |

Single-player games end on a crash (your score is your distance/length). Tron needs **two
phones connected** — selecting it shows a "waiting for players" screen until the second
phone joins, and a mid-round disconnect awards the round to the remaining rider.

## Hardware Requirements

- **ESP32-S3** development board (8 MB flash; uses a custom partition table)
- **SSD1327 OLED display** (128×128 panel, top 128×96 used, 4-bit grayscale, SPI)
- Jumper wires

## Display Wiring (SPI)

| SSD1327 Pin | ESP32-S3 GPIO |
|-------------|---------------|
| CS          | GPIO 6        |
| DC          | GPIO 5        |
| RST         | GPIO 4        |
| MOSI (SDA)  | GPIO 11       |
| SCLK (SCL)  | GPIO 12       |
| VCC         | 3.3V          |
| GND         | GND           |

Pins live in [`main/hw_config.h`](main/hw_config.h).

## How It Works

1. **Power on** the ESP32 — it creates a Wi-Fi access point with a **per-unit SSID**
   (`GameBox-XXXX`, where `XXXX` is from the board's MAC; pw `12345678`), so two consoles in
   one room never collide. With no phone connected the OLED runs an **attract screen** that
   names the exact AP and IP.
2. **Connect** your phone/laptop to that network. The OLED switches to the game menu — each
   game has its own icon, the selected row is highlighted, and a footer dot shows each phone.
3. **Open a browser** at `http://192.168.4.1`.
4. **Navigate the menu** with Up/Down + SELECT on the phone; the OLED highlights the choice.
5. **Play!** The phone swaps to that game's controls automatically. MENU returns you.

## Architecture

```
                 ┌───────────────────────────────────────────────┐
                 │                  engine.c                       │
                 │  app state machine: MENU / PLAYING / GAMEOVER   │
                 │  game registry[] + active-game vtable pointer   │
                 │  engine_update(dt) → active->tick / is_over     │
                 │  engine_dispatch_input(event) → menu or game    │
                 └───────┬───────────────────────────┬────────────┘
                         │ game_module_t vtable        │ gfx_* primitives
        ┌────────────────┴────────────┐        ┌───────┴───────────────┐
        │ pong / snake / racer / tron  │  draws │   display.c (gfx_*)    │
        │ each owns its state file-     │ ─────► │  SSD1327 driver + FB   │
        │ static; no global game struct │        │  (no game knowledge)   │
        └────────────────▲─────────────┘        └────────────────────────┘
                         │ generic input_event_t
                 ┌───────┴─────────────────┐
                 │        network.c          │
                 │  WS JSON → input_event    │
                 │  fd→player slot mapping   │
                 │  ws_broadcast() (server→  │
                 │  client: welcome/active/  │
                 │  waiting/over)            │
                 └───────────────────────────┘
```

- A **game is one `game_module_t` vtable** (`reset` / `on_input` / `tick` / `render` /
  `is_over` / `winner`) defined in its own `.c`, with all mutable state file-static.
- **`display.c` knows nothing about games** — it exposes generic `gfx_*` primitives
  (`gfx_clear/pixel/rect/circle/text`, `display_present`). The engine draws the menu &
  game-over card; each game draws its own world.
- **One mutex** (`s_engine_mutex`) guards the active game. The engine takes it around
  `tick`/`on_input`, so games never lock. Renderers read unlocked (one torn frame is OK).
- **Input is generic.** `network.c` parses the WS JSON into an `input_event_t`
  (`kind`, `player`, `analog`) and hands it to the engine — no game-specific parsing.

## WebSocket Protocol

Small flat JSON over `/ws`. `t` = message type.

**Client → Server**

| `t` | Fields | Meaning |
|-----|--------|---------|
| `hello`  | —                          | sent on connect; server replies `welcome` |
| `nav`    | `dir`: `-1` / `1`          | move menu highlight |
| `select` | —                          | launch highlighted game / play again |
| `back`   | —                          | return to menu / dismiss game-over |
| `input`  | `ev`: `up`/`down`/`left`/`right`/`primary` | discrete game input |
| `tilt`   | `g`: float (degrees)       | analog steering (Racer) |

**Server → Client**

| `t` | Fields | Meaning |
|-----|--------|---------|
| `welcome` | `player`: 0/1                       | assigned player slot |
| `screen`  | `mode`:`menu`, `games`:[labels], `idx`:n | menu state → **phone mirrors the OLED menu** |
| `active`  | `game`: id, `players`: n            | a game launched → **phone morphs its controls** |
| `waiting` | `need`: n, `have`: n                | 2-player game waiting for the second phone |
| `over`    | `winner`: -1/0/1, `score`: int      | round ended (`winner` -1 = none/draw; `score` -1 = n/a) |

The `active` message drives the controller morph — the phone JS swaps its control surface
based on `game`. The `screen` message mirrors the on-device menu so the phone shows the live
game list with the highlighted entry (instead of blind Up/Down). On `over`, single-player
games (Snake/Racer) report a `score` and the phone shows `Score: n`. The web controller keeps
an exponential-backoff reconnect and buzzes (`navigator.vibrate`) on input and round end.


## Building & Flashing

1. **Install ESP-IDF** (v6.0; the project targets ESP32-S3).
2. **Set up the environment:**
   ```bash
   source $IDF_PATH/export.sh
   ```
3. **Build, flash & monitor:**
   ```bash
   idf.py build flash monitor
   ```

The web controller (`spiffs_image/index.html`) is packed into the `storage` SPIFFS
partition automatically during the build.

## Updating firmware (OTA)

The flash uses a **dual-slot OTA partition table** (`ota_0` / `ota_1` / `otadata` in
`partitions.csv`), so the firmware can be updated wirelessly without a USB cable:

1. Build a new image (`idf.py build`) — the app binary is `build/<project>.bin`.
2. On the controller page, tap the **⚙** button (top-right), choose that `.bin`, and tap
   **FLASH**. The phone POSTs it to `/update`; the device streams it into the inactive OTA
   slot, marks it bootable, and reboots into the new image. The controller then reconnects.

A bad/truncated upload is rejected (`image invalid`) and the currently-running slot is left
untouched. The first USB flash still uses `idf.py flash`.

## Tilt on iOS

iOS Safari requires a user gesture before granting motion access. On the Racer screen tap
**"ENABLE TILT"** once — it calls `DeviceOrientationEvent.requestPermission()` and then
streams throttled (~25 Hz) `tilt` messages. On Android / desktop tilt arms immediately; the
Left/Right buttons always work as a fallback.
