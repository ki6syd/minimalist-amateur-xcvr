// this module had problems when named "wifi" due to the <WiFi.h> library

#pragma once
#include <Arduino.h>
#include <WiFi.h>   // header file for Arduino library

void wifi_init();
void wifi_scan();