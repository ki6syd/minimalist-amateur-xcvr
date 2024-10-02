#include "globals.h"
#include "io.h"
#include "radio_hf.h"
#include "keyer.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define NOTIFY_DIT            1
#define NOTIFY_DAH            2
#define NOTIFY_SK             4

TaskHandle_t xBlinkTaskHandle, xSpareTaskHandle0, xSpareTaskHandle1, xTxPulseTaskHandle, xKeyTaskHandle;
SemaphoreHandle_t btn_semaphore;

blink_type_t blink_mode = BLINK_NORMAL;

void spare_task_core_0(void *pvParameter);
void spare_task_core_1(void *pvParameter);
void blink_task(void *pvParameter);
void key_task(void *pvParameter);
void tx_pulse_task(void *pvParameter);
void io_enable_dit_isr(bool enable);
void io_enable_dah_isr(bool enable);

// only handles straight key / microphone PTT
ICACHE_RAM_ATTR void buttonISR() {
  // detach interrupt here, reattaches after taking semaphore
  detachInterrupt(digitalPinToInterrupt(BOOT_BTN));
  detachInterrupt(digitalPinToInterrupt(MIC_PTT));

  xSemaphoreGiveFromISR(btn_semaphore, NULL);
}

// only handles paddle inputs (dit, dah)
// reference for freertos code in this function: https://www.freertos.org/Documentation/02-Kernel/04-API-references/05-Direct-to-task-notifications/07-xTaskNotifyFromISR
ICACHE_RAM_ATTR void paddle_isr() {

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  // check which pin caused the interrupt. Detach its interrupt, notify the task
  if(digitalRead(KEY_DIT) == LOW) {
    io_enable_dit_isr(false);
    xTaskNotifyFromISR(xKeyTaskHandle, NOTIFY_DIT, eSetBits, &xHigherPriorityTaskWoken);
  }
  if(digitalRead(KEY_DAH) == LOW) {
    io_enable_dah_isr(false);
    xTaskNotifyFromISR(xKeyTaskHandle, NOTIFY_DAH, eSetBits, &xHigherPriorityTaskWoken);
  }

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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
  pinMode(KEY_DIT, INPUT);
  io_enable_dit_isr(true);
  io_enable_dah_isr(true);

  digitalWrite(SPARE_0, LOW);
  digitalWrite(LED_DBG_0, LOW);
  digitalWrite(LED_DBG_1, LOW);
  digitalWrite(LED_GRN, HIGH);
  digitalWrite(LED_RED, HIGH);

  // check if USB is plugged in before attempting to configure Serial
  // background: https://github.com/espressif/arduino-esp32/issues/6983
  if(Serial.isPlugged()) {
    Serial.begin(DEBUG_SERIAL_SPEED);
    // delay gives time to see serial port messages on monitor
    vTaskDelay(pdMS_TO_TICKS(5000));
    digitalWrite(LED_RED, LOW);
  }
  else {
    
    Serial.setTxTimeoutMs(0);
  }
    

  // Serial.begin(DEBUG_SERIAL_SPEED);
  // // delay gives time to see serial port messages on monitor
  // vTaskDelay(pdMS_TO_TICKS(5000));

  // // helps avoid some sort of overflow when USB is not connected
  // if(!Serial.isPlugged() && !Serial.isConnected())
  //   Serial.end();

  

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


  xTaskCreatePinnedToCore(
      key_task,
      "Key monitoring task",
      4096,
      NULL,
      TASK_PRIORITY_KEY_IO, // priority
      &xKeyTaskHandle,
      TASK_CORE_KEY_IO // core
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
    vTaskDelay(pdMS_TO_TICKS(1));
    digitalWrite(LED_DBG_0, LOW);
    taskYIELD();
  }
}
void spare_task_core_1(void *param) {
  while(true) {
    digitalWrite(LED_DBG_1, HIGH);
    vTaskDelay(pdMS_TO_TICKS(1));
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
    vTaskDelay(pdMS_TO_TICKS(on_duration));
    digitalWrite(LED_GRN, LOW);
    vTaskDelay(pdMS_TO_TICKS(off_duration));
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
    }
  }
}

void key_task(void *param) {
  uint32_t notified_value;
  while(true) {
    // paddle_isr() will unblock and force context switch
    if(xTaskNotifyWait(pdFALSE, ULONG_MAX, &notified_value, 10) == pdTRUE) {
      if(notified_value & NOTIFY_DIT) {
        keyer_dit();
        io_enable_dah_isr(true);
        io_enable_dit_isr(true);
      }
      if(notified_value & NOTIFY_DAH) {
        keyer_dah();
        io_enable_dit_isr(true);
        io_enable_dah_isr(true);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void io_enable_dit_isr(bool enabled) {
  if(enabled)
    attachInterrupt(digitalPinToInterrupt(KEY_DIT), paddle_isr, ONLOW);
  else
    detachInterrupt(digitalPinToInterrupt(KEY_DIT));
}

void io_enable_dah_isr(bool enabled) {
  if(enabled)
    attachInterrupt(digitalPinToInterrupt(KEY_DAH), paddle_isr, ONLOW);
  else
    detachInterrupt(digitalPinToInterrupt(KEY_DAH));
}