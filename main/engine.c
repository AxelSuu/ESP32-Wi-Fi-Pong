#include "engine.h"
#include "display.h"
#include "hw_config.h"
#include "net_config.h"
#include "network.h"
#include "pong.h"
#include "snake.h"
#include "racer.h"
#include "tron.h"
#include "icons.h"
#include <stdio.h>
#include <string.h>
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
static int                  s_last_score  = -1;
static uint32_t             s_anim_ms;    // free-running clock for attract animation
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

// Mirror the menu to connected phones (the "screen" message). Mutex held.
static void broadcast_menu_state(void)
{
    char games[96];
    int n = snprintf(games, sizeof(games), "[");
    for (int i = 0; i < N_GAMES; i++)
        n += snprintf(games + n, sizeof(games) - n,
                      "%s\"%s\"", i ? "," : "", s_games[i]->title);
    snprintf(games + n, sizeof(games) - n, "]");

    char buf[160];
    snprintf(buf, sizeof(buf),
             "{\"t\":\"screen\",\"mode\":\"menu\",\"games\":%s,\"idx\":%d}",
             games, s_menu_idx);
    net_broadcast_json(buf);
}

void engine_update(uint32_t dt_ms)
{
    xSemaphoreTake(s_engine_mutex, portMAX_DELAY);
    s_anim_ms += dt_ms;               // drives the attract-screen animation
    if (s_app == APP_PLAYING && s_active) {
        s_active->tick(dt_ms);
        if (s_active->is_over()) {
            s_last_winner = s_active->winner();
            s_last_score  = s_active->score();
            s_app = APP_GAMEOVER;
            net_broadcast_over(s_last_winner, s_last_score);
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
            broadcast_menu_state();
        } else if (ev->kind == INPUT_SELECT || ev->kind == INPUT_PRIMARY) {
            try_launch(s_games[s_menu_idx]);
        }
        break;
    case APP_PLAYING:
        if (ev->kind == INPUT_BACK) {
            s_app = APP_MENU;
            s_pending = NULL;
            broadcast_menu_state();
        } else if (s_active) {
            s_active->on_input(ev);
        }
        break;
    case APP_GAMEOVER:
        if ((ev->kind == INPUT_SELECT || ev->kind == INPUT_PRIMARY) && s_active) {
            const game_module_t *restart = s_active;
            s_app = APP_MENU;
            try_launch(restart);
        } else if (ev->kind == INPUT_BACK) {
            s_app = APP_MENU;
            broadcast_menu_state();
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
    } else if (s_app == APP_MENU) {
        broadcast_menu_state();                  // let the new phone draw the menu
    } else if (s_app == APP_PLAYING && s_active) {
        net_broadcast_active(s_active->id, s_active->min_players);  // re-morph
    } else if (s_app == APP_GAMEOVER) {
        net_broadcast_over(s_last_winner, s_last_score);
    }
    xSemaphoreGive(s_engine_mutex);
}

void engine_on_player_disconnect(int player)
{
    xSemaphoreTake(s_engine_mutex, portMAX_DELAY);
    if (s_app == APP_PLAYING && s_active && s_active->min_players >= 2) {
        // The remaining player wins the round.
        s_last_winner = (player == 0) ? 1 : 0;
        s_last_score  = -1;
        s_app = APP_GAMEOVER;
        net_broadcast_over(s_last_winner, s_last_score);
    }
    xSemaphoreGive(s_engine_mutex);
}

// --- rendering (runs unlocked; snapshot volatile-ish fields once) ---

static void center_text(int y, const char *s, uint8_t shade)
{
    int x = (SCREEN_WIDTH - (int)strlen(s) * 6) / 2;
    if (x < 0) x = 0;
    gfx_text(x, y, s, shade);
}

// 0..amp triangle wave with the given period (ms).
static int tri_wave(uint32_t t, uint32_t period, int amp)
{
    uint32_t p = t % period, half = period / 2;
    uint32_t v = (p < half) ? p : (period - p);   // 0..half
    return (int)((uint64_t)v * amp / half);
}

static void render_menu(void)
{
    int idx = s_menu_idx;

    // Header band + title.
    gfx_rect(0, 0, SCREEN_WIDTH, 11, 0x3);
    center_text(2, "GAMEBOX", 0xF);
    gfx_rect(0, 12, SCREEN_WIDTH, 1, 0x6);

    // Game rows: icon + label, selected row glows on a subtle bar.
    for (int i = 0; i < N_GAMES; i++) {
        int top = 15 + i * 16;
        bool sel = (i == idx);
        if (sel) {
            gfx_rect(0, top, SCREEN_WIDTH, 16, 0x2);   // highlight bar
            gfx_rect(0, top, 3, 16, 0xF);              // bright left accent
        }
        const uint8_t *icon = icon_for_id(s_games[i]->id);
        uint8_t shade = sel ? 0xF : 0x6;
        if (icon) gfx_bitmap(6, top, ICON_W, ICON_H, icon, shade);
        gfx_text(28, top + 4, s_games[i]->title, shade);
        if (s_games[i]->min_players >= 2)
            gfx_text(SCREEN_WIDTH - 16, top + 4, "2P", sel ? 0xC : 0x5);
    }

    // Footer: AP name + connected-phone dots.
    gfx_rect(0, 80, SCREEN_WIDTH, 1, 0x4);
    gfx_text(2, 84, net_ssid(), 0x9);
    int players = net_player_count();
    for (int i = 0; i < players && i < MAX_WS_CLIENTS; i++)
        gfx_circle(SCREEN_WIDTH - 8 - i * 7, 87, 2, 0xF);
}

static void render_attract(void)
{
    // Border frame for an arcade feel.
    gfx_rect(0, 0, SCREEN_WIDTH, 1, 0x4);
    gfx_rect(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, 1, 0x4);
    gfx_rect(0, 0, 1, SCREEN_HEIGHT, 0x4);
    gfx_rect(SCREEN_WIDTH - 1, 0, 1, SCREEN_HEIGHT, 0x4);

    center_text(6, "GAMEBOX", 0xF);
    gfx_rect(20, 16, SCREEN_WIDTH - 40, 1, 0x6);

    // Bouncing ball (different periods on each axis → lively path).
    int bx = 6 + tri_wave(s_anim_ms, 2600, SCREEN_WIDTH - 12);
    int by = 24 + tri_wave(s_anim_ms, 1700, 22);
    gfx_circle(bx, by, 3, 0xF);

    center_text(58, "JOIN WIFI", 0x9);
    center_text(70, net_ssid(), 0xF);
    center_text(84, "192.168.4.1", 0x9);
}

static void render_gameover(void)
{
    char line[24];
    if (s_last_winner >= 0) {
        gfx_text(25, 16, "GAME OVER", 0xF);
        snprintf(line, sizeof(line), "P%d WINS!", s_last_winner + 1);
        gfx_text(34, 38, line, 0xF);
    } else {
        gfx_text(25, 22, "GAME OVER", 0xF);
    }
    if (s_last_score >= 0) {
        snprintf(line, sizeof(line), "SCORE %d", s_last_score);
        gfx_text(34, (s_last_winner >= 0) ? 54 : 44, line, 0xC);
    }
    gfx_text(8, 76, "SELECT = again", 0x9);
    gfx_text(8, 86, "BACK = menu", 0x9);
}

void engine_render(void)
{
    app_state_t          st = s_app;
    const game_module_t *a  = s_active;

    gfx_clear(0x0);
    switch (st) {
    case APP_MENU:
        if (net_player_count() == 0) render_attract();   // idle: nobody connected
        else                         render_menu();
        break;
    case APP_PLAYING:  if (a) a->render();    break;
    case APP_GAMEOVER: render_gameover();     break;
    }
    display_present();
}
