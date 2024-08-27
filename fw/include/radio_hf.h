#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

typedef enum {
    MODE_RX,
    MODE_TX,
    MODE_QSK_COUNTDOWN
} radio_rxtx_mode_t;

// TODO: break out SSB into LSB and USB (at a later date). Assuming LSB below 10MHz for now.
typedef enum {
    BW_CW,
    BW_SSB
} radio_audio_bw_t;

typedef struct {
    uint64_t dial_freq;
    radio_audio_bw_t bw;
} radio_state_t;

extern TaskHandle_t xRadioTaskHandle;
extern void radio_task (void * pvParameter);

void radio_init();
void radio_key_on();
void radio_key_off();
void radio_set_dial_freq(uint64_t freq);
uint64_t radio_get_dial_freq();
radio_audio_bw_t radio_get_bw();