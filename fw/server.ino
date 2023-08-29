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


// this is mostly copied from Async FS Browser example
// Moved to a function to keep the main file simpler

void init_web_server() {

  // todo: put config password into JSON
  server.addHandler(new SPIFFSEditor(load_json_config(credential_file, "user_settings"), load_json_config(credential_file, "pass_settings")));

  // handlers for FT8 messages
  server.on("/ft8", HTTP_POST, [](AsyncWebServerRequest *request){
    print_request_details(request);
    handle_ft8(HTTP_POST, request);
  });
  server.on("/ft8", HTTP_DELETE, [](AsyncWebServerRequest *request){
    handle_ft8(HTTP_DELETE, request);
  });


  // handlers for CW messages
  server.on("/cw", HTTP_POST, [](AsyncWebServerRequest *request){
    handle_cw(HTTP_POST, request);
  });
  server.on("/cw", HTTP_DELETE, [](AsyncWebServerRequest *request){
    handle_cw(HTTP_DELETE, request);
  });
  

  // handler for digital mode messages
  server.on("/queue", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_queue(HTTP_GET, request);
  });
  server.on("/queue", HTTP_DELETE, [](AsyncWebServerRequest *request){
    handle_queue(HTTP_DELETE, request);
  });


  // handlers for time
  server.on("/time", HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_time(HTTP_PUT, request);
  });
  server.on("/time", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_time(HTTP_GET, request);
  });


  // handlers for frequency
  server.on("/frequency", HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_frequency(HTTP_PUT, request);
  });
  server.on("/frequency", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_frequency(HTTP_GET, request);
  });

  // handlers for rx bandwidth
  server.on("/rxBandwidth", HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_rx_bandwidth(HTTP_PUT, request);
  });
  server.on("/rxBandwidth", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_rx_bandwidth(HTTP_GET, request);
  });

  server.on("/sMeter", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(last_smeter));
  });

  server.on("/inputVoltage", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(last_vbat));
  });

  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(0));
  });

  // handlers for volume
  server.on("/volume", HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_volume(HTTP_PUT, request);
  });
  server.on("/volume", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_volume(HTTP_GET, request);
  });

  // handlers for volume
  server.on("/sidetone", HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_sidetone(HTTP_PUT, request);
  });
  server.on("/sidetone", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_sidetone(HTTP_GET, request);
  });

  // handlers for cw speed
  server.on("/cwSpeed", HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_cw_speed(HTTP_PUT, request);
  });
  server.on("/cwSpeed", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_cw_speed(HTTP_GET, request);
  });

  // handlers for antenna
  server.on("/antenna", HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_antenna(HTTP_PUT, request);
  });
  server.on("/antenna", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_antenna(HTTP_GET, request);
  });


  // handlers for speaker
  server.on("/speaker", HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_speaker(HTTP_PUT, request);
  });
  server.on("/speaker", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_speaker(HTTP_GET, request);
  });


  // handlers for antenna
  server.on("/lna", HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_lna(HTTP_PUT, request);
  });
  server.on("/lna", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_lna(HTTP_GET, request);
  });


  // handlers for special commands
  server.on("/debug", HTTP_POST, [](AsyncWebServerRequest *request){
    handle_debug(HTTP_POST, request);
  });
  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_debug(HTTP_GET, request);
  });

  server.on("/ota", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200);
      }, handleUpload);

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  server.onNotFound([](AsyncWebServerRequest *request){
    Serial.printf("NOT_FOUND: ");
    if(request->method() == HTTP_GET)
      Serial.printf("GET");
    else if(request->method() == HTTP_POST)
      Serial.printf("POST");
    else if(request->method() == HTTP_DELETE)
      Serial.printf("DELETE");
    else if(request->method() == HTTP_PUT)
      Serial.printf("PUT");
    else if(request->method() == HTTP_PATCH)
      Serial.printf("PATCH");
    else if(request->method() == HTTP_HEAD)
      Serial.printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
      Serial.printf("OPTIONS");
    else
      Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    print_request_details(request);

    request->send(404);
  });
  server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
      Serial.printf("UploadStart: %s\n", filename.c_str());
    Serial.printf("%s", (const char*)data);
    if(final)
      Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
      Serial.printf("BodyStart: %u\n", total);
    Serial.printf("%s", (const char*)data);
    if(index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });

  server.begin();
}


void print_request_details(AsyncWebServerRequest *request) {
  if(request->contentLength()){
    Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
    Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
  }

  int headers = request->headers();
  int i;
  for(i=0;i<headers;i++){
    AsyncWebHeader* h = request->getHeader(i);
    Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
  }

  int params = request->params();
  for(i=0;i<params;i++){
    AsyncWebParameter* p = request->getParam(i);
    if(p->isFile()){
      Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
    } else if(p->isPost()){
      Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    } else {
      Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
    }
  }
}
