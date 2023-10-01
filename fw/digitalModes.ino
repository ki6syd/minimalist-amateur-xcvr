// Mode defines
#define FT8_TONE_SPACING        625          // ~6.25 Hz
#define FT8_DELAY               159          // Delay value for FT8
#define FT8_MSG_LEN             13
#define FT8_TIME_CORR_MS        8

#define WSPR_TONE_SPACING       146.48       // ~1.46 Hz
#define WSPR_DELAY              683          // Delay value for WSPR
#define WSPR_TIME_CORR_MS       4



void handle_cw(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) { 
  if(request_type == HTTP_POST) {
    DigitalMessage tmp;
    tmp.type = MODE_CW;
    
    // look for required parameters in the message
    if(!request->hasParam("messageText")) {
      request->send(400, "text/plain", "messageText not found");
      return;
    }

    // reject the message if queue is already busy
    if(digital_queue.count() >= DIGITAL_QUEUE_LEN) {
      Serial.println("[CW] Queue is full");
      request->send(409, "text/plain", "CW queue is full.");
      return;
    }
  
    String message_text = request->getParam("messageText")->value();

    // set frequency based on command, if given
    if(request->hasParam("rfFrequency")) {
      // TODO: add better logic. Blindly follows command for now.
      tmp.freq = (uint64_t) request->getParam("rfFrequency")->value().toFloat();
    }
    else {
      tmp.freq = f_rf;
    }

    // package string into the buffer
    message_text.toCharArray((char *) tmp.buf, DIGITAL_QUEUE_LEN);
    // add null terminator
    tmp.buf[message_text.length()] = '\0';

    // add to queue;
    digital_queue.push(tmp);
    
    request->send(200, "text/plain", "OK");
  }
}

void handle_ft8(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) { 
  if(request_type == HTTP_POST) {
    // struct that we will enqueue at the end of the function
    DigitalMessage tmp;
    tmp.type = MODE_FT8;
    
    // look for required parameters in the message
    if(!request->hasParam("messageText")) {
      request->send(400, "text/plain", "messageText not found");
      return;
    }

    // reject the message if FT8 is already busy
    if(digital_queue.count() >= DIGITAL_QUEUE_LEN) {
      Serial.println("[FT8] Queue is full");
      request->send(409, "text/plain", "FT8 queue is full.");
      return;
    }
  
    String message_text = request->getParam("messageText")->value();
    
    // check message length
    if(message_text.length() != 13) {
      Serial.print("[FT8] Wrong message length: ");
      Serial.println(message_text.length());
      request->send(400, "text/plain", "messageText incorrect length");
      return;
    }
  
    // update time if we receive the timeNow parameter
    if(request->hasParam("timeNow")) {
      // TODO - delete the hack substring once the time module works in millseconds
      String time_string = request->getParam("timeNow")->value();
      time_string = time_string.substring(0, time_string.length()-3);
      time_update(time_string.toInt());
    }

    // update FT8 frequency if one is given
    if(request->hasParam("rfFrequency") && request->hasParam("audioFrequency")) {
      float rf_request = request->getParam("rfFrequency")->value().toFloat();
      float af_request = request->getParam("audioFrequency")->value().toFloat();
      // TODO - put some rationality checks on this.
      tmp.freq = (uint64_t) (rf_request + af_request);
    }
    else {
      // TODO - add more sophistocated logic than this default frequency
      tmp.freq = 14074000;
    }

    // encode the message. This runs in less than 1ms, probably OK to keep in the callback function
    // TODO - return an error code if encoding fails
    Serial.print("[FT8] Encoding: ");
    Serial.println(message_text);
    
    char message[] = {"SOTAMAT XXXXX"};
    message_text.toCharArray(message, FT8_MSG_LEN+1);
    Serial.println("[FT8] message char array: ");
    for(uint8_t i=0; i < FT8_MSG_LEN; i++) {
      Serial.print(message[i]);
      Serial.print(" ");
    }
    Serial.println();
    
    jtencode.ft8_encode(message, tmp.buf);

    Serial.println("[FT8] Calculated buffer: ");
    for(uint8_t i=0; i<FT8_SYMBOL_COUNT; i++) {
      Serial.print(tmp.buf[i]);
      Serial.print(" ");
    }
    Serial.println();
    
    digital_queue.push(tmp);

    request->send(200, "text/plain", "OK");
  }
}


