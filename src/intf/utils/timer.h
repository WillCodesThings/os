#pragma once

#include <stdint.h>

// Initialize the TSC timer by calibrating against PIT channel 2.
// Must be called after log_init() but before other inits.
// No heap dependency.
void timer_init(void);

// Read the current TSC value (raw cycle count).
uint64_t timer_ticks(void);

// Return elapsed milliseconds since the given TSC start value.
uint32_t timer_ms_since(uint64_t start);

// Return the calibrated TSC frequency in kHz.
uint32_t timer_get_frequency_khz(void);
