#include "globals.h"
#include "keyer.h"
#include "radio_hf.h"

#include <Arduino.h>

#define MORSE_SYMBOL_MS_WPM         1050
#define MORSE_DIT_LEN               1
#define MORSE_DAH_LEN               3
#define MORSE_INTERCHAR_LEN         3
#define MORSE_INTERWORD_LEN         7
#define MORSE_SPEED_MIN             5
#define MORSE_SPEED_MAX             30

uint16_t keyer_speed = 15;

void keyer_init() {
    // Todo: load keyer parameters from file system
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
//   dit_flag = false;
  
  radio_key_on();
  vTaskDelay((MORSE_SYMBOL_MS_WPM * MORSE_DIT_LEN / keyer_speed) / portTICK_PERIOD_MS);

  radio_key_off();
  vTaskDelay((MORSE_SYMBOL_MS_WPM  / keyer_speed) / portTICK_PERIOD_MS);

//   // start listening for key inputs again
//   if(key == KEY_PADDLE)
//     attach_paddle_isr(true, true);
//   else
//     attach_sk_isr(true);
}


void keyer_dah() {
//   dah_flag = false;
  
  radio_key_on();
  vTaskDelay((MORSE_SYMBOL_MS_WPM * MORSE_DAH_LEN / keyer_speed) / portTICK_PERIOD_MS);
  
  radio_key_off();
  vTaskDelay((MORSE_SYMBOL_MS_WPM  / keyer_speed) / portTICK_PERIOD_MS);

//   // start listening for key inputs again
//   if(key == KEY_PADDLE)
//     attach_paddle_isr(true, true);
//   else
//     attach_sk_isr(true);
}


// could accept a char, but leaving this as a String input to handle prosigns
void morse_letter(String letter) {
  if(letter == " " || isSpace(letter.charAt(0)) || letter == "_") {
    vTaskDelay((MORSE_INTERWORD_LEN  / keyer_speed) / portTICK_PERIOD_MS);
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
  vTaskDelay((MORSE_SYMBOL_MS_WPM * MORSE_INTERCHAR_LEN / keyer_speed) / portTICK_PERIOD_MS);
}