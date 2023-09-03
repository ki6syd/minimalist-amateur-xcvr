void init_wifi_mdns() {
  IPAddress ip;
  
  // this call before any others helps, per: https://stackoverflow.com/questions/44139082/esp8266-takes-long-time-to-connect
  WiFi.persistent(false);
  
  // attempt to connect to home wifi network
  Serial.println("[WIFI] Attempting to connect to home network.");
  WiFi.mode(WIFI_STA);
  WiFi.begin(load_json_config(credential_file, "ssid_home"), load_json_config(credential_file, "pass_home"));
  // try to connect for 10 seconds
  for (int i = 0; i < 50; i++) {
    if (WiFi.status() == WL_CONNECTED)
      break;
    gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
    my_delay(100);
    gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
    my_delay(100);
  }

  // start access point if we have failed to connect
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Failed to connect to home network. Starting AP.");
    // begin by forgetting old credentials
    WiFi.disconnect(true);

    // set up AP
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP(load_json_config(credential_file, "ssid_ap"), load_json_config(credential_file, "pass_ap"), load_json_config(credential_file, "wifi_channel").toInt());
    ip = WiFi.softAPIP();
  }
  else {
    Serial.println("[WIFI] Successful connection to home network.");
    ip = WiFi.localIP();
  }

  Serial.print("[WIFI] IP address: ");
  Serial.println(ip);

  Serial.print("[WIFI] Connection status: ");
  Serial.println(WiFi.status());

  // do a scan of all networks and print info. Useful info: https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
  // todo - add a flag to turn this debug on/off
  Serial.println("[WIFI] scan starting");
  int n = WiFi.scanNetworks();
  Serial.print("[WIFI] found networks: ");
  Serial.println(n);
  for(int i = 0; i < n; i++) {
    Serial.print("[WIFI] SSID: ");
    Serial.println(WiFi.SSID(i));
    Serial.print("[WIFI] RSSI: ");
    Serial.println(WiFi.RSSI(i));
    Serial.print("[WIFI] Encryption: ");
    Serial.println(WiFi.encryptionType(i));
    Serial.println("\n");

    my_delay(100); 
  }
  
  // set red LED for 10 seconds if we didn't get a normal IP, for debug
  // and do a scan
  if(ip[0] != 192) {
    gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
    gpio_write(OUTPUT_GREEN_LED, OUTPUT_OFF);
    my_delay(10000);
  }

  int wifi_dBm = load_json_config(preference_file, "wifi_power_dbm").toInt();
  WiFi.setOutputPower(wifi_dBm);
  Serial.print("[WIFI] Power set to ");
  Serial.print(wifi_dBm);
  Serial.println(" dBm");

  // start MDNS
  if (MDNS.begin(load_json_config(preference_file, "mdns_hostname"))) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[MDNS] Started MDNS with hostname from JSON");
  }

  // for testing only
  // WiFi.mode(WIFI_OFF);
}
