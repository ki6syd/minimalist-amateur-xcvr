#pragma once

#include "radio.h"
#include "globals.h"
#include <Arduino.h>

void vhf_init();
bool vhf_handshake();
void vhf_powerdown();
bool vhf_is_configured();
bool vhf_set_freq(uint64_t freq);
bool vhf_set_volume(uint16_t volume);
float vhf_get_s_meter();
void vhf_get_version();