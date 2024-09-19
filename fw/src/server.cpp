#include "server.h"
#include "handlers.h"
#include "globals.h"

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
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
static const server_handler_t handlers[] = {
    {API_V1,    HTTP_GET,   "frequency",        handler_frequency_get},
    {API_V1,    HTTP_GET,   "volume",           handler_volume_get}
};


bool server_request_handler(AsyncWebServerRequest *request);
void server_print_request(AsyncWebServerRequest *request);


void handle_frequency(WebRequestMethodComposite request_type, AsyncWebServerRequest *request) {
    if(request_type == HTTP_GET) {
        request->send(200, "text/plain", String(14000));
        digitalWrite(LED_RED, HIGH);
    }
}

void server_init() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(FILE_SYSTEM, "/index.html", "text/html");
    });
    
    // server.serveStatic("/", FILE_SYSTEM, "/");

    // not *only* for handling unknown requests
    // using this as a catch-all so we don't need an extreme number of .on() callback registrations
    server.onNotFound([](AsyncWebServerRequest *request){
        if(!server_request_handler(request)) {
            Serial.printf("NOT_FOUND: ");
            Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());
            server_print_request(request);
            request->send(404);
        }
    });

    server.begin();
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
bool server_request_handler(AsyncWebServerRequest *request) {
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