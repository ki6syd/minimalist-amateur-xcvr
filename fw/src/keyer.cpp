#include "globals.h"
#include "keyer.h"
#include "radio.h"
#include "io.h"

#include <Arduino.h>

#define MORSE_SYMBOL_MS_WPM         1050
#define MORSE_DIT_LEN               1
#define MORSE_DAH_LEN               3
#define MORSE_INTERCHAR_LEN         3
#define MORSE_INTERWORD_LEN         7

uint16_t keyer_speed = 16;

void keyer_init() {
    // Todo: load keyer parameters (min/max speed) from file system
}

bool keyer_set_speed(uint16_t wpm) {
    if(wpm >= MORSE_SPEED_MIN && wpm <= MORSE_SPEED_MAX) {
        keyer_speed = wpm;
        return true;
    }
    return false;
}

uint16_t keyer_get_speed() {
    return keyer_speed;
}

void keyer_dit() {
    radio_key_on();
    vTaskDelay(pdMS_TO_TICKS(MORSE_SYMBOL_MS_WPM * MORSE_DIT_LEN / keyer_speed));

    radio_key_off();
    vTaskDelay(pdMS_TO_TICKS(MORSE_SYMBOL_MS_WPM  / keyer_speed));
}


void keyer_dah() {
    radio_key_on();
    vTaskDelay(pdMS_TO_TICKS(MORSE_SYMBOL_MS_WPM * MORSE_DAH_LEN / keyer_speed));

    radio_key_off();
    vTaskDelay(pdMS_TO_TICKS(MORSE_SYMBOL_MS_WPM  / keyer_speed));
}


void keyer_send_msg(digi_msg_t *to_send) {
    uint8_t i=0;
    while(to_send->buf[i] != '\0' && i < 255) {
        // TODO: don't continue sending if there was a key press or TBD semaphore set
        keyer_letter(String((char) to_send->buf[i++]));
    }
}

// could accept a char, but leaving this as a String input to handle prosigns
void keyer_letter(String letter) {
  if(letter == " " || isSpace(letter.charAt(0)) || letter == "_") {
    vTaskDelay(pdMS_TO_TICKS(MORSE_INTERWORD_LEN  / keyer_speed));
  }
  
  if(letter == "a" || letter == "A") {
      keyer_dit();
      keyer_dah();
  }
  if(letter == "b" || letter == "B") {
      keyer_dah();
      keyer_dit();
      keyer_dit();
      keyer_dit();
  }
  if(letter == "c" || letter == "C") {
      keyer_dah();
      keyer_dit();
      keyer_dah();
      keyer_dit();
  }
  if(letter == "d" || letter == "D") {
      keyer_dah();
      keyer_dit();
      keyer_dit();
  }
  if(letter == "e" || letter == "E") {
      keyer_dit();
  }
  if(letter == "f" || letter == "F") {
      keyer_dit();
      keyer_dit();
      keyer_dah();
      keyer_dit();
  }
  if(letter == "g" || letter == "G") {
      keyer_dah();
      keyer_dah();
      keyer_dit();
  }
  if(letter == "h" || letter == "H") {
      keyer_dit();
      keyer_dit();
      keyer_dit();
      keyer_dit();
  }
  if(letter == "i" || letter == "I") {
      keyer_dit();
      keyer_dit();
  }
  if(letter == "j" || letter == "J") {
      keyer_dit();
      keyer_dah();
      keyer_dah();
      keyer_dah();
  }
  if(letter == "k" || letter == "K") {
      keyer_dah();
      keyer_dit();
      keyer_dah();
  }
  if(letter == "l" || letter == "L") {
      keyer_dit();
      keyer_dah();
      keyer_dit();
      keyer_dit();
  }
  if(letter == "m" || letter == "M") {
      keyer_dah();
      keyer_dah();
  }
  if(letter == "n" || letter == "N") {
      keyer_dah();
      keyer_dit();
  }
  if(letter == "o" || letter == "O") {
      keyer_dah();
      keyer_dah();
      keyer_dah();
  }
  if(letter == "p" || letter == "P") {
      keyer_dit();
      keyer_dah();
      keyer_dah();
      keyer_dit();
  }
  if(letter == "q" || letter == "Q") {
      keyer_dah();
      keyer_dah();
      keyer_dit();
      keyer_dah();
  }
  if(letter == "r" || letter == "R") {
      keyer_dit();
      keyer_dah();
      keyer_dit();
  }
  if(letter == "s" || letter == "S") {
      keyer_dit();
      keyer_dit();
      keyer_dit();
  }
  if(letter == "t" || letter == "T") {
      keyer_dah();
  }
  if(letter == "u" || letter == "U") {
      keyer_dit();
      keyer_dit();
      keyer_dah();
  }
  if(letter == "v" || letter == "V") {
      keyer_dit();
      keyer_dit();
      keyer_dit();
      keyer_dah();
  }
  if(letter == "w" || letter == "W") {
      keyer_dit();
      keyer_dah();
      keyer_dah();
  }
  if(letter == "x" || letter == "X") {
      keyer_dah();
      keyer_dit();
      keyer_dit();
      keyer_dah();
  }
  if(letter == "y" || letter == "Y") {
      keyer_dah();
      keyer_dit();
      keyer_dah();
      keyer_dah();
  }
  if(letter == "z" || letter == "Z") {
      keyer_dah();
      keyer_dah();
      keyer_dit();
      keyer_dit();
  }
  
  if(letter == "0") {
      keyer_dah();
      keyer_dah();
      keyer_dah();
      keyer_dah();
      keyer_dah();
  }

  if(letter == "1") {
      keyer_dit();
      keyer_dah();
      keyer_dah();
      keyer_dah();
      keyer_dah();
  }

  if(letter == "2") {
      keyer_dit();
      keyer_dit();
      keyer_dah();
      keyer_dah();
      keyer_dah();
  }

  if(letter == "3") {
      keyer_dit();
      keyer_dit();
      keyer_dit();
      keyer_dah();
      keyer_dah();
  }

  if(letter == "4") {
      keyer_dit();
      keyer_dit();
      keyer_dit();
      keyer_dit();
      keyer_dah();
  }

  if(letter == "5") {
      keyer_dit();
      keyer_dit();
      keyer_dit();
      keyer_dit();
      keyer_dit();
  }

  if(letter == "6") {
      keyer_dah();
      keyer_dit();
      keyer_dit();
      keyer_dit();
      keyer_dit();
  }

  if(letter == "7") {
      keyer_dah();
      keyer_dah();
      keyer_dit();
      keyer_dit();
      keyer_dit();
  }

  if(letter == "8") {
      keyer_dah();
      keyer_dah();
      keyer_dah();
      keyer_dit();
      keyer_dit();
  }

  if(letter == "9") {
      keyer_dah();
      keyer_dah();
      keyer_dah();
      keyer_dah();
      keyer_dit();
  }

  if(letter == "/") {
      keyer_dah();
      keyer_dit();
      keyer_dit();
      keyer_dah();
      keyer_dit();
  }

  if(letter == "?") {
    keyer_dit();
    keyer_dit();
    keyer_dah();
    keyer_dah();
    keyer_dit();
    keyer_dit();
  }
  
  // intercharacter delay
  vTaskDelay(pdMS_TO_TICKS(MORSE_SYMBOL_MS_WPM * MORSE_INTERCHAR_LEN / keyer_speed));
}