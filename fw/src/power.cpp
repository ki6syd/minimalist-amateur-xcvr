#include "globals.h"
#include "power.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define POWER_BUCK_FSW      510e3

TaskHandle_t xBatterySenseTaskHandle; 

uint32_t freq_buck = POWER_BUCK_FSW;

void battery_sense_task(void *pvParameter);

void power_init() {
    // set up a PWM channel that drives buck converter sync pin
    // feature for the future: select this frequency based on the dial frequency
    ledcSetup(PWM_CHANNEL_SYNC, freq_buck, 2);
    ledcAttachPin(BUCK_SYNC, PWM_CHANNEL_SYNC);
    ledcWrite(PWM_CHANNEL_SYNC, 2);

    xTaskCreatePinnedToCore(
        battery_sense_task,
        "VDD Sensing",
        4096,
        NULL,
        TASK_PRIORITY_POWER, // priority
        &xBatterySenseTaskHandle,
        TASK_CORE_POWER // core
    );
}


void battery_sense_task(void *param) {
  while(true) {
    Serial.print("VDD read: ");
    Serial.println((float) analogRead(ADC_VDD) * ADC_MAX_VOLT / ADC_VDD_SCALE / ADC_FS_COUNTS);

    // Serial.print("S-meter: ");
    // Serial.println(audio_get_rx_db());

    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

// updates buck converters to a new switching frequency [Hz]
// TODO: test this
void power_update_freq(uint32_t new_freq) {
    if(new_freq < 1e6 && new_freq > 100e3) {
        freq_buck = new_freq;
        ledcChangeFrequency(PWM_CHANNEL_SYNC, freq_buck, 2);
    }
}