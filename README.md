Embedded wireless pong with SPI, Wi-Fi communication and OLED peripheral in C++.
Full wireless architecture combining ESP32 Access Point mode with web server + real time WebSocket controller.

<table>
  <tr>
    <td><img src="bild3.png"/></td>
    <td><img src="Esp32_S3.png"/></td>
    <td><img src="Websocket_controller.png"/></td>
  </tr>
</table>

Microcontroller:
- ESP32-S3-DevKitC, 8 MB Flash, 8 MB PSRAM

Oled screen:
- SCREEN_WIDTH: 128
- SCREEN_HEIGHT: 96

Pin definitions:
- #define OLED_CS    6
- #define OLED_DC    5
- #define OLED_RST   4
- #define OLED_MOSI  11
- #define OLED_SCLK  12

library dependencies:
- Adafruit NeoPixel
- Adafruit GFX Library
- Adafruit SSD1327
- links2004 WebSockets
