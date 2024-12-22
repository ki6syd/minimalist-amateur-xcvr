#pragma once

#include "globals.h"
#include <Arduino.h>
#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"

// DSP-specific defines
#define MIXER_IDX_SIDETONE      0
#define MIXER_IDX_LEFT          1
#define MIXER_IDX_RIGHT         2
#define PGA_GAIN                24
#define INT16T_MAX              32767
#define BUFFER_CHUNK            64

void audio_dsp_init();
void audio_dsp_task(void *pvParameter);
void audio_dsp_task_restart();
void audio_dsp_set_mode(audio_mode_t mode);
void audio_dsp_set_filter(audio_filt_t filt);
void audio_dsp_set_volume(float vol);
void audio_dsp_set_sidetone(bool enable, float freq, float vol);
void audio_dsp_set_dacs(audio_mode_t mode);
void audio_dsp_set_mode(audio_mode_t mode);
float audio_dsp_get_rx_level(uint16_t num_avg, uint16_t delay_ms);
void audio_en_vol_clipping(bool enable);
void audio_dsp_set_input_volume(uint8_t volume);
void audio_dsp_set_mute(bool mute, uint8_t channel);


// Add extern declarations for variables needed by audio.cpp
extern bool pga_en;
extern float sidetone_vol;
extern float sidetone_freq;
extern float global_vol;
extern audio_mode_t cur_audio_mode;
extern uint32_t max_safe_vol;
extern bool sidetone_en;
extern audio_filt_t cur_filt;