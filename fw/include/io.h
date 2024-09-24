#pragma once

typedef enum {
    BLINK_NORMAL,
    BLINK_STARTUP,
    BLINK_ERROR
} blink_type_t;

void io_init();
void io_set_blink_mode(blink_type_t mode);