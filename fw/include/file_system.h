#pragma once
#include <Arduino.h>

void fs_init();
void fs_start_browser();
String fs_load_setting(String file_name, String param_name);