void init_beacon() {
  Serial.println("[BEACON] Initializing beacon");

  beacon_mode = load_json_config(preference_file, "beacon_mode");
  beacon_mode.toLowerCase();
  
  if(beacon_mode == "wspr" || beacon_mode == "cw" || beacon_mode == "ft8")
    beacon = true;

  beacon_interval = load_json_config(preference_file, "beacon_interval").toFloat();
  
  update_time_from_web();
}

void update_beacon() {
  // no action until next beacon interval
  if(millis() - last_beacon < BEACON_INTERVAL) 
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
    
    uint64_t freq_list[] = {7038600, 14095600, 21094600};
    tmp.freq = freq_list[random(0, 3)];
  
    // add audio freq plus some randomness
    tmp.freq += 1000;
    tmp.freq += random(0, 700);
    
    char call[] = {"KI6SYD"};
    char loc[] = {"CM86"};
    uint8_t dbm = 35;
  
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
