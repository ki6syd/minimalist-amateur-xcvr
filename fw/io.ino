#define ADC_SCALING_VBAT      0.0349
// ADC_SCALING_AUDIO converts ADC counts to volts at the processor pin
#define ADC_SCALING_AUDIO     0.000976

// relay operating time
#define RELAY_DELAY 5

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
      request->send(400, "text/plain", "Requested antenna pathway not a valid option");
      return;
    }

    // hardware update will happen through main loop
    flag_freq = true;
    request->send(201, "text/plain", "OK");
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
      request->send(400, "text/plain", "Requested LNA state not a valid option");
      return;
    }

    // hardware update will happen through main loop
    flag_freq = true;
    request->send(201, "text/plain", "OK");
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

  // figure out which hardware version we are running
  hardware_rev = load_json_config(hardware_file, "hardware_rev");

  // set up the relay driver IO expander
  if(!pcf8574_20.begin() && !pcf8574_21.begin())
    Serial.println("[PCF8574] Couldn't initialize");
  else {
    for(int i=0; i < 8; i++) {
      pcf8574_21.write(i, LOW);
      pcf8574_20.write(i, LOW);
    }
    Serial.print("[PCF8574] Connected?: ");
    Serial.print(pcf8574_21.isConnected());
    Serial.print(",");
    Serial.println(pcf8574_20.isConnected());
    Serial.println("[PCF8574] Initialized and set OUTPUT_OFF");
  }

  // turn off LEDs
  gpio_write(OUTPUT_GREEN_LED, OUTPUT_OFF);
  gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);

  // paddle GPIOs
  pinMode(12, INPUT_PULLUP);
  pinMode(13, INPUT_PULLUP);

  // detect key type at startup. straight key will have one pin shorted.
  if(digitalRead(13) == LOW) {
    Serial.println("[KEYER] Straight key detected");
    key = KEY_STRAIGHT;
  }
  else {
    Serial.println("[KEYER] Paddle detected");
    key = KEY_PADDLE;
  }

  // different interrupt actions for different key types
  if(key == KEY_PADDLE)
    attach_paddle_isr(true, true);
  else
    attach_sk_isr(true);

  analogWriteFreq(load_json_config(preference_file, "sidetone_pitch_hz").toInt());

  // select audio filter routing
  gpio_write(OUTPUT_BW_SEL, (output_state) rx_bw);

  // load audio default value
  vol = (uint16_t) load_json_config(preference_file, "af_gain_default").toFloat();
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
//  - set flag so main loop can dit() or dah()
ICACHE_RAM_ATTR void paddle_isr() {
  
  if(!digitalRead(12)) {
    attach_paddle_isr(false, true);
    dit_flag = true;
  }
  if(!digitalRead(13)) {
    attach_paddle_isr(true, false);
    dah_flag = true;
  }
}

// straight key interrupt enable/disable
void attach_sk_isr(bool key_en) {
  if(key_en)
    attachInterrupt(digitalPinToInterrupt(12), sk_isr, CHANGE);
  else
    detachInterrupt(digitalPinToInterrupt(12));    
}

// when we detect key falling:
// - turn off interrupt so key_on() or key_off() can complete before we look again
// - set flag so main loop can act
ICACHE_RAM_ATTR void sk_isr() {  
  // don't listen for interrupts for a bit
  attach_sk_isr(false);

  // check if we need to transition state from UP to DOWN
  if(sk_mode == MODE_SK_UP && digitalRead(12) == 0) {
    Serial.println("key pressed");
    sk_mode = MODE_SK_DOWN;
  }

  // check if we need to transition state from DONW to UP
  if(sk_mode == MODE_SK_DOWN && digitalRead(12) == 1) {
    Serial.println("key released");
    sk_mode = MODE_SK_UP;
  }

  // set a flag for the main loop to act on
  sk_flag = true;
}


