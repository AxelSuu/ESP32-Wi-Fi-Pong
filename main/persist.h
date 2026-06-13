#pragma once

// Tiny NVS-backed persistence (namespace "gamebox"): per-game high scores, the
// last-played menu index, and display brightness. All best-effort — a failed
// read returns the caller's fallback and a failed write is silently dropped, so
// the console still runs if NVS is unavailable. Assumes nvs_flash_init() has
// already run (app_main does it).
void persist_init(void);

int  persist_get_highscore(const char *game_id);          // 0 if unset
void persist_set_highscore(const char *game_id, int score);

int  persist_get_last_game(int fallback);                 // menu index
void persist_set_last_game(int idx);

int  persist_get_brightness(int fallback);                // 0..255
void persist_set_brightness(int value);
