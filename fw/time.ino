// TODO - rewrite this module using something besides the Time library that only considers seconds
void handle_time(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) {
  if(request_type == HTTP_PUT) {
    // look for required parameters in the message
    if(!request->hasParam("timeNow")) {
      Serial.println("[TIME] timeNow not received");
      request->send(400, "text/plain", "timeNow not found");
      return;
    }

    // hack: the library we're using only works in seconds, so cut off the last 3 digits
    String tmp = request->getParam("timeNow")->value();
    tmp = tmp.substring(0, tmp.length()-3);
    
    uint64_t time_now = tmp.toInt();
    Serial.print("[TIME] timeNow received: ");
    Serial.println(time_now);

    time_update(time_now);

    request->send(201, "text/plain", "OK");
  }
  if(request_type == HTTP_GET) {
    if(timeStatus() == timeNotSet) {
      request->send(404, "text/plain", String(0));
    }
    else {
      uint64_t time_ms = 1000 * (uint64_t) now();
      request->send(200, "text/plain", String(time_ms));
    }
  }
}

// updates time, including some rationality checks
// units are in SECONDS, not milliseconds.
void time_update(uint64_t new_time) {
  // TODO - some sanity checking on the received time

  Serial.print("[TIME] time error was: ");
  Serial.println((float) now() - (float) new_time);
  
  // update time
  setTime(new_time);
}

// loads time from the web, if possible
void update_time_from_web() {
  String time_server = load_json_config(preference_file, "time_server");
  String time_keyword = load_json_config(preference_file, "time_server_keyword");
  
  // attempt to get a text file from the web
  Serial.print("[TIME] Trying to connect to ");
  Serial.println(time_server);
  
  if (http.begin(client, time_server)) {
    uint16_t httpCode = http.GET();

    Serial.print("[TIME] HTTP Response: ");
    Serial.println(httpCode);

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = http.getString();

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);

        // print out string for debug
        serializeJsonPretty(doc, Serial);
        Serial.println();

        if(error) {
          Serial.println("parseObject() failed");
        }

        uint64_t epoch_sec = doc[time_keyword];
        Serial.print("[TIME] unix time: ");
        Serial.println(epoch_sec);

        // update time with parsed value
        time_update(epoch_sec);
      }
    }
  }
  else {
    Serial.println("[TIME] Unable to reach time server");
  }
}
