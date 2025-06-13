#pragma once

#include "globals.h"
#include <si5351.h>
#include <Arduino.h>

#define SI5351_IDX_VFO        SI5351_CLK0
#define SI5351_IDX_BFO_I      SI5351_CLK1
#define SI5351_IDX_BFO_Q      SI5351_CLK2

typedef enum {
    MODE_RX,
    MODE_TX,
    MODE_QSK_COUNTDOWN,
    MODE_SELF_TEST,
    MODE_STARTUP
} radio_rxtx_mode_t;

// band numbering in schematic is 1-indexed. Control GPIOs are 0-indexed.
typedef enum {
    BAND_HF_1,
    BAND_HF_2,
    BAND_HF_3,
    BAND_HF_4,
    BAND_HF_5,
    BAND_HF_6,
    BAND_HF_7,
    BAND_VHF,
    BAND_SELFTEST_LOOPBACK,
    BAND_UNKNOWN
} radio_band_t;

typedef struct {
    uint64_t dial_freq;
    radio_modulation_t mod;
    sideband_t sideband;
    float power;
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
    uint16_t num_rx_modulations;
    uint16_t num_tx_modulations;
    radio_modulation_t rx_modulations[8];
    radio_modulation_t tx_modulations[8];
} radio_band_capability_t;

void radio_init();
void radio_key_on();
void radio_key_off();
void radio_set_rxtx_mode(radio_rxtx_mode_t new_mode);
void radio_set_band(radio_band_t new_band);
bool radio_set_dial_freq(uint64_t freq);
bool radio_set_dial_freq_fine(uint64_t freq_dHz);
bool radio_set_modulation(radio_modulation_t mod);
radio_band_t radio_get_band(uint64_t freq);
bool radio_freq_valid(uint64_t freq);
uint64_t radio_get_dial_freq();
radio_modulation_t radio_get_modulation();
sideband_t radio_get_sideband();
String radio_band_to_string(radio_band_t band);
String radio_modulation_to_string(radio_modulation_t bw);
String radio_sideband_to_string(sideband_t sideband);
bool radio_modulation_valid(radio_modulation_t mod);
String radio_freq_string();
float radio_get_s_meter();
bool radio_set_power(float power_level);
float radio_get_power();
void radio_enable_tx(bool en);
void radio_debug(debug_action_t action, void *value);

extern Si5351 si5351;   // TODO: get rid of this by avoiding any mention in radio.cpp
extern uint64_t freq_if;