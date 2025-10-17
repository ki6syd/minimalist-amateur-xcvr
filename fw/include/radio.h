#pragma once

#include "globals.h"
#include <si5351.h>
#include <Arduino.h>

typedef struct {
    uint64_t f_lower;
    uint64_t f_center;
    uint64_t f_upper;
} radio_filt_properties_t;

void radio_init();
bool radio_set_dial_freq(uint64_t freq);
uint64_t radio_get_dial_freq();
uint64_t radio_get_dial_freq();

extern Si5351 si5351;   // TODO: get rid of this by avoiding any mention in radio.cpp