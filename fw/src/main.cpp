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
      String info_output = "";
    
      info_output += "\n\n";
      info_output += "----------INFORMATION----------\n";
    
      info_output += "Input voltage: ";
      info_output += String(power_get_input_volt()) + "\n";
    
      info_output += "PA Current: ";
      info_output += String(power_get_pa_current()) + "\n";
    
      info_output += "PA Voltage: ";
      info_output += String(power_get_pa_volt()) + "\n";
    
      info_output += "PA Temperature: ";
      info_output += String(power_get_pa_temp()) + "\n";
    
      info_output += "Transmit Power: ";
      info_output += String(radio_get_power()) + "\n";
    
      info_output += "Bias duty CH0: ";
      info_output += String(power_get_bias_duty(BIAS_CHANNEL_0));
      info_output += "\t\tCH1: ";
      info_output += String(power_get_bias_duty(BIAS_CHANNEL_1));
      info_output += "\t\tAGC duty: ";
      info_output += String(power_get_agc_duty()) + "\n";
    
      info_output += "S-meter: ";
      info_output += String(radio_get_s_meter()) + "\n";
    
      info_output += "Audio dB: ";
      info_output += String(audio_get_loudness()) + "\n";
    
      info_output += "PGA: ";
      info_output += String(audio_get_pga());
      info_output += "\t\tVolume: ";
      info_output += String(audio_get_volume());
      info_output += "\t\tSidetone Level: ";
      info_output += String(audio_get_sidetone_volume()) + "\n";
    
      info_output += String(radio_freq_string()) + "\n";
    
      info_output += "Current band: ";
      info_output += String(radio_band_to_string(radio_get_band(radio_get_dial_freq()))) + "\n";
    
      info_output += "Current Radio Modulation: ";
      info_output += String(radio_modulation_to_string(radio_get_modulation()));
    
      info_output += "\t\tCurrent Audio Bandwidth: ";
      info_output += String(radio_audio_filt_to_string(audio_dsp_get_filter()));

      info_output += "\t\tCurrent Sideband: ";
      info_output += String(radio_sideband_to_string(radio_get_sideband())) + "\n";
    
      info_output += "IP Address: ";
      info_output += String(wifi_get_ip()) + "\n";
    
      info_output += "MAC Address: ";
      info_output += String(wifi_get_mac()) + "\n";
    
      info_output += "Firmware: ";
      info_output += String(AUTO_VERSION) + "\n";
    
      info_output += "Dit: ";
      info_output += String(digitalRead(KEY_DIT));
      info_output += "\tDah: ";
      info_output += String(digitalRead(KEY_DAH));
      info_output += "\tBOOT: ";
      info_output += String(digitalRead(BOOT_BTN));
      info_output += "\tPTT_MIC: ";
      info_output += String(digitalRead(PTT_MIC));
      info_output += "\tPTT_HP: ";
      info_output += String(digitalRead(PTT_HP)) + "\n";
    
      info_output += "\n";
    
      Serial.print(info_output);

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}


