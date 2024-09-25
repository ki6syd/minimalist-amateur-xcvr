#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

void radio_vhf_init();
bool radio_vhf_set_freq(uint64_t freq);