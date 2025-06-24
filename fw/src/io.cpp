#include "globals.h"
#include "io.h"
#include "radio.h"
#include "keyer.h"
#include "digi_modes.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define NOTIFY_DIT            (1 << 0)
#define NOTIFY_DAH            (1 << 1)
#define NOTIFY_BTN            (1 << 2)

TaskHandle_t xBlinkTaskHandle, xSpareTaskHandle0, xSpareTaskHandle1, xTxPulseTaskHandle, xKeyTaskHandle;

blink_type_t blink_mode = BLINK_NORMAL;

void spare_task_core_0(void *pvParameter);
void spare_task_core_1(void *pvParameter);
void blink_task(void *pvParameter);
void key_task(void *pvParameter);
void io_enable_dit_isr(bool enable);
void io_enable_dah_isr(bool enable);
void io_enable_btn_isr(bool enable);

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


void io_init() {
  blink_mode = BLINK_STARTUP;
  
  pinMode(LED_GRN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(SPARE_0, OUTPUT);

  pinMode(BOOT_BTN, INPUT_PULLUP);
  pinMode(PTT_MIC, INPUT);
  pinMode(PTT_HP, INPUT);
  
  pinMode(KEY_DAH, INPUT);
  pinMode(KEY_DIT, INPUT);

  digitalWrite(SPARE_0, LOW);
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

  
  xTaskCreatePinnedToCore(
      blink_task,
      "Blinky light",
      4096,
      NULL,
      TASK_PRIORITY_BLINK, // priority
      &xBlinkTaskHandle,
      TASK_CORE_BLINK // core
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

void key_task(void *param) {
  uint32_t notified_value;
  bool last_button_state = false;

  // attach interrupts. do it in the task to avoid running ISR before task starts
  io_enable_dit_isr(true);
  io_enable_dah_isr(true);

  while(true) {
    // paddle_isr() will unblock and force context switch. Waits 10ms
    if(xTaskNotifyWait(pdFALSE, ULONG_MAX, &notified_value, pdMS_TO_TICKS(10)) == pdTRUE) {
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

      // any value of notified_value should cancel currently sending messages
      digi_mode_queue_clear();
    }
    
    // poll pushbuttons. call key_on/key_off if button state has changed
    if(digitalRead(BOOT_BTN) == LOW || digitalRead(PTT_MIC) == LOW || digitalRead(PTT_HP) == LOW) {
      if(!last_button_state) {  
        radio_key_on();
        last_button_state = true;
      }
    }
    if(digitalRead(BOOT_BTN) == HIGH && digitalRead(PTT_MIC) == HIGH && digitalRead(PTT_HP) == HIGH) {
      if(last_button_state) {  
        radio_key_off();
        last_button_state = false;
      }
    }
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