#pragma once

#include <Arduino.h>

typedef enum {
    ADC_CHANNEL_VIN
} adc_channel_t;


void power_init();
void power_update_freq(uint32_t freq);
float power_adc_conversion(adc_channel_t channel);
float power_get_input_volt();