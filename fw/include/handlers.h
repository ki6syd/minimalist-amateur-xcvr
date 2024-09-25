#pragma once
#include <ESPAsyncWebServer.h>

void handler_ft8_post(AsyncWebServerRequest *request);
void handler_cw_post(AsyncWebServerRequest *request);
void handler_queue_get(AsyncWebServerRequest *request);
void handler_queue_delete(AsyncWebServerRequest *request);
void handler_time_get(AsyncWebServerRequest *request);
void handler_time_set(AsyncWebServerRequest *request);
void handler_frequency_get(AsyncWebServerRequest *request);
void handler_frequency_set(AsyncWebServerRequest *request);
void handler_volume_get(AsyncWebServerRequest *request);
void handler_volume_set(AsyncWebServerRequest *request);
void handler_sidetone_get(AsyncWebServerRequest *request);
void handler_sidetone_set(AsyncWebServerRequest *request);
void handler_bandwidth_get(AsyncWebServerRequest *request);
void handler_bandwidth_set(AsyncWebServerRequest *request);
void handler_keyer_speed_get(AsyncWebServerRequest *request);
void handler_keyer_speed_set(AsyncWebServerRequest *request);
void handler_input_voltage_get(AsyncWebServerRequest *request);
void handler_smeter_get(AsyncWebServerRequest *request);
void handler_githash_get(AsyncWebServerRequest *request);
void handler_heap_get(AsyncWebServerRequest *request);
void handler_mac_get(AsyncWebServerRequest *request);
void handler_ip_get(AsyncWebServerRequest *request);
void handler_revision_get(AsyncWebServerRequest *request);
void handler_serial_get(AsyncWebServerRequest *request);
void handler_api_get(AsyncWebServerRequest *request);