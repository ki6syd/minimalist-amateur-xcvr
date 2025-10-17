#pragma once

#include <Arduino.h>

typedef enum {
    ADC_CHANNEL_VIN,
    ADC_CHANNEL_PA_VDD,
    ADC_CHANNEL_PA_IDD,
    ADC_CHANNEL_PA_TEMP
} adc_channel_t;


void power_init();
void power_update_freq(uint32_t freq);
float power_adc_conversion(adc_channel_t channel);
float power_get_input_volt();
float power_get_pa_current();
float power_get_pa_volt();
float power_get_pa_temp();