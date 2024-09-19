#include "file_system.h"

#include <Arduino.h>
#include "LittleFS.h"
#include <ArduinoJSON.h>

void fs_init() {
    if (!LittleFS.begin()) {
        Serial.println("An error has occurred while mounting LittleFS");
    }
    Serial.println("LittleFS mounted successfully");

    // test read
    File file = LittleFS.open("/test.txt");
    
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }
    
    Serial.println("File Content:");
    while(file.available()){
        Serial.write(file.read());
    }
    Serial.println();
    file.close();
}