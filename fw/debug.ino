void handle_debug(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) {
  if(request_type == HTTP_POST) {
    // look for required parameters in the message
    if(!request->hasParam("command")) {
      Serial.println("[DEBUG] command not sent");
      request->send(400, "text/plain", "command not sent");
      return;
    }

    special = request->getParam("command")->value().toInt();

    flag_special = true;
    request->send(201, "text/plain", "OK");
  }
  else if(request_type == HTTP_GET) {
    request->send(200, "text/plain", String (0));
  }
}

// TODO - break out basic GPIO controls as separtate from cal routines
// TODO - add some more controls so TX into BPF isn't possible
// TODO - send curves to web browser
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
        uint64_t f_if_min = f_if-10000;
        uint64_t f_if_max = f_if+10000;
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
        // https://kholia.github.io/ft8_encoder.html
        // S KI6SYD/6QUG
        // uint8_t fixed_buffer[] = {3,1,4,0,6,5,2,3,7,6,5,3,4,7,2,7,4,7,1,0,5,3,5,5,7,1,4,0,5,3,4,0,1,1,4,2,3,1,4,0,6,5,2,6,2,0,0,2,7,0,2,1,3,4,6,0,1,5,3,6,0,0,7,6,3,2,5,7,5,7,5,1,3,1,4,0,6,5,2};

        // testing
        uint8_t fixed_buffer[] = {3,1,4,0,6,5,2,3,7,6,5,3,4,7,2,7,4,7,1,0,5,3,5,4,5,0,1,6,6,4,4,0,0,5,3,6,3,1,4,0,6,5,2,0,4,2,2,0,7,4,6,1,0,7,2,0,5,3,7,1,3,5,2,0,1,6,5,7,0,7,0,4,3,1,4,0,6,5,2};

        // CQ
        // uint8_t fixed_buffer[] = {3,1,4,0,6,5,2,0,0,0,0,0,0,0,0,1,1,1,2,6,5,3,1,0,4,0,0,3,2,0,6,4,2,4,6,0,3,1,4,0,6,5,2,4,6,6,0,5,5,1,0,6,4,6,0,4,7,1,5,5,2,7,0,1,5,1,3,4,1,6,1,7,3,1,4,0,6,5,2};

        for(uint64_t f_tx = 14074000; f_tx < 14076001; f_tx += 250) {
          key_on();
          
          for(uint8_t i = 0; i < 79; i++)
          {
            f_rf = f_tx + ((uint64_t) (fixed_buffer[i] * 6.25));
            set_clocks(f_bfo, f_vfo, f_rf);
  
            my_delay(145);
          }
  
          key_off();

          my_delay(2350);
        }
        
        break;
      }

      case 29: {
        // routine for unsticking relays

        output_pin filter_relays[] = {OUTPUT_LPF_1, OUTPUT_LPF_2, OUTPUT_LPF_3, OUTPUT_BPF_1, OUTPUT_BPF_2, OUTPUT_BPF_3};
        
        for(uint16_t i=0; i < 5; i++) {
          for(uint8_t j=0; j < 6; j++) {
            gpio_write(OUTPUT_LPF_1, OUTPUT_OFF);
            gpio_write(OUTPUT_LPF_2, OUTPUT_OFF);
            gpio_write(OUTPUT_LPF_3, OUTPUT_OFF);
            gpio_write(OUTPUT_BPF_1, OUTPUT_OFF);
            gpio_write(OUTPUT_BPF_2, OUTPUT_OFF);
            gpio_write(OUTPUT_BPF_3, OUTPUT_OFF);  
            my_delay(5);

            gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
            for(uint8_t k=0; k < 100; k++) {
              gpio_write(filter_relays[j], OUTPUT_ON);
              my_delay(5);
              gpio_write(filter_relays[j], OUTPUT_OFF);
              my_delay(5);
            }
            gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
          }
          my_delay(250);
        }

        break;
      }
    }
}
