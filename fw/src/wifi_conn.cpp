#include "wifi_conn.h"

// #include <ESP8266WiFi.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>

// #include <WiFiClient.h>
// #include <ESP8266HTTPClient.h>

// #define WIFI_SCAN

void wifi_init() {
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet(255, 255, 255, 0);

    // this call before any others helps, per: https://stackoverflow.com/questions/44139082/esp8266-takes-long-time-to-connect
    WiFi.persistent(false);

    // attempt to connect to home wifi network
    Serial.println("[WIFI] Attempting to connect to home network.");
    WiFi.mode(WIFI_STA);

    WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);

    for (int i = 0; i < 25; i++) {
        if (WiFi.status() == WL_CONNECTED)
            break;
        digitalWrite(LED_RED, HIGH);
        delay(100);
        digitalWrite(LED_RED, LOW);
        delay(100);
    }


    // start access point if we have failed to connect
    // TODO: look at using waitForConnectResult. https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/station-class.html#waitforconnectresult
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WIFI] Failed to connect to home network. Starting AP.");
        // begin by forgetting old credentials
        WiFi.disconnect(true);

        // set up AP with hardcoded 192.168.1.1 address
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
        WiFi.softAP("TEST", "password", 11);
        ip = WiFi.softAPIP();
    }
    else {
        Serial.println("[WIFI] Successful connection to home network.");
        ip = WiFi.localIP();
        Serial.println(ip.toString());

        Serial.print("[WIFI] Connection status: ");
        Serial.println(WiFi.status());
    }

    // lower power to prevent audio noise (TODO: test if this is present in latest hardware)
    // maximum power is WIFI_POWER_19_5dBm
    // WiFi.setTxPower(WIFI_POWER_5dBm);

#ifdef WIFI_SCAN
    wifi_scan();
#endif

    // start MDNS
    if (MDNS.begin("radio")) {
        MDNS.addService("http", "tcp", 80);
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