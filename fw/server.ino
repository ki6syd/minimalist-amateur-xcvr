#define CONCAT(ONE, TWO)  ONE TWO
#define API_VERSION   "v1"
#define API_BASE_URL  "/api/v1/"

// this is mostly copied from Async FS Browser example
// Moved to a function to keep the main file simpler
void init_web_server() {  
  server.addHandler(new SPIFFSEditor(load_json_config(wifi_file, "user_settings"), load_json_config(wifi_file, "pass_settings")));

  // handlers for FT8 messages
  server.on(CONCAT(API_BASE_URL, "ft8"), HTTP_POST, [](AsyncWebServerRequest *request){
    handle_ft8(HTTP_POST, request);
  });
  server.on(CONCAT(API_BASE_URL, "ft8"), HTTP_DELETE, [](AsyncWebServerRequest *request){
    handle_ft8(HTTP_DELETE, request);
  });


  // handlers for CW messages
  server.on(CONCAT(API_BASE_URL, "cw"), HTTP_POST, [](AsyncWebServerRequest *request){
    handle_cw(HTTP_POST, request);
  });
  server.on(CONCAT(API_BASE_URL, "cw"), HTTP_DELETE, [](AsyncWebServerRequest *request){
    handle_cw(HTTP_DELETE, request);
  });
  

  // handler for digital mode messages
  server.on(CONCAT(API_BASE_URL, "queue"), HTTP_GET, [](AsyncWebServerRequest *request){
    handle_queue(HTTP_GET, request);
  });
  server.on(CONCAT(API_BASE_URL, "queue"), HTTP_DELETE, [](AsyncWebServerRequest *request){
    handle_queue(HTTP_DELETE, request);
  });


  // handlers for time
  server.on(CONCAT(API_BASE_URL, "time"), HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_time(HTTP_PUT, request);
  });
  server.on(CONCAT(API_BASE_URL, "time"), HTTP_GET, [](AsyncWebServerRequest *request){
    handle_time(HTTP_GET, request);
  });


  // handlers for frequency
  server.on(CONCAT(API_BASE_URL, "frequency"), HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_frequency(HTTP_PUT, request);
  });
  server.on(CONCAT(API_BASE_URL, "frequency"), HTTP_GET, [](AsyncWebServerRequest *request){
    handle_frequency(HTTP_GET, request);
  });

  // handlers for rx bandwidth
  server.on(CONCAT(API_BASE_URL, "rxBandwidth"), HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_rx_bandwidth(HTTP_PUT, request);
  });
  server.on(CONCAT(API_BASE_URL, "rxBandwidth"), HTTP_GET, [](AsyncWebServerRequest *request){
    handle_rx_bandwidth(HTTP_GET, request);
  });

  server.on(CONCAT(API_BASE_URL, "inputVoltage"), HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(last_vbat));
  });

  // handlers for volume
  server.on(CONCAT(API_BASE_URL, "volume"), HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_volume(HTTP_PUT, request);
  });
  server.on(CONCAT(API_BASE_URL, "volume"), HTTP_GET, [](AsyncWebServerRequest *request){
    handle_volume(HTTP_GET, request);
  });

  // handlers for volume
  server.on(CONCAT(API_BASE_URL, "sidetone"), HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_sidetone(HTTP_PUT, request);
  });
  server.on(CONCAT(API_BASE_URL, "sidetone"), HTTP_GET, [](AsyncWebServerRequest *request){
    handle_sidetone(HTTP_GET, request);
  });

  // handlers for cw speed
  server.on(CONCAT(API_BASE_URL, "cwSpeed"), HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_cw_speed(HTTP_PUT, request);
  });
  server.on(CONCAT(API_BASE_URL, "cwSpeed"), HTTP_GET, [](AsyncWebServerRequest *request){
    handle_cw_speed(HTTP_GET, request);
  });

  // handlers for antenna
  server.on(CONCAT(API_BASE_URL, "lna"), HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_lna(HTTP_PUT, request);
  });
  server.on(CONCAT(API_BASE_URL, "lna"), HTTP_GET, [](AsyncWebServerRequest *request){
    handle_lna(HTTP_GET, request);
  });
  // handlers for antenna
  server.on(CONCAT(API_BASE_URL, "antenna"), HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_antenna(HTTP_PUT, request);
  });
  server.on(CONCAT(API_BASE_URL, "antenna"), HTTP_GET, [](AsyncWebServerRequest *request){
    handle_antenna(HTTP_GET, request);
  });


  // handlers for speaker
  server.on(CONCAT(API_BASE_URL, "speaker"), HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_speaker(HTTP_PUT, request);
  });
  server.on(CONCAT(API_BASE_URL, "speaker"), HTTP_GET, [](AsyncWebServerRequest *request){
    handle_speaker(HTTP_GET, request);
  });

  server.on(CONCAT(API_BASE_URL, "sMeter"), HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(last_smeter));
  }); 

  server.on(CONCAT(API_BASE_URL, "githash"), HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(GIT_VERSION));
  });

  server.on(CONCAT(API_BASE_URL, "address"), HTTP_GET, [](AsyncWebServerRequest *request){
    IPAddress ip = WiFi.localIP();
    request->send(200, "text/plain", String(ip.toString()));
  });

  server.on(CONCAT(API_BASE_URL, "hwRevision"), HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", hardware_rev);
  });

  server.on(CONCAT(API_BASE_URL, "unitSerial"), HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", unit_serial);
  });

  // register callbacks for api with or without path
  server.on(CONCAT(API_BASE_URL, "api"), HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(API_VERSION));
  });
  server.on("/api", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(API_VERSION));
  });

  server.on(CONCAT(API_BASE_URL, "selfTest"), HTTP_POST, [](AsyncWebServerRequest *request){
    handle_selftest(HTTP_POST, request);
  });

  server.on(CONCAT(API_BASE_URL, "rawSamples"), HTTP_POST, [](AsyncWebServerRequest *request){
    handle_raw_samples(HTTP_POST, request);
  });

  // handlers for special commands
  server.on(CONCAT(API_BASE_URL, "debug"), HTTP_POST, [](AsyncWebServerRequest *request){
    handle_debug(HTTP_POST, request);
  });
  server.on(CONCAT(API_BASE_URL, "debug"), HTTP_GET, [](AsyncWebServerRequest *request){
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
      // there is no isPut() function
      Serial.printf("_GET or _PUT [%s]: %s\n", p->name().c_str(), p->value().c_str());
    }
  }
}
