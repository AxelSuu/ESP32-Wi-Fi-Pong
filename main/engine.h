#pragma once

#include <stdint.h>
#include "game_module.h"

// Top-level application: menu / playing / game-over state machine plus the
// game registry. Owns the single engine mutex that protects whichever game
// is active.
void engine_init(void);

// Advance the active game by dt_ms (no-op in menu / game-over). Called from
// the game task. Takes the engine mutex.
void engine_update(uint32_t dt_ms);

// Route one generic input event by the current app state. Called from the
// httpd task. Takes the engine mutex.
void engine_dispatch_input(const input_event_t *ev);

// Draw the current screen (menu / active game / game-over) and present it.
// Reads state unlocked (torn-frame-OK).
void engine_render(void);

// Network notifies the engine of client connect/disconnect (player = slot id).
// Used for the 2-player "waiting" launch flow and mid-round disconnect.
void engine_on_player_connect(int player);
void engine_on_player_disconnect(int player);
