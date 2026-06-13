#pragma once

#include "esp_err.h"
#include "gfx.h"   // drawing primitives live in gfx.c; games include display.h and get them

// SSD1327 panel driver: SPI bring-up + flushing the gfx framebuffer to the glass.
// All drawing is done through the gfx_* primitives (see gfx.h).

// Bring up the SSD1327 panel (SPI bus, GPIO, init sequence).
esp_err_t display_init(void);

// Flush the gfx framebuffer to the panel (single DMA transfer).
void display_present(void);

// Set panel contrast/brightness (0x00..0xFF). Shares the SPI bus with
// display_present(), so call it only from the render (game) task.
void display_set_contrast(uint8_t value);
