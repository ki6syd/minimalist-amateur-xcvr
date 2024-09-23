#include "file_system.h"
#include "globals.h"

#include <Arduino.h>
#include "LittleFS.h"
#include <ArduinoJSON.h>
#include <ESPxWebFlMgr.h>

TaskHandle_t xFileSystemTask;

ESPxWebFlMgr filemgr(8080);

void fs_task(void *pvParameter);

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

void fs_start_browser() {
    xTaskCreatePinnedToCore(
        fs_task,
        "File System Task",
        4096,
        NULL,
        TASK_PRIORITY_FS, // priority
        &xFileSystemTask,
        TASK_CORE_FS // core
    );

    
}

void fs_task(void *pvParameter) {
    filemgr.begin();

    while(1) {
        filemgr.handleClient();
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}