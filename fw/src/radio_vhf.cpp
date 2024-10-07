#include "globals.h"
#include "radio_vhf.h"
#include "audio.h"

#include <Arduino.h>
#include <HardwareSerial.h>

#define NUM_PWR_CYCLE       5
#define NUM_RETRIES         20
#define DELAY_WAKEUP        1000
#define DELAY_NEXT_CMD      50

HardwareSerial VHFserial(1);
bool configured = false;

String vhf_command(String command);
bool vhf_response_success(String response);

void vhf_init() {
    pinMode(VHF_EN, OUTPUT);
    digitalWrite(VHF_EN, LOW);

    // RX pin needs too be low when powering on
    pinMode(VHF_RX_ESP_TX, OUTPUT);
    digitalWrite(VHF_RX_ESP_TX, LOW);

    pinMode(VHF_PTT, OUTPUT);
    digitalWrite(VHF_PTT, HIGH);  // RX mode, then delay before enabling - don't want to accidentally transmit
    vTaskDelay(pdMS_TO_TICKS(10));
    

    // attempt to handshake
    if(!vhf_handshake()) {
        vhf_powerdown();
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(DELAY_NEXT_CMD));

    // for debug: print out version
    vhf_get_version();
    vTaskDelay(pdMS_TO_TICKS(DELAY_NEXT_CMD));

    // attempt to set frequency
    if(!vhf_set_freq(146580000)) {
        vhf_powerdown();
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(DELAY_NEXT_CMD));

    // attempt to set volume
    // TODO: parametrize, get rid of magic number
    if(!vhf_set_volume(8)) {
        vhf_powerdown();
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(DELAY_NEXT_CMD));

    vTaskDelay(pdMS_TO_TICKS(1000));
    digitalWrite(VHF_PTT, LOW);
    digitalWrite(LED_RED, HIGH);
    vTaskDelay(pdMS_TO_TICKS(1000));
    digitalWrite(VHF_PTT, HIGH);

    vTaskDelay(pdMS_TO_TICKS(100));
    vhf_get_s_meter();

    // disable VHF now that config is complete
    vhf_powerdown();
}


String vhf_command(String command) {
    // empty buffer before reading, unclear why needed...
    while(VHFserial.available()) VHFserial.read();

    // send serial commands
    Serial.print("VHF Sending: ");
    Serial.println(command);
    VHFserial.print(command + "\r\n");   
    VHFserial.flush();

    vTaskDelay(pdMS_TO_TICKS(DELAY_NEXT_CMD));

    Serial.print("VHF Response: ");
    String response = "";
    while(VHFserial.available() > 1) {
        char next_char = VHFserial.read();
        response += next_char;
    }
    Serial.println(response);

    return response;
}

// verify that we got something like +DMOCONNECT:0, by looking for ":0"
// also make sure the response wasn't ridiculously long
bool vhf_response_success(String response) {
    int16_t idx = response.indexOf(":");

    // throw out long responses
    if(response.length() > 20)
        return false;

    if(idx != -1 && idx + 1 < response.length()) {
        if(response.charAt(idx + 1) == '0')
            return true;
    }
    return false;
}

bool vhf_handshake() {
    for(uint16_t i = 0; i < NUM_PWR_CYCLE; i++) {
        // wake up and wait before sending
        digitalWrite(VHF_EN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(DELAY_WAKEUP));

        // start serial communication
        VHFserial.begin(VHF_SERIAL_SPEED, SERIAL_8N1, VHF_TX_ESP_RX, VHF_RX_ESP_TX);
        // set VHFserial timeout (milliseconds)
        VHFserial.setTimeout(10);
        vTaskDelay(pdMS_TO_TICKS(10));

        for(uint16_t j = 0; j < NUM_RETRIES; j++) {
            // attempt to connect
            String response = vhf_command("AT+DMOCONNECT");
            if(vhf_response_success(response)) {
                vTaskDelay(pdMS_TO_TICKS(DELAY_NEXT_CMD));
                configured = true;
                return true;
            }            
            // if we got here: delay, then send again
            vTaskDelay(pdMS_TO_TICKS(DELAY_NEXT_CMD));
        }
        // cycle the enable line, try again
        digitalWrite(VHF_EN, LOW);
        vTaskDelay(pdMS_TO_TICKS(DELAY_WAKEUP));
    }
    // failure
    vhf_powerdown();
    return false;
}

void vhf_powerdown() {
    configured = false;
    digitalWrite(VHF_EN, LOW);
    VHFserial.end();
    pinMode(VHF_RX_ESP_TX, OUTPUT);
    digitalWrite(VHF_RX_ESP_TX, LOW);
}

bool vhf_is_configured() {
    return configured;
}

bool vhf_set_freq(uint64_t new_freq) {
    if(!configured)
        return false;

    vTaskDelay(pdMS_TO_TICKS(DELAY_NEXT_CMD));

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

    vTaskDelay(pdMS_TO_TICKS(DELAY_NEXT_CMD));

    String formatted_volume = String(((float) volume), 1);
    String prefix = "AT+DMOSETVOLUME=";
    String to_send = prefix + formatted_volume;

    String response = vhf_command(to_send);

    return vhf_response_success(response);
}

// TODO: return something
void vhf_get_version() {
    String version = "AT+VERSION";
    vhf_command(version);
}