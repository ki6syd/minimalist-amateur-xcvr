// this module had problems when named "wifi" due to the <WiFi.h> library

#pragma once
#include <Arduino.h>

void wifi_init();
void wifi_scan();
String wifi_get_mac();