#include "engine.h"
#include "display.h"
#include "hw_config.h"
#include "net_config.h"
#include "network.h"
#include "pong.h"
#include "snake.h"
#include "racer.h"
#include "tron.h"
#include "breakout.h"
#include "persist.h"
#include "icons.h"
#include "proto.h"
#include "fx.h"
#include <stdio.h>
#include <string.h>
#include "esp_app_desc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef enum { APP_MENU, APP_PLAYING, APP_GAMEOVER } app_state_t;

// Game registry — order is the menu order.
static const game_module_t *const s_games[] = { &PONG, &SNAKE, &RACER, &TRON, &BREAKOUT };
#define N_GAMES ((int)(sizeof(s_games) / sizeof(s_games[0])))

#define DEFAULT_BRIGHTNESS 0x80
#define WIPE_FRAMES        6    // length of the state-change wipe transition

static int                  s_menu_idx;
static const game_module_t *s_active;
static const game_module_t *s_pending;  // 2-player game awaiting enough phones
static app_state_t          s_app;
static int                  s_last_winner = -1;
static int                  s_last_score  = -1;
static int                  s_last_best   = -1;   // high score for the last round's game
static bool                 s_new_best;           // did the last round set a new best?
static uint32_t             s_anim_ms;    // free-running clock for attract animation
static SemaphoreHandle_t    s_engine_mutex;
static volatile int         s_pending_brightness = -1;  // set by httpd task, applied on render task
static volatile int         s_pending_persist    = 0;   // also save to NVS when applying
static app_state_t          s_render_prev = APP_MENU;   // for detecting state changes (wipe)
static int                  s_wipe;                      // remaining wipe-transition frames

void engine_init(void)
{
    s_engine_mutex = xSemaphoreCreateMutex();
    persist_init();
    fx_reset();
    s_active   = NULL;
    s_pending  = NULL;
    s_app      = APP_MENU;

    // Restore the last-played menu cursor and saved brightness (display is
    // already up — app_main calls display_init() before engine_init()).
    s_menu_idx = persist_get_last_game(0);
    if (s_menu_idx < 0 || s_menu_idx >= N_GAMES) s_menu_idx = 0;
    display_set_contrast((uint8_t)persist_get_brightness(DEFAULT_BRIGHTNESS));
}

// Called from the httpd task; the actual SSD1327 write is deferred to the render
// task (display_present shares the SPI bus) — see engine_render().
void engine_set_brightness(int value, int persist)
{
    if (value < 0)   value = 0;
    if (value > 255) value = 255;
    s_pending_persist    = persist;
    s_pending_brightness = value;   // set last — this is what the render task polls
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
             "{\"v\":%d,\"t\":\"screen\",\"mode\":\"menu\",\"games\":%s,\"idx\":%d}",
             PROTO_SCHEMA_VERSION, games, s_menu_idx);
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
            s_new_best    = false;
            s_last_best   = -1;
            if (s_last_score >= 0) {                 // scored (single-player) game
                int best = persist_get_highscore(s_active->id);
                if (s_last_score > best) {
                    best = s_last_score;
                    persist_set_highscore(s_active->id, best);
                    s_new_best = true;
                }
                s_last_best = best;
            }
            fx_flash();              // arcade death punch (game task; safe vs fx_update)
            fx_shake(3, 8);
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
            persist_set_last_game(s_menu_idx);
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
        s_last_best   = -1;
        s_new_best    = false;
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

static void center_text_scaled(int y, const char *s, uint8_t shade, int scale)
{
    int x = (SCREEN_WIDTH - gfx_text_width(s, scale)) / 2;
    if (x < 0) x = 0;
    gfx_text_scaled(x, y, s, shade, scale);
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

    // Compact header: title + connected-phone dots (ssid lives on the attract
    // screen; by the time the menu shows, players have already joined).
    gfx_rect(0, 0, SCREEN_WIDTH, 11, 0x3);
    gfx_text(3, 2, "GAMEBOX", 0xF);
    int players = net_player_count();
    for (int i = 0; i < players && i < MAX_WS_CLIENTS; i++)
        gfx_circle(SCREEN_WIDTH - 7 - i * 7, 5, 2, 0xF);
    gfx_hline(0, 11, SCREEN_WIDTH, 0x6);

    // Game rows (fits all five between the header and the bottom edge).
    bool blink = (s_anim_ms / 250) & 1;
    for (int i = 0; i < N_GAMES; i++) {
        int top = 13 + i * 16;
        bool sel = (i == idx);
        if (sel) {
            gfx_rect(0, top, SCREEN_WIDTH, 15, 0x3);            // highlight bar
            gfx_rect(0, top, 3, 15, blink ? 0xF : 0x7);        // pulsing accent
        }
        const uint8_t *icon = icon_for_id(s_games[i]->id);
        uint8_t shade = sel ? 0xF : 0x7;
        if (icon) gfx_bitmap(6, top, ICON_W, ICON_H, icon, shade);
        gfx_text(28, top + 4, s_games[i]->title, shade);
        if (s_games[i]->min_players >= 2)
            gfx_text(SCREEN_WIDTH - 16, top + 4, "2P", sel ? 0xC : 0x5);
    }
}

