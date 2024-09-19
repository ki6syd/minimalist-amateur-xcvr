#include "handlers.h"
#include "globals.h"

#include <ESPAsyncWebServer.h>
#include <Arduino.h>

void handler_frequency_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(14000));
}

void handler_volume_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(90));
}