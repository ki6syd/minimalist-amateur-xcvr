void handle_frequency(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) {
  if(request_type == HTTP_PUT) {
    // look for required parameters in the message
    if(!request->hasParam("frequency")) {
      Serial.println("[RADIO] frequency not sent");
      request->send(400, "text/plain", "frequency not sent");
      return;
    }

    uint64_t freq_request = request->getParam("frequency")->value().toFloat();

    // check frequency bounds before doing anything else
    if(!freq_valid(freq_request)) {
      request->send(409, "text/plain", "Radio hardware does not support this frequency.");
      return;
    }

    // set flag, main loop will update clocks
    flag_freq = true;
    f_rf = freq_request;
    // TODO: make sure we don't get negative numbers
    // TODO: consider making this part of frequency setting?
    f_vfo = update_vfo(f_rf, f_bfo, f_audio);
  
    Serial.print("RF: ");
    print_uint64_t(f_rf);
    Serial.print("\tVFO: ");
    print_uint64_t(f_vfo);
    Serial.print("\tBFO: ");
    print_uint64_t(f_bfo);
    Serial.println();

    request->send(201, "text/plain", "OK");
  }
  if(request_type == HTTP_GET) {
    request->send(200, "text/plain", String(f_rf));
  }
}

void set_dial_freq(String freq) {
  uint64_t tmp = freq.toFloat() * 1000000;
  set_dial_freq(tmp);
}

void set_dial_freq(uint64_t freq) {
  Serial.print("[FREQ UPDATE] ");
  Serial.print(freq);
  Serial.print("\t");
  
  // check frequency bounds before doing anything else
  if(!freq_valid(freq))
    return;

  // force relay updates, audio path updates, etc if needed
  f_rf = freq;
  // TODO: make sure we don't get negative numbers
  f_vfo = update_vfo(f_rf, f_bfo, f_audio);

  change_freq();

  Serial.print("RF: ");
  print_uint64_t(f_rf);
  Serial.print("\tVFO: ");
  print_uint64_t(f_vfo);
  Serial.print("\tBFO: ");
  print_uint64_t(f_bfo);
  Serial.println();
}

// todo: support arbitrary num bands
bool freq_valid(uint64_t freq) {
  if(freq > f_rf_min_band1 && freq < f_rf_max_band1)
    return true;
  if(freq > f_rf_min_band2 && freq < f_rf_max_band2)
    return true;
  if(freq > f_rf_min_band3 && freq < f_rf_max_band3)
    return true;

  return false;
}

// updates the relay references
// turns off all relays and then enables the new RX relay
void update_relays(uint64_t f_rf) {
  // turn off the currently selected relays
  gpio_write(bpf_relay, OUTPUT_OFF);
  gpio_write(lpf_relay, OUTPUT_OFF);
  
  // choose the new LPF and BPF based on frequency
  if(f_rf >= f_rf_min_band1 && f_rf <= f_rf_max_band1) {
    lpf_relay = OUTPUT_LPF_1;
    bpf_relay = OUTPUT_BPF_1;
  }
  else if(f_rf >= f_rf_min_band2 && f_rf <= f_rf_max_band2) {
    lpf_relay = OUTPUT_LPF_2;
    bpf_relay = OUTPUT_BPF_2;
  }
  else if(f_rf >= f_rf_min_band3 && f_rf <= f_rf_max_band3) {
    lpf_relay = OUTPUT_LPF_3;
    bpf_relay = OUTPUT_BPF_3;
  }
  else {
    Serial.print("[FILTER] Unable to select relays based on freq: ");
    Serial.println(f_rf);
  }
  
  // set relays to RX configuration
  gpio_write(bpf_relay, OUTPUT_ON);
  gpio_write(lpf_relay, OUTPUT_OFF);
  // make a call to set_mode just in case. it'll return if there is no change
  set_mode(MODE_RX);
}


