#pragma once
// Host mock of esp_timer.h — a controllable monotonic clock (see stubs.c / mock.h).
#include <stdint.h>
int64_t esp_timer_get_time(void);
