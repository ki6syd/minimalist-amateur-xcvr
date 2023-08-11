#define ADC_SCALING_VBAT      0.0349
#define ADC_SCALING_AUDIO     0.00976


void handle_antenna(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) {
  if(request_type == HTTP_PUT) {
    // look for required parameters in the message
    if(!request->hasParam("antennaPath")) {
      Serial.println("[IO] antennaPath not received");
      request->send(400, "text/plain", "antennaPath not found");
      return;
    }

    String ant_path = request->getParam("antennaPath")->value();

    if(ant_path == "DIRECT")
      ant = OUTPUT_ANT_DIRECT;
    else if(ant_path = "EFHW")
      ant = OUTPUT_ANT_XFMR;
    else {
      request->send(400, "text/plain", "Requested antenna pathway not possible");
      return;
    }

    // hardware update will happen through main loop
    flag_freq = true;
    request->send(200, "text/plain", "OK");
  }
  else if(request_type == HTTP_GET) {
    String result;
    if(ant == OUTPUT_ANT_DIRECT)
      result = "DIRECT";
    else if(ant == OUTPUT_ANT_XFMR)
      result = "EFHW";
    
    request->send(200, "text/plain", result);
  }
}


void handle_lna(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) {
  if(request_type == HTTP_PUT) {
    // look for required parameters in the message
    if(!request->hasParam("lnaState")) {
      Serial.println("[IO] lnaState not received");
      request->send(400, "text/plain", "lnaState not found");
      return;
    }

    String lna = request->getParam("lnaState")->value();

    if(lna == "ON")
      lna_state = true;
    else if(lna = "OFF")
      lna_state = false;
    else {
      request->send(400, "text/plain", "Requested LNA state not possible");
      return;
    }

    // hardware update will happen through main loop
    flag_freq = true;
    request->send(200, "text/plain", "OK");
  }
  else if(request_type == HTTP_GET) {
    String lna = "";
    if(lna_state)
      lna = "ON";
    else
      lna = "OFF";
    
    request->send(200, "text/plain", lna);
  }
}


// TODO: get rid of hard-coded pin numbering below

