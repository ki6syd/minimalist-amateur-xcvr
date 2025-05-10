#pragma once

#include "globals.h"
#include <Arduino.h>
#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"

#define PGA_GAIN                24
#define INT16T_MAX              32767
#define BUFFER_CHUNK            128

void audio_dsp_init();
void audio_dsp_task(void *pvParameter);
void audio_dsp_task_restart();
void audio_dsp_set_mode(audio_mode_t mode);
void audio_dsp_set_filter(audio_filt_t filt);
audio_filt_t audio_dsp_get_filter();
String radio_audio_filt_to_string(audio_filt_t filt);
void audio_dsp_set_sideband(sideband_t sideband);
void audio_dsp_set_modulation(radio_modulation_t mod);
void audio_dsp_set_volume(float vol);
void audio_dsp_set_dacs(audio_mode_t mode);
void audio_dsp_set_mode(audio_mode_t mode);
float audio_dsp_get_rx_level(uint16_t num_avg, uint16_t delay_ms);
void audio_dsp_set_pga_gain(uint8_t volume);

// Add extern declarations for variables needed by audio.cpp
extern bool pga_en;
extern float sidetone_vol;
extern float sidetone_freq;
extern float global_vol;
extern float tx_power;
extern audio_mode_t cur_audio_mode;
extern uint16_t max_safe_vol;
extern audio_filt_t cur_filt;
extern sideband_t cur_sideband;
extern float i_rx_gain;
extern float q_rx_gain;
extern float i_tx_gain;
extern float q_tx_gain;

extern SineWaveGenerator<int16_t> sidetone_wave;
extern SineWaveGenerator<int16_t> imd_test_wave;
extern VolumeStream iq_balance;
extern VolumeStream hp_vol;
extern VolumeStream tx_vol;
extern VolumeMeter vol_meas;
extern AudioEffectStream effects;
extern Distortion volume_limiter;
extern FilteredStream<int16_t, float> hilbert;