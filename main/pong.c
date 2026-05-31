#include "pong.h"
#include "display.h"
#include "hw_config.h"
#include <stdio.h>
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"

static const char *TAG = "pong";

// --- Pong tuning (was in config.h) ---
#define PADDLE_SPEED        10
#define PADDLE_WIDTH        4
#define PADDLE_HEIGHT       20
#define BALL_RADIUS         3
#define BALL_INITIAL_SPEED_X 4
#define BALL_INITIAL_SPEED_Y 1
#define WIN_SCORE           3

#define AI_MIN_REACTION_DELAY   250
#define AI_MAX_REACTION_DELAY   500
#define AI_TARGET_OFFSET_RANGE  10
#define AI_MOVE_SPEED           2
#define AI_MISTAKE_CHANCE       10
#define SPEED_INCREASE_INTERVAL 10000

typedef struct { int x, y, w, h; } paddle_t;
typedef struct { int x, y, dx, dy, r; } ball_t;

// --- file-static game state ---
static paddle_t s_player;
static paddle_t s_enemy;
static ball_t   s_ball;
static int      s_player_score;
static int      s_enemy_score;

static int64_t  s_last_speed_increase;
static int64_t  s_last_ai_update;
static int      s_ai_reaction_delay;
static int      s_ai_target_offset;

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void reset_ball(void)
{
    s_ball.x  = SCREEN_WIDTH / 2;
    s_ball.y  = SCREEN_HEIGHT / 2;
    s_ball.dx = BALL_INITIAL_SPEED_X;
    s_ball.dy = (esp_random() % 2 == 0) ? BALL_INITIAL_SPEED_Y : -BALL_INITIAL_SPEED_Y;
    s_ball.r  = BALL_RADIUS;
}

static void pong_reset(void)
{
    s_player = (paddle_t){5,                30, PADDLE_WIDTH, PADDLE_HEIGHT};
    s_enemy  = (paddle_t){SCREEN_WIDTH - 9, 30, PADDLE_WIDTH, PADDLE_HEIGHT};
    s_player_score = 0;
    s_enemy_score  = 0;
    reset_ball();
    int64_t t             = now_ms();
    s_last_speed_increase = t;
    s_last_ai_update      = t;
    s_ai_reaction_delay   = AI_MIN_REACTION_DELAY +
                            (int)(esp_random() % (AI_MAX_REACTION_DELAY - AI_MIN_REACTION_DELAY));
    s_ai_target_offset    = 0;
}

static void pong_on_input(const input_event_t *ev)
{
    if (ev->player != 0) return;          // Pong: only player 0 controls
    if (ev->kind == INPUT_UP)   s_player.y -= PADDLE_SPEED;
    if (ev->kind == INPUT_DOWN) s_player.y += PADDLE_SPEED;
    if (s_player.y < 0) s_player.y = 0;
    if (s_player.y + s_player.h > SCREEN_HEIGHT) s_player.y = SCREEN_HEIGHT - s_player.h;
}

static bool pong_is_over(void)
{
    return s_player_score >= WIN_SCORE || s_enemy_score >= WIN_SCORE;
}

static int pong_winner(void)
{
    if (s_player_score >= WIN_SCORE) return 0;
    if (s_enemy_score  >= WIN_SCORE) return 1;
    return -1;
}

static int pong_score(void)
{
    return -1;
}

