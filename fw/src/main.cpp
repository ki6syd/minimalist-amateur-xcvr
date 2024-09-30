#include <Arduino.h>

#include "globals.h"
#include "git-version.h"
#include "io.h"
#include "power.h"
#include "wifi_conn.h"
#include "server.h"
#include "file_system.h"
#include "audio.h"
#include "radio_hf.h"
#include "radio_vhf.h"
#include "digi_modes.h"
#include "time_keeping.h"

int t = 0;
int counter = 0;

TaskHandle_t xInfoTaskHandle;


void info_task(void *pvParameter);

void setup() {
  // TODO: make sure any slow initializations are happening in parallel
  fs_init();
  power_init();
  io_init();
  wifi_init();
  server_init();

  audio_init();
  audio_set_sidetone_volume(AUDIO_SIDE_DEFAULT);
  audio_set_volume(AUDIO_VOL_DEFAULT);

  radio_hf_init();
  radio_vhf_init();

  time_init();
  digi_mode_init();

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
  if(millis() - t > 1000) {
    if(counter % 2 == 0) {      
      // radio_set_dial_freq(14060000);

      // audio_test(true);
    }
    else {
      // radio_set_dial_freq(14061000);

      // audio_test(false);

    }      
    counter++;
    t = millis();
  } 
}

void info_task(void *param) {
  while(true) {
    Serial.println("\n\n----------INFORMATION----------");

    Serial.print("Input voltage: ");
    Serial.println(power_get_input_volt());

    Serial.print("S-meter: ");
    Serial.println(audio_get_s_meter());

    Serial.println(radio_freq_string());

    Serial.print("IP Address: ");
    Serial.println(wifi_get_ip());

    Serial.print("MAC Address: ");
    Serial.println(wifi_get_mac());

    Serial.print("Firmware: ");
    Serial.println(GIT_VERSION);
    
    Serial.println();
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}
