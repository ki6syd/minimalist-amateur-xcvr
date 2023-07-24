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
    handle_ft8(HTTP_POST, request);
  });


  // handlers for time
  server.on("/time", HTTP_PUT, [](AsyncWebServerRequest *request){
    handle_time(HTTP_PUT, request);
  });
  // handlers for time
  server.on("/time", HTTP_GET, [](AsyncWebServerRequest *request){
    handle_time(HTTP_GET, request);
  });







  

  // handler for sotamat message
  server.on("/sotamat", HTTP_POST, [](AsyncWebServerRequest *request){

    print_request_details(request);
    
    int params = request->params();
    if(params == 2) {
      handle_sotamat(request->getParam("call")->value(), request->getParam("suffix")->value());
    }
    else {
      Serial.println("[SERVER] Did not understand sotamat message, wrong number of parameters");
    }
    
    request->send(200, "text/plain", "OK");
  });

  // handler for frequency setting
  server.on("/set_freq", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String input_message;
    if (request->hasParam("value"))
      input_message = request->getParam("value")->value();
    else
      input_message = "No message sent";
    
    handle_set_freq(input_message);
    request->send(200, "text/plain", "OK");
  });  

  // handler for frequency getting
  server.on("/get_freq", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_get_freq());
  });

  // handler for S-Meter request
  server.on("/get_s", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_get_smeter());
  });

  // handler for battery voltage request
  server.on("/get_vbat", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_get_vbat());
  });

  // handler for dbg1
  server.on("/get_debug", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_debug());
  });

  // handler for increasing volume
  server.on("/incr_vol", HTTP_GET, [] (AsyncWebServerRequest *request) {
    handle_incr_volume();
    request->send(200, "text/plain", "OK");
  });

  // handler for decreasing volume
  server.on("/decr_vol", HTTP_GET, [] (AsyncWebServerRequest *request) {
    handle_decr_volume();
    request->send(200, "text/plain", "OK");
  });

  // handler for getting volume
  server.on("/get_vol", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_get_volume());
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

  // handler for increasing speed
  server.on("/incr_speed", HTTP_GET, [] (AsyncWebServerRequest *request) {
    handle_incr_speed();
    request->send(200, "text/plain", "OK");
  });

  // handler for decreasing speed
  server.on("/decr_speed", HTTP_GET, [] (AsyncWebServerRequest *request) {
    handle_decr_speed();
    request->send(200, "text/plain", "OK");
  });

  // handler for getting speed
  server.on("/get_speed", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_get_speed());
  });

  // handler for bandwidth button
  server.on("/press_bw", HTTP_GET, [] (AsyncWebServerRequest *request) {
    handle_press_bw();
    request->send(200, "text/plain", "OK");
  });

  // handler for getting speed
  server.on("/get_bw", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_get_bw());
  });

  // handler for antenna button
  server.on("/press_ant", HTTP_GET, [] (AsyncWebServerRequest *request) {
    handle_press_ant();
    request->send(200, "text/plain", "OK");
  });

  // handler for getting speed
  server.on("/get_ant", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_get_ant());
  });

  // handler for lna button
  server.on("/press_lna", HTTP_GET, [] (AsyncWebServerRequest *request) {
    handle_press_lna();
    request->send(200, "text/plain", "OK");
  });

  // handler for getting lna
  server.on("/get_lna", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_get_lna());
  });

  // handler for lna button
  server.on("/press_mon", HTTP_GET, [] (AsyncWebServerRequest *request) {
    handle_press_mon();
    request->send(200, "text/plain", "OK");
  });

  // handler for getting monitor volume
  server.on("/get_mon", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_get_mon());
  });

  // handler for getting queue length
  server.on("/get_queue_len", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", handle_get_queue_len());
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
