#include "server.h"
#include "handlers.h"
#include "globals.h"
#include "radio_hf.h"   // won't be needed after ESP-NOW handler moved into handlers.h
#include "file_system.h"

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <esp_now.h>
#include "LittleFS.h"
#include <ArduinoJSON.h>

typedef enum {
    API_V1 = 1,
    API_V2 = 2
} api_number_t;


typedef struct {
    api_number_t api_version;
    WebRequestMethodComposite method;
    const char *url;
    ArRequestHandlerFunction handler_function;
} server_handler_t;


String api_prefix = "/api/v";
AsyncWebServer server(80);
AsyncEventSource events("/events");


static const server_handler_t handlers[] = {
    {API_V1,    HTTP_GET,   "time",             handler_time_get},
    {API_V1,    HTTP_PUT,   "time",             handler_time_set},
    {API_V1,    HTTP_GET,   "frequency",        handler_frequency_get},
    {API_V1,    HTTP_PUT,   "frequency",        handler_frequency_set},
    {API_V1,    HTTP_GET,   "volume",           handler_volume_get},
    {API_V1,    HTTP_PUT,   "volume",           handler_volume_set},
    {API_V1,    HTTP_GET,   "sidetone",         handler_sidetone_get},
    {API_V1,    HTTP_PUT,   "sidetone",         handler_sidetone_set},
    {API_V1,    HTTP_GET,   "rxBandwidth",      handler_bandwidth_get},
    {API_V1,    HTTP_PUT,   "rxBandwidth",      handler_bandwidth_set},
    {API_V1,    HTTP_GET,   "cwSpeed",          handler_keyer_speed_get},
    {API_V1,    HTTP_PUT,   "cwSpeed",          handler_keyer_speed_set},
    {API_V1,    HTTP_GET,   "inputVoltage",     handler_input_voltage_get},
    {API_V1,    HTTP_GET,   "sMeter",           handler_smeter_get},
    {API_V1,    HTTP_GET,   "githash",          handler_githash_get},
    {API_V1,    HTTP_GET,   "heap",             handler_heap_get},
    {API_V1,    HTTP_GET,   "mac",              handler_mac_get},
    {API_V1,    HTTP_GET,   "ip",               handler_ip_get},
    {API_V1,    HTTP_GET,   "hwRevision",       handler_revision_get},
    {API_V1,    HTTP_GET,   "unitSerial",       handler_serial_get},
    {API_V1,    HTTP_GET,   "api",              handler_api_get}
};


bool server_http_handler(AsyncWebServerRequest *request);
void server_print_request(AsyncWebServerRequest *request);
void server_espnow_handler(const uint8_t * mac, const uint8_t *incomingData, int len);


// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
    char action[32];
    uint32_t counter;
} struct_message;
// Create a struct_message called myData
struct_message myData;

uint32_t last_counter_val = 0;

void server_init() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(FILE_SYSTEM, "/index.html", "text/html");
    });
    
    // server.serveStatic("/", FILE_SYSTEM, "/");

    // not *only* for handling unknown requests
    // using this as a catch-all so we don't need an extreme number of .on() callback registrations
    server.onNotFound([](AsyncWebServerRequest *request){
        if(!server_http_handler(request)) {
            Serial.printf("NOT_FOUND: ");
            Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());
            server_print_request(request);
            request->send(404);
        }
    });

    server.begin();

    // register ESP-NOW callback
    esp_now_register_recv_cb(esp_now_recv_cb_t(server_espnow_handler));

    // start the web-based file browser now that the server is up
    fs_start_browser();
}


void server_print_request(AsyncWebServerRequest *request) {
    if(request->method() == HTTP_GET)
        Serial.printf("GET");
    else if(request->method() == HTTP_POST)
        Serial.printf("POST");
    else if(request->method() == HTTP_DELETE)
        Serial.printf("DELETE");
    else if(request->method() == HTTP_PUT)
        Serial.printf("PUT");
    else if(request->method() == HTTP_PATCH)
        Serial.printf("PATCH");
    else if(request->method() == HTTP_HEAD)
        Serial.printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
        Serial.printf("OPTIONS");
    else
        Serial.printf("UNKNOWN");

    if(request->contentLength()){
        Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
        Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
        AsyncWebHeader* h = request->getHeader(i);
        Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isFile()){
            Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
        } else if(p->isPost()){
            Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        } else {
            // there is no isPut() function
            Serial.printf("_GET or _PUT [%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
    }
}

// returns true if the request was properly handled
bool server_http_handler(AsyncWebServerRequest *request) {
    WebRequestMethodComposite request_type = request->method();
    String full_url = request->url();

    // unknown request, if it doesn't start with "/api"
    if(!full_url.startsWith(api_prefix))
        return false;

    uint16_t idx_start = api_prefix.length();
    uint16_t idx_end = full_url.indexOf("/", idx_start);
    uint16_t api_version = (uint16_t) full_url.substring(idx_start, idx_end).toInt();
    String url = full_url.substring(idx_end+1);

    // find the correct handler
    uint16_t num_handlers = sizeof(handlers) / sizeof(server_handler_t);
    for(uint16_t i = 0; i < num_handlers; i++) {
        // check for a match on api version, handler URL, and method
        if(api_version == handlers[i].api_version && url == handlers[i].url && request_type == handlers[i].method) {
            Serial.print("Method: ");
            Serial.print(request_type);
            Serial.print("\tURL: ");
            Serial.println(url);
            handlers[i].handler_function(request);
            return true;
        }
    }

    // no handler found
    return false;
}

// callback function that will be executed when data is received
// TODO: use the handler mapping from above, ESP-NOW can implement the same API
void server_espnow_handler(const uint8_t * mac, const uint8_t *incomingData, int len) {
    memcpy(&myData, incomingData, sizeof(myData));

    // check for duplicates before proceeding
    if(myData.counter != last_counter_val) {
        last_counter_val = myData.counter;

        Serial.println(myData.action);
        
        // hack for testing: "dit" keys on, "dah" keys off
        if(strcmp(myData.action, "dit")) {
            radio_key_on();
        }
        else if(strcmp(myData.action, "dah")) {
            radio_key_off();
        }
    }
}