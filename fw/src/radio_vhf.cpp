#include "globals.h"
#include "radio_vhf.h"
#include "audio.h"

#include <Arduino.h>
#include <HardwareSerial.h>

HardwareSerial VHFserial(1);
bool configured = false;

String vhf_command(String command);
void vhf_complete_config(bool pass);
bool vhf_response_success(String response);

void vhf_init() {
    pinMode(VHF_EN, OUTPUT);
    pinMode(VHF_PTT, OUTPUT);

    digitalWrite(VHF_PTT, HIGH);  // RX mode, then delay before enabling - don't want to accidentally transmit
    vTaskDelay(pdMS_TO_TICKS(10));
    
    
    VHFserial.begin(VHF_SERIAL_SPEED, SERIAL_8N1, VHF_TX_ESP_RX, VHF_RX_ESP_TX);
    // set VHFserial timeout (milliseconds)
    VHFserial.setTimeout(100);
    vTaskDelay(pdMS_TO_TICKS(10));

    for(uint16_t i = 0; i < 2; i++) {   
        digitalWrite(VHF_EN, HIGH);   // enabled     
        // TODO: understand why this delay is needed, what the min value is, whether we can configure in parallel with other things, etc
        vTaskDelay(pdMS_TO_TICKS(500));

        // attempt to handshake
        if(!vhf_handshake()) {
            vhf_complete_config(false);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(100));

        // attempt to set frequency
        if(!vhf_set_freq(146580000)) {
            vhf_complete_config(false);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(100));

        // attempt to set volume
        // TODO: parametrize, get rid of magic number
        if(!vhf_set_volume(8)) {
            vhf_complete_config(false);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(100));

        vTaskDelay(pdMS_TO_TICKS(1000));
        Serial.println("PTT");
        digitalWrite(VHF_PTT, LOW);
        digitalWrite(LED_RED, HIGH);
        vTaskDelay(pdMS_TO_TICKS(1000));
        digitalWrite(VHF_PTT, HIGH);

        vTaskDelay(pdMS_TO_TICKS(100));
        vhf_get_s_meter();

        // disable VHF now that config is complete
        digitalWrite(VHF_EN, LOW);
        vTaskDelay(pdMS_TO_TICKS(100));
    }


    configured = true;
}

void vhf_complete_config(bool pass) {
    if(pass)
        return;
    
    Serial.println("VHF module could not be configured");
    digitalWrite(VHF_EN, LOW);
    configured = false;
}


String vhf_command(String command) {
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

bool vhf_response_success(String response) {
    int16_t idx = response.indexOf(":");

    if(idx != -1 && idx + 1 < response.length()) {
        if(response.charAt(idx + 1) == '0')
            return true;
    }
    
    return false;
}

bool vhf_handshake() {
    for(uint16_t i = 0; i < 5; i++) {
        String response = vhf_command("AT+DMOCONNECT");
        if(vhf_response_success(response))
            return true;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
        
    return false;
}

bool vhf_set_freq(uint64_t new_freq) {
    // construct the message. Example: "AT+DMOSETGROUP=0,146.5800,146.5800,0000,1,0000"
    String formatted_freq = String(((float) new_freq) / 1000000, 4);
    String prefix = "AT+DMOSETGROUP=0,";    // 12.5kHz wide
    String suffix = ",0000,1,0000";         // no CTCSS on TX or RX. Squelch level 1
    String to_send = prefix + formatted_freq + "," + formatted_freq + suffix;   // assume TX and RX on the same frequency

    String response = vhf_command(to_send);

    return vhf_response_success(response);
}

float vhf_get_s_meter() {
    String to_send = "RSSI?";
    
    String response = vhf_command(to_send);

    return 0.0;
}

bool vhf_set_volume(uint16_t volume) {
    // check bounds on volume request
    // TODO: parametrize this better
    if(volume < 0 || volume > 8)
        return false;

    String formatted_volume = String(((float) volume), 1);
    String prefix = "AT+DMOSETVOLUME=";
    String to_send = prefix + formatted_volume;

    String response = vhf_command(to_send);

    return vhf_response_success(response);
}