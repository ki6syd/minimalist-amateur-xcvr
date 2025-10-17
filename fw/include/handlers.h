#pragma once
#include <ESPAsyncWebServer.h>

void handler_clocks_set(AsyncWebServerRequest *request);
void handler_phase_set(AsyncWebServerRequest *request);
void handler_input_voltage_get(AsyncWebServerRequest *request);
void handler_githash_get(AsyncWebServerRequest *request);
void handler_heap_get(AsyncWebServerRequest *request);
void handler_mac_get(AsyncWebServerRequest *request);
void handler_ip_get(AsyncWebServerRequest *request);
void handler_revision_get(AsyncWebServerRequest *request);
void handler_serial_get(AsyncWebServerRequest *request);
void handler_api_get(AsyncWebServerRequest *request);
void handler_debug_post(AsyncWebServerRequest *request);
void handler_clock_toggle(AsyncWebServerRequest *request);