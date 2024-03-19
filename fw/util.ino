// read a specific parameter out of the JSON file
// TODO: look into whether there is enough memory to pull this from the file at startup and avoid repeated reads.
// Repeated reads will be slow, but maybe doesn't matter.
String load_json_config(String file_name, String param_name) {
  File configFile = SPIFFS.open(file_name, "r");

  if (!configFile) {
    Serial.println("Config file not found");
    recovery();
  }

  size_t sz = configFile.size();
  if (sz > 1024) {
    Serial.println("Config file size is too large");
    recovery();
  }

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, configFile);

  configFile.close();

  if(error) {
    Serial.print("Failed to parse config file while trying to read: ");
    Serial.print(file_name);
    Serial.print(": ");
    Serial.println(param_name);
    recovery();
  }

  const char* tmp = doc[param_name];

  // todo: look for cases where param_name does not exist

  Serial.print("[JSON READ] ");
  Serial.print(param_name);
  Serial.print(": ");
  Serial.println(tmp);
  return tmp;
}

// loads a JSON array into the queue, passed by reference
// array must be at top level of the json hierarchy
void load_json_array(String file_name, String param_name, Queue<uint64_t> &queue) {
  File configFile = SPIFFS.open(file_name, "r");

  if (!configFile) {
    Serial.println("Config file not found");
    recovery();
  }

  size_t sz = configFile.size();
  if (sz > 1024) {
    Serial.println("Config file size is too large");
    recovery();
  }

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  JsonArray arr = doc[param_name].as<JsonArray>();

  Serial.print("[JSON READ] ");
  Serial.print(param_name);
  Serial.println(":");
  serializeJsonPretty(arr, Serial);
  Serial.println();

  for (size_t i = 0; i < arr.size(); i++) {
    queue.push(strtoull(arr[i].as<const char*>(), nullptr, 10));
  }
}

void load_json_bands(String file_name, Queue<BandCapability> &bands) {
  File configFile = SPIFFS.open(file_name, "r");

  if (!configFile) {
    Serial.println("Config file not found");
    recovery();
  }
  
  size_t sz = configFile.size();
  if (sz > 1024) {
    Serial.println("Config file size is too large");
    recovery();
  }

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  JsonArray arr = doc["bands"].as<JsonArray>();

  Serial.print("[JSON READ] ");

  for (size_t i = 0; i < arr.size(); i++) {
    BandCapability tmp;
    JsonObject entry = arr[i];

    // pull out information from the JSON object
    tmp.band_num = entry["band_num"].as<String>().toInt();
    tmp.min_freq = strtoull(entry["min_freq"].as<const char*>(), nullptr, 10);
    tmp.max_freq = strtoull(entry["max_freq"].as<const char*>(), nullptr, 10);

    /*
     * comment this in when it's time to start consuming mode capability
    JsonArray tx_arr = entry["tx_modes"].as<JsonArray>();
    JsonArray rx_arr = entry["rx_modes"].as<JsonArray>();
    serializeJsonPretty(tx_arr, Serial);    
    Serial.println();
    serializeJsonPretty(rx_arr, Serial);    
    Serial.println();
    */

    // add to band capability variable
    bands.push(tmp);
  }
}

// https://stackoverflow.com/questions/45974514/serial-print-uint64-t-in-arduino
void print_uint64_t(uint64_t num) {

  char rev[128]; 
  char *p = rev+1;

  while (num > 0) {
    *p++ = '0' + ( num % 10);
    num/= 10;
  }
  p--;
  /*Print the number which is now in reverse*/
  while (p > rev) {
    Serial.print(*p--);
  }
  Serial.println();
}


// non-blocking delay function to minimize impact on wifi
void my_delay(uint64_t dly) {
  uint64_t start_time = millis();

  while(millis() - start_time < dly) {
    delay(1);
    ESP.wdtFeed();
  }
}

// programmatic reset
void reboot() {
 wdt_disable();
 wdt_enable(WDTO_15MS);
 while (1) {}
}
