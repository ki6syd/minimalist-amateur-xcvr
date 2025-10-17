#pragma once

#include "globals.h"
#include "radio.h"
#include <Arduino.h>

void hf_init();
bool hf_set_phase(int16_t phase);
