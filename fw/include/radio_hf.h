#pragma once

#include "globals.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// comment out to prevent accidental TX
// #define RADIO_ALLOW_TX

#define DB_PER_S_UNIT       6

typedef enum {
    MODE_RX,
    MODE_TX,
    MODE_QSK_COUNTDOWN,
    MODE_SELF_TEST,
    MODE_STARTUP
} radio_rxtx_mode_t;

// TODO: pull this enum into a radio.h module, and add VHF
// band numbering in schematic is 1-indexed. Control GPIOs are 0-indexed.
typedef enum {
    BAND_HF_1,
    BAND_HF_2,
    BAND_HF_3,
    BAND_VHF,
    BAND_SELFTEST_LOOPBACK,
    BAND_UNKNOWN
} radio_band_t;

// TODO: break out SSB into LSB and USB (at a later date). Assuming LSB below 10MHz for now.
// TODO: break apart the concept of audio bandwidth and RF mode
typedef enum {
    BW_CW,
    BW_SSB,
    BW_FM
} radio_audio_bw_t;

typedef struct {
    uint64_t dial_freq;
    radio_audio_bw_t bw;
} radio_state_t;

typedef struct {
    uint64_t f_center;
    uint64_t f_span;
    uint16_t num_steps;
    uint16_t num_to_avg;
    float rolloff;
} radio_filt_sweep_t;

typedef struct {
    uint64_t f_lower;
    uint64_t f_center;
    uint64_t f_upper;
} radio_filt_properties_t;

// TODO: stop conflating audio_bw and RF mode
typedef struct {
    radio_band_t band_name;
    uint64_t min_freq;
    uint64_t max_freq;
    uint16_t num_rx_bandwidths;
    uint16_t num_tx_bandwidths;
    radio_audio_bw_t rx_bandwidths[8];
    radio_audio_bw_t tx_bandwidths[8];
} radio_band_capability_t;

void radio_hf_init();
void radio_key_on();
void radio_key_off();
bool radio_set_dial_freq(uint64_t freq);
bool radio_set_dial_freq_fine(uint64_t freq_dHz);
radio_band_t radio_get_band(uint64_t freq);
bool radio_freq_valid(uint64_t freq);
uint64_t radio_get_dial_freq();
radio_audio_bw_t radio_get_bw();
String radio_band_to_string(radio_band_t band);
String radio_bandwidth_to_string(radio_audio_bw_t bw);
String radio_freq_string();
float radio_get_s_meter();
void radio_enable_tx(bool en);
void radio_debug(debug_action_t action, void *value);