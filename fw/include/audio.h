#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// #define AUDIO_EN_OUT_VBAN
// #define AUDIO_EN_OUT_CSV

// #define AUDIO_PATH_IQ

// audio_mode_t: defines DSP pathway configuration, it is *unrelated* to the concept of TX, RX, QSK. 
// "CW" refers to direct TX carrier generation (not baseband audio input). _RXTX_CW could be using SSB bandwidth filtering
typedef enum {
    AUDIO_HF_RXTX_CW,
    AUDIO_HF_RX_SSB,
    AUDIO_HF_TX_SSB,
    AUDIO_VHF_RX,
    AUDIO_VHF_TX
} audio_mode_t;

// audio_filt_t: simple definition for now, assumes that sidetone frequency is always centered in the CW filter passband
typedef enum {
    AUDIO_FILT_CW,
    AUDIO_FILT_SSB
} audio_filt_t;

extern TaskHandle_t xAudioStreamTaskHandle;
extern void audio_stream_task (void * pvParameter);

void audio_init();
void audio_set_mode(audio_mode_t mode);
void audio_set_filt(audio_filt_t filt);
void audio_en_pga(bool gain);
void audio_en_sidetone(bool tone);
void audio_en_rx_audio(bool en);
void audio_set_volume(float vol);
void audio_set_sidetone_volume(float vol);
void audio_set_sidetone_freq(float freq);
float audio_get_rx_db();
float audio_get_sidetone_freq();