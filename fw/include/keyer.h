#pragma once

#include <Arduino.h>

void keyer_init();
void keyer_dit();
void keyer_dah();
void keyer_letter(String leter);
bool keyer_set_speed(uint16_t wpm);
uint16_t keyer_get_speed();