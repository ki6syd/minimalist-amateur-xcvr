#include <Arduino.h>
#include "wifi_conn.h"

// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  pinMode(LED_GRN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  Serial.begin(460800);

  delay(2000);

  wifi_init();

}

void loop() {
  // put your main code here, to run repeatedly:

  Serial.println("test");

  digitalWrite(LED_GRN, HIGH);
  // wait for a second
  delay(1000);
  // turn the LED off by making the voltage LOW
  digitalWrite(LED_GRN, LOW);
   // wait for a second
  delay(1000);
}
