#pragma once

#include "radio.h"
#include "globals.h"
#include <Arduino.h>

void radio_vhf_init();
bool radio_vhf_set_freq(uint64_t freq);