#pragma once

#include "globals.h"
#include <Arduino.h>

// Public interface for rest of the system
void audio_init();
bool audio_set_volume(float vol);
float audio_get_volume();
void audio_set_mode(audio_mode_t mode);
bool audio_set_sidetone_volume(float vol);
float audio_get_sidetone_volume();
bool audio_set_sidetone_freq(float freq);
float audio_get_sidetone_freq();
void audio_en_sidetone(bool tone);
void audio_en_rx_audio(bool en);
void audio_en_pga(bool gain);
bool audio_get_pga();
float audio_get_rx_db(uint16_t num_to_avg, uint16_t delay_ms);
float audio_get_rx_vol();
float audio_get_loudness();
void audio_debug(debug_action_t command_num);
audio_filt_t audio_get_filt();
bool audio_set_filt(audio_filt_t filt);
