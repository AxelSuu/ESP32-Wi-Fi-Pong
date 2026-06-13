// Host stubs for the game-logic test build. The real gfx.c is linked (so games
// draw into a real framebuffer harmlessly and new primitives never break the
// link), but the panel driver (display.c), the effects layer (fx.c), and the
// ESP-IDF clock/RNG are stubbed. The clock and RNG are test-controlled.
#include <stdint.h>
#include "mock.h"
#include "display.h"   // pulls gfx.h; for esp_err_t / ESP_OK
#include "fx.h"

// --- monotonic clock (esp_timer) ---
static int64_t s_clock_us = 0;
int64_t esp_timer_get_time(void)         { return s_clock_us; }
void    mock_clock_reset(void)           { s_clock_us = 0; }
void    mock_clock_advance_us(int64_t d) { s_clock_us += d; }

// --- RNG (esp_random): queued values first, then a deterministic LCG ---
static uint32_t s_lcg = 0x12345678u;
static uint32_t s_queue[64];
static int      s_qhead, s_qtail;
void mock_random_reset(void)      { s_qhead = s_qtail = 0; s_lcg = 0x12345678u; }
void mock_random_push(uint32_t v) { s_queue[s_qtail++ & 63] = v; }
uint32_t esp_random(void)
{
    if (s_qhead < s_qtail) return s_queue[s_qhead++ & 63];
    s_lcg = s_lcg * 1103515245u + 12345u;
    return s_lcg;
}

// --- panel driver (display.c) — not exercised in logic tests ---
esp_err_t display_init(void)              { return ESP_OK; }
void display_present(void)                { }
void display_set_contrast(uint8_t value)  { (void)value; }

// --- effects (fx.c) — no-ops so game tick() can trigger juice freely ---
void fx_reset(void)                       { }
void fx_flash(void)                       { }
void fx_shake(int amplitude, int frames)  { (void)amplitude; (void)frames; }
void fx_spark(int x, int y)               { (void)x; (void)y; }
void fx_update(void)                      { }
void fx_draw(void)                        { }
