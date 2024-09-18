#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void power_init();
void power_update_freq(uint32_t freq);