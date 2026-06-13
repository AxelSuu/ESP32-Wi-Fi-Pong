#pragma once
// Host mock of esp_random.h. The implementation in stubs.c serves queued values
// first (so tests can pin food placement) then falls back to a deterministic LCG.
#include <stdint.h>
uint32_t esp_random(void);
