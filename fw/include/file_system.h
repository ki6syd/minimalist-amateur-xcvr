#pragma once
#include <Arduino.h>
#include "radio_hf.h"
#include "globals.h"

void fs_init();
void fs_start_browser();
String fs_load_setting(String file_name, String param_name);
float fs_load_setting_float(String file_name, String param_name, float min_value, float max_value);
long fs_load_setting_long(String file_name, String param_name, long min_value, long max_value);
bool fs_setting_exists(String file_name, String param_name);
void fs_load_bands(String file_name, radio_band_capability_t (&bands)[NUMBER_BANDS]);