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
  

  uint8_t tx_buffer[255];
  memset(tx_buffer, 0, 255);
  jtencode.ft8_encode(message, tx_buffer);

  Serial.println("[FT8] Calculated buffer: ");

  for(uint8_t i=0; i<255; i++) {
    Serial.print(tx_buffer[i]);
    Serial.print(" ");
  }
  Serial.println();
}
