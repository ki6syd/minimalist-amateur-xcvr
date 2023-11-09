void init_beacon() {
  Serial.println("[BEACON] Initializing beacon");

  beacon_mode = load_json_config(beacon_file, "beacon_mode");
  beacon_mode.toLowerCase();
  
  if(beacon_mode == "wspr" || beacon_mode == "cw" || beacon_mode == "ft8")
    beacon = true;

  beacon_interval = load_json_config(beacon_file, "beacon_interval").toFloat();

  // fill up the queue of beacon frequencies
  if(beacon_mode == "wspr") {
    /*
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.to<JsonArray>();
    load_json_array(beacon_file, "wspr_freq", arr);
    size_t arr_len = arr.size();

    for (size_t i = 0; i < arr.size(); ++i) {
      String tmp = arr[i].as<const char *>();
      Serial.println(tmp);
      // wsprFreqValues[i] = strtoull(arr[i].as<const char*>(), nullptr, 10);
    }

    

    for(uint8_t i = 0; i < arr_len; i++) {
      Serial.println(i);
      String tmp = arr[i].as<String>();
      // uint64_t tmp_uint = strtoull(arr[i].as<const char*>(), nullptr, 10);
      Serial.print(tmp);
      Serial.print("\t");
      // Serial.println(tmp_uint);
      // beacon_freqs.push(tmp_uint);
      Serial.println(i);
      Serial.println(arr.size());
    }
    */


    

    
    
    File configFile = SPIFFS.open("/beacon.json", "r");

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();
  
    JsonArray arr = doc["wspr_freq"].as<JsonArray>();

    serializeJsonPretty(arr, Serial);

    uint64_t wsprFreqValues[arr.size()];

    // Convert and store each element of the JsonArray as uint64_t
    for (size_t i = 0; i < arr.size(); ++i) {
      String tmp = arr[i].as<String>();
      Serial.println(tmp);
      wsprFreqValues[i] = strtoull(arr[i].as<const char*>(), nullptr, 10);
      print_uint64_t(wsprFreqValues[i]);
    }
  
    
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
