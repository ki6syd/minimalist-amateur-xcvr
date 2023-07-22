// Mode defines
#define FT8_TONE_SPACING        625          // ~6.25 Hz

#define FT8_DELAY               159          // Delay value for FT8

#define FT8_DEFAULT_FREQ        14075000UL

#define FT8_MSG_LEN             13


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

  for(uint8_t i=0; i<255; i++) {
    Serial.print(ft8_buffer[i]);
    Serial.print(" ");
  }
  Serial.println();

  // set a flag indicating there is something in the queue
  // TODO - need to set up a queue. And, should not be doing the encoding in the handler. Left it here for proof of concept.
  flag_ft8 = true;
}

// TODO - this is a placeholder, should accept some sort of object out of a queue
void send_ft8() {
  uint64_t f_rf_orig = f_rf;

  // loop over frequencies
  for(uint64_t f_tx = 14074000; f_tx < 14075001; f_tx += 250) {
    key_on();

    // send symbols
    for(uint8_t i = 0; i < 79; i++)
    {
      f_rf = f_tx + ((uint64_t) (ft8_buffer[i] * 6.25));
      set_clocks(f_bfo, f_vfo, f_rf);

      my_delay(145);
    }

    key_off();

    my_delay(2350);
  }

  // return to original frequency
  f_rf = f_rf_orig;
  f_vfo = update_vfo(f_rf, f_bfo, f_audio);
  set_clocks(f_bfo, f_vfo, f_rf);
}
