#pragma once

#include <Arduino.h>

#define BIAS_CURRENT_CW     0.05

void power_init();
void power_update_freq(uint32_t freq);
float power_get_input_volt();
float power_get_pa_current();
void power_bias_to_current(float total_current);