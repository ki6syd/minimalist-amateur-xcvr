#pragma once
#include "digi_modes.h"

void ft8_init();
void ft8_send_msg(digi_msg_t *to_send);
void ft8_string_process(String &message_text, digi_msg_t *msg_struct);
void ft8_cancel();