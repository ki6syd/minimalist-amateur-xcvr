#include <Arduino.h>

#include "globals.h"
#include "io.h"
#include "power.h"
#include "wifi_conn.h"
#include "server.h"
#include "file_system.h"
#include "radio.h"

int t = 0;
int counter = 0;

TaskHandle_t xInfoTaskHandle;

void info_task(void *pvParameter);

void setup() {
  vTaskDelay(pdMS_TO_TICKS(5000));

  // TODO: make sure any slow initializations are happening in parallel
  io_init();
  fs_init();
  power_init();
  wifi_init();
  server_init();
  radio_init();

  // todo: signal that setup is complete with some sort of semaphore, the init() functions *don't* block until setup is complete
  io_set_blink_mode(BLINK_NORMAL);

  xTaskCreatePinnedToCore(
        info_task,
        "Information Task",
        4096,
        NULL,
        TASK_PRIORITY_INFO, // priority
        &xInfoTaskHandle,
        TASK_CORE_INFO // core
    );

}

// code in loop() is just for testing, don't want to actually do anything here
void loop() { 

}

void info_task(void *param) {
  while(true) {
    Serial.println("\n\n----------INFORMATION----------");

    Serial.print("Input voltage: ");
    Serial.println(power_get_input_volt());    


    Serial.print("IP Address: ");
    Serial.println(wifi_get_ip());

    Serial.print("MAC Address: ");
    Serial.println(wifi_get_mac());

    Serial.print("Firmware: ");
    Serial.println(AUTO_VERSION);
    
    Serial.println();

    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}


