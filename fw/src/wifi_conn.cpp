#include "wifi_conn.h"
#include "file_system.h"
#include "globals.h"

#include <WiFi.h>
#include <esp_wifi.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>
#include <esp_now.h>

// #define WIFI_SCAN

IPAddress ip;

void wifi_init() {
    IPAddress gateway;
    IPAddress subnet(255, 255, 255, 0);

    // this call before any others helps, per: https://stackoverflow.com/questions/44139082/esp8266-takes-long-time-to-connect
    WiFi.persistent(false);

    // attempt to connect to home wifi network
    Serial.println("[WIFI] Attempting to connect to home network (FLASH credentials)");
    WiFi.mode(WIFI_STA);

    // try credentials assigned at compile time
    WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);
    for (int i = 0; i < 50; i++) {
        if (WiFi.status() == WL_CONNECTED)
            break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // try credentials from JSON file
    if(WiFi.status() != WL_CONNECTED) {
        Serial.println("[WIFI] Attempting to connect to home network (JSON credentials)");
        WiFi.begin(fs_load_setting(PREFERENCE_FILE, "home_ssid"), fs_load_setting(PREFERENCE_FILE, "home_password"));
        for (int i = 0; i < 50; i++) {
            if (WiFi.status() == WL_CONNECTED)
                break;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // start access point if we have failed to connect
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WIFI] Failed to connect to home network. Starting AP.");
        // begin by forgetting old credentials
        WiFi.disconnect(true);

        // set up AP with hardcoded 192.168.1.1 address
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
        WiFi.softAP(fs_load_setting(PREFERENCE_FILE, "ap_ssid"), fs_load_setting(PREFERENCE_FILE, "ap_password"), 11);
        ip = WiFi.softAPIP();
    }
    else {
        Serial.println("[WIFI] Successful connection to home network.");
        ip = WiFi.localIP();
        Serial.println(ip.toString());

        Serial.print("[WIFI] Connection status: ");
        Serial.println(WiFi.status());

        Serial.print("[WIFI] Channel: ");
        Serial.println(WiFi.channel());
    }

    // higher power level helps with TCP audio streaming, lower power makes less audible noise
    if(fs_setting_exists(PREFERENCE_FILE, "wifi_power"))
        WiFi.setTxPower((wifi_power_t) fs_load_setting(PREFERENCE_FILE, "wifi_power").toInt());
    else    
        WiFi.setTxPower(WIFI_POWER_19_5dBm);

#ifdef WIFI_SCAN
    wifi_scan();
#endif

    // avoiding power saving mode helps with ESP-NOW
    esp_wifi_set_ps(WIFI_PS_NONE);

    // start MDNS
    if (MDNS.begin(fs_load_setting(PREFERENCE_FILE, "mdns_hostname"))) {
        MDNS.addService("http", "tcp", 80);
    }

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
    return;
    }
}


void wifi_scan() {
  // do a scan of all networks and print info. Useful info: https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
  Serial.println("[WIFI] scan starting");
  int n = WiFi.scanNetworks();
  Serial.print("[WIFI] Found networks: ");
  Serial.println(n);
  for(int i = 0; i < n; i++) {
    Serial.print("[WIFI] SSID: ");
    Serial.println(WiFi.SSID(i));
    Serial.print("[WIFI] RSSI: ");
    Serial.println(WiFi.RSSI(i));
    Serial.print("[WIFI] Encryption: ");
    Serial.println(WiFi.encryptionType(i));
    Serial.print("[WIFI] Channel: ");
    Serial.println(WiFi.channel(i));
    Serial.println("\n");
  }
}

String wifi_get_ip() {
    return ip.toString();
}

// returns the MAC address as a formatted string
String wifi_get_mac() {
    uint8_t base_mac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, base_mac);
    if(ret != ESP_OK)
        return "UNKNOWN";

    String mac_str = "";
    for (int i = 0; i < 6; i++) {
        if (base_mac[i] < 16) {
            // Add leading zero if the value is less than 16 (i.e., single hex digit)
            mac_str += "0";  
        }
        mac_str += String(base_mac[i], HEX);
        if (i < 5) {
            // Add colon between MAC address bytes
            mac_str += ":";  
        }
    }

    // Convert to uppercase since String(baseMac[i], HEX) outputs lowercase letters
    mac_str.toUpperCase();
    return mac_str;
}