// handles relay switching and mute
// TODO: figure out what to do with the random delay
void set_mode(qsk_state_type new_mode) {
  
  // for re-setting the mode to TX, reset the QSK counter
  if(new_mode == MODE_TX);
    qsk_counter = qsk_period;

  // no further action needed if there is no change in mode, or if we went from QSK_COUNTDOWN to TX
  if(new_mode == tx_rx_mode || (new_mode == MODE_TX && tx_rx_mode == MODE_QSK_COUNTDOWN))
    return;
  else
    tx_rx_mode = new_mode;

  gpio_write(OUTPUT_GREEN_LED, OUTPUT_OFF);

  if(new_mode == MODE_TX) {
    // do not proceed for out of band frequencies
    if(!freq_valid(f_rf))
      return;
    
    // mute and set volume to lowest setting before using sidetone
    gpio_write(OUTPUT_RX_MUTE, OUTPUT_MUTED);
    
    // TODO: create a sidetone level option in json file
    // TODO: update 
    if(vol + mon_offset < 1)
      update_volume(1);
    else if(vol + mon_offset >= 5)
      update_volume(5);
    else
      update_volume(vol + mon_offset);
      
    // this was originally a hack for v1: CW audio shows distortion with sidetone. use SSB.
    // carried over into v2 as well, sounds more pleasant.
    if(hardware_rev == "max-3b_v1") {
      gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_SSB);
    }
    if(hardware_rev == "max-3b_v2") {
      gpio_write(OUTPUT_BW_SEL, OUTPUT_SEL_SSB);
    }

    my_delay(10);
    
    // change relays over to TX
    gpio_write(lpf_relay, OUTPUT_ON);
    gpio_write(bpf_relay, OUTPUT_OFF);

    // turn off LNA for TX
    gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);

    // delay for settling. TODO - check what happens if this is absent.
    my_delay(50);

    // start the power amp
    si5351.output_enable(SI5351_CLK2, 1);

    Serial.println("[MODE] TX");
  }
  if(new_mode == MODE_RX) {    
    // this delay helps make sure switching depletes all voltage on the drain of the FETs
    my_delay(25);
    si5351.output_enable(SI5351_CLK2, 0);

    // this delay prevents RX chain from starting up before switching has stopped
    my_delay(25);
    
    // change over clocks to RX
    si5351.output_enable(SI5351_CLK0, 1);
    si5351.output_enable(SI5351_CLK1, 1);
    
    // change over relays before letting RX chain drive the audio
    gpio_write(lpf_relay, OUTPUT_OFF);
    gpio_write(bpf_relay, OUTPUT_ON);

    // restore LNA to its previous state
    if(lna_state)
      gpio_write(OUTPUT_LNA_SEL, OUTPUT_ON);
    else
      gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);

    // restore audio bandwidth to its previous value
    gpio_write(OUTPUT_BW_SEL, (output_state) rx_bw);

    // delay for everything to settle
    my_delay(10);

    // unmute to allow audio through
    gpio_write(OUTPUT_RX_MUTE, OUTPUT_UNMUTED);

    // restore audio volume, was at minimum setting for sidetone 
    update_volume(vol);

    Serial.println("[MODE] RX");
  }
  gpio_write(OUTPUT_GREEN_LED, OUTPUT_OFF);
}

// called when frequency flag is set
// only do this if we are in RX mode, to avoid changing relays while transmitting
void change_freq() {  
  if (tx_rx_mode == MODE_RX) {
    flag_freq = false;

    gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
    update_relays(f_rf);
    gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);

    gpio_write(OUTPUT_RED_LED, OUTPUT_ON);

    // update antenna connection
    gpio_write(OUTPUT_ANT_SEL, (output_state) ant);

    // set LNA hardware
    if(lna_state)
      gpio_write(OUTPUT_LNA_SEL, OUTPUT_ON);
    else
      gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);
    
    // TODO: this logic might make more sense elsewhere. Also should consider the concept of USB/LSB, dial freq, and mode rather than this mess of if statements.
    if(rx_bw == OUTPUT_SEL_CW)
      set_clocks(f_bfo, f_vfo, f_rf);
    else {
      if(f_rf < 10e6)
        set_clocks(f_bfo, f_vfo, f_rf - f_audio);
      else
        set_clocks(f_bfo, f_vfo, f_rf + f_audio);
    }
    gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
  }
  else
    Serial.println("[SAFETY] Did not update frequency because radio was transmitting.");

  // update AF filter routing, regardless of whether tx_rx_mode changed
  // TODO - add a flag for this?
  gpio_write(OUTPUT_BW_SEL, (output_state) rx_bw);
}