void handle_wspr(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) { 
  // delete me
  print_request_details(request);
  
  if(request_type == HTTP_POST) {
    // struct that we will enqueue at the end of the function
    DigitalMessage tmp;
    tmp.type = MODE_WSPR;

    // reject the message if FT8 is already busy
    if(digital_queue.count() >= DIGITAL_QUEUE_LEN) {
      Serial.println("[WSPR] Queue is full");
      request->send(409, "text/plain", "WSPR queue is full.");
      return;
    }
    
    // look for required parameters in the message
    if(!request->hasParam("callSign")) {
      request->send(400, "text/plain", "callSign not found");
      return;
    }
    String call_sign = request->getParam("callSign")->value();

    // check message length
    if(call_sign.length() > 6) {
      Serial.println("[WSPR] Callsign too long");
      request->send(400, "text/plain", "Callsign incorrect length");
      return;
    }

    // todo: put defualt into JSON
    String grid_square = "CM87";
    if(request->hasParam("gridSquare")) {
      grid_square = request->getParam("gridSquare")->value();
    }

    // todo: put defualt into JSON
    String power = "35";
    if(request->hasParam("power")) {
      power = request->getParam("power")->value();
    }


    // update WSPR frequency if one is given
    if(request->hasParam("rfFrequency") && request->hasParam("audioFrequency")) {
      float rf_request = request->getParam("rfFrequency")->value().toFloat();
      float af_request = request->getParam("audioFrequency")->value().toFloat();
      // TODO - put some rationality checks on this.
      tmp.freq = (uint64_t) (rf_request + af_request);
    }
    else {
      // TODO - add more sophistocated logic than this default frequency
      tmp.freq = 14095600;
    }

    // create arrays for encoding, copy data into them
    // one extra character in length for null terminator
    char call[7];
    call_sign.toCharArray(call, 7);
    char loc[5];
    grid_square.toCharArray(loc, 5);
    uint8_t dbm = power.toInt();

    Serial.print("[WSPR] Call: ");
    Serial.println(call);
    Serial.print("[WSPR] Grid: ");
    Serial.println(loc);
    Serial.print("[WSPR] Power: ");
    Serial.println(dbm);
    jtencode.wspr_encode(call, loc, dbm, tmp.buf);

    Serial.println("[WSPR] Calculated buffer: ");
    for(uint8_t i=0; i<WSPR_SYMBOL_COUNT; i++) {
      Serial.print(tmp.buf[i]);
      Serial.print(",");
    }
    Serial.println();
    
    digital_queue.push(tmp);

    request->send(200, "text/plain", "OK");
  }
}

void handle_queue(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) {
  uint8_t total_queue_len = digital_queue.count();
  
  if(request_type == HTTP_GET) {
    request->send(200, "text/plain", String(total_queue_len));
  }
  else if(request_type == HTTP_DELETE) {
    if(total_queue_len == 0)
      request->send(404, "text/plain", "Nothing in digital modes queue.");

    // FT8 sending loop watches this variable. Will abort when it sees ft8_busy==false
    keyer_abort = true;
    empty_digital_queue();

    Serial.println("[QUEUE] Deleting all entries in queue");
    
    request->send(204, "text/plain", "Stopped sending digital modes.");
  }
}

// handles a single entry in the queue, then returns
void service_digital_queue() {
  // only proceed if we aren't already transmitting
  if(tx_rx_mode == MODE_TX)
    return;

  // save the original frequency so we can return to it after sending
  uint64_t f_rf_orig = f_rf;
  
  DigitalMessage to_send = digital_queue.peek();
  
  // Update radio frequency if needed
  if(f_rf != to_send.freq) {
    f_rf = to_send.freq;
    change_freq();
  }

  // send something depending on mode
  if(to_send.type == MODE_CW)
    send_cw_from_queue();
  else if(to_send.type == MODE_FT8)
    send_ft8_from_queue();
  else if(to_send.type == MODE_WSPR)
    send_wspr_from_queue();

  // todo: should this use some form of set_dial_freq()?
  f_rf = f_rf_orig;
  f_vfo = update_vfo(f_rf, f_bfo, f_audio);
  set_clocks(f_bfo, f_vfo, f_rf);
  
  // clear this flag
  keyer_abort = false;
}

// clears out the whole queue, of all data types
void empty_digital_queue() {
  while(digital_queue.count() > 0)
      digital_queue.pop();

  // set the flag in case queue is empty but something is sending currently
  keyer_abort = true;
}

// function returns when time rolls past the first 15 second interval since calling
void wait_ft8_window() {
  Serial.println("[FT8] Waiting for window to begin");
  Serial.print("[FT8] Current second() value is: ");
  Serial.println(second());

  // wait for a 15 second increment
  while(true) {
    if(second()%15 == 0)
      return;

    if(keyer_abort)
      return;

    my_delay(1);
  }

  Serial.println("[FT8] Window open");
  
  return;
}

