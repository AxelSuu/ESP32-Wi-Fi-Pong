## Esp32 wifi pong

Embedded wireless pong with SPI, Wi-Fi communication and OLED peripheral in C++.
Full wireless architecture combining ESP32 Access Point mode with web server + real time WebSocket controller.

## Hardware

**Microcontroller:**
- ESP32-S3-DevKitC, 8 MB Flash, 8 MB PSRAM

**OLED Screen:**
- 128 x 96 pixels (SSD1327)

**Pin Definitions:**
| Pin | Function |
|-----|----------|
| 6   | OLED_CS  |
| 5   | OLED_DC  |
| 4   | OLED_RST |
| 11  | OLED_MOSI|
| 12  | OLED_SCLK|

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

## How to Play

1. Connect to Wi-Fi network: **ESP32-Pong** (password: `12345678`)
2. Open browser and go to `192.168.4.1`
3. Press **START GAME** to begin
4. Use **Up** / **Down** buttons to control your paddle
5. First to 3 points wins

## Setup Instructions

1. Install [PlatformIO](https://platformio.org/install)
2. Clone this repository
3. Check if ESP32-S3 is on port COM3, otherwise change in `platformio.ini`
4. Build and upload firmware: `pio run -t upload`
5. Upload web files to SPIFFS: `pio run -t uploadfs`

## Library Dependencies

- Adafruit GFX Library
- Adafruit SSD1327
- links2004 WebSockets
