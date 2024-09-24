#include "file_system.h"
#include "globals.h"

#include <Arduino.h>
#include "LittleFS.h"
#include <ArduinoJSON.h>
#include <ESPxWebFlMgr.h>

TaskHandle_t xFileSystemTask;

ESPxWebFlMgr filemgr(8080);

bool fs_mounted = false;

radio_band_t string_to_radio_band(const char* band_str);
radio_audio_bw_t string_to_radio_audio_bw(const char* bw_str);
const char* radio_band_to_string(radio_band_t band);
const char* radio_bandwidth_to_string(radio_audio_bw_t bw);
void print_band_capability(radio_band_capability_t (&bands)[NUMBER_BANDS]);
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

radio_band_t string_to_radio_band(const char* band_str) {
    if (strcmp(band_str, "BAND_HF_1") == 0) return BAND_HF_1;
    if (strcmp(band_str, "BAND_HF_2") == 0) return BAND_HF_2;
    if (strcmp(band_str, "BAND_HF_3") == 0) return BAND_HF_3;
    if (strcmp(band_str, "BAND_VHF") == 0) return BAND_VHF;
    return BAND_UNKNOWN;
}

radio_audio_bw_t string_to_radio_audio_bw(const char* bw_str) {
    if (strcmp(bw_str, "cw") == 0) return BW_CW;
    if (strcmp(bw_str, "ssb") == 0) return BW_SSB;
    if (strcmp(bw_str, "fm") == 0) return BW_FM;
    return BW_CW;  // Default to CW if unrecognized
}

const char* radio_band_to_string(radio_band_t band) {
    switch (band) {
        case BAND_HF_1: return "BAND_HF_1";
        case BAND_HF_2: return "BAND_HF_2";
        case BAND_HF_3: return "BAND_HF_3";
        case BAND_VHF: return "BAND_VHF";
        case BAND_SELF_TEST: return "BAND_SELF_TEST";
        default: return "UNKNOWN_BAND";
    }
}

// Convert radio_audio_bw_t enum to string
const char* radio_bandwidth_to_string(radio_audio_bw_t bw) {
    switch (bw) {
        case BW_CW: return "CW";
        case BW_SSB: return "SSB";
        case BW_FM: return "FM";
        default: return "UNKNOWN_BW";
    }
}

void print_band_capability(radio_band_capability_t (&bands)[NUMBER_BANDS]) {
    for (int i = 0; i < NUMBER_BANDS; i++) {
        // Print the band name
        Serial.print("Band Name: ");
        Serial.println(radio_band_to_string(bands[i].band_name));

        // Print the frequency range
        Serial.print("Min Frequency: ");
        Serial.println(bands[i].min_freq);
        Serial.print("Max Frequency: ");
        Serial.println(bands[i].max_freq);

        Serial.print("Num RX Bandwidths: ");
        Serial.println(bands[i].num_rx_bandwidths);

        // Print the bandwidths
        Serial.print("RX Bandwidths: ");
        for (int j = 0; j < bands[i].num_rx_bandwidths; j++) {
            Serial.print(radio_bandwidth_to_string(bands[i].rx_bandwidths[j]));
            Serial.print(", ");
        }
        Serial.println();

        Serial.print("Num TX Bandwidths: ");
        Serial.println(bands[i].num_tx_bandwidths);

        // Print the bandwidths
        Serial.print("TX Bandwidths: ");
        for (int j = 0; j < bands[i].num_tx_bandwidths; j++) {
            Serial.print(radio_bandwidth_to_string(bands[i].tx_bandwidths[j]));
            Serial.print(", ");
        }

        Serial.println();  // End the bandwidth list line
        Serial.println();
    }
}

// TODO: error checking, handle missing parameters
void fs_load_bands(String file_name, radio_band_capability_t (&bands)[NUMBER_BANDS]) {
     // check if file system is mounted yet
    if(!fs_mounted) {
        Serial.println("Couldn't read, file system not properly mounted.");
        return;
    }

    // attempt to read file
    File file = LittleFS.open(file_name);
    if(!file) {
        Serial.print("Couldn't read file: ");
        Serial.println(file_name);
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    JsonArray arr = doc["bands"].as<JsonArray>();
    uint16_t num_bands = arr.size();
    if(num_bands > NUMBER_BANDS)
        num_bands = NUMBER_BANDS;

    for (size_t i = 0; i < arr.size(); i++) {
        radio_band_capability_t tmp;
        JsonObject entry = arr[i];

        // pull out information from the JSON object
        const char* band_num = entry["band_num"];
        tmp.band_name = string_to_radio_band(band_num);
        tmp.min_freq = strtoull(entry["min_freq"].as<const char*>(), nullptr, 10);
        tmp.max_freq = strtoull(entry["max_freq"].as<const char*>(), nullptr, 10);

        // Parse rx_modes into the bandwidths array
        JsonArray rx_modes = entry["rx_modes"].as<JsonArray>();
        tmp.num_rx_bandwidths = rx_modes.size();
        size_t bw_index = 0;
        for (const char* mode : rx_modes) {
            if (bw_index >= 8) break;  // Ensure we don't exceed bandwidths array size. TODO: parametrize this
            tmp.rx_bandwidths[bw_index] = string_to_radio_audio_bw(mode);
            bw_index++;
        }

        // Parse tx_modes into the bandwidths array
        JsonArray tx_modes = entry["tx_modes"].as<JsonArray>();
        tmp.num_tx_bandwidths = tx_modes.size();
        bw_index = 0;
        for (const char* mode : tx_modes) {
            if (bw_index >= 8) break;  // Ensure we don't exceed bandwidths array size. TODO: parametrize this
            tmp.tx_bandwidths[bw_index] = string_to_radio_audio_bw(mode);
            bw_index++;
        }

        bands[i] = tmp;
    }

    // debug
    print_band_capability(bands);
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