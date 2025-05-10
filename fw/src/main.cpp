#include <Arduino.h>

#include "globals.h"
#include "io.h"
#include "power.h"
#include "wifi_conn.h"
#include "server.h"
#include "file_system.h"
#include "audio.h"
#include "audio_dsp.h"
#include "radio.h"
#include "digi_modes.h"
#include "time_keeping.h"

int t = 0;
int counter = 0;

TaskHandle_t xInfoTaskHandle;

void info_task(void *pvParameter);

void setup() {
  // TODO: make sure any slow initializations are happening in parallel
  io_init();
  fs_init();
  power_init();
  wifi_init();
  server_init();
  audio_init();
  radio_init();
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
  if(millis() - t > 4000) {
    if(counter % 2 == 0) {      
      // audio_test(true);
    }
    else {
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

    Serial.print("PA Current: ");
    Serial.println(power_get_pa_current());

    Serial.print("PA Voltage: ");
    Serial.println(power_get_pa_volt());

    Serial.print("PA Temperature: ");
    Serial.println(power_get_pa_temp());

    Serial.print("Transmit Power: ");
    Serial.println(radio_get_power());
    
    Serial.print("Bias duty: CH0:");
    Serial.print(power_get_bias_duty(BIAS_CHANNEL_0));
    Serial.print("\tCH1: ");
    Serial.println(power_get_bias_duty(BIAS_CHANNEL_1)); 
    Serial.print("AGC duty:");
    Serial.println(power_get_agc_duty());

    Serial.print("S-meter: ");
    Serial.println(radio_get_s_meter());

    Serial.print("Audio dB: ");
    Serial.println(audio_get_loudness());

    Serial.print("PGA: ");
    Serial.print(audio_get_pga());

    Serial.print("\tVolume: ");
    Serial.print(audio_get_volume());

    Serial.print("\tSidetone Level: ");
    Serial.println(audio_get_sidetone_volume());

    Serial.println(radio_freq_string());    

    Serial.print("Current band: ");
    Serial.println(radio_band_to_string(radio_get_band(radio_get_dial_freq())));

    Serial.print("Current Radio Modulation: ");
    Serial.println(radio_modulation_to_string(radio_get_modulation()));

    Serial.print("Current Audio Bandwidth: ");
    Serial.println(radio_audio_filt_to_string(audio_dsp_get_filter()));

    // TODO: print out the audio module bandwidth

    Serial.print("IP Address: ");
    Serial.println(wifi_get_ip());

    Serial.print("MAC Address: ");
    Serial.println(wifi_get_mac());

    Serial.print("Firmware: ");
    Serial.println(AUTO_VERSION);

    Serial.print("Dit: ");
    Serial.print(digitalRead(KEY_DIT));
    Serial.print("\tDah: ");
    Serial.print(digitalRead(KEY_DAH));
    Serial.print("\tBOOT: ");
    Serial.print(digitalRead(BOOT_BTN));
    Serial.print("\tPTT: ");
    Serial.println(digitalRead(PTT_MIC));
    
    Serial.println();

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}


