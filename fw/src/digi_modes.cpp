#include "globals.h"
#include "digi_modes.h"
#include "keyer.h"
#include "ft8.h"
#include "radio.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define DIGI_QUEUE_LEN      10

QueueHandle_t xDigiMessageQueue;
TaskHandle_t xDigiModeTaskHandle;

void digi_mode_task(void *pvParameter);
const char* digi_mode_to_string(digi_type_t type);

void digi_mode_init() {
    // TODO: parametrize the length of this queue
    xDigiMessageQueue = xQueueCreate(DIGI_QUEUE_LEN, sizeof(digi_msg_t));

    xTaskCreatePinnedToCore(
        digi_mode_task,
        "Digital Mode Task",
        4096,
        NULL,
        TASK_PRIORITY_DIGI, // priority
        &xDigiModeTaskHandle,
        TASK_CORE_DIGI // core
    );
}

void digi_mode_task(void *pvParameter) {
    keyer_init();
    ft8_init();

    digi_msg_t tmp;
    while(1) {
        // block until there is something to pull out of the message queue
        if(xQueueReceive(xDigiMessageQueue, (void *) &tmp, pdMS_TO_TICKS(100)) == pdTRUE) {
            digi_mode_print(&tmp);

            // change frequency, if the message has one
            if(tmp.freq != 0)
            radio_set_dial_freq(tmp.freq);

            if(tmp.type == DIGI_MODE_CW) {
                keyer_send_msg(&tmp);
            }
            else if(tmp.type == DIGI_MODE_FT8) {
                ft8_send_msg(&tmp);
            }
        }
    }
}

// accepts a pointer, but will make a copy when enqueueing
bool digi_mode_enqueue(digi_msg_t *new_msg) {
    if(xQueueSend(xDigiMessageQueue, (void *) new_msg, 0) != pdTRUE) {
        Serial.println("Unable to queue message, queue full");
        return false;
    }
    return true;
}

const char* digi_mode_to_string(digi_type_t type) {
    switch (type) {
        case DIGI_MODE_CW: return "CW";
        case DIGI_MODE_FT8: return "FT8";
        default: return "Unknown";
    }
}

void digi_mode_print(const digi_msg_t* msg) {
    Serial.println("Digi Message:");
    Serial.print("  Type: ");
    Serial.println(digi_mode_to_string(msg->type));
    Serial.print("  Ignore Time: ");
    Serial.println(msg->ignore_time ? "true" : "false");
    Serial.print("  Frequency: ");
    Serial.print(msg->freq);
    Serial.println(" Hz");
    Serial.print("  Buffer: ");
    
    // Print buffer contents, stopping at null terminator
    for (int i = 0; i < sizeof(msg->buf); i++) {
        if (msg->buf[i] == 0) break; // Stop if null terminator is found
        Serial.print((char)msg->buf[i]);
    }
    
    Serial.println();
}

uint16_t digi_mode_queue_size() {
    return DIGI_QUEUE_LEN - (uint16_t) uxQueueSpacesAvailable(xDigiMessageQueue);
}

void digi_mode_queue_clear() {
    xQueueReset(xDigiMessageQueue);
    // TODO: set a flag that stops whatever the digimode queue is working on
}