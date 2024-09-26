#pragma once
#include <Arduino.h>

// #define AUDIO_EN_OUT_VBAN
// #define AUDIO_EN_OUT_CSV
// #define AUDIO_EN_OUT_ESPNOW

#ifdef RX_ARCHITECTURE_QSD
#define AUDIO_PATH_IQ
#endif

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

void audio_init();
void audio_set_mode(audio_mode_t mode);
bool audio_set_filt(audio_filt_t filt);
audio_filt_t audio_get_filt();
void audio_en_pga(bool gain);
void audio_en_sidetone(bool tone);
void audio_en_rx_audio(bool en);
bool audio_set_volume(float vol);
bool audio_set_sidetone_volume(float vol);
bool audio_set_sidetone_freq(float freq);
float audio_get_sidetone_volume();
float audio_get_rx_db(uint16_t num_to_avg=1, uint16_t delay_ms=0);
float audio_get_s_meter();
float audio_get_sidetone_freq();
float audio_get_volume();

void audio_test(bool swap); // delete me later - used for debug only