void handle_rx_bandwidth(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) {
  if(request_type == HTTP_PUT) {
    // look for required parameters in the message
    if(!request->hasParam("bw")) {
      Serial.println("[AUDIO] bw not received");
      request->send(400, "text/plain", "bw not found");
      return;
    }

    String bw = request->getParam("bw")->value();

    if(bw == "CW") {
      rx_bw = OUTPUT_SEL_CW;
    }
    else if(bw == "SSB"){
      rx_bw = OUTPUT_SEL_SSB;
    }
    else {
      request->send(400, "text/plain", "Requested bandwidth not possible");
    }

    Serial.print("[AUDIO] bw: ");
    Serial.println(rx_bw);

    // indicate that relays need an update
    flag_freq = true;

    request->send(200, "text/plain", "OK");
  }
  if(request_type == HTTP_GET) {
    String bw = "";
    if(rx_bw == OUTPUT_SEL_CW)
      bw = "CW";
    else if(rx_bw == OUTPUT_SEL_SSB)
      bw = "SSB";
    
    request->send(200, "text/plain", bw);
  }
}


void handle_volume(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) {
  if(request_type == HTTP_PUT) {
    // look for required parameters in the message
    if(!request->hasParam("audioLevel")) {
      Serial.println("[AUDIO] audioLevel not received");
      request->send(400, "text/plain", "audioLevel not found");
      return;
    }

    uint8_t new_vol = request->getParam("audioLevel")->value().toInt();

    if(new_vol < vol_min || new_vol > vol_max) {
      request->send(400, "text/plain", "Unable to set requested volume");
      return;
    }

    // update volume and flag that main loop needs to adjust hardware
    vol = new_vol;
    flag_vol = true;

    request->send(200, "text/plain", "OK");
  }
  if(request_type == HTTP_GET) {    
    request->send(200, "text/plain", String(vol));
  }
}


void handle_sidetone(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) {
  if(request_type == HTTP_PUT) {
    // look for required parameters in the message
    if(!request->hasParam("sidetoneLevelOffset")) {
      Serial.println("[AUDIO] sidetoneLevelOffset not received");
      request->send(400, "text/plain", "sidetoneLevelOffset not found");
      return;
    }

    int16_t new_sidetone_offset = request->getParam("sidetoneLevelOffset")->value().toInt();

    // TODO: bounds checking
    /*
    if(new_sidetone_offset < vol_min || new_vol > vol_max) {
      request->send(400, "text/plain", "Unable to set requested sidetone value");
      return;
    }*/

    mon_offset = new_sidetone_offset;

    request->send(200, "text/plain", "OK");
  }
  if(request_type == HTTP_GET) {    
    request->send(200, "text/plain", String(mon_offset));
  }
}
