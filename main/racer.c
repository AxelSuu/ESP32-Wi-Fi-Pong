#include "racer.h"
#include "display.h"
#include "hw_config.h"
#include "esp_random.h"
#include <stdio.h>

// --- Racer tuning ---
#define CAR_W       12
#define CAR_H       14
#define OBST_MAX    8
#define OBST_H      12
#define STEER_BTN   10            // px nudge per button press
#define STEER_GAIN  0.35f         // px per degree of tilt, per tick
#define SPAWN_MS    650
#define START_SPEED 2
#define SPEED_RAMP  600           // score per +1 scroll speed

typedef struct { int x, y, w; bool active; } obst_t;

// --- file-static state ---
static int      s_car_x;
static int      s_car_y;
static float    s_tilt;           // latest gamma (degrees) from the phone
static obst_t   s_obst[OBST_MAX];
static int      s_speed;
static uint32_t s_spawn_acc;
static int      s_lane_off;
static long     s_score;
static bool     s_over;

static void clamp_car(void)
{
    if (s_car_x < 0)                       s_car_x = 0;
    if (s_car_x > SCREEN_WIDTH - CAR_W)    s_car_x = SCREEN_WIDTH - CAR_W;
}

static void racer_reset(void)
{
    s_car_x     = (SCREEN_WIDTH - CAR_W) / 2;
    s_car_y     = SCREEN_HEIGHT - CAR_H - 2;
    s_tilt      = 0;
    s_speed     = START_SPEED;
    s_spawn_acc = 0;
    s_lane_off  = 0;
    s_score     = 0;
    s_over      = false;
    for (int i = 0; i < OBST_MAX; i++) s_obst[i].active = false;
}

static void racer_on_input(const input_event_t *ev)
{
    switch (ev->kind) {
    case INPUT_TILT:  s_tilt = ev->analog; break;
    case INPUT_LEFT:  s_car_x -= STEER_BTN; clamp_car(); break;
    case INPUT_RIGHT: s_car_x += STEER_BTN; clamp_car(); break;
    default: break;
    }
}

static void spawn_obst(void)
{
    for (int i = 0; i < OBST_MAX; i++) {
        if (!s_obst[i].active) {
            s_obst[i].w      = 14 + (esp_random() % 30);
            s_obst[i].x      = esp_random() % (SCREEN_WIDTH - s_obst[i].w);
            s_obst[i].y      = -OBST_H;
            s_obst[i].active = true;
            return;
        }
    }
}

static bool overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static void racer_tick(uint32_t dt_ms)
{
    if (s_over) return;

    s_car_x += (int)(s_tilt * STEER_GAIN);   // tilt steering
    clamp_car();

    s_lane_off = (s_lane_off + s_speed) % 16;

    for (int i = 0; i < OBST_MAX; i++) {
        if (!s_obst[i].active) continue;
        s_obst[i].y += s_speed;
        if (s_obst[i].y > SCREEN_HEIGHT) { s_obst[i].active = false; continue; }
        if (overlap(s_car_x, s_car_y, CAR_W, CAR_H,
                    s_obst[i].x, s_obst[i].y, s_obst[i].w, OBST_H)) {
            s_over = true;
            return;
        }
    }

    s_spawn_acc += dt_ms;
    if (s_spawn_acc >= SPAWN_MS) { s_spawn_acc = 0; spawn_obst(); }

    s_score++;
    s_speed = START_SPEED + (int)(s_score / SPEED_RAMP);
}

static void racer_render(void)
{
    gfx_rect(0, 0, 2, SCREEN_HEIGHT, 0x6);                 // left edge
    gfx_rect(SCREEN_WIDTH - 2, 0, 2, SCREEN_HEIGHT, 0x6);  // right edge

    for (int y = s_lane_off - 16; y < SCREEN_HEIGHT; y += 16)
        gfx_rect(SCREEN_WIDTH / 2 - 1, y, 2, 8, 0x5);      // center lane dashes

    for (int i = 0; i < OBST_MAX; i++)
        if (s_obst[i].active)
            gfx_rect(s_obst[i].x, s_obst[i].y, s_obst[i].w, OBST_H, 0x9);

    gfx_rect(s_car_x, s_car_y, CAR_W, CAR_H, 0xF);         // player car

    char s[16];
    snprintf(s, sizeof s, "%ld", s_score / 10);
    gfx_text(4, 4, s, 0xF);
}

static bool racer_is_over(void) { return s_over; }
static int  racer_winner(void)  { return -1; }

const game_module_t RACER = {
    .id          = "racer",
    .title       = "RACER",
    .min_players = 1,
    .reset       = racer_reset,
    .on_input    = racer_on_input,
    .tick        = racer_tick,
    .render      = racer_render,
    .is_over     = racer_is_over,
    .winner      = racer_winner,
};
