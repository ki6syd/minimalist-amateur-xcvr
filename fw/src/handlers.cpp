#include "handlers.h"
#include "globals.h"
#include "radio.h"
#include "audio.h"
#include "keyer.h"
#include "power.h"
#include "wifi_conn.h"
#include "time_keeping.h"
#include "file_system.h"
#include "digi_modes.h"
#include "ft8.h"

#include <ESPAsyncWebServer.h>
#include <Arduino.h>

// returns false if the request does not have the required parameter, and sends error code
bool handler_require_param(AsyncWebServerRequest *request, String param_name) {
    if(!request->hasParam(param_name)) {
        request->send(400, "text/plain", "Required parameter \"" + param_name + "\" was not sent");
        return false;
    }
    return true;
}

void handler_ft8_post(AsyncWebServerRequest *request) {
    if(!handler_require_param(request, "messageText"))
        return;

    digi_msg_t tmp;
    tmp.type = DIGI_MODE_FT8;
    tmp.ignore_time = false;

    String message_text = request->getParam("messageText")->value();

    // update FT8 frequency if one is given
    if(request->hasParam("rfFrequency") && request->hasParam("audioFrequency")) {
        float rf_request = request->getParam("rfFrequency")->value().toFloat();
        float af_request = request->getParam("audioFrequency")->value().toFloat();
        tmp.freq = (uint64_t) (rf_request + af_request);

        // check that frequency is valid, abort early if not
        if(!radio_freq_valid(tmp.freq)) {
            request->send(400, "text/plain", "Radio does not support this frequency");
            return;
        }
    }
    else {
        tmp.freq = radio_get_dial_freq();
    }

    if(request->hasParam("ignoreTime") && request->getParam("ignoreTime")->value() == "true")
        tmp.ignore_time = true;

    // update time if parameter exists
    if(request->hasParam("timeNow")) {
        // TODO - delete the hack substring once the time module works in millseconds
        String time_string = request->getParam("timeNow")->value();
        time_string = time_string.substring(0, time_string.length()-3);
        time_update(time_string.toInt());
    }
    
    // turn the string into an encoded message, try adding to queue
    ft8_string_process(message_text, &tmp);
    if(digi_mode_enqueue(&tmp))
        request->send(201, "text/plain", "OK");
    else
        request->send(400, "text/plain", "Unable to add to queue");
}

void handler_cw_post(AsyncWebServerRequest *request) {
    if(!handler_require_param(request, "messageText"))
        return;

    digi_msg_t tmp;
    tmp.type = DIGI_MODE_CW;
    tmp.ignore_time = true;

    String message_text = request->getParam("messageText")->value();

    // update frequency if one is given
    if(request->hasParam("rfFrequency")) {
        uint64_t rf_request = request->getParam("rfFrequency")->value().toInt();
        tmp.freq = rf_request;

        // check that frequency is valid, abort early if not
        if(!radio_freq_valid(tmp.freq)) {
            request->send(400, "text/plain", "Radio does not support this frequency");
            return;
        }
    }
    else {
        tmp.freq = 0;
    }

    // package string into the buffer
    message_text.toCharArray((char *) tmp.buf, 255);
    // add null terminator
    tmp.buf[message_text.length()] = '\0';
    // add to queue, error code if it didn't add successfully
    if(digi_mode_enqueue(&tmp))
        request->send(201, "text/plain", "OK");
    else
        request->send(400, "text/plain", "Unable to add to queue");
}

void handler_queue_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(digi_mode_queue_size()));
}

void handler_queue_delete(AsyncWebServerRequest *request) {
    if(digi_mode_queue_size() > 0) {
        digi_mode_queue_clear();
        request->send(204, "text/plain", "Queue cleared");
    }
    else
        request->send(404, "text/plain", "Unable to clear queue, might have been empty");
}


void handler_time_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(time_ms()));
}

void handler_time_set(AsyncWebServerRequest *request) {
    if(!handler_require_param(request, "timeNow"))
        return;

    Serial.print("New time: ");
    Serial.println(request->getParam("timeNow")->value());

    // TODO - delete the hack substring once the time module works in millseconds
    String time_string = request->getParam("timeNow")->value();
    time_string = time_string.substring(0, time_string.length()-3);
    uint64_t new_time = time_string.toInt();
    if(time_update(new_time))
        request->send(201, "text/plain", "OK");
    else {
        request->send(400, "text/plain", "Unable to update time");
    }
}

void handler_frequency_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(radio_get_dial_freq()));
}

void handler_frequency_set(AsyncWebServerRequest *request) {
    if(!handler_require_param(request, "frequency"))
        return;

    uint64_t freq_request = request->getParam("frequency")->value().toInt();
    if(radio_set_dial_freq(freq_request))
        request->send(201, "text/plain", "OK");
    else {
        request->send(400, "text/plain", "Frequency out of range");
    }
}

