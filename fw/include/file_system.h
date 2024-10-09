#pragma once
#include <Arduino.h>
#include "radio_hf.h"
#include "globals.h"

void fs_init();
void fs_start_browser();
String fs_load_setting(String file_name, String param_name);
bool fs_setting_exists(String file_name, String param_name);
void fs_load_bands(String file_name, radio_band_capability_t (&bands)[NUMBER_BANDS]);