// relay related commands add a 5ms delay so function doesn't return until it should have toggled
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
        pcf8574_20.write(0, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574_20.write(0, LOW);
      my_delay(RELAY_DELAY);
      break;
      
    case OUTPUT_BPF_1:
      if(state == OUTPUT_ON)
        pcf8574_20.write(5, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574_20.write(5, LOW);
      my_delay(RELAY_DELAY);
      break;

    case OUTPUT_LPF_2:
      if(state == OUTPUT_ON)
        pcf8574_20.write(1, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574_20.write(1, LOW);
      my_delay(RELAY_DELAY);
      break;

    case OUTPUT_BPF_2:
      if(state == OUTPUT_ON)
        pcf8574_20.write(4, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574_20.write(4, LOW);
      my_delay(RELAY_DELAY);
      break;

    case OUTPUT_LPF_3:
      if(state == OUTPUT_ON)
        pcf8574_20.write(2, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574_20.write(2, LOW);
      my_delay(RELAY_DELAY);
      break;

    case OUTPUT_BPF_3:
      if(state == OUTPUT_ON)
        pcf8574_20.write(3, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574_20.write(3, LOW);
      my_delay(RELAY_DELAY);
      break;

    case OUTPUT_BW_SEL:
      if(hardware_rev == "max-3b_v1") {
        if(state == OUTPUT_SEL_CW)
          pcf8574_21.write(4, HIGH);
        if(state == OUTPUT_SEL_SSB)
          pcf8574_21.write(4, LOW);
      }
      if(hardware_rev == "max-3b_v2") {
        if(state == OUTPUT_SEL_CW)
          pcf8574_21.write(6, HIGH);
        if(state == OUTPUT_SEL_SSB)
          pcf8574_21.write(6, LOW);
      }
      break;

    case OUTPUT_LNA_SEL:
      if(hardware_rev == "max-3b_v1") {
        if(state == OUTPUT_ON) {
          pcf8574_20.write(6, HIGH);
        }
        if(state == OUTPUT_OFF) {
          pcf8574_20.write(6, LOW);
        }
      }
      if(hardware_rev == "max-3b_v2") {
        if(state == OUTPUT_ON) {
          pcf8574_20.write(7, HIGH);
        }
        if(state == OUTPUT_OFF) {
          pcf8574_20.write(7, LOW);
        }
      }
      break;

    case OUTPUT_ANT_SEL:
      if(hardware_rev == "max-3b_v1") {
        if(state == OUTPUT_ANT_DIRECT) {
          pcf8574_20.write(7, LOW);
        }
        if(state == OUTPUT_ANT_XFMR) {
          pcf8574_20.write(7, HIGH);
        }
      }
      if(hardware_rev == "max-3b_v2") {
        if(state == OUTPUT_ANT_DIRECT) {
          pcf8574_21.write(0, LOW);
        }
        if(state == OUTPUT_ANT_XFMR) {
          pcf8574_21.write(0, HIGH);
        }
      }
      break;

    case OUTPUT_SPKR_EN:
      // not supported in hardware v1
      if(hardware_rev == "max-3b_v2") {
        if(state == OUTPUT_ON) {
          pcf8574_20.write(6, HIGH);
        }
        if(state == OUTPUT_OFF) {
          pcf8574_20.write(6, LOW);
        }
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
  uint8_t num_samples = 100;
  uint16_t sample_interval = 200;
  uint64_t last_sample_time = micros();
  float sum = 0, avg = 0, rms = 0, min_val = 0, max_val = 0;
  float readings[num_samples];

  // pick audio input for ADC
  gpio_write(OUTPUT_ADC_SEL, OUTPUT_SEL_AUDIO);
  // delay to let ADC input settle
  my_delay(10);

  // sampling loop
  for(uint8_t i=0; i<num_samples; i++) {
    // delay to get correct sample rate (roughly)
    while(micros() - last_sample_time < sample_interval);
    
    // sample
    readings[i] = ADC_SCALING_AUDIO * analogRead(A0);
    // update time
    last_sample_time = micros();
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
  last_smeter = rms;
}

// TODO: implement a maximum volume control based on JSON file
// TODO: add min/max valid inputs
void update_volume(uint16_t volume) {
  if(hardware_rev == "max-3b_v1") {
    uint8_t vol_1 = 0;
    uint8_t vol_2 = 1;
    uint8_t vol_3 = 2;
    uint8_t vol_4 = 3;
  
    // TODO: speed this up by only doing a single i2c write.
    switch(volume) {
      case 1:
        pcf8574_21.write(vol_1, LOW);
        pcf8574_21.write(vol_2, LOW);
        pcf8574_21.write(vol_3, LOW);
        pcf8574_21.write(vol_4, LOW);
        break;
      case 2:
        pcf8574_21.write(vol_1, LOW);
        pcf8574_21.write(vol_2, LOW);
        pcf8574_21.write(vol_3, LOW);
        pcf8574_21.write(vol_4, HIGH);
        break;
      case 3:
        pcf8574_21.write(vol_1, LOW);
        pcf8574_21.write(vol_2, LOW);
        pcf8574_21.write(vol_3, HIGH);
        pcf8574_21.write(vol_4, LOW);
        break;
      case 4:
        pcf8574_21.write(vol_1, LOW);
        pcf8574_21.write(vol_2, HIGH);
        pcf8574_21.write(vol_3, LOW);
        pcf8574_21.write(vol_4, LOW);
        break;
      case 5:
        pcf8574_21.write(vol_1, HIGH);
        pcf8574_21.write(vol_2, LOW);
        pcf8574_21.write(vol_3, LOW);
        pcf8574_21.write(vol_4, LOW);
    }
  }
  if(hardware_rev == "max-3b_v2") {
    uint8_t vol_1 = 2;
    uint8_t vol_2 = 5;
    uint8_t vol_3 = 1;
    uint8_t vol_4 = 3;
    uint8_t vol_5 = 4;
  
    // TODO: speed this up by only doing a single i2c write.
    switch(volume) {
      case 1:
        pcf8574_21.write(vol_1, LOW);
        pcf8574_21.write(vol_2, LOW);
        pcf8574_21.write(vol_3, LOW);
        pcf8574_21.write(vol_4, LOW);
        pcf8574_21.write(vol_5, LOW);
        break;
      case 2:
        pcf8574_21.write(vol_1, LOW);
        pcf8574_21.write(vol_2, LOW);
        pcf8574_21.write(vol_3, LOW);
        pcf8574_21.write(vol_4, LOW);
        pcf8574_21.write(vol_5, HIGH);
        break;
      case 3:
        pcf8574_21.write(vol_1, LOW);
        pcf8574_21.write(vol_2, LOW);
        pcf8574_21.write(vol_3, LOW);
        pcf8574_21.write(vol_4, HIGH);
        pcf8574_21.write(vol_5, LOW);
        break;
      case 4:
        pcf8574_21.write(vol_1, LOW);
        pcf8574_21.write(vol_2, LOW);
        pcf8574_21.write(vol_3, HIGH);
        pcf8574_21.write(vol_4, LOW);
        pcf8574_21.write(vol_5, LOW);
        break;
      case 5:
        pcf8574_21.write(vol_1, LOW);
        pcf8574_21.write(vol_2, HIGH);
        pcf8574_21.write(vol_3, LOW);
        pcf8574_21.write(vol_4, LOW);
        pcf8574_21.write(vol_5, LOW);
        break;
      case 6:
        pcf8574_21.write(vol_1, HIGH);
        pcf8574_21.write(vol_2, LOW);
        pcf8574_21.write(vol_3, LOW);
        pcf8574_21.write(vol_4, LOW);
        pcf8574_21.write(vol_5, LOW);
    }

    // set speaker
    if(speaker_state)
      gpio_write(OUTPUT_SPKR_EN, OUTPUT_ON);
    else
      gpio_write(OUTPUT_SPKR_EN, OUTPUT_OFF);
  }
}
