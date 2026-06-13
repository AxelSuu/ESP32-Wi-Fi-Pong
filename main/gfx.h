#pragma once

#include <stdint.h>

// Game-agnostic 2D graphics drawing into an in-RAM framebuffer for the SSD1327
// (4-bit grayscale: 0x0 = black .. 0xF = white). Pure framebuffer math with no
// SPI/ESP-IDF dependency, so the whole toolkit is host-unit-tested
// (test/host/test_gfx.c). display.c flushes gfx_framebuffer() to the panel.

void    gfx_clear(uint8_t shade);                              // fill whole framebuffer
void    gfx_pixel(int x, int y, uint8_t shade);               // single pixel (honors origin)
uint8_t gfx_get_pixel(int x, int y);                          // read raw pixel (no origin), 0 if OOB

void gfx_rect(int x, int y, int w, int h, uint8_t shade);     // filled rectangle
void gfx_frame(int x, int y, int w, int h, uint8_t shade);    // 1px outline rectangle
void gfx_hline(int x, int y, int w, uint8_t shade);           // horizontal run
void gfx_vline(int x, int y, int h, uint8_t shade);           // vertical run
void gfx_line(int x0, int y0, int x1, int y1, uint8_t shade); // arbitrary line (Bresenham)
void gfx_circle(int cx, int cy, int r, uint8_t shade);        // filled circle

void gfx_text(int x, int y, const char *s, uint8_t shade);    // 5x7 font, 6px advance
void gfx_text_scaled(int x, int y, const char *s, uint8_t shade, int scale); // integer-scaled
int  gfx_text_width(const char *s, int scale);                // pixel width including advance

// Blit a 1-bit-per-pixel sprite: row-major, MSB-first, each row padded to whole
// bytes. Set bits drawn in `shade`; clear bits transparent.
void gfx_bitmap(int x, int y, int w, int h, const uint8_t *bits, uint8_t shade);

// Global draw offset added by gfx_pixel (and therefore every primitive). Used
// for screen-shake; call gfx_set_origin(0, 0) to reset.
void gfx_set_origin(int dx, int dy);

// Framebuffer access for the panel driver (display.c) and host tests.
const uint8_t *gfx_framebuffer(void);
int            gfx_framebuffer_size(void);
