#include "globals.h"
#include "io.h"
#include "radio_hf.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

TaskHandle_t xBlinkTaskHandle, xSpareTaskHandle0, xSpareTaskHandle1, xTxPulseTaskHandle;
SemaphoreHandle_t btn_semaphore;

blink_type_t blink_mode = BLINK_NORMAL;

void spare_task_core_0(void *pvParameter);
void spare_task_core_1(void *pvParameter);
void blink_task(void *pvParameter);
void tx_pulse_task(void *pvParameter);

ICACHE_RAM_ATTR void buttonISR() {
  // detach interrupt here, reattaches after taking semaphore
  detachInterrupt(digitalPinToInterrupt(BOOT_BTN));
  detachInterrupt(digitalPinToInterrupt(MIC_PTT));
  detachInterrupt(digitalPinToInterrupt(KEY_DIT));
  detachInterrupt(digitalPinToInterrupt(KEY_DAH));

  xSemaphoreGiveFromISR(btn_semaphore, NULL);
}

static void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base == ARDUINO_HW_CDC_EVENTS) {
    switch (event_id) {
      case ARDUINO_HW_CDC_CONNECTED_EVENT:
        digitalWrite(LED_RED, HIGH);
        break;
    }
  }
}

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
  pinMode(LED_DBG_0, OUTPUT);
  pinMode(LED_DBG_1, OUTPUT);
  pinMode(SPARE_0, OUTPUT);

  pinMode(BOOT_BTN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOOT_BTN), buttonISR, CHANGE);
  pinMode(MIC_PTT, INPUT);
  attachInterrupt(digitalPinToInterrupt(MIC_PTT), buttonISR, FALLING);
  pinMode(KEY_DAH, INPUT);
  attachInterrupt(digitalPinToInterrupt(KEY_DAH), buttonISR, FALLING);
  pinMode(KEY_DIT, INPUT);
  attachInterrupt(digitalPinToInterrupt(KEY_DIT), buttonISR, FALLING);

  digitalWrite(SPARE_0, LOW);
  digitalWrite(LED_DBG_0, LOW);
  digitalWrite(LED_DBG_1, LOW);

  Serial.begin(DEBUG_SERIAL_SPEED);
  // delay gives time to see serial port messages on monitor
  delay(5000);

  // helps avoid some sort of overflow when USB is not connected
  if(!Serial.isPlugged() && !Serial.isConnected())
    Serial.end();

  // PSRAM initialization
  // https://community.platformio.org/t/how-to-use-psram-on-esp32-s3-devkitc-1-under-esp-idf/32127/17
  if(psramInit())
      Serial.println("\nPSRAM is correctly initialized");
  else
      Serial.println("PSRAM not available");
  Serial.println(psramFound());

  // check PSRAM
  Serial.print("Size PSRAM: ");
  Serial.println(ESP.getPsramSize());
  Serial.print("Free PSRAM: ");
  Serial.println(ESP.getFreePsram());

  Serial.println("allocating to PSRAM...");
  byte* psram_buffer = (byte*) ps_malloc(1024);

  Serial.print("Free PSRAM: ");
  Serial.println(ESP.getFreePsram());

  // run on core 0
  xTaskCreatePinnedToCore(
      spare_task_core_0,
      "Debug LED spare time task",
      4096,
      NULL,
      TASK_PRIORITY_LOWEST, // priority
      &xSpareTaskHandle0,
      0 // core
  );

  // run on core 1
  xTaskCreatePinnedToCore(
      spare_task_core_1,
      "Debug LED spare time task",
      4096,
      NULL,
      TASK_PRIORITY_LOWEST, // priority
      &xSpareTaskHandle1,
      1 // core
  );

  
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
  xTaskCreatePinnedToCore(
      tx_pulse_task,
      "TX pulse generator",
      4096,
      NULL,
      TASK_PRIORITY_HIGHEST, // priority
      &xTxPulseTaskHandle,
      1 // core
  );
}

void io_set_blink_mode(blink_type_t mode) {
  // TODO: check that it's a valid input
  blink_mode = mode;
}

// if LED_DBG_x is *not* illuminated, spareTask is starved
void spare_task_core_0(void *param) {
  while(true) {
    digitalWrite(LED_DBG_0, HIGH);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    digitalWrite(LED_DBG_0, LOW);
    taskYIELD();
  }
}
void spare_task_core_1(void *param) {
  while(true) {
    digitalWrite(LED_DBG_1, HIGH);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    digitalWrite(LED_DBG_1, LOW);
    taskYIELD();
  }
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
    vTaskDelay(on_duration / portTICK_PERIOD_MS);
    digitalWrite(LED_GRN, LOW);
    vTaskDelay(off_duration / portTICK_PERIOD_MS);
  }
}

void tx_pulse_task(void *param) {
  while(true) {
    if(xSemaphoreTake(btn_semaphore, portMAX_DELAY) == pdPASS) {
      if(digitalRead(BOOT_BTN) == LOW)
        radio_key_on();
      if(digitalRead(BOOT_BTN) == HIGH)
        radio_key_off();


      attachInterrupt(digitalPinToInterrupt(BOOT_BTN), buttonISR, CHANGE);
      attachInterrupt(digitalPinToInterrupt(MIC_PTT), buttonISR, FALLING);
      attachInterrupt(digitalPinToInterrupt(KEY_DIT), buttonISR, FALLING);
      attachInterrupt(digitalPinToInterrupt(KEY_DAH), buttonISR, FALLING);
    }
  }
}
