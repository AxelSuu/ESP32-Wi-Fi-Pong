#include "snake.h"
#include "display.h"
#include "hw_config.h"
#include "esp_random.h"
#include <stdio.h>

// --- Snake tuning (lives here, not in a shared config) ---
#define CELL      6
#define COLS      (SCREEN_WIDTH  / CELL)   // 21
#define ROWS      (SCREEN_HEIGHT / CELL)   // 16
#define MAX_CELLS (COLS * ROWS)
#define STEP_MS   140
#define START_LEN 3

typedef struct { int8_t x, y; } cell_t;

// --- file-static state ---
static cell_t   s_body[MAX_CELLS + 1];   // +1 scratch slot for the shift
static int      s_len;
static int      s_dx, s_dy;              // applied heading
static int      s_ndx, s_ndy;            // pending heading (set by input)
static cell_t   s_food;
static bool     s_over;
static uint32_t s_acc;

static bool on_snake(int x, int y, int upto)
{
    for (int i = 0; i < upto; i++)
        if (s_body[i].x == x && s_body[i].y == y) return true;
    return false;
}

static void place_food(void)
{
    do {
        s_food.x = esp_random() % COLS;
        s_food.y = esp_random() % ROWS;
    } while (on_snake(s_food.x, s_food.y, s_len));
}

static void snake_reset(void)
{
    s_len = START_LEN;
    int cx = COLS / 2, cy = ROWS / 2;
    for (int i = 0; i < s_len; i++) {
        s_body[i].x = cx - i;
        s_body[i].y = cy;
    }
    s_dx = 1; s_dy = 0;
    s_ndx = 1; s_ndy = 0;
    s_over = false;
    s_acc  = 0;
    place_food();
}

static void snake_on_input(const input_event_t *ev)
{
    // Guard against 180-degree reversal by checking the applied heading.
    switch (ev->kind) {
    case INPUT_UP:    if (s_dy == 0) { s_ndx = 0;  s_ndy = -1; } break;
    case INPUT_DOWN:  if (s_dy == 0) { s_ndx = 0;  s_ndy = 1;  } break;
    case INPUT_LEFT:  if (s_dx == 0) { s_ndx = -1; s_ndy = 0;  } break;
    case INPUT_RIGHT: if (s_dx == 0) { s_ndx = 1;  s_ndy = 0;  } break;
    default: break;
    }
}

static void step(void)
{
    s_dx = s_ndx; s_dy = s_ndy;
    int nx = s_body[0].x + s_dx;
    int ny = s_body[0].y + s_dy;

    if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) { s_over = true; return; }

    bool grow  = (nx == s_food.x && ny == s_food.y);
    int  check = grow ? s_len : s_len - 1;   // tail vacates its cell when not growing
    if (on_snake(nx, ny, check)) { s_over = true; return; }

    for (int i = s_len; i > 0; i--) s_body[i] = s_body[i - 1];
    s_body[0].x = nx;
    s_body[0].y = ny;
    if (grow && s_len < MAX_CELLS) {
        s_len++;
        place_food();
    }
}

static void snake_tick(uint32_t dt_ms)
{
    if (s_over) return;
    s_acc += dt_ms;
    while (s_acc >= STEP_MS) {
        s_acc -= STEP_MS;
        step();
        if (s_over) return;
    }
}

static void snake_render(void)
{
    gfx_rect(s_food.x * CELL, s_food.y * CELL, CELL - 1, CELL - 1, 0x8);   // food (dim)
    for (int i = 0; i < s_len; i++)
        gfx_rect(s_body[i].x * CELL, s_body[i].y * CELL, CELL - 1, CELL - 1, 0xF);

    char s[12];
    snprintf(s, sizeof s, "%d", s_len - START_LEN);
    gfx_text(2, 2, s, 0xF);
}

static bool snake_is_over(void) { return s_over; }
static int  snake_winner(void)  { return -1; }
static int  snake_score(void)   { return s_len - START_LEN; }

const game_module_t SNAKE = {
    .id          = "snake",
    .title       = "SNAKE",
    .min_players = 1,
    .reset       = snake_reset,
    .on_input    = snake_on_input,
    .tick        = snake_tick,
    .render      = snake_render,
    .is_over     = snake_is_over,
    .winner      = snake_winner,
    .score       = snake_score,
};
