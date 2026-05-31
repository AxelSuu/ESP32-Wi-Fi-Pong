#pragma once

#include "esp_err.h"
#include <stdint.h>

// Bring up the SSD1327 panel (SPI bus, GPIO, init sequence).
esp_err_t display_init(void);

// --- Game-agnostic graphics primitives ---
// All shades are 4-bit (0x0 = black .. 0xF = full white) on the grayscale panel.
void gfx_clear(uint8_t shade);                              // fill whole framebuffer
void gfx_pixel(int x, int y, uint8_t shade);               // single pixel
void gfx_rect(int x, int y, int w, int h, uint8_t shade);  // filled rectangle
void gfx_circle(int cx, int cy, int r, uint8_t shade);     // filled circle
void gfx_text(int x, int y, const char *s, uint8_t shade); // 5x7 string, 6px advance

// Flush the framebuffer to the panel (single DMA transfer).
void display_present(void);