void init_gpio() {  
  pinMode(0, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(15, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(16, OUTPUT);

  // set up the relay driver IO expander
  if(!pcf8574_relays.begin() && !pcf8574_audio.begin())
    Serial.println("[PCF8574] Couldn't initialize");
  else {
    for(int i=0; i < 8; i++) {
      pcf8574_audio.write(i, LOW);
      pcf8574_relays.write(i, LOW);
    }
    Serial.print("[PCF8574] Connected?: ");
    Serial.print(pcf8574_audio.isConnected());
    Serial.print(",");
    Serial.println(pcf8574_relays.isConnected());
    Serial.println("[PCF8574] Initialized and set OUTPUT_OFF");
  }

  // turn off LEDs
  gpio_write(OUTPUT_GREEN_LED, OUTPUT_OFF);
  gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);

  // paddle GPIOs
  pinMode(12, INPUT_PULLUP);
  pinMode(13, INPUT_PULLUP);
  attach_paddle_isr(true, true);

  analogWriteFreq(load_json_config(hw_config_file, "sidetone_pitch_hz").toInt());

  // select audio filter routing
  gpio_write(OUTPUT_BW_SEL, (output_state) rx_bw);

  // load audio default value
  vol = (uint16_t) load_json_config(hw_config_file, "af_gain_default").toFloat();
}


// attaches/deattaches interrupt on dit/dah pins
void attach_paddle_isr(bool dit_en, bool dah_en) {
  
  if(dit_en)
    attachInterrupt(digitalPinToInterrupt(12), paddle_isr, ONLOW);
  if(!dit_en)
    detachInterrupt(digitalPinToInterrupt(12));

  if(dah_en)
    attachInterrupt(digitalPinToInterrupt(13), paddle_isr, ONLOW);
  if(!dah_en)
    detachInterrupt(digitalPinToInterrupt(13));
    
}

// when we detect dit/dah paddle:
//  - turn off the interrupt to prevent future events
//  - set a flag
ICACHE_RAM_ATTR void paddle_isr() {
  
  if(!digitalRead(12)) {
    attach_paddle_isr(false, true);
    dit_flag = true;
  }
  if(!digitalRead(13)) {
    attach_paddle_isr(true, false);
    dah_flag = true;
  }

  // empty the keyer queue so touching the paddle stops any ongoing messages
  tx_queue = "";
}


// TODO: accept a gpio number that doesn't necessarily map to ESP12. This would allow an IO expander to work with this function
void gpio_write(output_pin pin, output_state state) {
  switch(pin) {
    case OUTPUT_RX_MUTE:
      if(state == OUTPUT_UNMUTED)
        digitalWrite(15, LOW);
      if(state == OUTPUT_MUTED)
        digitalWrite(15, HIGH);
      break;
      
    case OUTPUT_TX_VDD_EN:
      if(state == OUTPUT_ON)
        digitalWrite(14, HIGH);
      if(state == OUTPUT_OFF)
        digitalWrite(14, LOW);
      break;
      
    case OUTPUT_ADC_SEL:
      if(state == OUTPUT_SEL_VBAT)
        digitalWrite(16, LOW);
      if(state == OUTPUT_SEL_AUDIO)
        digitalWrite(16, HIGH);
      break;
      
    case OUTPUT_GREEN_LED:
      if(state == OUTPUT_ON)
        digitalWrite(0, HIGH);
      if(state == OUTPUT_OFF)
        digitalWrite(0, LOW);
      break;

    case OUTPUT_RED_LED:
      if(state == OUTPUT_ON)
        digitalWrite(2, HIGH);
      if(state == OUTPUT_OFF)
        digitalWrite(2, LOW);
      break;
      
    case OUTPUT_LPF_1:
      if(state == OUTPUT_ON)
        pcf8574_relays.write(0, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574_relays.write(0, LOW);
      break;
      
    case OUTPUT_BPF_1:
      if(state == OUTPUT_ON)
        pcf8574_relays.write(5, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574_relays.write(5, LOW);
      break;

    case OUTPUT_LPF_2:
      if(state == OUTPUT_ON)
        pcf8574_relays.write(1, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574_relays.write(1, LOW);
      break;

    case OUTPUT_BPF_2:
      if(state == OUTPUT_ON)
        pcf8574_relays.write(4, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574_relays.write(4, LOW);
      break;

    case OUTPUT_LPF_3:
      if(state == OUTPUT_ON)
        pcf8574_relays.write(2, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574_relays.write(2, LOW);
      break;

    case OUTPUT_BPF_3:
      if(state == OUTPUT_ON)
        pcf8574_relays.write(3, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574_relays.write(3, LOW);
      break;

    case OUTPUT_BW_SEL:
      if(state == OUTPUT_SEL_CW)
        pcf8574_audio.write(4, HIGH);
      if(state == OUTPUT_SEL_SSB)
        pcf8574_audio.write(4, LOW);
      break;

    case OUTPUT_LNA_SEL:
      if(state == OUTPUT_ON) {
        pcf8574_relays.write(6, HIGH);
      }
      if(state == OUTPUT_OFF) {
        pcf8574_relays.write(6, LOW);
      }
      break;

    case OUTPUT_ANT_SEL:
      if(state == OUTPUT_ANT_DIRECT) {
        pcf8574_relays.write(7, LOW);
      }
      if(state == OUTPUT_ANT_XFMR) {
        pcf8574_relays.write(7, HIGH);
      }
      break;

  }
}

// returns analog readings in the correct units (as a float)
// handles any required muxing
float analog_read(input_pin pin) {
  int16_t adc_counts;
  float reading;
  
  switch(pin) {
    case INPUT_VBAT:
      // change to the correct mux setting
      gpio_write(OUTPUT_ADC_SEL, OUTPUT_SEL_VBAT);
      // settling time
      my_delay(1);
      adc_counts = analogRead(A0);
      reading = adc_counts * ADC_SCALING_VBAT;
      break;

    case INPUT_AUDIO:
      gpio_write(OUTPUT_ADC_SEL, OUTPUT_SEL_AUDIO);
      // settling time
      my_delay(1);
      adc_counts = analogRead(A0);
      reading = adc_counts * ADC_SCALING_AUDIO;
      break;
  }

  return reading;
}


// take "n" samples and compute amplitude/sum/average
void update_smeter () {
  uint8_t num_samples = 50;
  float sum = 0, avg = 0, rms = 0;
  float readings[num_samples];

  // pick audio input for ADC
  gpio_write(OUTPUT_ADC_SEL, OUTPUT_SEL_AUDIO);

  // sampling loop
  for(uint8_t i=0; i<num_samples; i++) {
    readings[i] = ADC_SCALING_AUDIO * analogRead(A0);
  }

  // averaging loop
  for(uint8_t i=0; i<num_samples; i++) {
    sum += readings[i];
  }
  avg = sum / num_samples;
  
  for(uint8_t i=0; i<num_samples; i++) {
    readings[i] -= avg;
    rms += readings[i] * readings[i];
  }
  rms = sqrt(rms/num_samples);


  // TODO - make a flag for printing this out
  /*
  Serial.print("[S-METER]\tAvg: ");
  Serial.print(avg);
  Serial.print("\tSum: ");
  Serial.print(sum);
  Serial.print("\tRMS: ");
  Serial.println(rms);
  */

  last_smeter = rms;

  my_delay(10);
}

// TODO: implement a maximum volume control based on JSON file
// TODO: add min/max valid inputs
void update_volume(uint16_t volume) {
  uint8_t vol_1 = 0;
  uint8_t vol_2 = 1;
  uint8_t vol_3 = 2;
  uint8_t vol_4 = 3;

  // TODO: speed this up by only doing a single i2c write.
  switch(volume) {
    case 1:
      pcf8574_audio.write(vol_1, LOW);
      pcf8574_audio.write(vol_2, LOW);
      pcf8574_audio.write(vol_3, LOW);
      pcf8574_audio.write(vol_4, LOW);
      break;
    case 2:
      pcf8574_audio.write(vol_1, LOW);
      pcf8574_audio.write(vol_2, LOW);
      pcf8574_audio.write(vol_3, LOW);
      pcf8574_audio.write(vol_4, HIGH);
      break;
    case 3:
      pcf8574_audio.write(vol_1, LOW);
      pcf8574_audio.write(vol_2, LOW);
      pcf8574_audio.write(vol_3, HIGH);
      pcf8574_audio.write(vol_4, LOW);
      break;
    case 4:
      pcf8574_audio.write(vol_1, LOW);
      pcf8574_audio.write(vol_2, HIGH);
      pcf8574_audio.write(vol_3, LOW);
      pcf8574_audio.write(vol_4, LOW);
      break;
    case 5:
      pcf8574_audio.write(vol_1, HIGH);
      pcf8574_audio.write(vol_2, LOW);
      pcf8574_audio.write(vol_3, LOW);
      pcf8574_audio.write(vol_4, LOW);
  }
}
