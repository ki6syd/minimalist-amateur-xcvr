#include "globals.h"
#include "power.h"
#include "radio_hf.h"
#include "io.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define POWER_BUCK_FSW      510e3
// TODO: move this into JSON preferences
#define VBAT_CELL_LOW       2.9
#define VBAT_NUM_CELL       3
#define VDIODE              0.6
#define VUSB                5.5

TaskHandle_t xAnalogSenseTaskHandle;

uint32_t freq_buck = POWER_BUCK_FSW;
float input_volt = 0;
uint32_t num_low_samples = 0;

void analog_sense_task(void *pvParameter);

void power_init() {
    // set up a PWM channel that drives buck converter sync pin
    // feature for the future: select this frequency based on the dial frequency
    ledcSetup(PWM_CHANNEL_SYNC, freq_buck, 2);
    ledcAttachPin(BUCK_SYNC, PWM_CHANNEL_SYNC);
    ledcWrite(PWM_CHANNEL_SYNC, 2);

    xTaskCreatePinnedToCore(
        analog_sense_task,
        "Analog Sensing",
        4096,
        NULL,
        TASK_PRIORITY_POWER, // priority
        &xAnalogSenseTaskHandle,
        TASK_CORE_POWER // core
    );
}

// TODO: force transition in radio module if battery power drops too low
void analog_sense_task(void *param) {
  while(true) {
    input_volt = (float) analogRead(ADC_VDD) * ADC_MAX_VOLT / ADC_VDD_SCALE / ADC_FS_COUNTS;

    // figure out whether we're on USB power? Don't run this logic if voltage is very low
    if(input_volt + VDIODE > VUSB) {
      // increment counter if input voltage (corrected for diode drop) is too low
      if(input_volt + VDIODE < (VBAT_NUM_CELL * VBAT_CELL_LOW)) {
        num_low_samples++;
      }
      else {
        num_low_samples = 0;
        // comment in to allow TX without rebooting
        // radio_enable_tx(true);
      }

      // disallow TX if input voltage drops too low
      if(num_low_samples > 10) {
        radio_enable_tx(false);
        io_set_blink_mode(BLINK_ERROR);
        Serial.println("*****Input voltage has dropped below safe value*****");
      }
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
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

float power_get_input_volt() {
  return input_volt;
}