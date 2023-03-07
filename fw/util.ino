// read a specific parameter out of the JSON file
// TODO: look into whether there is enough memory to pull this from the file at startup and avoid repeated reads.
// Repeated reads will be slow, but maybe doesn't matter.
String load_json_config(String file_name, String param_name) {
  File configFile = SPIFFS.open(file_name, "r");

  if (!configFile) {
    Serial.println("- failed to open config file for writing");
    return "ERROR";
  }

  size_t sz = configFile.size();
  if (sz > 1024) {
    Serial.println("Config file size is too large");
    return "ERROR";
  }

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, configFile);

  configFile.close();

  if(error) {
    Serial.println("Failed to parse config file");
    return "ERROR";
  }

  const char* tmp = doc[param_name];

  Serial.print("[JSON READ] ");
  Serial.print(param_name);
  Serial.print(": ");
  Serial.println(tmp);
  return tmp;
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

  while(millis() - start_time < dly)
    delay(1);
}

// programmatic reset
void reboot() {
 wdt_disable();
 wdt_enable(WDTO_15MS);
 while (1) {}
}
