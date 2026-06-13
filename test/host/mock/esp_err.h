#pragma once
// Host mock of ESP-IDF's esp_err.h — just enough for display.h to compile.
typedef int esp_err_t;
#define ESP_OK   0
#define ESP_FAIL -1
