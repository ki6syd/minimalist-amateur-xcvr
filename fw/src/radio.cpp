#include "globals.h"
#include "radio.h"
#include "radio_hf.h"
#include "io.h"
#include "file_system.h"
#include "power.h"

#include <Arduino.h>

#define FREQ_PLL                  (SI5351_PLL_FIXED)

TaskHandle_t xRadioTaskHandle;

void radio_task(void * pvParameter);

void radio_init() {

  hf_init();

  xTaskCreatePinnedToCore(
    radio_task,
    "Radio Task",
    16384,
    NULL,
    TASK_PRIORITY_RADIO, // priority
    &xRadioTaskHandle,
    TASK_CORE_RADIO // core
  );
}


void radio_task(void *param) {
  uint32_t notifiedValue;

  while(true) {

    // wait 5 ms to allow other tasks to run
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