static void render_attract(void)
{
    // Border frame for an arcade feel.
    gfx_rect(0, 0, SCREEN_WIDTH, 1, 0x4);
    gfx_rect(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, 1, 0x4);
    gfx_rect(0, 0, 1, SCREEN_HEIGHT, 0x4);
    gfx_rect(SCREEN_WIDTH - 1, 0, 1, SCREEN_HEIGHT, 0x4);

    center_text_scaled(2, "GAMEBOX", 0xF, 2);     // bold title
    gfx_rect(20, 18, SCREEN_WIDTH - 40, 1, 0x6);

    // Firmware version, dim in the top-left corner (full string is also in the
    // phone's system_info / OTA overlay). gfx_text clips at the screen edge.
    gfx_text(2, 2, esp_app_get_description()->version, 0x6);

    // Bouncing ball (different periods on each axis → lively path).
    int bx = 6 + tri_wave(s_anim_ms, 2600, SCREEN_WIDTH - 12);
    int by = 26 + tri_wave(s_anim_ms, 1700, 18);
    gfx_circle(bx, by, 3, 0xF);

    center_text(54, "JOIN WIFI", ((s_anim_ms / 400) & 1) ? 0x9 : 0x4);  // blink
    center_text(64, net_ssid(), 0xF);
    center_text(74, "gamebox.local", 0xF);   // mDNS name (phones)
    center_text(84, "192.168.4.1", 0x6);      // IP fallback
}

static void render_gameover(void)
{
    char line[24];
    center_text_scaled(8, "GAME OVER", 0xF, 2);    // bold title

    int y = 32;
    if (s_last_winner >= 0) {
        snprintf(line, sizeof(line), "P%d WINS!", s_last_winner + 1);
        center_text(y, line, 0xF);
        y += 12;
    }
    if (s_last_score >= 0) {
        snprintf(line, sizeof(line), "SCORE %d", s_last_score);
        center_text(y, line, 0xC);
        y += 12;
    }
    if (s_last_best >= 0) {
        // Flash a new record; show the standing best steadily otherwise.
        if (!s_new_best || ((s_anim_ms / 250) & 1)) {
            snprintf(line, sizeof(line), s_new_best ? "NEW BEST %d" : "BEST %d", s_last_best);
            center_text(y, line, s_new_best ? 0xF : 0x9);
        }
    }
    center_text(78, "SELECT = AGAIN", 0x9);
    center_text(88, "BACK = MENU", 0x9);
}

void engine_render(void)
{
    // Apply a brightness change requested by the httpd task here, on the render
    // task, so SSD1327 writes never race display_present() on the shared SPI bus.
    int b = s_pending_brightness;
    if (b >= 0) {
        display_set_contrast((uint8_t)b);
        if (s_pending_persist) persist_set_brightness(b);
        s_pending_brightness = -1;
    }

    app_state_t          st = s_app;
    const game_module_t *a  = s_active;

    if (st != s_render_prev) { s_wipe = WIPE_FRAMES; s_render_prev = st; }

    fx_update();                 // advance effects + apply shake origin
    gfx_clear(0x0);
    switch (st) {
    case APP_MENU:
        if (net_player_count() == 0) render_attract();   // idle: nobody connected
        else                         render_menu();
        break;
    case APP_PLAYING:  if (a) a->render();    break;
    case APP_GAMEOVER: render_gameover();     break;
    }
    fx_draw();                   // sparks + flash overlay (rides the shake origin)

    if (s_wipe > 0) {            // state-change wipe: black curtain receding right
        gfx_set_origin(0, 0);    // keep the curtain stable even during a shake
        int revealed = (WIPE_FRAMES - s_wipe) * SCREEN_WIDTH / WIPE_FRAMES;
        gfx_rect(revealed, 0, SCREEN_WIDTH - revealed, SCREEN_HEIGHT, 0x0);
        s_wipe--;
    }

    display_present();
}

void engine_render_error(const char *msg)
{
    gfx_clear(0x0);
    gfx_rect(0, 0, SCREEN_WIDTH, 1, 0x6);
    gfx_rect(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, 1, 0x6);
    center_text(30, "! ERROR !", 0xF);
    center_text(48, msg, 0x9);
    display_present();
}
