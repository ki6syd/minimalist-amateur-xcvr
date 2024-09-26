#include "globals.h"
#include "radio_vhf.h"
#include "audio.h"

#include <Arduino.h>
#include <HardwareSerial.h>

HardwareSerial VHFserial(1);

String radio_vhf_command(String command);
bool radio_vhf_handshake();
bool radio_vhf_set_volume(uint16_t volume);
bool radio_vhf_response_success(String response);

void radio_vhf_init() {
    pinMode(VHF_EN, OUTPUT);
    pinMode(VHF_PTT, OUTPUT);

    digitalWrite(VHF_PTT, HIGH);  // RX mode, then delay before enabling - don't want to accidentally transmit
    vTaskDelay(pdMS_TO_TICKS(10));
    digitalWrite(VHF_EN, HIGH);   // enabled
    
    VHFserial.begin(VHF_SERIAL_SPEED, SERIAL_8N1, VHF_TX_ESP_RX, VHF_RX_ESP_TX);

    // TODO: understand why this delay is needed, what the min value is, whether we can configure in parallel with other things, etc
    vTaskDelay(pdMS_TO_TICKS(2000));

    // set VHFserial timeout (milliseconds)
    VHFserial.setTimeout(100);

    // attempt to handshake
    if(!radio_vhf_handshake()) {
        // TODO: do something about it
    }

    // attempt to set frequency
    if(!radio_vhf_set_freq(146580000)) {
        // TODO: do something about it
    }

    // attempt to set volume
    if(!radio_vhf_set_volume(8)) {
        // TODO: do something about it
    }

    // disable VHF now that config is complete
    digitalWrite(VHF_EN, LOW);
}


String radio_vhf_command(String command) {
    // send serial commands
    Serial.print("VHF Sending: ");
    Serial.println(command);
    VHFserial.print(command + "\r\n");   

    vTaskDelay(100 / portTICK_PERIOD_MS); 


    Serial.print("VHF Response: ");
    String response = "";
    while(VHFserial.available() > 1) {
        char next_char = VHFserial.read();
        response += next_char;
    }
    Serial.println(response);

    return response;
}

bool radio_vhf_response_success(String response) {
    int16_t idx = response.indexOf(":");

    if(idx != -1 && idx + 1 < response.length()) {
        if(response.charAt(idx + 1) == '0')
            return true;
    }
    
    return false;
}

bool radio_vhf_handshake() {
    String response = radio_vhf_command("AT+DMOCONNECT");
    return radio_vhf_response_success(response);
}

bool radio_vhf_set_freq(uint64_t new_freq) {
    // construct the message. Example: "AT+DMOSETGROUP=0,146.5800,146.5800,0000,1,0000"
    String formatted_freq = String(((float) new_freq) / 1000000, 4);
    String prefix = "AT+DMOSETGROUP=0,";    // 12.5kHz wide
    String suffix = ",0000,1,0000";         // no CTCSS on TX or RX. Squelch level 1
    String to_send = prefix + formatted_freq + "," + formatted_freq + suffix;   // assume TX and RX on the same frequency

    String response = radio_vhf_command(to_send);

    return radio_vhf_response_success(response);
}

bool radio_vhf_set_volume(uint16_t volume) {
    // check bounds on volume request
    // TODO: parametrize this better
    if(volume < 0 || volume > 8)
        return false;

    String formatted_volume = String(((float) volume), 1);
    String prefix = "AT+DMOSETVOLUME=";
    String to_send = prefix + formatted_volume;

    String response = radio_vhf_command(to_send);

    return radio_vhf_response_success(response);
}