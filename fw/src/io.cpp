#include "globals.h"
#include "io.h"
#include "radio.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define NOTIFY_DIT            (1 << 0)
#define NOTIFY_DAH            (1 << 1)
#define NOTIFY_SK             (1 << 2)
#define NOTIFY_BTN            (1 << 3)

TaskHandle_t xBlinkTaskHandle, xSpareTaskHandle0, xSpareTaskHandle1, xTxPulseTaskHandle, xKeyTaskHandle;
SemaphoreHandle_t btn_semaphore;

blink_type_t blink_mode = BLINK_NORMAL;

void blink_task(void *pvParameter);

const char *_hwcdc_status[] = {
  " USB Plugged but CDC is NOT connected\r\n",
  " USB Plugged and CDC is connected\r\n",
  " USB Unplugged and CDC NOT connected\r\n",
  " USB Unplugged BUT CDC is connected :: PROBLEM\r\n",
};

const char *HWCDC_Status() {
  int i = Serial.isPlugged() ? 0 : 2;
  if (Serial.isConnected()) {
    i += 1;
  }
  return _hwcdc_status[i];
}


void io_init() {
  blink_mode = BLINK_STARTUP;
  
  pinMode(LED_GRN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  digitalWrite(LED_GRN, HIGH);

  // check if USB is plugged in before attempting to configure Serial
  // background: https://github.com/espressif/arduino-esp32/issues/6983
  if(Serial.isPlugged()) {
    Serial.begin(DEBUG_SERIAL_SPEED);
    // delay gives time to see serial port messages on monitor
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
  else {
    
    Serial.setTxTimeoutMs(0);
  }  

  
  xTaskCreatePinnedToCore(
      blink_task,
      "Blinky light",
      4096,
      NULL,
      TASK_PRIORITY_BLINK, // priority
      &xBlinkTaskHandle,
      TASK_CORE_BLINK // core
  );

  btn_semaphore = xSemaphoreCreateBinary();

}

void io_set_blink_mode(blink_type_t mode) {
  // TODO: check that it's a valid input
  blink_mode = mode;
}

void blink_task(void *param) {
  uint16_t on_duration, off_duration;
  while(true) {
    switch(blink_mode) {
      case BLINK_NORMAL:
        on_duration = 500;
        off_duration = 500;
        break;
      case BLINK_STARTUP:
        on_duration = 100;
        off_duration = 100;
        break;
      case BLINK_ERROR:
        on_duration = 2000;
        off_duration = 2000;
        break;
    }

    digitalWrite(LED_GRN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(on_duration));
    digitalWrite(LED_GRN, LOW);
    vTaskDelay(pdMS_TO_TICKS(off_duration));
  }
}