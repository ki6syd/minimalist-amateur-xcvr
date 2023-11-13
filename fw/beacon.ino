void init_beacon() {
  Serial.println("[BEACON] Initializing beacon");

  beacon_mode = load_json_config(beacon_file, "beacon_mode");
  beacon_mode.toLowerCase();
  
  if(beacon_mode == "wspr" || beacon_mode == "cw" || beacon_mode == "ft8")
    beacon = true;

  beacon_interval = load_json_config(beacon_file, "beacon_interval").toFloat();

  // fill up the queue of beacon frequencies
  if(beacon_mode == "wspr") {    
    load_json_array(beacon_file, "wspr_freq", beacon_freqs);
  }
  
  update_time_from_web();
}

void update_beacon() {
  // no action until next beacon interval
  if(millis() - last_beacon < beacon_interval) 
    return;

  // no action if there's something in the queue
  if(digital_queue.count() > 0)
    return;

  // try to sync time before queueing the message
  update_time_from_web();

  // todo: parametrize this logic with a json file. magic numbers in the code until then
  DigitalMessage tmp;
  tmp.ignore_time = false;
  tmp.type = MODE_CW;
  tmp.buf[0] = '\0';

  if(beacon_mode == "wspr") {
    tmp.type = MODE_WSPR;

    // pick from the front of the queue, and insert back at the end
    tmp.freq = beacon_freqs.pop();
    beacon_freqs.push(tmp.freq);
  
    // add audio freq plus some randomness
    tmp.freq += 1000;
    tmp.freq += random(0, 700);


    // load up char arrays from JSON configuration
    char call[7];
    String call_sign = load_json_config(beacon_file, "wspr_call");
    call_sign.toCharArray(call, 7);
    
    char loc[5];
    String grid_square = load_json_config(beacon_file, "wspr_grid");
    grid_square.toCharArray(loc, 5);

    String power = load_json_config(beacon_file, "wspr_power");
    uint8_t dbm = power.toInt();

  
    Serial.print("[WSPR] Freq: ");
    Serial.println(tmp.freq);
    Serial.print("[WSPR] Call: ");
    Serial.println(call);
    Serial.print("[WSPR] Grid: ");
    Serial.println(loc);
    Serial.print("[WSPR] Power: ");
    Serial.println(dbm);
    jtencode.wspr_encode(call, loc, dbm, tmp.buf);
  
    Serial.println("[WSPR] Calculated buffer: ");
    for(uint8_t i=0; i<WSPR_SYMBOL_COUNT; i++) {
      Serial.print(tmp.buf[i]);
      Serial.print(",");
    }
    Serial.println();
  }
  // todo: other beacon modes

  // push onto queue
  digital_queue.push(tmp);

  // reset beacon timer
  last_beacon = millis();
}