// TODO: pull out the magic 2ms delay
void key_on() {    
  // do not proceed for out of band frequencies
  if(!freq_valid(f_rf))
    return;
  
  // change over relays and sidetone
  set_mode(MODE_TX);

  si5351.output_enable(SI5351_CLK2, 1);

  // TODO - figure out minimum bound here.
  my_delay(2);
  
  gpio_write(OUTPUT_GREEN_LED, OUTPUT_ON);
  gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_ON);
}

void key_off() {
  gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_OFF); 
  
  set_mode(MODE_QSK_COUNTDOWN);

  gpio_write(OUTPUT_GREEN_LED, OUTPUT_OFF);
  
  // TODO - parametrize this, move into the mode switching logic?
  // this delay must be long enough for switching to fully discharge VBATT-SW
  // otherwise, some voltage remains when key_on() gets called and there is a click.
  my_delay(25);
  
  si5351.output_enable(SI5351_CLK2, 0);
}


void update_qsk_timer() {
  if(tx_rx_mode == MODE_QSK_COUNTDOWN && qsk_counter > 0) {
    qsk_counter -= 10;
    my_delay(10);
  }

  if(qsk_counter <= 0) {
    qsk_counter = 0;
    set_mode(MODE_RX);  
  }
}


// TODO: load calibration factors and freq offsets out of the JSON file
void init_radio() {
  // load frequency limits from flash
  f_rf_min_band1 = load_json_config(preference_file, "f_rf_min_hz_band1").toFloat();
  f_rf_max_band1 = load_json_config(preference_file, "f_rf_max_hz_band1").toFloat();
  f_rf_min_band2 = load_json_config(preference_file, "f_rf_min_hz_band2").toFloat();
  f_rf_max_band2 = load_json_config(preference_file, "f_rf_max_hz_band2").toFloat();
  f_rf_min_band3 = load_json_config(preference_file, "f_rf_min_hz_band3").toFloat();
  f_rf_max_band3 = load_json_config(preference_file, "f_rf_max_hz_band3").toFloat();
  
  // load frequencies from flash
  // TODO: need a better way to update all the frequencies at once. Introduce concept of USB/LSB, RX mode, and dial frequency.
  uint64_t xtal = load_json_config(hardware_file, "xtal_freq_hz").toFloat();
  f_audio = load_json_config(preference_file, "sidetone_pitch_hz").toFloat();
  f_if = load_json_config(hardware_file, "if_freq_hz").toFloat();
  f_bfo = f_if + f_audio;
  // f_bfo = f_if + 2500;

  // load default frequency from JSON, use function in server module
  set_dial_freq(load_json_config(preference_file, "f_rf_default_mhz"));


  // initialize radio hardare
  Wire.begin();
  Serial.print("[SI5351] ");
  Serial.println(si5351.si5351_read(SI5351_DEVICE_STATUS));
  si5351.init(SI5351_CRYSTAL_LOAD_8PF , xtal , 0);
  
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);


  set_clocks(f_bfo, f_vfo, f_rf);

  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA);
  
  si5351.output_enable(SI5351_CLK0, 1);
  // si5351.output_driver(SI5351_CLK0, 1);

  si5351.output_enable(SI5351_CLK1, 1);
  // si5351.output_driver(SI5351_CLK1, 1);

  si5351.output_enable(SI5351_CLK2, 0);
  // si5351.output_driver(SI5351_CLK2, 0);

  set_mode(MODE_RX);
}
