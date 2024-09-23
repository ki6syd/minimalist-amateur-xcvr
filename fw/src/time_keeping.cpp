#include "globals.h"
#include "time_keeping.h"
#include <TimeLib.h>
#include <Arduino.h>

void time_init() {

}

bool time_update(uint64_t current_time) {
    Serial.print("[TIME] time error was: ");
    Serial.println((float) now() - (float) current_time);

    setTime(current_time);

    //TODO: do something smarter than returning true
    return true;
}

uint64_t time_ms() {
    return 1000 * (uint64_t) now();
}