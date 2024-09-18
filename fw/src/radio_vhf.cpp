#include "globals.h"
#include "radio_vhf.h"
#include "audio.h"

#include <Arduino.h>

HardwareSerial VHFserial(1);

void radio_vhf_init() {
    pinMode(VHF_EN, OUTPUT);
    pinMode(VHF_PTT, OUTPUT);

    // digitalWrite(VHF_EN, HIGH);   // enabled
    digitalWrite(VHF_EN, LOW);
    digitalWrite(VHF_PTT, HIGH);  // RX

    VHFserial.begin(VHF_SERIAL_SPEED, SERIAL_8N1, VHF_TX_ESP_RX, VHF_RX_ESP_TX);

    // set VHFserial timeout (milliseconds)
    VHFserial.setTimeout(100);

    // send serial commands
    Serial.println("VHF connecting...");
    VHFserial.println("AT+DMOCONNECT");

    delay(1000);

    // todo: parametrize length
    char buf[64];
    int read_len = VHFserial.readBytes(buf, 64);

    Serial.println("VHF Response: ");
    for(int i=0; i < read_len; i++) {
    Serial.print(buf[i]);
    }
    // todo: parse this response and make sure it's good.

    // set frequency
    // see: https://www.dorji.com/docs/data/DRA818V.pdf
    VHFserial.println("AT+DMOSETGROUP=0,146.5400,146.5400,0000,1,0000");
    delay(1000);
    Serial.println("VHF Response: ");
    while(VHFserial.available() > 1) {
    Serial.print((char) VHFserial.read());
    }

    Serial.println();
}