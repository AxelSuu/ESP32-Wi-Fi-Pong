#include "fx.h"
#include "gfx.h"
#include "hw_config.h"

#define MAX_SPARKS   24
#define SPARK_BURST  8
#define FLASH_FRAMES 3

typedef struct { int x, y, vx, vy, life; } spark_t;

static spark_t  s_spark[MAX_SPARKS];
static int      s_flash;
static int      s_shake_frames, s_shake_amp;
static uint32_t s_rng = 0xC0FFEE11u;

static int rnd(int n)
{
    s_rng = s_rng * 1103515245u + 12345u;
    return n > 0 ? (int)((s_rng >> 16) % (unsigned)n) : 0;
}

void fx_reset(void)
{
    for (int i = 0; i < MAX_SPARKS; i++) s_spark[i].life = 0;
    s_flash = 0;
    s_shake_frames = 0;
    gfx_set_origin(0, 0);
}

void fx_flash(void) { s_flash = FLASH_FRAMES; }

void fx_shake(int amplitude, int frames)
{
    s_shake_amp    = amplitude;
    s_shake_frames = frames;
}

void fx_spark(int x, int y)
{
    int spawned = 0;
    for (int i = 0; i < MAX_SPARKS && spawned < SPARK_BURST; i++) {
        if (s_spark[i].life > 0) continue;
        s_spark[i].x    = x;
        s_spark[i].y    = y;
        s_spark[i].vx   = rnd(7) - 3;
        s_spark[i].vy   = rnd(7) - 4;
        s_spark[i].life = 8 + rnd(8);
        spawned++;
    }
}

void fx_update(void)
{
    if (s_flash > 0) s_flash--;

    int ox = 0, oy = 0;
    if (s_shake_frames > 0) {
        s_shake_frames--;
        ox = rnd(2 * s_shake_amp + 1) - s_shake_amp;
        oy = rnd(2 * s_shake_amp + 1) - s_shake_amp;
    }
    gfx_set_origin(ox, oy);

    for (int i = 0; i < MAX_SPARKS; i++) {
        if (s_spark[i].life <= 0) continue;
        s_spark[i].x += s_spark[i].vx;
        s_spark[i].y += s_spark[i].vy;
        s_spark[i].vy += 1;                 // gravity
        s_spark[i].life--;
    }
}

void fx_draw(void)
{
    for (int i = 0; i < MAX_SPARKS; i++)
        if (s_spark[i].life > 0) gfx_pixel(s_spark[i].x, s_spark[i].y, 0xF);

    // Oversized so a concurrent shake can't reveal an unflashed edge.
    if (s_flash > 0) gfx_rect(-4, -4, SCREEN_WIDTH + 8, SCREEN_HEIGHT + 8, 0xF);
}
