void init_wifi_mdns() {
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet(255, 255, 255, 0); 
  
  // this call before any others helps, per: https://stackoverflow.com/questions/44139082/esp8266-takes-long-time-to-connect
  WiFi.persistent(false);
  
  // attempt to connect to home wifi network
  Serial.println("[WIFI] Attempting to connect to home network.");
  WiFi.mode(WIFI_STA);

  // check if we are using DHCP. If not, assign a static IP from the JSON file
  if(load_json_config(wifi_file, "ip_home") != "DHCP") {
    Serial.println("[WIFI] Static IP requested.");
    ip.fromString(load_json_config(wifi_file, "ip_home"));
    gateway.fromString(load_json_config(wifi_file, "gateway_home"));
    WiFi.config(ip, gateway, subnet);
  }

  // TODO: give an option for PHY mode (setPhyMode())

  // start WiFi, try to connect for 10 seconds before giving up
  WiFi.begin(load_json_config(wifi_file, "ssid_home"), load_json_config(wifi_file, "pass_home"));
  for (int i = 0; i < 50; i++) {
    if (WiFi.status() == WL_CONNECTED)
      break;
    gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
    my_delay(100);
    gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
    my_delay(100);
  }

  // start access point if we have failed to connect
  // TODO: look at using waitForConnectResult. https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/station-class.html#waitforconnectresult
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Failed to connect to home network. Starting AP.");
    // begin by forgetting old credentials
    WiFi.disconnect(true);

    // set up AP with hardcoded 192.168.1.1 address
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP(load_json_config(wifi_file, "ssid_ap"), load_json_config(wifi_file, "pass_ap"), load_json_config(wifi_file, "wifi_channel").toInt());
    ip = WiFi.softAPIP();
  }
  else {
    Serial.println("[WIFI] Successful connection to home network.");
    ip = WiFi.localIP();

    // set red LED for 10 seconds if we didn't get a normal IP. Delete this if debug tool not needed.
    // and do a scan
    if(ip[0] != 192) {
      gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
      gpio_write(OUTPUT_GREEN_LED, OUTPUT_OFF);
      my_delay(10000);
      wifi_scan();
    }
  }

  Serial.print("[WIFI] IP address: ");
  ip_address = ip.toString();
  Serial.println(ip_address);

  Serial.print("[WIFI] Connection status: ");
  Serial.println(WiFi.status());

  // for testing only
  wifi_scan();
  
  // set wifi power
  int wifi_dBm = load_json_config(wifi_file, "wifi_power_dbm").toInt();
  WiFi.setOutputPower(wifi_dBm);
  Serial.print("[WIFI] Power set to ");
  Serial.print(wifi_dBm);
  Serial.println(" dBm");

  // start MDNS
  if (MDNS.begin(load_json_config(wifi_file, "mdns_hostname"))) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[MDNS] Started MDNS with hostname from JSON");
  }

  // for testing only
  // WiFi.mode(WIFI_OFF);
}

void wifi_scan() {
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
}
