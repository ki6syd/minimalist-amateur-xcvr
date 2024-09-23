#pragma once

#include <Arduino.h>

void time_init();
bool time_update(uint64_t current_time);
uint64_t time_ms();