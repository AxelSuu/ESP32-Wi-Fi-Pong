#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define NETWORK_READY_BIT BIT0

extern EventGroupHandle_t net_event_group;

void network_wifi_init_ap(void);

// --- Server -> client messaging (used by the engine) ---

// Number of currently connected WebSocket clients (= occupied player slots).
int  net_player_count(void);

// Broadcast typed messages to all connected phones. Safe to call while the
// engine mutex is held (these only touch the fd table + copied strings).
void net_broadcast_active(const char *game_id, int players);  // controller morph
void net_broadcast_waiting(int need, int have);
void net_broadcast_over(int winner);
