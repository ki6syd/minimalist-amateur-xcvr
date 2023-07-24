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

    request->send(200, "text/plain", "OK");
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
