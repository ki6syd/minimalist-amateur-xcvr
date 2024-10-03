#pragma once

#include "globals.h"
#include "radio.h"
#include <Arduino.h>


void hf_init();
void hf_set_dial_freq(uint64_t freq_dial);
float hf_get_s_meter();
String hf_freq_string();
void hf_cal_tx_10MHz();
void hf_cal_if_filt(radio_filt_sweep_t sweep, radio_filt_properties_t *properties);
void hf_cal_bpf_filt(radio_band_t band, radio_filt_sweep_t sweep, radio_filt_properties_t *properties);
