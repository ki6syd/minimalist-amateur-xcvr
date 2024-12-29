#pragma once

#include "globals.h"
#include "radio.h"
#include <Arduino.h>

void hf_init();
void hf_set_dial_freq(uint64_t freq_dial, sideband_t sideband);  // calling radio_set_dial_freq() is thread-safe
float hf_get_s_meter();
String hf_freq_string();
void hf_cal_tx_10MHz();
