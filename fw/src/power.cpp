#include "globals.h"
#include "power.h"
#include "radio.h"
#include "io.h"
#include "file_system.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define POWER_BUCK_FSW      510e3
#define VDIODE              0.6
#define VUSB_MAX            5.5


TaskHandle_t xAnalogSenseTaskHandle;
SemaphoreHandle_t xADCmutex;

uint32_t freq_buck = POWER_BUCK_FSW;
float input_volt = 0, pa_curr = 0, pa_volt = 0, pa_temp = 0;
float vbat_cell_low = 3.0;
uint16_t num_cell = 3;

void analog_sense_task(void *pvParameter);

void power_init() {

  // initialize mutex for ADC readings through the mux
  xADCmutex = xSemaphoreCreateMutex();
  if(xADCmutex == NULL) {
    Serial.println("Error creating mutex xADCmutex");
  }

  // load configuration from JSON file
  if(fs_setting_exists(PREFERENCE_FILE, "vbat_cell_low"))
    vbat_cell_low = fs_load_setting_float(PREFERENCE_FILE, "vbat_cell_low", 0.0, 5.0);
  if(fs_setting_exists(PREFERENCE_FILE, "num_battery"))
    num_cell = fs_load_setting_long(PREFERENCE_FILE, "num_battery", 0, 12);

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
      TASK_PRIORITY_ADC, // priority
      &xAnalogSenseTaskHandle,
      TASK_CORE_ADC // core
  );
}

// TODO: force transition in radio module if battery power drops too low
void analog_sense_task(void *param) {
  while(true) {
    // update all ADC readings, only if mutex is available. Blocks until available.
    if(xSemaphoreTake(xADCmutex, portMAX_DELAY) == pdTRUE) {
      // read all ADC channels
      input_volt = power_adc_conversion(ADC_CHANNEL_VIN);

      // give back the mutex after reading
      xSemaphoreGive(xADCmutex);
    }

    // check if power source is likely battery, and then if minimum cell voltage is an issue
    if(input_volt > 5.0 && input_volt / num_cell < vbat_cell_low) {
      io_set_blink_mode(BLINK_ERROR);
      digitalWrite(LED_RED, HIGH);
    }
    else {
      io_set_blink_mode(BLINK_NORMAL);
      digitalWrite(LED_RED, LOW);
    }

    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

// updates buck converters to a new switching frequency [Hz]
// TODO: test this, implement logic to set it based on dial frequency
void power_update_freq(uint32_t new_freq) {
    if(new_freq < 1e6 && new_freq > 100e3) {
        freq_buck = new_freq;
        ledcChangeFrequency(PWM_CHANNEL_SYNC, freq_buck, 2);
    }
}

float power_adc_conversion(adc_channel_t channel) {
  if(channel == ADC_CHANNEL_VIN)
    return (float) analogRead(ADC_VDD) * ADC_MAX_VOLT / ADC_VDD_SCALE / ADC_FS_COUNTS;
  else {
    // placeholder
  }

  return 0;
}

float power_get_input_volt() {
  return input_volt;
}
