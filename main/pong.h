#pragma once

#include "game_module.h"

// Pong as a game module. State is file-static inside pong.c.
extern const game_module_t PONG;

// Attract-demo mode: when on, the AI plays BOTH paddles so the engine can run
// Pong as a self-playing demo on the idle attract screen.
void pong_set_demo(bool on);
