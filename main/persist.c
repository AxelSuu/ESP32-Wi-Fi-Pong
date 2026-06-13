#include "persist.h"
#include "nvs.h"
#include <stdio.h>

#define NS "gamebox"

void persist_init(void)
{
    // nvs_flash_init() runs in app_main; nothing else to set up here.
}

static int read_i32(const char *key, int fallback)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return fallback;
    int32_t v = fallback;
    esp_err_t e = nvs_get_i32(h, key, &v);
    nvs_close(h);
    return (e == ESP_OK) ? (int)v : fallback;
}

static void write_i32(const char *key, int value)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
    if (nvs_set_i32(h, key, (int32_t)value) == ESP_OK) nvs_commit(h);
    nvs_close(h);
}

// NVS keys are capped at 15 chars; "hs_breakout" (11) is the longest we form.
static void hs_key(char *out, size_t n, const char *game_id)
{
    snprintf(out, n, "hs_%s", game_id);
}

int persist_get_highscore(const char *game_id)
{
    char k[16];
    hs_key(k, sizeof k, game_id);
    return read_i32(k, 0);
}

void persist_set_highscore(const char *game_id, int score)
{
    char k[16];
    hs_key(k, sizeof k, game_id);
    write_i32(k, score);
}

int  persist_get_last_game(int fallback) { return read_i32("lastgame", fallback); }
void persist_set_last_game(int idx)      { write_i32("lastgame", idx); }
int  persist_get_brightness(int fallback) { return read_i32("bright", fallback); }
void persist_set_brightness(int value)    { write_i32("bright", value); }
