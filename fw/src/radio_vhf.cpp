#include "globals.h"
#include "radio_vhf.h"
#include "audio.h"

#include <Arduino.h>
#include <HardwareSerial.h>

HardwareSerial VHFserial(1);

bool radio_vhf_response_success(String response);

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

    radio_vhf_set_freq(146580000);
}

bool radio_vhf_response_success(String response) {
    int16_t idx = response.indexOf(":");

    if(idx != -1 && idx + 1 < response.length()) {
        if(response.charAt(idx + 1) == '0')
            return true;
    }
    
    return false;
}

bool radio_vhf_set_freq(uint64_t new_freq) {
    Serial.println("New freq: ");
    Serial.println(new_freq);
    uint64_t MHz = new_freq / 1000000;
    uint64_t kHz = (new_freq - MHz) / 1000;
    String tmp = String(MHz, 3) + "." + String(kHz, 4);
    Serial.print("Formatted: ");
    Serial.println(tmp);

    // see: https://www.dorji.com/docs/data/DRA818V.pdf
    VHFserial.println("AT+DMOSETGROUP=0,146.5400,146.5400,0000,1,0000");
    delay(1000);
    Serial.println("VHF Response: ");
    String response = "";
    while(VHFserial.available() > 1) {
        char next_char = VHFserial.read();
        Serial.print(next_char);
        response += next_char;
    }
    Serial.println();
    Serial.println(radio_vhf_response_success(response));

    Serial.println();

    // TODO: something more intelligent than this
    return true;
}