// function returns when time rolls to the next WSPR window
void wait_wspr_window() {
  Serial.println("[WSPR] Waiting for window to begin");
  Serial.print("[WSPR] Current minute() value is: ");
  Serial.println(minute());
  Serial.print("[WSPR] Current second() value is: ");
  Serial.println(second());

  // wait for a 2 minute increment
  while(true) {
    if((minute()%2 == 0) && (second() == 0))
      return;

    if(keyer_abort)
      return;

    my_delay(1);
  }

  Serial.println("[WSPR] Window open");
  Serial.print("[WSPR] Current minute() value is: ");
  Serial.println(minute());
  Serial.print("[WSPR] Current second() value is: ");
  Serial.println(second());
  
  return;
}

void send_cw_from_queue() {
  DigitalMessage to_send = digital_queue.pop();
  
  Serial.print("[CW] Keying: ");
  
  uint8_t i=0;
  while(to_send.buf[i] != '\0' && i < 255) {
    if(keyer_abort)
      continue;

    Serial.println(String((char) to_send.buf[i]));
    morse_letter(String((char) to_send.buf[i++]));
  }
}

// TODO - get rid of magic delays
void send_ft8_from_queue() {
  Serial.println("[FT8] Starting to send FT8");

  DigitalMessage to_send = digital_queue.pop();

  // change to correct frequency before keying up
  f_rf = to_send.freq + to_send.buf[0];
  f_vfo = update_vfo(f_rf, f_bfo, f_audio);
  set_clocks(f_bfo, f_vfo, f_rf);

  // SSB for listening
  gpio_write(OUTPUT_BW_SEL, (output_state) OUTPUT_SEL_SSB);

  // wait for the next window to begin
  wait_ft8_window();

  // send FT8
  key_on();
  uint16_t start_time = millis();
  for(uint8_t i = 0; i < FT8_SYMBOL_COUNT; i++)
  {
    // abort sending if a callback has said that FT8 is no longer busy
    if(keyer_abort)
      continue;

    /*
    f_rf = to_send.freq + ((uint64_t) (to_send.buf[i] * FT8_TONE_SPACING));
    set_clocks(f_bfo, f_vfo, f_rf);
    */

    set_clk2_fine((to_send.freq * 100) + ((uint64_t) to_send.buf[i] * FT8_TONE_SPACING));

    // allow volume updates. convenience feature to avoid 15 seconds of painfully loud noise
    if (flag_vol) {
      flag_vol = false;
      update_volume(vol);
    }

    // todo: calibrate this properly, don't use a magic number
    my_delay(FT8_DELAY - FT8_TIME_CORR_MS);    
  }
  uint16_t end_time = millis();
  key_off();

  // restore BW
  gpio_write(OUTPUT_BW_SEL, (output_state) rx_bw);

  Serial.println("[FT8] Done sending FT8");
  Serial.print("[FT8] Took: ");
  Serial.print(end_time - start_time);
  Serial.println("ms");
}


void send_wspr_from_queue() {
  Serial.println("[WSPR] Starting to send WSPR");

  DigitalMessage to_send = digital_queue.pop();

  // change to correct frequency before keying up
  f_vfo = update_vfo(f_rf, f_bfo, f_audio);
  set_clocks(f_bfo, f_vfo, f_rf);
  set_clk2_fine((to_send.freq * 100) + ((uint64_t) to_send.buf[0] * 146));

  // SSB for listening
  gpio_write(OUTPUT_BW_SEL, (output_state) OUTPUT_SEL_SSB);

  // wait for the next window to begin
  wait_wspr_window();

  // send WSPR
  key_on();
  uint16_t start_time = millis();
  for(uint8_t i = 0; i < WSPR_SYMBOL_COUNT; i++)
  {
    // abort sending if a callback has said that FT8 is no longer busy
    if(keyer_abort)
      continue;
    
    set_clk2_fine((to_send.freq * 100) + ((uint64_t) to_send.buf[i] * WSPR_TONE_SPACING));

    // allow volume updates. convenience feature to avoid 120 seconds of painfully loud noise
    if (flag_vol) {
      flag_vol = false;
      update_volume(vol);
    }

    // todo: calibrate this properly, don't use a magic number
    my_delay(WSPR_DELAY - WSPR_TIME_CORR_MS);    
  }
  uint16_t end_time = millis();
  key_off();

  // restore BW
  gpio_write(OUTPUT_BW_SEL, (output_state) rx_bw);

  Serial.println("[WSPR] Done sending WSPR");
  Serial.print("[WSPR] Took: ");
  Serial.print(end_time - start_time);
  Serial.println("ms");
}
