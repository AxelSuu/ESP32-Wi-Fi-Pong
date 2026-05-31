#include "engine.h"
#include "display.h"
#include "hw_config.h"
#include "net_config.h"
#include "network.h"
#include "pong.h"
#include "snake.h"
#include "racer.h"
#include "tron.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef enum { APP_MENU, APP_PLAYING, APP_GAMEOVER } app_state_t;

// Game registry — order is the menu order.
static const game_module_t *const s_games[] = { &PONG, &SNAKE, &RACER, &TRON };
#define N_GAMES ((int)(sizeof(s_games) / sizeof(s_games[0])))

static int                  s_menu_idx;
static const game_module_t *s_active;
static const game_module_t *s_pending;  // 2-player game awaiting enough phones
static app_state_t          s_app;
static int                  s_last_winner = -1;
static SemaphoreHandle_t    s_engine_mutex;

void engine_init(void)
{
    s_engine_mutex = xSemaphoreCreateMutex();
    s_menu_idx = 0;
    s_active   = NULL;
    s_pending  = NULL;
    s_app      = APP_MENU;
}

static int wrap_idx(int i)
{
    if (i < 0)         i = N_GAMES - 1;
    if (i >= N_GAMES)  i = 0;
    return i;
}

// --- must be called with the engine mutex held ---
static void try_launch(const game_module_t *g)
{
    int have = net_player_count();
    if (have < g->min_players) {
        s_pending = g;
        net_broadcast_waiting(g->min_players, have);
        return;                              // stay in menu until enough phones
    }
    s_pending = NULL;
    s_active  = g;
    s_active->reset();
    s_app     = APP_PLAYING;
    net_broadcast_active(g->id, g->min_players);
}

void engine_update(uint32_t dt_ms)
{
    xSemaphoreTake(s_engine_mutex, portMAX_DELAY);
    if (s_app == APP_PLAYING && s_active) {
        s_active->tick(dt_ms);
        if (s_active->is_over()) {
            s_last_winner = s_active->winner();
            s_app = APP_GAMEOVER;
            net_broadcast_over(s_last_winner);
        }
    }
    xSemaphoreGive(s_engine_mutex);
}

void engine_dispatch_input(const input_event_t *ev)
{
    xSemaphoreTake(s_engine_mutex, portMAX_DELAY);
    switch (s_app) {
    case APP_MENU:
        if (ev->kind == INPUT_NAV) {
            int dir = (ev->analog < 0) ? -1 : 1;
            s_menu_idx = wrap_idx(s_menu_idx + dir);
        } else if (ev->kind == INPUT_SELECT || ev->kind == INPUT_PRIMARY) {
            try_launch(s_games[s_menu_idx]);
        }
        break;
    case APP_PLAYING:
        if (ev->kind == INPUT_BACK) {
            s_app = APP_MENU;
            s_pending = NULL;
        } else if (s_active) {
            s_active->on_input(ev);
        }
        break;
    case APP_GAMEOVER:
        if (ev->kind == INPUT_SELECT || ev->kind == INPUT_BACK ||
            ev->kind == INPUT_PRIMARY) {
            s_app = APP_MENU;
        }
        break;
    }
    xSemaphoreGive(s_engine_mutex);
}

void engine_on_player_connect(int player)
{
    (void)player;
    xSemaphoreTake(s_engine_mutex, portMAX_DELAY);
    if (s_pending && s_app == APP_MENU &&
        net_player_count() >= s_pending->min_players) {
        try_launch(s_pending);
    }
    xSemaphoreGive(s_engine_mutex);
}

void engine_on_player_disconnect(int player)
{
    xSemaphoreTake(s_engine_mutex, portMAX_DELAY);
    if (s_app == APP_PLAYING && s_active && s_active->min_players >= 2) {
        // The remaining player wins the round.
        s_last_winner = (player == 0) ? 1 : 0;
        s_app = APP_GAMEOVER;
        net_broadcast_over(s_last_winner);
    }
    xSemaphoreGive(s_engine_mutex);
}

// --- rendering (runs unlocked; snapshot volatile-ish fields once) ---

static void render_menu(void)
{
    gfx_text(28, 4, "GAME BOX", 0xF);
    int idx = s_menu_idx;
    for (int i = 0; i < N_GAMES; i++) {
        int y = 24 + i * 12;
        if (i == idx) gfx_text(8, y, ">", 0xF);
        gfx_text(20, y, s_games[i]->title, (i == idx) ? 0xF : 0x6);
    }
    gfx_text(2, 76, "WiFi: " WIFI_SSID, 0x9);
    gfx_text(2, 86, "192.168.4.1", 0x9);
}

static void render_gameover(void)
{
    gfx_text(25, 16, "GAME OVER", 0xF);
    if (s_last_winner < 0) {
        gfx_text(40, 40, "DRAW", 0xF);
    } else {
        char line[24];
        snprintf(line, sizeof(line), "P%d WINS!", s_last_winner + 1);
        gfx_text(30, 40, line, 0xF);
    }
    gfx_text(8, 80, "SELECT = menu", 0x9);
}

void engine_render(void)
{
    app_state_t          st = s_app;
    const game_module_t *a  = s_active;

    gfx_clear(0x0);
    switch (st) {
    case APP_MENU:     render_menu();         break;
    case APP_PLAYING:  if (a) a->render();    break;
    case APP_GAMEOVER: render_gameover();     break;
    }
    display_present();
}
