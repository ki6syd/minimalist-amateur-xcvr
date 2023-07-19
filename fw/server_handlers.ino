

void handle_set_freq(String freq) {
  Serial.print("[FREQ UPDATE] ");
  Serial.print(freq);
  Serial.print("\t");
  
  uint64_t tmp = freq.toFloat() * 1000000;

  // check frequency bounds before doing anything else
  if((tmp < f_rf_min_band1 && tmp > f_rf_max_band1) && (tmp < f_rf_min_band2 && tmp > f_rf_max_band2) && (tmp < f_rf_min_band3 && tmp > f_rf_max_band3))
    return;

  flag_freq = true;
  f_rf = tmp;
  // TODO: make sure we don't get negative numbers
  f_vfo = update_vfo(f_rf, f_bfo, f_audio);

  Serial.print("RF: ");
  print_uint64_t(f_rf);
  Serial.print("\tVFO: ");
  print_uint64_t(f_vfo);
  Serial.print("\tBFO: ");
  print_uint64_t(f_bfo);
  Serial.println();
}

String handle_get_freq() {
  return String(f_rf);
}

String handle_get_smeter() {
  // commented out because this one is a little noisy
  // Serial.println("[S-METER UPDATE]");
  // TODO - remove this magic number
  return String(last_smeter * 30);
}

String handle_get_vbat() {
  // commented out because this one is a little noisy
  // Serial.println("[BATTERY VOLTAGE UPDATE]");
  return String(last_vbat);
}

String handle_debug() {
  return String("0.0");
}


void handle_incr_volume() {
  if(vol < vol_max) {
    flag_vol = true;
    vol++;

    Serial.print("[VOLUME UPDATE] ");
    Serial.println(vol);
  }
}

void handle_decr_volume() {
  if(vol > vol_min) {
    flag_vol = true;
    vol--;

    Serial.print("[VOLUME UPDATE] ");
    Serial.println(vol);
  }
}

String handle_get_volume() {
  return String(vol);
}

void handle_enqueue(String new_text) {
  Serial.print("[ENQUEUE] ");
  Serial.print(new_text);
  Serial.println("<end>");

  // add to queue if there is no "*" (used to clear queue from UI
  if(new_text.indexOf("*") < 0)
    tx_queue += new_text;
  else
    tx_queue = "";
}

void handle_decr_speed() {
  if(keyer_speed > keyer_min) {
    keyer_speed--;
  
    Serial.print("[KEYER SPEED UPDATE] ");
    Serial.println(keyer_speed);
  }
}

void handle_incr_speed() {
  if(keyer_speed < keyer_max) {
    keyer_speed++;
  
    Serial.print("[KEYER SPEED UPDATE] ");
    Serial.println(keyer_speed);
  }
}

String handle_get_speed() {
  return String(keyer_speed);
}

void handle_press_bw() {
  Serial.print("[BW CHANGE] ");
  Serial.println(rx_bw);

  flag_freq = true;

  if(rx_bw == OUTPUT_SEL_SSB)
    rx_bw = OUTPUT_SEL_CW;
  else
    rx_bw = OUTPUT_SEL_SSB;

  // TODO: adjust the ~700hz audio offset we put in for sidetone in CW mode, but won't want in SSB
}

String handle_get_bw() {
  if(rx_bw == OUTPUT_SEL_CW)
    return String("CW");
  else
    return String("SSB");
}


void handle_press_ant() {
  Serial.print("[ANT CHANGE] ");
  Serial.println(ant);

  flag_freq = true;

  // TODO: create a flag for antenna changes, don't want to change at unsafe time.
  if(ant == OUTPUT_ANT_DIRECT) {
    ant = OUTPUT_ANT_XFMR;
  }
  else {
    ant = OUTPUT_ANT_DIRECT;
  }
  gpio_write(OUTPUT_ANT_SEL, (output_state) ant);
}

String handle_get_ant() {
  if(ant == OUTPUT_ANT_DIRECT)
    return String("50 Ω");
  else
    return String("EFHW");
}

void handle_press_lna() {
  Serial.print("[LNA CHANGE] ");

  if(lna_state) {
    lna_state = false;
    gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);
  }
  else {
    gpio_write(OUTPUT_LNA_SEL, OUTPUT_ON);
    lna_state = true;
  }

  Serial.println(lna_state);
      
  // force update
  set_mode(MODE_RX);
  flag_freq = true;

}

String handle_get_lna() {
  if(lna_state)
    return String("ON");
  else
    return String("OFF");
}

String handle_get_queue_len() {
  return String(tx_queue.length());
}

void handle_press_mon() {
  Serial.print("[MON CHANGE] ");

  mon_offset++;

  if(mon_offset > 2)
    mon_offset = -2;

  Serial.println(mon_offset);
}

String handle_get_mon() {
  if(mon_offset < 0)
    return String(mon_offset);
  else
    return String("+") + String(mon_offset);
}

void handle_special(String special_setting) {
  Serial.print("[SPECIAL] ");
  Serial.println(special_setting);

  flag_special = true;
  special = special_setting.toInt();
}
