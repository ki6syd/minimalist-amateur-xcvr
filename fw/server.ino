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


void handle_freq(String freq) {
  Serial.print("[FREQ UPDATE] ");
  Serial.print(freq);
  Serial.print("\t");
  
  uint64_t tmp = freq.toFloat() * 1000000;

  // check frequency bounds before doing anything else
  if((tmp < f_rf_min_band1 && tmp > f_rf_max_band1) && (tmp < f_rf_min_band2 && tmp > f_rf_max_band2) && (tmp < f_rf_min_band3 && tmp > f_rf_max_band3))
    return;

  flag_freq = true;
  f_rf = tmp;
  // TODO: make sure we don't get negative numbers
  f_vfo = update_vfo(f_rf, f_bfo, f_audio);

  Serial.print("RF: ");
  print_uint64_t(f_rf);
  Serial.print("\tVFO: ");
  print_uint64_t(f_vfo);
  Serial.print("\tBFO: ");
  print_uint64_t(f_bfo);
  Serial.println();
}

void handle_clocks(String clk0_string, String clk1_string, String clk2_string) {
  Serial.print("[CLOCK UPDATE] ");
  Serial.print(clk0_string);
  Serial.print("\t");
  Serial.print(clk1_string);
  Serial.print("\t");
  Serial.println(clk2_string);
  
  uint64_t clk0 = clk0_string.toFloat() * 1000000;
  uint64_t clk1 = clk1_string.toFloat() * 1000000;
  uint64_t clk2 = clk2_string.toFloat() * 1000000;

  flag_freq = false;

  set_clocks(clk0, clk1, clk2);
}

String handle_s_meter() {
  // commented out because this one is a little noisy
  // Serial.println("[S-METER UPDATE]");
  return String(last_smeter);
}

String handle_batt() {
  // commented out because this one is a little noisy
  // Serial.println("[BATTERY VOLTAGE UPDATE]");
  return String(last_vbat);
}

String handle_debug_1() {
  return String(dbg_1);
}

String handle_debug_2() {
  return String(dbg_2);
}

void handle_volume(String volume) {
  flag_vol = true;
  // TODO: add bounds on the input
  vol = volume.toInt();
}

void handle_enqueue(String new_text) {
  Serial.print("[ENQUEUE] ");
  Serial.print(new_text);
  Serial.println("<end>");

  tx_queue += new_text;
}

void handle_speed(String speed_wpm) {
  Serial.print("[KEYER SPEED UPDATE] ");
  Serial.println(speed_wpm);
  
  keyer_speed = speed_wpm.toInt();
}

void handle_bandwidth(String bw_setting) {
  Serial.print("[BW CHANGE] ");
  Serial.println(bw_setting);

  flag_freq = true;

  // TODO: handle and validate enums, rather than accepting any integer  
  rx_bw = bw_setting.toInt();
  if(rx_bw == 1)
    rx_bw = OUTPUT_SEL_CW;
  else
    rx_bw = OUTPUT_SEL_SSB;


  // TODO: adjust the ~700hz audio offset we put in for sidetone in CW mode, but won't want in SSB
}

void handle_special(String special_setting) {
  Serial.print("[SPECIAL] ");
  Serial.println(special_setting);

  flag_special = true;
  special = special_setting.toInt();
}


// this is mostly copied from Async FS Browser example
// Moved to a function to keep the main file simpler
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

  // heap size check
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  // handler for frequency setting
  server.on("/freq", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String input_message;
    if (request->hasParam("value"))
      input_message = request->getParam("value")->value();
    else
      input_message = "No message sent";
    
    handle_freq(input_message);
    request->send(200, "text/plain", "OK");
  });

  // handler for setting individual clocks
  server.on("/clk", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String input_message, string1, string2, string3;
    if (request->hasParam("value"))
      input_message = request->getParam("value")->value();
    else
      input_message = "No message sent";
    
    int index1 = input_message.indexOf("_");
    int index2 = input_message.indexOf("_", index1 + 1);

    string1 = input_message.substring(0, index1);
    string2 = input_message.substring(index1 + 1, index2);
    string3 = input_message.substring(index2 + 1);

    handle_clocks(string1, string2, string3);

    request->send(200, "text/plain", "OK");
  });

  

  // handler for adding text to queue
  server.on("/enqueue", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String input_message;
    if (request->hasParam("value"))
      input_message = request->getParam("value")->value();
    else
      input_message = "No message sent";

    handle_enqueue(input_message);
    request->send(200, "text/plain", "OK");
  });

  // handler for adding text to queue
  server.on("/vol", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String input_message;
    if (request->hasParam("value"))
      input_message = request->getParam("value")->value();
    else
      input_message = "No message sent";

    handle_volume(input_message);
    request->send(200, "text/plain", "OK");
  });

  // handler for adding text to queue
  server.on("/speed", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String input_message;
    if (request->hasParam("value"))
      input_message = request->getParam("value")->value();
    else
      input_message = "No message sent";

    handle_speed(input_message);
    request->send(200, "text/plain", "OK");
  });

  // handler for changing audio bandwidth
  server.on("/bandwidth", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String input_message;
    if (request->hasParam("value"))
      input_message = request->getParam("value")->value();
    else
      input_message = "No message sent";

    handle_bandwidth(input_message);
    request->send(200, "text/plain", "OK");
  });

  // handler for changing audio bandwidth
  server.on("/special", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String input_message;
    if (request->hasParam("value"))
      input_message = request->getParam("value")->value();
    else
      input_message = "No message sent";

    handle_special(input_message);
    request->send(200, "text/plain", "OK");
  });

  // handler for S-Meter request
  server.on("/s", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_s_meter());
  });

  // handler for battery voltage request
  server.on("/vbat", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_batt());
  });

  // handler for dbg1
  server.on("/dbg1", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_debug_1());
  });

  // handler for dbg2
  server.on("/dbg2", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_debug_2());
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
