#include "handlers.h"
#include "globals.h"
#include "radio_hf.h"
#include "audio.h"
#include "keyer.h"
#include "wifi_conn.h"

#include <ESPAsyncWebServer.h>
#include <Arduino.h>

// returns false if the request does not have the required parameter, and sends error code
bool handler_require_param(AsyncWebServerRequest *request, String param_name) {
    if(!request->hasParam(param_name)) {
        request->send(400, "text/plain", "Required parameter " + param_name + " was not sent");
        return false;
    }
    return true;
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
        request->send(400, "text/plain", "Sidetone level out of range");
    }
}

void handler_mac_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", wifi_get_mac());
}