static void pong_tick(uint32_t dt_ms)
{
    (void)dt_ms;
    int64_t now = now_ms();

    // Ball speed increase every SPEED_INCREASE_INTERVAL ms
    if (now - s_last_speed_increase >= SPEED_INCREASE_INTERVAL) {
        s_ball.dx += (s_ball.dx > 0) ? 1 : -1;
        s_last_speed_increase = now;
        ESP_LOGI(TAG, "Ball speed increased: dx=%d", s_ball.dx);
    }

    s_ball.x += s_ball.dx;
    s_ball.y += s_ball.dy;

    // Wall collisions (top/bottom)
    if (s_ball.y - s_ball.r <= 0 || s_ball.y + s_ball.r >= SCREEN_HEIGHT) {
        s_ball.dy *= -1;
    }

    // Player paddle collision
    if (s_ball.x - s_ball.r <= s_player.x + s_player.w &&
        s_ball.y >= s_player.y &&
        s_ball.y <= s_player.y + s_player.h &&
        s_ball.dx < 0) {
        s_ball.dx *= -1;
    }

    // Enemy paddle collision
    if (s_ball.x + s_ball.r >= s_enemy.x &&
        s_ball.y >= s_enemy.y &&
        s_ball.y <= s_enemy.y + s_enemy.h &&
        s_ball.dx > 0) {
        s_ball.dx *= -1;
    }

    // Scoring
    if (s_ball.x < 0) {
        s_enemy_score++;
        if (s_enemy_score >= WIN_SCORE) return;   // engine moves to game-over
        reset_ball();
        s_last_speed_increase = now_ms();
    } else if (s_ball.x > SCREEN_WIDTH) {
        s_player_score++;
        if (s_player_score >= WIN_SCORE) return;
        reset_ball();
        s_last_speed_increase = now_ms();
    }

    // AI decision update
    if (now - s_last_ai_update > (int64_t)s_ai_reaction_delay) {
        s_last_ai_update    = now;
        s_ai_reaction_delay = AI_MIN_REACTION_DELAY +
                              (int)(esp_random() % (AI_MAX_REACTION_DELAY - AI_MIN_REACTION_DELAY));
        s_ai_target_offset  = (int)(esp_random() % (2 * AI_TARGET_OFFSET_RANGE + 1)) - AI_TARGET_OFFSET_RANGE;
    }

    // AI movement (only when ball approaches)
    if (s_ball.dx > 0) {
        int enemy_center = s_enemy.y + s_enemy.h / 2;
        int target_y     = s_ball.y + s_ai_target_offset;

        if (target_y < enemy_center - 3) {
            s_enemy.y -= AI_MOVE_SPEED;
        } else if (target_y > enemy_center + 3) {
            s_enemy.y += AI_MOVE_SPEED;
        }

        if ((int)(esp_random() % 100) < AI_MISTAKE_CHANCE) {
            s_enemy.y += (int)(esp_random() % 13) - 6;
        }
    }

    // Clamp paddles
    if (s_enemy.y < 0)                                s_enemy.y = 0;
    if (s_enemy.y + s_enemy.h > SCREEN_HEIGHT)        s_enemy.y = SCREEN_HEIGHT - s_enemy.h;
    if (s_player.y < 0)                               s_player.y = 0;
    if (s_player.y + s_player.h > SCREEN_HEIGHT)      s_player.y = SCREEN_HEIGHT - s_player.h;
}

static void pong_render(void)
{
    gfx_rect(s_player.x, s_player.y, s_player.w, s_player.h, 0xF);
    gfx_rect(s_enemy.x,  s_enemy.y,  s_enemy.w,  s_enemy.h,  0xF);
    gfx_circle(s_ball.x, s_ball.y, s_ball.r, 0xF);

    char s[4];
    snprintf(s, sizeof(s), "%d", s_player_score);
    gfx_text(30, 5, s, 0xF);
    snprintf(s, sizeof(s), "%d", s_enemy_score);
    gfx_text(90, 5, s, 0xF);

    // Dashed center line
    for (int y = 0; y < SCREEN_HEIGHT; y += 4) {
        gfx_pixel(SCREEN_WIDTH / 2, y, 0xF);
    }
}

const game_module_t PONG = {
    .id          = "pong",
    .title       = "PONG",
    .min_players = 1,
    .reset       = pong_reset,
    .on_input    = pong_on_input,
    .tick        = pong_tick,
    .render      = pong_render,
    .is_over     = pong_is_over,
    .winner      = pong_winner,
    .score       = pong_score,
};
