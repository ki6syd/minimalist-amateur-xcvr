// TODO: figure out this constant.
#define MORSE_SYMBOL_MS_WPM       1050
#define MORSE_DIT_LEN             1
#define MORSE_DAH_LEN             3
#define MORSE_INTERCHAR_LEN       3
#define MORSE_INTERWORD_LEN       7

void init_keyer() {
  keyer_speed = load_json_config(hw_config_file, "keyer_speed_default_wpm").toInt();

  // detect key type
//  if(digitalRead(D6) == LOW) {
  if(digitalRead(12) == LOW) {
    Serial.println("[KEYER] Straight key detected");
    key_type = KEY_STRAIGHT;
  }
  else {
    Serial.println("[KEYER] Paddle detected");
    key_type = KEY_PADDLE;
  }
}

void handle_keyer_queue() {
  // handle one character in the queue
  if(tx_queue.length() > 0) {   
    Serial.print("[DEQUEUE] ");
    Serial.println(tx_queue[0]);
    morse_letter(String(tx_queue[0]));
    tx_queue = tx_queue.substring(1);
  }
}

void dit() {
  dit_flag = false;
  
  key_on();
  my_delay(MORSE_SYMBOL_MS_WPM * MORSE_DIT_LEN / keyer_speed);

  key_off();
  my_delay(MORSE_SYMBOL_MS_WPM / keyer_speed);

  // start listening for key inputs again
  attach_paddle_isr(true, true);
}


void dah() {
  dah_flag = false;
  
  key_on();
  my_delay(MORSE_SYMBOL_MS_WPM * MORSE_DAH_LEN / keyer_speed);
  
  key_off();
  my_delay(MORSE_SYMBOL_MS_WPM  / keyer_speed);

  // start listening for key inputs again
  attach_paddle_isr(true, true);
}


// could accept a char, but leaving this as a String input to handle prosigns
void morse_letter(String letter) {
  if(letter == " " || isSpace(letter.charAt(0)) || letter == "_") {
    my_delay(MORSE_INTERWORD_LEN / keyer_speed);
  }
  
  if(letter == "a" || letter == "A") {
      dit();
      dah();
  }
  if(letter == "b" || letter == "B") {
      dah();
      dit();
      dit();
      dit();
  }
  if(letter == "c" || letter == "C") {
      dah();
      dit();
      dah();
      dit();
  }
  if(letter == "d" || letter == "D") {
      dah();
      dit();
      dit();
  }
  if(letter == "e" || letter == "E") {
      dit();
  }
  if(letter == "f" || letter == "F") {
      dit();
      dit();
      dah();
      dit();
  }
  if(letter == "g" || letter == "G") {
      dah();
      dah();
      dit();
  }
  if(letter == "h" || letter == "H") {
      dit();
      dit();
      dit();
      dit();
  }
  if(letter == "i" || letter == "I") {
      dit();
      dit();
  }
  if(letter == "j" || letter == "J") {
      dit();
      dah();
      dah();
      dah();
  }
  if(letter == "k" || letter == "K") {
      dah();
      dit();
      dah();
  }
  if(letter == "l" || letter == "L") {
      dit();
      dah();
      dit();
      dit();
  }
  if(letter == "m" || letter == "M") {
      dah();
      dah();
  }
  if(letter == "n" || letter == "N") {
      dah();
      dit();
  }
  if(letter == "o" || letter == "O") {
      dah();
      dah();
      dah();
  }
  if(letter == "p" || letter == "P") {
      dit();
      dah();
      dah();
      dit();
  }
  if(letter == "q" || letter == "Q") {
      dah();
      dah();
      dit();
      dah();
  }
  if(letter == "r" || letter == "PR") {
      dit();
      dah();
      dit();
  }
  if(letter == "s" || letter == "S") {
      dit();
      dit();
      dit();
  }
  if(letter == "t" || letter == "T") {
      dah();
  }
  if(letter == "u" || letter == "U") {
      dit();
      dit();
      dah();
  }
  if(letter == "v" || letter == "V") {
      dit();
      dit();
      dit();
      dah();
  }
  if(letter == "w" || letter == "W") {
      dit();
      dah();
      dah();
  }
  if(letter == "x" || letter == "X") {
      dah();
      dit();
      dit();
      dah();
  }
  if(letter == "y" || letter == "Y") {
      dah();
      dit();
      dah();
      dah();
  }
  if(letter == "z" || letter == "Z") {
      dah();
      dah();
      dit();
      dit();
  }
  
  if(letter == "0") {
      dah();
      dah();
      dah();
      dah();
      dah();
  }

  if(letter == "1") {
      dit();
      dah();
      dah();
      dah();
      dah();
  }

  if(letter == "2") {
      dit();
      dit();
      dah();
      dah();
      dah();
  }

  if(letter == "3") {
      dit();
      dit();
      dit();
      dah();
      dah();
  }

  if(letter == "4") {
      dit();
      dit();
      dit();
      dit();
      dah();
  }

  if(letter == "5") {
      dit();
      dit();
      dit();
      dit();
      dit();
  }

  if(letter == "6") {
      dah();
      dit();
      dit();
      dit();
      dit();
  }

  if(letter == "7") {
      dah();
      dah();
      dit();
      dit();
      dit();
  }

  if(letter == "8") {
      dah();
      dah();
      dah();
      dit();
      dit();
  }

  if(letter == "9") {
      dah();
      dah();
      dah();
      dah();
      dit();
  }

  if(letter == "/") {
      dah();
      dit();
      dit();
      dah();
      dit();
  }

  if(letter == "?") {
    dit();
    dit();
    dah();
    dah();
    dit();
    dit();
  }
  
  // intercharacter delay
  my_delay(MORSE_SYMBOL_MS_WPM * MORSE_INTERCHAR_LEN / keyer_speed);
}
