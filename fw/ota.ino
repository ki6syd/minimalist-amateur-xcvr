// useful reference for OTA: https://github.com/lbernstone/asyncUpdate/blob/master/AsyncUpdate.ino 

// OTA update function
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  gpio_write(OUTPUT_GREEN_LED, OUTPUT_ON);

  // decide which partition to write. Relies on "spiffs" being in name
  if (!index){
    Serial.println("[OTA] Upload Beginning");

    uint32_t free_space = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    uint32_t sketch_size = ESP.getSketchSize();
    content_len = request->contentLength();

    // SPIFFS selected for upload
    if(filename.indexOf("spiffs") > -1) {
      // check file size. Fail fast if too large
      if(content_len > free_space) {
        Serial.println("[OTA] Cannot OTA.");
        Serial.print("[OTA] Content length: ");
        Serial.println(content_len);
        Serial.print("[OTA] Free space: ");
        Serial.println(free_space);
        request->send(400, "text/plain", "Content length is larger than available space");
        return;
      }

      // begin update
      Update.runAsync(true);
      if (!Update.begin(free_space, U_FS)) {
        Update.printError(Serial);
        request->send(400, "text/plain", "Upload failure, see serial log.");
        return;
      }
    }
    // program upload
    else {
      // begin update
      Update.runAsync(true);
      if (!Update.begin(content_len, U_FLASH)) {
        Update.printError(Serial);
        request->send(400, "text/plain", "Upload failure, see serial log.");
        return;
      }
    }
  }
  
  // receive data, write it to flash, show progress
  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  }
  // todo: add callback function for webpage display
  Serial.printf("Progress: %d%%\n", (Update.progress()*100)/Update.size());


  // upload complete
  if (final) {
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
    response->addHeader("Refresh", "20");  
    response->addHeader("Location", "/");
    
    request->send(response);
    if (!Update.end(true)){
      Update.printError(Serial);
    } else {
      Serial.println("[OTA] Update complete.");
      Serial.flush();
      ESP.restart();
    }
  }

  gpio_write(OUTPUT_GREEN_LED, OUTPUT_OFF);
}


// puts radio in a special file system recovery mode in case of corruption
// does not rely on JSON file for configuration
void recovery() {
  // reconfigure wifi
  Serial.println("[RECOVERY] Starting wifi network");
  WiFi.mode(WIFI_OFF);
  my_delay(1000);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("max-3b_recovery");
  WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
  Serial.print("[RECOVERY] IP address: ");
  Serial.println(WiFi.softAPIP());

  // register callbacks
  Serial.println("[RECOVERY] Starting wifi network");
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", "<form method='POST' action='/ota' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
  });
  server.on("/ota", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200);
  }, handleUpload);

  server.begin();

  // spin
  while(1) {
    gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
    my_delay(500);
    gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
    my_delay(500);
  }
}
