#include <Arduino.h>

#include "globals.h"
#include "io.h"
#include "power.h"
#include "wifi_conn.h"
#include "audio.h"
#include "radio_hf.h"
#include "radio_vhf.h"

int t = 0;
int counter = 0;

void setup() {
  power_init();
  io_init();  
  Serial.println("initializing");
  wifi_init();

  Serial.println("initializing audio");
  audio_init();
  audio_set_sidetone_volume(AUDIO_SIDE_DEFAULT);
  audio_set_volume(AUDIO_VOL_DEFAULT);

  Serial.println("initializing hf radio");
  radio_hf_init();
  radio_vhf_init();
}

// code in loop() is just for testing, don't want to actually do anything here
void loop() { 
  if(millis() - t > 1000) {
    if(counter % 2 == 0) {
      Serial.println("0");
      radio_set_dial_freq(14060000);

      audio_test(true);
    }
    else {
      radio_set_dial_freq(14061000);

      audio_test(false);

    }      
    counter++;
    t = millis();
  } 
}
