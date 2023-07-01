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
        gpio_write(OUTPUT_BPF_1, OUTPUT_OFF);
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
        gpio_write(OUTPUT_BPF_3, OUTPUT_OFF);
        my_delay(1000);
        si5351.output_enable(SI5351_CLK2, 1);
        gpio_write(OUTPUT_LPF_1, OUTPUT_ON);
        gpio_write(OUTPUT_LPF_2, OUTPUT_ON);
        gpio_write(OUTPUT_LPF_3, OUTPUT_ON);
        my_delay(5000);
        si5351.output_enable(SI5351_CLK2, 0);
        break;
      case 18: {
        // xtal sweep: hold rf and audio frequencies constant, but adjust the two LO frequencies

        gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_SSB);
        gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);
        gpio_write(OUTPUT_RED_LED, OUTPUT_ON);

        si5351.output_enable(SI5351_CLK2, 1);

        Serial.print("VFO: ");
        print_uint64_t(f_vfo);
        Serial.print("BFO: ");
        print_uint64_t(f_bfo);
        Serial.println();

        uint64_t f_if_orig = f_if;
        uint64_t f_if_min = f_if-5000;
        uint64_t f_if_max = f_if+3000;
        uint16_t step_size = 200;
        uint16_t num_points = (f_if_max-f_if_min)/step_size;

        float meter_readings[num_points];
        uint16_t i = 0;

        for(f_if=f_if_min; f_if < f_if_max; f_if += step_size) {
          f_bfo = f_if + f_audio;
          f_vfo = update_vfo(f_rf, f_bfo, f_audio);
          set_clocks(f_bfo, f_vfo, f_rf);

          Serial.print("VFO: ");
          print_uint64_t(f_vfo);
          Serial.print("BFO: ");
          print_uint64_t(f_bfo);
          Serial.println();

          update_smeter();
          meter_readings[i] = last_smeter;
          i++;
          
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


        // TODO - clean this up
        for(i=0; i < num_points; i++) {
          Serial.print("F_if (rel)=");
          Serial.print(((int64_t) (f_if_min+i*step_size)) - (int64_t) f_if_orig);
          
          // this next statement is a hack to help with readability in printout
          if(((int64_t) (f_if_min+i*step_size)) - (int64_t) f_if_orig > -1000)
            Serial.print("\t");
            
          Serial.print("\t");
          Serial.print(meter_readings[i]);
          Serial.print("\t");
          for(uint16_t j=0; j < meter_readings[i]/0.005; j++)
            Serial.print("-");
          Serial.println();
        }
        
        break;
      }
      case 19: {
        // BPF sweep: hold if and audio frequencies constant, but adjust the first LO and RF frequency
        
        gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_SSB);
        gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
        gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);

        // turn on CLK2
        si5351.output_enable(SI5351_CLK2, 1);

        uint64_t f_rf_orig = f_rf;
        uint64_t f_rf_min = f_rf-1500000;
        uint64_t f_rf_max = f_rf+1500000;

        uint16_t step_size = 100000;
        uint16_t num_points = (f_rf_max-f_rf_min)/step_size;

        float meter_readings[num_points];
        uint16_t i = 0;


        for(f_rf=f_rf_min; f_rf < f_rf_max; f_rf += step_size) {
          f_vfo = update_vfo(f_rf, f_bfo, f_audio);
          set_clocks(f_bfo, f_vfo, f_rf);          

          Serial.print("VFO: ");
          print_uint64_t(f_vfo);
          Serial.print("BFO: ");
          print_uint64_t(f_bfo);
          Serial.println();

          update_smeter();
          meter_readings[i] = last_smeter;
          i++;

          my_delay(50);
        }
        
        // return if to original value
        f_rf = f_rf_orig;
        f_vfo = update_vfo(f_rf, f_bfo, f_audio);
        set_clocks(f_bfo, f_vfo, f_rf);
        si5351.output_enable(SI5351_CLK2, 0);
        gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
        Serial.println("Done sweeping BPF");

        // TODO - clean this up
        for(i=0; i < num_points; i++) {
          Serial.print("F_rf (kHz)=");
          Serial.print((f_rf_min+i*step_size)/1000);

          // this next statement is a hack to help with readability in printout
          if((f_rf_min+i*step_size)/1000 < 10000)
            Serial.print("\t");
          
          Serial.print("\t");
          Serial.print(meter_readings[i]);
          Serial.print("\t");
          for(uint16_t j=0; j < meter_readings[i]/0.005; j++)
            Serial.print("-");
          Serial.println();
        }

        
        break;
      }
      case 20: {
        // af sweep: hold rf and if frequencies constant, but adjust BFO to sweep AF
        // NOTE: this isn't working with CW filter selected, due to distortion.

        gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);
        gpio_write(OUTPUT_RED_LED, OUTPUT_ON);

        si5351.output_enable(SI5351_CLK2, 1);

        uint64_t f_bfo_orig = f_bfo;
        uint64_t f_bfo_min = f_bfo-f_audio-2000;
        uint64_t f_bfo_max = f_bfo+2000;

        uint16_t step_size = 100;
        uint16_t num_points = (f_bfo_max-f_bfo_min)/step_size;

        float meter_readings[num_points];
        uint16_t i = 0;


        for(f_bfo=f_bfo_min; f_bfo < f_bfo_max; f_bfo += step_size) {
          set_clocks(f_bfo, f_vfo, f_rf);

          Serial.print("VFO: ");
          print_uint64_t(f_vfo);
          Serial.print("BFO: ");
          print_uint64_t(f_bfo);
          Serial.println();

          update_smeter();
          meter_readings[i] = last_smeter;
          i++;
          
          my_delay(100);
        }
        // return if to original value
        f_bfo = f_bfo_orig;
        f_vfo = update_vfo(f_rf, f_bfo, f_audio);
        set_clocks(f_bfo, f_vfo, f_rf);
        si5351.output_enable(SI5351_CLK2, 0);
        Serial.println("Done sweeping AF");
        gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);

        // TODO - clean this up
        for(i=0; i < num_points; i++) {
          Serial.print("F (rel) =");
          Serial.print(((int64_t) f_bfo_min+i*step_size) - (int64_t) f_bfo_orig);
          Serial.print("\t");
          Serial.print(meter_readings[i]);
          Serial.print("\t");
          for(uint16_t j=0; j < meter_readings[i]/0.005; j++)
            Serial.print("-");
          Serial.println();
        }

        break;
      }
      case 21: {
        // sideband suppression test
        // inject +audio and -audio freqs, just so this can ignore upper vs lower sideband modes

        gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);
        gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_SSB);
        gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
        si5351.output_enable(SI5351_CLK2, 1);
        
        uint64_t f_rf_nom = f_rf;
        uint64_t f_rf_sideband_1 = f_rf - 2*f_audio;
        uint64_t f_rf_sideband_2 = f_rf + 2*f_audio;

        uint64_t f_rf_list[] = {f_rf_nom, f_rf_sideband_1, f_rf_sideband_2};

        float meter_readings[3];

        for(uint8_t i=0; i<1; i++) {
          for(uint8_t j=0; j<3; j++) {
            f_rf = f_rf_list[j];
            set_clocks(f_bfo, f_vfo, f_rf);
          
            Serial.print("RF: ");
            print_uint64_t(f_rf);
            Serial.println();

            update_smeter();
            meter_readings[j] = last_smeter;
            
            delay(1000);
          }
        }

        // return to original value
        f_rf = f_rf_nom;
        set_clocks(f_bfo, f_vfo, f_rf);
        si5351.output_enable(SI5351_CLK2, 0);
        Serial.println("Done sweeping AF");
        gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);


        // TODO - clean this up
        for(uint8_t i=0; i < 3; i++) {
          Serial.print("F=");
          Serial.print(f_rf_list[i]);
          Serial.print("\t");
          Serial.print(meter_readings[i]);
          Serial.print("\t");
          for(uint16_t j=0; j < meter_readings[i]/0.005; j++)
            Serial.print("-");
          Serial.println();
        }
        
        break;
      }
      case 22: {
        reboot();
        break;
      }
      case 23: {
        gpio_write(OUTPUT_LNA_SEL, OUTPUT_ON);
        break;
      }
      case 24: {
        gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);
        break;
      }
      case 25: {
        gpio_write(OUTPUT_ANT_SEL, OUTPUT_ANT_DIRECT);
        break;
      }
      case 26: {
        gpio_write(OUTPUT_ANT_SEL, OUTPUT_ANT_XFMR);
        break;
      }

      case 27: {
        gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_SSB);
        gpio_write(OUTPUT_LNA_SEL, OUTPUT_ON);
        gpio_write(OUTPUT_RED_LED, OUTPUT_ON);

        uint64_t f_rf_orig = f_rf;
        uint64_t f_rf_min = f_rf - 25000;
        uint64_t f_rf_max = f_rf + 25000;
        uint16_t step_size = 2500;
        uint16_t num_points = (f_rf_max-f_rf_min)/step_size;

        float meter_readings[num_points];
        uint16_t i = 0;

        for(f_rf=f_rf_min; f_rf < f_rf_max; f_rf += step_size) {
          f_vfo = update_vfo(f_rf, f_bfo, f_audio);
          set_clocks(f_bfo, f_vfo, f_rf);

          Serial.print("VFO: ");
          print_uint64_t(f_vfo);
          Serial.print("BFO: ");
          print_uint64_t(f_bfo);
          Serial.println();

          my_delay(50);

          update_smeter();
          meter_readings[i] = last_smeter;
          i++;
        }
        // return if to original value
        f_rf = f_rf_orig;
        f_vfo = update_vfo(f_rf, f_bfo, f_audio);
        set_clocks(f_bfo, f_vfo, f_rf);
        
        gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
        Serial.println("Done measuring noise level");


        // TODO - clean this up
        for(i=0; i < num_points; i++) {
          Serial.print("F_rf (rel)=");
          Serial.print(((int64_t) (f_rf_min+i*step_size)) - (int64_t) f_rf_orig);
            
          Serial.print("\t");
          Serial.print(meter_readings[i]);
          Serial.print("\t");
          for(uint16_t j=0; j < meter_readings[i]/0.001; j++)
            Serial.print("-");
          Serial.println();
        }
        
        break;
      }

      case 28: {
        uint64_t f_rf_orig = f_rf;

        // from: https://github.com/kholia/Easy-FT8-Beacon-v3/blob/master/Easy-FT8-Beacon-v3/Easy-FT8-Beacon-v3.ino
        uint8_t fixed_buffer[] = {3, 1, 4, 0, 6, 5, 2, 7, 0, 7, 4, 2, 2, 2, 2, 5, 6, 4, 1, 7, 5, 7, 2, 6, 0, 6, 1, 2, 0, 5, 2, 0, 3, 6, 2, 1, 3, 1, 4, 0, 6, 5, 2, 3, 1, 7, 6, 4, 3, 2, 5, 6, 1, 5, 7, 1, 5, 4, 2, 2, 5, 6, 3, 2, 5, 7, 5, 0, 7, 7, 7, 3, 3, 1, 4, 0, 6, 5, 2};

        gpio_write(OUTPUT_RED_LED, OUTPUT_ON);

        si5351.output_enable(SI5351_CLK2, 1);
        
        for(uint8_t i = 0; i < 79; i++)
        {
          f_rf = f_rf_orig + ((uint64_t) (fixed_buffer[i] * 6.25)) + 1700 - f_audio;
          set_clocks(f_bfo, f_vfo, f_rf);

          my_delay(145);
        }

        si5351.output_enable(SI5351_CLK2, 0);

        gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
        
        break;
      }
    }
}
