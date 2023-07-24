// Mode defines
#define FT8_TONE_SPACING        625          // ~6.25 Hz
#define FT8_DELAY               159          // Delay value for FT8
#define FT8_DEFAULT_FREQ        14075000UL
#define FT8_MSG_LEN             13
#define FT8_WINDOW_START        15           // FT8 begins at 15 second windows


void handle_ft8(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) { 
  if(request_type == HTTP_POST) {
    // look for required parameters in the message
    if(!request->hasParam("messageText")) {
      request->send(400, "text/plain", "messageText not found");
      return;
    }

    // reject the message if FT8 is already busy
    if(ft8_busy) {
      Serial.println("[FT8] Queue Busy");
      request->send(409, "text/plain", "FT8 queue is full.");
      return;
    }
  
    String message_text = request->getParam("messageText")->value();
    
    // check message length
    if(message_text.length() != 13) {
      Serial.println("[FT8] Wrong message length");
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
      ft8_freq = (uint64_t) (rf_request + af_request);
    }
    else {
      // TODO - add more sophistocated logic than this default frequency
      ft8_freq = 14074000;
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
    
    memset(ft8_buffer, 0, 255);
    jtencode.ft8_encode(message, ft8_buffer);

    Serial.println("[FT8] Calculated buffer: ");
    for(uint8_t i=0; i<255; i++) {
      Serial.print(ft8_buffer[i]);
      Serial.print(" ");
    }
    Serial.println();
  
    // mark tht FT8 is busy so we don't accept more messages
    // TODO - replace available/busy with a queue
    ft8_busy = true;
    // tell main loop that there is something to send
    flag_ft8 = true;

    request->send(200, "text/plain", "OK");
  }
  else if(request_type == HTTP_GET) {
    
  }
  if(request_type == HTTP_DELETE) {
    if(!ft8_busy)
      request->send(404, "text/plain", "Not sending FT8.");

    // FT8 sending loop watches this variable. Will abort when it sees ft8_busy==false
    ft8_busy = false;
    request->send(204, "text/plain", "Stopped sending FT8.");
  }
  
  
}

/*
 * Delete me! Part of older interface
 * 
void handle_sotamat(String call, String suffix) {

  Serial.print("[FT8] Call:");
  Serial.print(call);
  Serial.print(" + suffix: ");
  Serial.println(suffix);
  
  // TODO - some enforcement on call and suffix length. plenty of vulnerability here, right now.
  
  // use this as a starting point
  char message[] = {"SOTAMAT XXXXX"};
  
  // copy in suffix
  for(uint8_t i=0; i < suffix.length(); i++)
    message[FT8_MSG_LEN - i - 1] = suffix.charAt(suffix.length()-i-1);

  // copy in slash (/)
  uint8_t slash_start = FT8_MSG_LEN - suffix.length() - 1;
  message[slash_start] = '/';

  // copy in suffix
  for(uint8_t i=0; i < call.length(); i++)
    message[FT8_MSG_LEN - suffix.length() - i - 2] = call.charAt(call.length()-i-1);

  // copy in space
  uint8_t space_start = FT8_MSG_LEN - suffix.length() - call.length() - 2;
  message[space_start] = ' ';
  
  memset(ft8_buffer, 0, 255);
  jtencode.ft8_encode(message, ft8_buffer);

  Serial.println("[FT8] Calculated buffer: ");
  for(uint8_t i=0; i<79; i++) {
    Serial.print(ft8_buffer[i]);
    Serial.print(" ");
  }
  Serial.println();

  // set a flag indicating there is something in the queue
  // TODO - need to set up a queue. And, should not be doing the encoding in the handler. Left it here for proof of concept.
  flag_ft8 = true;
}
*/

// function returns when time rolls past the first 15 second interval since calling
void wait_ft8_window() {
  Serial.println("[FT8] Waiting for window to begin");
  Serial.print("[FT8] Current second() value is: ");
  Serial.println(second());
  
  // this assumes now() returns integer numbers of seconds
  while(second() % 15 != 0) {
    my_delay(50);
  }

  Serial.println("[FT8] Window open");
  
  return;
}

// TODO - this is a placeholder, should accept some sort of object out of a queue
// TODO - get rid of magic delays
void send_ft8() {
  Serial.println("[FT8] Starting to send FT8");

  // store the previous rf frequency
  uint64_t f_rf_orig = f_rf;

  // change to correct frequency before keying up
  f_rf = ft8_freq + ft8_buffer[0];
  set_clocks(f_bfo, f_vfo, f_rf);

  // wait for the next window to begin
  wait_ft8_window();

  // send FT8
  key_on();
  for(uint8_t i = 0; i < 79; i++)
  {
    // abort sending if a callback has said that FT8 is no longer busy
    if(!ft8_busy)
      continue;
    
    f_rf = ft8_freq + ((uint64_t) (ft8_buffer[i] * 6.25));
    set_clocks(f_bfo, f_vfo, f_rf);

    my_delay(145);    
  }
  key_off();

  // return to original frequency before exiting
  f_rf = f_rf_orig;
  f_vfo = update_vfo(f_rf, f_bfo, f_audio);
  set_clocks(f_bfo, f_vfo, f_rf);

  // indicate availability for more FT8 messages, and clear flag
  ft8_busy = false;
  flag_ft8 = false;

  Serial.println("[FT8] Done sending FT8");
}
