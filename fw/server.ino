  #define SERVER_DEBUG


// copied directly from Async FS Browser examples
// moved to separate tab to keep main file simple.

/*
// this is basically everything copied from the FS Browser example. 
// Moved to a function to keep the main file simpler
void init_file_system() {
  // initialize filesystem
  fileSystemConfig.setAutoFormat(false);
  fileSystem->setConfig(fileSystemConfig);
  fsOK = fileSystem->begin();
  Serial.println(fsOK ? F("Filesystem initialized.") : F("Filesystem init failed!"));
  
  // Debug: dump on console contents of filesystem with no filter and check filenames validity
  Dir dir = fileSystem->openDir("");
  Serial.println(F("List of files at root of filesystem:"));
  while (dir.next()) {
    String error = checkForUnsupportedPath(dir.fileName());
    String fileInfo = dir.fileName() + (dir.isDirectory() ? " [DIR]" : String(" (") + dir.fileSize() + "b)");
    Serial.println(error + fileInfo);
    if (error.length() > 0) {
      unsupportedFiles += error + fileInfo + '\n';
    }
  }
  Serial.println();

  // Keep the "unsupportedFiles" variable to show it, but clean it up
  unsupportedFiles.replace("\n", "<br/>");
  unsupportedFiles = unsupportedFiles.substring(0, unsupportedFiles.length() - 5);
}
*/



// this is mostly copied from Async FS Browser example
// Moved to a function to keep the main file simpler

// TODO: use other HTTP_xxx methods
void init_web_server() {
  /*
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  events.onConnect([](AsyncEventSourceClient *client){
    client->send("hello!",NULL,millis(),1000);
  });
  server.addHandler(&events);
  */

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


  // handlers for antenna
  server.on("/lna", HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_lna(HTTP_PUT, request);
  });
  server.on("/lna", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_lna(HTTP_GET, request);
  });


  // handlers for special commands
  server.on("/debug", HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_debug(HTTP_PUT, request);
  });
  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_debug(HTTP_GET, request);
  });

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
