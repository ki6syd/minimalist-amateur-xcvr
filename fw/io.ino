#define ADC_SCALING_VBAT      0.0349
#define ADC_SCALING_AUDIO     0.00976

// TODO: get rid of hard-coded pin numbering below

void init_gpio() {
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(0, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(15, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(16, OUTPUT);

  // set up the relay driver IO expander
  if(!pcf8574.begin())
    Serial.println("[PCF8574] Couldn't initialize");
  else {
    for(int i=0; i < 8; i++) {
      pcf8574.write(i, LOW);
    }
    Serial.print("[PCF8574] Connected?: ");
    Serial.println(pcf8574.isConnected());
    Serial.println("[PCF8574] Initialized and set OUTPUT_OFF");
  }

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

void sidetone_on() {
  //analogWrite(D4, 127);
}

void sidetone_off() {
  //analogWrite(D4, 0);
}


// attaches/deattaches interrupt on dit/dah pins
void attach_paddle_isr(bool dit_en, bool dah_en) {
  
  if(dit_en)
    attachInterrupt(digitalPinToInterrupt(13), paddle_isr, ONLOW);
  if(!dit_en)
    detachInterrupt(digitalPinToInterrupt(13));

  if(dah_en)
    attachInterrupt(digitalPinToInterrupt(12), paddle_isr, ONLOW);
  if(!dah_en)
    detachInterrupt(digitalPinToInterrupt(12));
    
}

// when we detect dit/dah paddle:
//  - turn off the interrupt to prevent future events
//  - set a flag
ICACHE_RAM_ATTR void paddle_isr() {
  /*
  if(!digitalRead(D7)) {
    attach_paddle_isr(false, true);
    dit_flag = true;
  }
  //if(!digitalRead(D6)) {
  if(!digitalRead(12)) {
    attach_paddle_isr(true, false);
    dah_flag = true;
  }
  */

  // empty the keyer queue so touching the paddle stops any ongoing messages
  tx_queue = "";
}


// TODO: accept a gpio number that doesn't necessarily map to ESP12. This would allow an IO expander to work with this function
void gpio_write(output_pin pin, output_state state) {
  switch(pin) {
    case OUTPUT_RX_MUTE:
      if(state == OUTPUT_AUDIO_RX)
        digitalWrite(15, LOW);
      if(state == OUTPUT_AUDIO_SIDETONE)
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
        pcf8574.write(1, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574.write(1, LOW);
      break;

    case OUTPUT_BPF_1:
      if(state == OUTPUT_ON)
        pcf8574.write(3, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574.write(3, LOW);
      break;

    case OUTPUT_LPF_2:
      if(state == OUTPUT_ON)
        pcf8574.write(0, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574.write(0, LOW);
      break;

    case OUTPUT_BPF_2:
      if(state == OUTPUT_ON)
        pcf8574.write(2, HIGH);
      if(state == OUTPUT_OFF)
        pcf8574.write(2, LOW);
      break;

    case OUTPUT_BW_SEL:
      if(state == OUTPUT_SEL_SSB) {
        pcf8574.write(7, HIGH);
      }
      if(state == OUTPUT_SEL_CW) {
        pcf8574.write(7, LOW);
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
void update_volume(uint16_t volume) {
  uint8_t vol_1 = 4;
  uint8_t vol_2 = 5;
  uint8_t vol_3 = 6;

  Serial.print("[VOLUME] ");
  Serial.println(volume);

  switch(volume) {
    case 1:
      pcf8574.write(vol_1, LOW);
      pcf8574.write(vol_2, LOW);
      pcf8574.write(vol_3, LOW);
      break;
    case 2:
      pcf8574.write(vol_1, LOW);
      pcf8574.write(vol_2, LOW);
      pcf8574.write(vol_3, HIGH);
      break;
    case 3:
      pcf8574.write(vol_1, LOW);
      pcf8574.write(vol_2, HIGH);
      pcf8574.write(vol_3, LOW);
      break;
    case 4:
      pcf8574.write(vol_1, HIGH);
      pcf8574.write(vol_2, LOW);
      pcf8574.write(vol_3, LOW);
      break;
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
        gpio_write(OUTPUT_BPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_2, OUTPUT_OFF);
        break;
      case 4:
        gpio_write(OUTPUT_LPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_1, OUTPUT_ON);
        gpio_write(OUTPUT_BPF_2, OUTPUT_OFF);
        break;
      case 5:
        gpio_write(OUTPUT_LPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_2, OUTPUT_ON);
        gpio_write(OUTPUT_BPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_2, OUTPUT_OFF);
        break;
      case 6:
        gpio_write(OUTPUT_LPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_2, OUTPUT_ON);
        break;
      case 7:
        gpio_write(OUTPUT_LPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_LPF_2, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_1, OUTPUT_OFF);
        gpio_write(OUTPUT_BPF_2, OUTPUT_OFF);
        break;
      case 8:
        gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_ON);
        break;
      case 9:
        gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_OFF);
        break;
      case 10:
        sidetone_on();
        break;
      case 11:
        sidetone_off();
        break;
      case 12:
        gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_ON);
        my_delay(100);
        gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_OFF);
        break;
      case 13:
        key_on();
        break;
      case 14:
        key_off();
        break;
      case 15:
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
      case 16: {
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
        uint64_t f_if_min = f_if-7500;
        uint64_t f_if_max = f_if+7500;

        for(f_if=f_if_min; f_if < f_if_max; f_if += 50) {
          f_bfo = f_if + f_audio;
          f_vfo = update_vfo(f_rf, f_bfo, f_audio);
          set_clocks(f_bfo, f_vfo, f_rf);

          Serial.print("VFO: ");
          print_uint64_t(f_vfo);
          Serial.print("BFO: ");
          print_uint64_t(f_bfo);
          Serial.println();
          
          my_delay(50);
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
      case 17: {
        // BPF sweep: hold if and audio frequencies constant, but adjust the first LO and RF frequency
        
        // assume we're already looking at the correct frequency and BPF
        gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_CW);

        gpio_write(OUTPUT_RED_LED, OUTPUT_ON);

        // turn on CLK2
        si5351.output_enable(SI5351_CLK2, 1);

        uint64_t f_rf_orig = f_rf;
        uint64_t f_rf_min = f_rf-2000000;
        uint64_t f_rf_max = f_rf+2000000;


        for(f_rf=f_rf_min; f_rf < f_rf_max; f_rf += 25000) {
          f_vfo = update_vfo(f_rf, f_bfo, f_audio);
          set_clocks(f_bfo, f_vfo, f_rf);          

          Serial.print("VFO: ");
          print_uint64_t(f_vfo);
          Serial.print("BFO: ");
          print_uint64_t(f_bfo);
          Serial.println();

          my_delay(50);
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
      case 18: {
        // af sweep: hold rf and if frequencies constant, but adjust BFO to sweep AF

        // assume we're already looking at the correct frequency and BPF
        gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_CW);

        gpio_write(OUTPUT_RED_LED, OUTPUT_ON);

        si5351.output_enable(SI5351_CLK2, 1);

        uint64_t f_bfo_orig = f_bfo;
        uint64_t f_bfo_min = f_bfo-f_audio;
        uint64_t f_bfo_max = f_bfo+5000;

        for(f_bfo=f_bfo_min; f_bfo < f_bfo_max; f_bfo += 50) {
          set_clocks(f_bfo, f_vfo, f_rf);

          Serial.print("VFO: ");
          print_uint64_t(f_vfo);
          Serial.print("BFO: ");
          print_uint64_t(f_bfo);
          Serial.println();
          
          my_delay(50);
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
      case 19: {
        reboot();
      }
    }
}
