#include "tron.h"
#include "display.h"
#include "hw_config.h"

// --- Tron tuning ---
#define CELL    4
#define COLS    (SCREEN_WIDTH  / CELL)   // 32
#define ROWS    (SCREEN_HEIGHT / CELL)   // 24
#define STEP_MS 90

// Trail shade per player (kept below 0xF so the bright heads stand out).
static const uint8_t PLAYER_SHADE[2] = { 0xA, 0x6 };

typedef struct { int x, y, dx, dy; bool alive; } cycle_t;

// --- file-static state ---
static uint8_t  s_grid[COLS * ROWS];   // 0 = empty, else (player + 1)
static cycle_t  s_p[2];
static uint32_t s_acc;
static bool     s_over;
static int      s_winner;

static inline uint8_t *cell(int x, int y) { return &s_grid[y * COLS + x]; }

static void tron_reset(void)
{
    for (int i = 0; i < COLS * ROWS; i++) s_grid[i] = 0;

    s_p[0] = (cycle_t){ COLS / 4,       ROWS / 2,  1, 0, true };
    s_p[1] = (cycle_t){ COLS - COLS / 4, ROWS / 2, -1, 0, true };
    for (int i = 0; i < 2; i++) *cell(s_p[i].x, s_p[i].y) = (uint8_t)(i + 1);

    s_acc    = 0;
    s_over   = false;
    s_winner = -1;
}

static void tron_on_input(const input_event_t *ev)
{
    int i = ev->player;
    if (i < 0 || i > 1 || !s_p[i].alive) return;

    int dx = s_p[i].dx, dy = s_p[i].dy;
    if (ev->kind == INPUT_LEFT)  { s_p[i].dx = dy;  s_p[i].dy = -dx; }  // turn CCW
    if (ev->kind == INPUT_RIGHT) { s_p[i].dx = -dy; s_p[i].dy = dx;  }  // turn CW
}

static void step(void)
{
    int nx[2], ny[2];
    bool dead[2] = { false, false };

    for (int i = 0; i < 2; i++) {
        if (!s_p[i].alive) continue;
        nx[i] = s_p[i].x + s_p[i].dx;
        ny[i] = s_p[i].y + s_p[i].dy;
        if (nx[i] < 0 || nx[i] >= COLS || ny[i] < 0 || ny[i] >= ROWS ||
            *cell(nx[i], ny[i]) != 0) {
            dead[i] = true;
        }
    }

    // Head-on collision into the same cell kills both.
    if (s_p[0].alive && s_p[1].alive &&
        nx[0] == nx[1] && ny[0] == ny[1]) {
        dead[0] = dead[1] = true;
    }

    for (int i = 0; i < 2; i++) {
        if (!s_p[i].alive) continue;
        if (dead[i]) {
            s_p[i].alive = false;
        } else {
            s_p[i].x = nx[i];
            s_p[i].y = ny[i];
            *cell(nx[i], ny[i]) = (uint8_t)(i + 1);
        }
    }

    int alive = (s_p[0].alive ? 1 : 0) + (s_p[1].alive ? 1 : 0);
    if (alive <= 1) {
        s_over   = true;
        s_winner = s_p[0].alive ? 0 : (s_p[1].alive ? 1 : -1);
    }
}

static void tron_tick(uint32_t dt_ms)
{
    if (s_over) return;
    s_acc += dt_ms;
    while (s_acc >= STEP_MS) {
        s_acc -= STEP_MS;
        step();
        if (s_over) return;
    }
}

static void tron_render(void)
{
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++) {
            uint8_t c = *cell(x, y);
            if (c)
                gfx_rect(x * CELL, y * CELL, CELL - 1, CELL - 1, PLAYER_SHADE[c - 1]);
        }

    // Bright heads with a halo so each rider's leading edge is obvious.
    for (int i = 0; i < 2; i++) {
        if (!s_p[i].alive) continue;
        int hx = s_p[i].x * CELL, hy = s_p[i].y * CELL;
        gfx_rect(hx, hy, CELL - 1, CELL - 1, 0xF);
        gfx_frame(hx - 1, hy - 1, CELL + 1, CELL + 1, 0xF);
    }
}

static bool tron_is_over(void) { return s_over; }
static int  tron_winner(void)  { return s_winner; }
static int  tron_score(void)   { return -1; }

const game_module_t TRON = {
    .id          = "tron",
    .title       = "TRON",
    .min_players = 2,
    .reset       = tron_reset,
    .on_input    = tron_on_input,
    .tick        = tron_tick,
    .render      = tron_render,
    .is_over     = tron_is_over,
    .winner      = tron_winner,
    .score       = tron_score,
};
