void handle_selftest(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) {
  print_request_details(request);
  
  if(request_type == HTTP_POST) {
    // look for required parameters in the message
    if(!request->hasParam("value")) {
      Serial.println("[DEBUG] value not sent");
      request->send(400, "text/plain", "value not sent");
      return;
    }

    // sweep the filter

    // package up data and return it

    String test_type = request->getParam("value")->value();

    // check available space
    // Serial.print("available size:");
    // Serial.println(ESP.getMaxFreeBlockSize() - 512);

    DynamicJsonDocument doc(8192);
    JsonArray data = doc.createNestedArray("data");
    for(uint16_t i=0; i < 200; i++) {
      JsonObject data_0 = data.createNestedObject();
      data_0["x"] = i;
      data_0["y"] = i * i;

      Serial.print(doc.memoryUsage());
      Serial.print("\t");
      Serial.println(doc.overflowed());

      my_delay(50);

      // watchdog won't get fed in this long function
      ESP.wdtFeed();
    }

    String response;
    serializeJson(doc, response);
    request->send(201, "application/json", response);
  }
}
