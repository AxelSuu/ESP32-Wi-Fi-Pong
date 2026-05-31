#pragma once

// --- Physical hardware configuration (screen + OLED SPI pins) ---
// Anything tied to the board lives here; game tuning never does.

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  96

#define OLED_CS    6
#define OLED_DC    5
#define OLED_RST   4
#define OLED_MOSI  11
#define OLED_SCLK  12