void handler_volume_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(audio_get_volume()));
}

void handler_volume_set(AsyncWebServerRequest *request) {
    if(!handler_require_param(request, "audioLevel"))
        return;

    float vol_request = request->getParam("audioLevel")->value().toFloat();
    if(audio_set_volume(vol_request))
        request->send(201, "text/plain", "OK");
    else {
        request->send(400, "text/plain", "Volume out of range");
    }
}

void handler_sidetone_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(audio_get_sidetone_volume()));
}

void handler_sidetone_set(AsyncWebServerRequest *request) {
    if(!handler_require_param(request, "sidetoneLevelOffset"))
        return;

    float vol_request = request->getParam("sidetoneLevelOffset")->value().toFloat();
    if(audio_set_sidetone_volume(vol_request))
        request->send(201, "text/plain", "OK");
    else {
        request->send(400, "text/plain", "Sidetone level out of range");
    }
}

void handler_bandwidth_get(AsyncWebServerRequest *request) {
    String ret_val = "";
    switch(audio_get_filt()) {
        case AUDIO_FILT_CW:
            ret_val = "CW";
            break;
        case AUDIO_FILT_SSB:
            ret_val = "SSB";
            break;
        default:
            ret_val = "UNKNOWN";
    }
    request->send(200, "text/plain", ret_val);
}

void handler_bandwidth_set(AsyncWebServerRequest *request) {
    if(!handler_require_param(request, "bw"))
        return;

    String bw = request->getParam("bw")->value();

    if(bw == "CW")
        audio_set_filt(AUDIO_FILT_CW);
    else if(bw == "SSB")
        audio_set_filt(AUDIO_FILT_SSB);
    else {
        request->send(400, "text/plain", "Unknown bandwidth requested");
        return;
    }
    request->send(201, "text/plain", "OK");
}

void handler_keyer_speed_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(keyer_get_speed()));
}

void handler_keyer_speed_set(AsyncWebServerRequest *request) {
    if(!handler_require_param(request, "speed"))
        return;

    uint16_t speed_request = request->getParam("speed")->value().toInt();
    if(keyer_set_speed(speed_request))
        request->send(201, "text/plain", "OK");
    else {
        request->send(400, "text/plain", "Keyer speed out of range");
    }
}

void handler_input_voltage_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(power_get_input_volt()));
}

void handler_smeter_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(radio_get_s_meter()));
}

void handler_mac_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", wifi_get_mac());
}

void handler_githash_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", AUTO_VERSION);
}

void handler_heap_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(xPortGetFreeHeapSize()));
}

void handler_ip_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", wifi_get_ip());
}

// TODO: terrible idea to access the file system in this handler...
void handler_revision_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", fs_load_setting(HARDWARE_FILE, "hardware_rev"));
}

void handler_serial_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", fs_load_setting(HARDWARE_FILE, "serial_number"));
}

void handler_api_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", API_IMPLEMENTED);
}

void handler_debug_post(AsyncWebServerRequest *request) {
    if(!handler_require_param(request, "command"))
        return;

    uint64_t command_num = request->getParam("command")->value().toInt();

    // toggles the TX clock of the si5351
    if(command_num == DEBUG_CMD_TXCLK) {
        if(!handler_require_param(request, "value"))
            return;
        
        String value = request->getParam("value")->value();
        bool on_off;

        if(strcmp(value.c_str(), "on") == 0)
            on_off = true;
        else if(strcmp(value.c_str(), "off") == 0) 
            on_off = false;
        else {
            request->send(400, "text/plain", "Unknown value requested");
            return;
        }
        radio_debug(DEBUG_CMD_TXCLK, &on_off);
    }
    else if(command_num == DEBUG_CMD_REBOOT) {
        esp_restart();
    }
    else if(command_num == DEBUG_CMD_CAL_XTAL || command_num == DEBUG_CMD_CAL_IF || command_num == DEBUG_CMD_CAL_BPF || command_num == DEBUG_STOP_CLOCKS) {
        radio_debug((debug_action_t) command_num, nullptr);
    }
    else if(command_num == DEBUG_CMD_MAX_VOL) {
        audio_debug((debug_action_t) command_num);
    }
    else if(command_num == DEBUG_CMD_SPOT) {
         if(!handler_require_param(request, "value"))
            return;

        String value = request->getParam("value")->value();
        if(strcmp(value.c_str(), "on") == 0)
            audio_en_sidetone(true);
        else if(strcmp(value.c_str(), "off") == 0) 
            audio_en_sidetone(false);
        else {
            request->send(400, "text/plain", "Unknown value requested");
            return;
        }
    }
    request->send(201, "text/plain", "OK");
}