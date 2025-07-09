#pragma once

#include <Arduino.h>

#define BIAS_CURRENT_CW     0.1
#define BIAS_CURRENT_SSB    0.4
#define BIAS_MAX_PER_CH     0.5
#define BIAS_VOLT_CW        3.5

#define AGC_VOLT_RX         2.5
#define AGC_VOLT_TX_CW      2.5
#define AGC_VOLT_TX_SSB     3.0

typedef enum {
    ADC_CHANNEL_VIN,
    ADC_CHANNEL_PA_VDD,
    ADC_CHANNEL_PA_IDD,
    ADC_CHANNEL_PA_TEMP
} adc_channel_t;

typedef enum {
    BIAS_CHANNEL_0 = 0,
    BIAS_CHANNEL_1 = 1,
} power_bias_channel_t;

void power_init();
void power_update_freq(uint32_t freq);
float power_adc_conversion(adc_channel_t channel);
float power_get_input_volt();
float power_get_pa_current();
float power_get_pa_volt();
float power_get_pa_temp();
void power_bias_to_current(float total_current);
void power_bias_to_voltage(float voltage);
float power_get_bias_target();
float power_get_bias_duty(power_bias_channel_t channel);
float power_get_agc_duty();
void power_agc_to_voltage(float voltage);