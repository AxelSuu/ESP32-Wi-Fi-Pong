#include "breakout.h"
#include "display.h"
#include "fx.h"
#include "hw_config.h"
#include <stdio.h>

// --- Breakout tuning (reuses Pong-style integer ball physics) ---
#define COLS        8
#define ROWS        4
#define BW          (SCREEN_WIDTH / COLS)   // 16
#define BH          5
#define BRICK_TOP   12
#define MAX_HP      3
#define PADDLE_W    24
#define PADDLE_H    3
#define PADDLE_Y    (SCREEN_HEIGHT - 8)     // 88
#define PADDLE_STEP 8
#define BALL_R      2
#define BALL_SPEED  2
#define START_LIVES 3

typedef struct { int x, y, dx, dy; } ball_t;

// --- file-static state ---
static uint8_t s_brick[ROWS][COLS];   // hit-points remaining (0 = gone)
static int     s_remaining;
static int     s_level;
static int     s_px;          // paddle left edge
static ball_t  s_ball;
static int     s_score;
static int     s_lives;
static bool    s_over;

static void launch_ball(void)
{
    s_ball.x  = s_px + PADDLE_W / 2;
    s_ball.y  = PADDLE_Y - BALL_R - 2;
    s_ball.dx = BALL_SPEED;
    s_ball.dy = -BALL_SPEED;
}

// Build a wall: lower rows are weak, upper rows tougher; odd levels skip every
// other brick for a different shape. HP rises slightly with the level.
static void build_level(int level)
{
    s_remaining = 0;
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            bool gap = (level % 2 == 1) && ((r + c) & 1);
            if (gap) { s_brick[r][c] = 0; continue; }
            int hp = 1 + (ROWS - 1 - r) + (level - 1) / 2;
            if (hp > MAX_HP) hp = MAX_HP;
            s_brick[r][c] = (uint8_t)hp;
            s_remaining++;
        }
    }
}

static void breakout_reset(void)
{
    s_level  = 1;
    s_score  = 0;
    s_lives  = START_LIVES;
    s_over   = false;
    s_px     = (SCREEN_WIDTH - PADDLE_W) / 2;
    build_level(s_level);
    launch_ball();
}

static void clamp_paddle(void)
{
    if (s_px < 0) s_px = 0;
    if (s_px + PADDLE_W > SCREEN_WIDTH) s_px = SCREEN_WIDTH - PADDLE_W;
}

static void breakout_on_input(const input_event_t *ev)
{
    if (ev->player != 0) return;             // single-player
    if (ev->kind == INPUT_LEFT)  s_px -= PADDLE_STEP;
    if (ev->kind == INPUT_RIGHT) s_px += PADDLE_STEP;
    if (ev->kind == INPUT_TILT) {
        float t = ev->analog;                // gamma degrees (~-45..45)
        if (t < -45) t = -45;
        if (t >  45) t =  45;
        s_px = (int)((t + 45) / 90.0f * (SCREEN_WIDTH - PADDLE_W));
    }
    clamp_paddle();
}

static void breakout_tick(uint32_t dt_ms)
{
    (void)dt_ms;
    if (s_over) return;

    s_ball.x += s_ball.dx;
    s_ball.y += s_ball.dy;

    // Side + top walls.
    if (s_ball.x - BALL_R <= 0)            { s_ball.x = BALL_R; s_ball.dx = -s_ball.dx; }
    if (s_ball.x + BALL_R >= SCREEN_WIDTH) { s_ball.x = SCREEN_WIDTH - BALL_R; s_ball.dx = -s_ball.dx; }
    if (s_ball.y - BALL_R <= 0)            { s_ball.y = BALL_R; s_ball.dy = -s_ball.dy; }

    // Brick collision (one brick per step is enough for this ball size).
    if (s_ball.y >= BRICK_TOP && s_ball.y < BRICK_TOP + ROWS * BH) {
        int col = s_ball.x / BW;
        int row = (s_ball.y - BRICK_TOP) / BH;
        if (row >= 0 && row < ROWS && col >= 0 && col < COLS && s_brick[row][col] > 0) {
            s_brick[row][col]--;
            s_score++;
            s_ball.dy = -s_ball.dy;
            if (s_brick[row][col] == 0) {
                s_remaining--;
                fx_spark(col * BW + BW / 2, BRICK_TOP + row * BH + BH / 2);
                if (s_remaining == 0) {            // board cleared → next level
                    s_level++;
                    build_level(s_level);
                    launch_ball();
                    return;
                }
            }
        }
    }

    // Paddle bounce — deflection depends on where the ball strikes.
    if (s_ball.dy > 0 &&
        s_ball.y + BALL_R >= PADDLE_Y && s_ball.y + BALL_R <= PADDLE_Y + PADDLE_H + 2 &&
        s_ball.x >= s_px && s_ball.x <= s_px + PADDLE_W) {
        s_ball.dy = -s_ball.dy;
        int offset = s_ball.x - (s_px + PADDLE_W / 2);   // -12..12
        s_ball.dx  = offset / 5;                          // -2..2
        if (s_ball.dx == 0) s_ball.dx = (offset < 0) ? -1 : 1;
    }

    // Ball lost below the paddle.
    if (s_ball.y - BALL_R > SCREEN_HEIGHT) {
        if (--s_lives <= 0) { s_over = true; return; }
        launch_ball();
    }
}

static void breakout_render(void)
{
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            uint8_t hp = s_brick[r][c];
            if (!hp) continue;
            int x = c * BW, y = BRICK_TOP + r * BH;
            if (hp >= 2) gfx_rect(x, y, BW - 1, BH - 1, 0xF);     // solid
            else         gfx_frame(x, y, BW - 1, BH - 1, 0xF);    // cracked (1 hit left)
        }
    }

    gfx_rect(s_px, PADDLE_Y, PADDLE_W, PADDLE_H, 0xF);
    gfx_circle(s_ball.x, s_ball.y, BALL_R, 0xF);

    char s[16];
    snprintf(s, sizeof s, "%d", s_score);
    gfx_text(2, 2, s, 0xF);
    snprintf(s, sizeof s, "L%d", s_level);
    gfx_text(SCREEN_WIDTH / 2 - 6, 2, s, 0x9);
    for (int i = 0; i < s_lives; i++)
        gfx_circle(SCREEN_WIDTH - 6 - i * 7, 4, 2, 0xC);
}

static bool breakout_is_over(void) { return s_over; }
static int  breakout_winner(void)  { return -1; }
static int  breakout_score(void)   { return s_score; }

const game_module_t BREAKOUT = {
    .id          = "breakout",
    .title       = "BREAKOUT",
    .min_players = 1,
    .reset       = breakout_reset,
    .on_input    = breakout_on_input,
    .tick        = breakout_tick,
    .render      = breakout_render,
    .is_over     = breakout_is_over,
    .winner      = breakout_winner,
    .score       = breakout_score,
};
