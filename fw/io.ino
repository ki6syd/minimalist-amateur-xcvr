#define ADC_SCALING_VBAT      0.0349
#define ADC_SCALING_AUDIO     0.00976

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
  // set sidetone pin low
  //analogWrite(D4, 0);

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
        digitalWrite(2, LOW);
      if(state == OUTPUT_OFF)
        digitalWrite(2, HIGH);
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
        pcf8574_relays.write(0, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574_relays.write(0, LOW);
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

  return analogRead(A0);

  // TODO: delete above lines after debugging
  
  int16_t adc_counts;
  float reading;
  
  switch(pin) {
    case INPUT_VBAT:
      // change to the correct mux setting
      gpio_write(OUTPUT_ADC_SEL, OUTPUT_SEL_VBAT);
      // settling time
      my_delay(5);
      adc_counts = analogRead(A0);
      reading = adc_counts * ADC_SCALING_VBAT;
      break;

    case INPUT_AUDIO:
      gpio_write(OUTPUT_ADC_SEL, OUTPUT_SEL_AUDIO);
      // settling time
      my_delay(5);
      adc_counts = analogRead(A0);
      reading = adc_counts * ADC_SCALING_AUDIO;
      break;
  }

  return reading;
}

// TODO: implement a maximum volume control based on JSON file
// TODO: add min/max valid inputs
void update_volume(uint16_t volume) {
  uint8_t vol_1 = 0;
  uint8_t vol_2 = 1;
  uint8_t vol_3 = 2;
  uint8_t vol_4 = 3;

  Serial.print("[VOLUME] ");
  Serial.println(volume);

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

void special_mode(uint16_t special_mode) {
  switch(special) {
      case 1:
        si5351.output_enable(SI5351_CLK2, 1);
        break;
      case 2:
        si5351.output_enable(SI5351_CLK2, 0);
        break;
      case 3:
        gpio_write(OUTPUT_LPF_1, OUTPUT_ON);
        gpio_write(OUTPUT_LPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_3, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_3, OUTPUT_OFF);
        break;
      case 4:
        gpio_write(OUTPUT_LPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_3, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_1, OUTPUT_ON);
        gpio_write(OUTPUT_BPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_3, OUTPUT_OFF);
        break;
      case 5:
        gpio_write(OUTPUT_LPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_2, OUTPUT_ON);
        gpio_write(OUTPUT_LPF_3, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_1, OUTPUT_ON);
        gpio_write(OUTPUT_BPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_3, OUTPUT_OFF);
        break;
      case 6:
        gpio_write(OUTPUT_LPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_3, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_2, OUTPUT_ON);
        gpio_write(OUTPUT_BPF_3, OUTPUT_OFF);
        break;
      case 7:
        gpio_write(OUTPUT_LPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_3, OUTPUT_ON);
        gpio_write(OUTPUT_BPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_3, OUTPUT_OFF);
        break;
      case 8:
        gpio_write(OUTPUT_LPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_3, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_3, OUTPUT_ON);
        break;
       case 9:
        gpio_write(OUTPUT_LPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_3, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_3, OUTPUT_OFF);
        break;
      case 10:
        gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_ON);
        break;
      case 11:
        gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_OFF);
        break;
      case 12:
        gpio_write(OUTPUT_RX_MUTE, OUTPUT_UNMUTED);
        break;
      case 13:
        gpio_write(OUTPUT_RX_MUTE, OUTPUT_MUTED);
        break;
      case 14:
        gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_ON);
        my_delay(100);
        gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_OFF);
        break;
      case 15:
        key_on();
        break;
      case 16:
        key_off();
        break;
      case 17:
        gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_OFF);
        my_delay(1000);
        gpio_write(OUTPUT_BPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_2, OUTPUT_OFF);
        my_delay(1000);
        si5351.output_enable(SI5351_CLK2, 1);
        gpio_write(OUTPUT_LPF_1, OUTPUT_ON);
        gpio_write(OUTPUT_LPF_2, OUTPUT_ON);
        my_delay(5000);
        si5351.output_enable(SI5351_CLK2, 0);
        break;
      case 18: {
        // xtal sweep: hold rf and audio frequencies constant, but adjust the two LO frequencies

        // assume we're already looking at the correct frequency and BPF

        gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_CW);
        
        gpio_write(OUTPUT_RED_LED, OUTPUT_ON);

        si5351.output_enable(SI5351_CLK2, 1);

        Serial.print("VFO: ");
        print_uint64_t(f_vfo);
        Serial.print("BFO: ");
        print_uint64_t(f_bfo);
        Serial.println();

        uint64_t f_if_orig = f_if;
        uint64_t f_if_min = f_if-10000;
        uint64_t f_if_max = f_if+10000;

        for(f_if=f_if_min; f_if < f_if_max; f_if += 500) {
          f_bfo = f_if + f_audio;
          f_vfo = update_vfo(f_rf, f_bfo, f_audio);
          set_clocks(f_bfo, f_vfo, f_rf);

          Serial.print("VFO: ");
          print_uint64_t(f_vfo);
          Serial.print("BFO: ");
          print_uint64_t(f_bfo);
          Serial.println();
          
          my_delay(100);
        }
        // return if to original value
        f_if = f_if_orig;
        f_bfo = f_if + f_audio;
        f_vfo = update_vfo(f_rf, f_bfo, f_audio);
        set_clocks(f_bfo, f_vfo, f_rf);
        si5351.output_enable(SI5351_CLK2, 0);
        gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
        Serial.println("Done sweeping crystal");
        break;
      }
      case 19: {
        // BPF sweep: hold if and audio frequencies constant, but adjust the first LO and RF frequency
        
        // assume we're already looking at the correct frequency and BPF
        gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_CW);

        gpio_write(OUTPUT_RED_LED, OUTPUT_ON);

        // turn on CLK2
        si5351.output_enable(SI5351_CLK2, 1);

        uint64_t f_rf_orig = f_rf;
        uint64_t f_rf_min = f_rf-2000000;
        uint64_t f_rf_max = f_rf+2000000;


        for(f_rf=f_rf_min; f_rf < f_rf_max; f_rf += 100000) {
          f_vfo = update_vfo(f_rf, f_bfo, f_audio);
          set_clocks(f_bfo, f_vfo, f_rf);          

          Serial.print("VFO: ");
          print_uint64_t(f_vfo);
          Serial.print("BFO: ");
          print_uint64_t(f_bfo);
          Serial.println();

          my_delay(100);
        }
        
        // return if to original value
        f_rf = f_rf_orig;
        f_vfo = update_vfo(f_rf, f_bfo, f_audio);
        set_clocks(f_bfo, f_vfo, f_rf);
        si5351.output_enable(SI5351_CLK2, 0);
        gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
        Serial.println("Done sweeping BPF");
        break;
      }
      case 20: {
        // af sweep: hold rf and if frequencies constant, but adjust BFO to sweep AF

        // assume we're already looking at the correct frequency and BPF
        gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_CW);

        gpio_write(OUTPUT_RED_LED, OUTPUT_ON);

        si5351.output_enable(SI5351_CLK2, 1);

        uint64_t f_bfo_orig = f_bfo;
        uint64_t f_bfo_min = f_bfo-f_audio-2000;
        uint64_t f_bfo_max = f_bfo+2000;

        for(f_bfo=f_bfo_min; f_bfo < f_bfo_max; f_bfo += 100) {
          set_clocks(f_bfo, f_vfo, f_rf);

          Serial.print("VFO: ");
          print_uint64_t(f_vfo);
          Serial.print("BFO: ");
          print_uint64_t(f_bfo);
          Serial.println();
          
          my_delay(100);
        }
        // return if to original value
        f_bfo = f_bfo_orig;
        f_vfo = update_vfo(f_rf, f_bfo, f_audio);
        set_clocks(f_bfo, f_vfo, f_rf);
        si5351.output_enable(SI5351_CLK2, 0);
        Serial.println("Done sweeping AF");
        gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
        break;
      }
      case 21: {
        reboot();
        break;
      }
      case 22: {
        gpio_write(OUTPUT_LNA_SEL, OUTPUT_ON);
        break;
      }
      case 23: {
        gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);
        break;
      }
      case 24: {
        gpio_write(OUTPUT_ANT_SEL, OUTPUT_ANT_DIRECT);
        break;
      }
      case 25: {
        gpio_write(OUTPUT_ANT_SEL, OUTPUT_ANT_XFMR);
        break;
      }
    }
}
