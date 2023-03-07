
// updates the vfo frequency based on f_rf, f_bfo, and f_cw
// TODO: handle high side or low side injection on both VFO and BFO
// TODO: make sure we don't generate negative frequencies
uint64_t update_vfo(uint64_t f_rf, uint64_t f_bfo, uint64_t f_audio) {
  if(f_rf > f_if)
    f_vfo = f_rf + f_if;
  else
    f_vfo = f_if - f_rf;
    
  return f_vfo;
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
  else {
    Serial.print("[FILTER] Unable to select relays based on freq: ");
    // Serial.println(f_rf);
  }
  
  // set relays to RX configuration
  gpio_write(bpf_relay, OUTPUT_ON);
  gpio_write(lpf_relay, OUTPUT_OFF);
  // make a call to set_mode just in case. it'll return if there is no change
  set_mode(MODE_RX);
}

// update SI5351 clocks
void set_clocks(uint64_t clk_0, uint64_t clk_1, uint64_t clk_2) {    
  si5351.set_freq(clk_0 * 100, SI5351_CLK0);
  si5351.set_freq(clk_1 * 100, SI5351_CLK1);
  si5351.set_freq(clk_2 * 100, SI5351_CLK2);
}

// handles relay switching and mute
// TODO: figure out what to do with the random delay
void set_mode(mode_type new_mode) {
  
  // for re-setting the mode to TX, reset the QSK counter
  // TODO: use a real number here.
  if(new_mode == MODE_TX);
    qsk_counter = 250;

  // no further action needed if there is no change in mode, or if we went from QSK_COUNTDOWN to TX
  if(new_mode == tx_rx_mode || (new_mode == MODE_TX && tx_rx_mode == MODE_QSK_COUNTDOWN))
    return;
  else
    tx_rx_mode = new_mode;

  if(new_mode == MODE_TX) {
    // audio source as sidetone
    gpio_write(OUTPUT_RX_MUTE, OUTPUT_AUDIO_SIDETONE);
    // commented out: use leakage through RX path as sidetone.
    // gpio_write(OUTPUT_RX_MUTE, OUTPUT_AUDIO_RX);

    // set volume to lowest setting before using sidetone
    // TODO: create a sidetone level option in json file
    update_volume(1);
    
    my_delay(10);
    
    // change relays over to TX
    gpio_write(lpf_relay, OUTPUT_ON);
    gpio_write(bpf_relay, OUTPUT_OFF);
    my_delay(50);

    // change over clocks to TX
    // commented out: keep CLK0 and CLK1 running so leakage turns into a sidetone.
    // si5351.output_enable(SI5351_CLK0, 0);
    // si5351.output_enable(SI5351_CLK1, 0);

    my_delay(5);
    si5351.output_enable(SI5351_CLK2, 1);

    Serial.println("[MODE] TX");
  }
  if(new_mode == MODE_RX) {
    si5351.output_enable(SI5351_CLK2, 0);

    my_delay(5);
    // change over clocks to RX
    si5351.output_enable(SI5351_CLK0, 1);
    si5351.output_enable(SI5351_CLK1, 1);
    
    // change over relays before letting RX chain drive the audio
    gpio_write(lpf_relay, OUTPUT_OFF);
    gpio_write(bpf_relay, OUTPUT_ON);
    my_delay(10);

    // audio source as RX
    gpio_write(OUTPUT_RX_MUTE, OUTPUT_AUDIO_RX);

    // restore audio volume, was at minimum setting for sidetone 
    update_volume(vol);

    Serial.println("[MODE] RX");
  }
}

// TODO: pull out the magic 10ms delay
void key_on() {  
  // change over relays and sidetone
  set_mode(MODE_TX);

  sidetone_on();
  
  si5351.output_enable(SI5351_CLK2, 1);

  // TODO - figure out minimum bound here.
  my_delay(2);
  
  gpio_write(OUTPUT_USER_LED, OUTPUT_ON);
  gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_ON);
}

void key_off() {
  set_mode(MODE_QSK_COUNTDOWN);

  sidetone_off();
  
  gpio_write(OUTPUT_USER_LED, OUTPUT_OFF);
  gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_OFF); 

  // TODO - parametrize this
  // this delay must be long enough for switching to fully discharge VBATT-SW
  // otherwise, some voltage remains when key_on() gets called and there is a click.
  my_delay(10);
  
  si5351.output_enable(SI5351_CLK2, 0);
}


void handle_qsk_timer() {
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
  f_rf_min_band1 = load_json_config(hw_config_file, "f_rf_min_hz_band1").toFloat();
  f_rf_max_band1 = load_json_config(hw_config_file, "f_rf_max_hz_band1").toFloat();
  f_rf_min_band2 = load_json_config(hw_config_file, "f_rf_min_hz_band2").toFloat();
  f_rf_max_band2 = load_json_config(hw_config_file, "f_rf_max_hz_band2").toFloat();
  
  // load frequencies from flash
  uint64_t xtal = load_json_config(hw_config_file, "xtal_freq_hz").toFloat();
  f_audio = load_json_config(hw_config_file, "sidetone_pitch_hz").toFloat();
  f_if = load_json_config(hw_config_file, "if_freq_hz").toFloat();
  f_bfo = f_if + f_audio;
  // f_bfo = f_if + 2500;

  // calculate clocks based on the default
  handle_freq(load_json_config(hw_config_file, "f_rf_default_mhz"));


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
