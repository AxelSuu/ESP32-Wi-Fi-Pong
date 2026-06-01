#pragma once

// --- Network & device identity configuration ---

// Each unit advertises a unique SoftAP so two consoles in one room never
// collide: a "factory" NVS-provisioned name wins, else "<prefix>-XXXX" derived
// from the SoftAP MAC. See net_derive_ssid() / net_ssid() in network.c.
#define WIFI_SSID_PREFIX "GameBox"
#define WIFI_PASSWORD   "12345678"
#define WIFI_AP_CHANNEL 1
#define WIFI_MAX_CONN   4

#define SPIFFS_BASE_PATH "/spiffs"
#define MAX_WS_CLIENTS   4
