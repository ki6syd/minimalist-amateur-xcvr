#include "globals.h"
#include "time_keeping.h"
#include <TimeLib.h>
#include <Arduino.h>

#define FT8_START_TIME          15          // messages start at 15s increments

void time_init() {

}

bool time_update(uint64_t current_time) {
    Serial.print("[TIME] time error was: ");
    Serial.println((float) now() - (float) current_time);

    setTime(current_time);

    //TODO: do something smarter than returning true all the time
    return true;
}

uint64_t time_ms() {
    return 1000 * (uint64_t) now();
}

void time_delay_ft8() {
    Serial.println("[FT8] Waiting for window to begin");
    Serial.print("[FT8] Current second() value is: ");
    Serial.println(second());

    // wait for a 15 second increment by delaying 100ms at a time
    while(true) {
    if(second() % FT8_START_TIME == 0)
        return;

    vTaskDelay(pdMS_TO_TICKS(100));
    }

    Serial.println("[FT8] Window open");

    return;
}