#pragma once
#include <Arduino.h>

typedef enum {
    DIGI_MODE_CW,
    DIGI_MODE_FT8
} digi_type_t;

typedef struct {
    digi_type_t type;
    bool ignore_time;
    uint64_t freq;
    uint8_t buf[255];
} digi_msg_t;

void digi_mode_init();
bool digi_mode_enqueue(digi_msg_t *new_msg);
void digi_mode_print(const digi_msg_t* to_print);