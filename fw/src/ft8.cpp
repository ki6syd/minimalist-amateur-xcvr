#include "globals.h"
#include "ft8.h"
#include "digi_modes.h"
#include "time_keeping.h"
#include "radio_hf.h"
#include "audio.h"

#include <Arduino.h>
#include <JTEncode.h>

#define FT8_TONE_SPACING        625          // ~6.25 Hz
#define FT8_DELAY               159          // Delay value for FT8
#define FT8_MSG_LEN             13
#define FT8_TIME_CORR_MS        8

JTEncode jtencode;

bool ft8_cancel_flag = false;

void ft8_init() {

}

void ft8_send_msg(digi_msg_t *to_send) {
    ft8_cancel_flag = false;

    float f_sidetone_original = audio_get_sidetone_freq();

    // wait for FT8 window to begin
    if(!to_send->ignore_time)
        time_delay_ft8();

    // key on, slight delay to ensure frequency change and keying are complete (needed??)
    // TODO: implement interface from radio module that indicates it's done with the queued requests
    radio_set_dial_freq(to_send->freq + (to_send->buf[0] * FT8_TONE_SPACING));
    radio_key_on();
    vTaskDelay(pdMS_TO_TICKS(10));

    // start the clock
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t xSymbolFrequency = pdMS_TO_TICKS(FT8_DELAY);

    for(uint8_t i = 0; i < FT8_SYMBOL_COUNT; i++)
    {
        if(ft8_cancel_flag)
            break;
        
        // TODO: fine clock setting (sub-hz)
        radio_set_dial_freq(to_send->freq + (to_send->buf[i] * FT8_TONE_SPACING) / 100);

        // set sidetone to follow ft8 audio frequency
        audio_set_sidetone_freq(f_sidetone_original + (to_send->buf[i] * FT8_TONE_SPACING) / 100);

        // delay until next increment of FT8_DELAY
        vTaskDelayUntil(&xLastWakeTime, xSymbolFrequency);
    }

    radio_key_off();
    audio_set_sidetone_freq(f_sidetone_original);
}

void ft8_string_process(String &message_text, digi_msg_t *msg_struct) {
    Serial.print("[FT8] Encoding: ");
    Serial.println(message_text);

    char message[] = {"SOTAMAT XXXXX"};
    message_text.toCharArray(message, FT8_MSG_LEN+1);
    Serial.println("[FT8] message char array: ");
    for(uint8_t i=0; i < FT8_MSG_LEN; i++) {
        Serial.print(message[i]);
        Serial.print(" ");
    }
    Serial.println();

    jtencode.ft8_encode(message, msg_struct->buf);

    Serial.println("[FT8] Calculated buffer: ");
    for(uint8_t i=0; i<FT8_SYMBOL_COUNT; i++) {
        Serial.print(msg_struct->buf[i]);
        Serial.print(" ");
    }
    Serial.println();
}

// TODO: use a semaphore or other freertos item
void ft8_cancel() {
    ft8_cancel_flag = true;
}