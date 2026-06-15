#pragma once
// Test-side controls for the host mocks in stubs.c.
#include <stdint.h>

// Monotonic clock backing esp_timer_get_time().
void mock_clock_reset(void);
void mock_clock_advance_us(int64_t delta_us);

// esp_random() source: queued values are returned first (FIFO), then a
// deterministic LCG. Lets a test pin a game's RNG-driven placement.
void mock_random_reset(void);
void mock_random_push(uint32_t value);
