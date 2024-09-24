#include "file_system.h"
#include "globals.h"

#include <Arduino.h>
#include "LittleFS.h"
#include <ArduinoJSON.h>
#include <ESPxWebFlMgr.h>

TaskHandle_t xFileSystemTask;

ESPxWebFlMgr filemgr(8080);

bool fs_mounted = false;

void fs_task(void *pvParameter);

// TODO: have fs_init() load everything into memory
// then lookups don't need to access the file system repeatedly, and less thread-safety issue accessing file system
// maybe useful library for this: https://github.com/arkhipenko/Dictionary
void fs_init() {
    if (!LittleFS.begin()) {
        Serial.println("An error has occurred while mounting LittleFS");
        fs_mounted = false;
    }
    else {
        Serial.println("LittleFS mounted successfully");
        fs_mounted = true;
    }
    
    /*

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

    */
}

String fs_load_setting(String file_name, String param_name) {
    // check if file system is mounted yet
    if(!fs_mounted) {
        Serial.println("Couldn't read, file system not properly mounted.");
        return "";
    }

    // attempt to read file
    File file = LittleFS.open(file_name);
    if(!file) {
        Serial.print("Couldn't read file: ");
        Serial.println(file_name);
        return "";
    }

    // check file size
    // TODO: parametrize the maximum file size
    size_t sz = file.size();
    if (sz > 1024) {
        Serial.println("Config file size is too large");
        return "";
    }
    
    // load data
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if(error) {
        Serial.print("Failed to parse config file while trying to read: ");
        Serial.print(file_name);
        Serial.print(": ");
        Serial.println(param_name);
        return "";
    }

    const char* tmp = doc[param_name];

    // todo: look for cases where param_name does not exist

    Serial.print("[JSON READ] ");
    Serial.print(param_name);
    Serial.print(": ");
    Serial.println(tmp);
    return tmp;
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