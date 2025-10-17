#include "handlers.h"
#include "globals.h"
#include "radio.h"
#include "radio_hf.h"
#include "power.h"
#include "wifi_conn.h"
#include "file_system.h"

#include <ESPAsyncWebServer.h>
#include <Arduino.h>

// returns false if the request does not have the required parameter, and sends error code
bool handler_require_param(AsyncWebServerRequest *request, String param_name) {
    if(!request->hasParam(param_name)) {
        request->send(400, "text/plain", "Required parameter \"" + param_name + "\" was not sent");
        return false;
    }
    return true;
}


// new: set three clock outputs (expects clk0, clk1, clk2 in Hz)
void handler_clocks_set(AsyncWebServerRequest *request) {
    if(!handler_require_param(request, "clk0") || !handler_require_param(request, "clk1") || !handler_require_param(request, "clk2"))
        return;

    uint64_t clk0_request = request->getParam("clk0")->value().toInt();
    uint64_t clk1_request = request->getParam("clk1")->value().toInt();
    uint64_t clk2_request = request->getParam("clk2")->value().toInt();

    // Log the requested clock frequencies
    Serial.println("Setting clock frequencies:");
    Serial.print("CLK0: "); Serial.println(clk0_request);
    Serial.print("CLK1: "); Serial.println(clk1_request);
    Serial.print("CLK2: "); Serial.println(clk2_request);

    // set_freq expects values scaled similarly to other uses in code (multiply by 100)
    si5351.set_freq(clk0_request * 100, SI5351_CLK0);
    si5351.set_freq(clk1_request * 100, SI5351_CLK1);
    si5351.set_freq(clk2_request * 100, SI5351_CLK2);

    request->send(201, "text/plain", "OK");
}

// new: set per-clock phase values (expects phase0, phase1, phase2 as small integers)
void handler_phase_set(AsyncWebServerRequest *request) {
    if(!handler_require_param(request, "phase0") || !handler_require_param(request, "phase1") || !handler_require_param(request, "phase2"))
        return;

    int16_t phase0 = request->getParam("phase0")->value().toInt();
    int16_t phase1 = request->getParam("phase1")->value().toInt();
    int16_t phase2 = request->getParam("phase2")->value().toInt();

    // Log the requested phase values
    Serial.println("Setting clock phases:");
    Serial.print("Phase0: "); Serial.println(phase0);
    Serial.print("Phase1: "); Serial.println(phase1);
    Serial.print("Phase2: "); Serial.println(phase2);

    // set phase for each clock output
    si5351.set_phase(SI5351_CLK0, phase0);
    si5351.set_phase(SI5351_CLK1, phase1);
    si5351.set_phase(SI5351_CLK2, phase2);

    // reset PLLs if needed; here reset both PLLs to ensure changes take effect
    si5351.pll_reset(SI5351_PLLA);
    si5351.pll_reset(SI5351_PLLB);

    request->send(201, "text/plain", "OK");
}

void handler_clock_toggle(AsyncWebServerRequest *request) {
    if(!handler_require_param(request, "clk") || !handler_require_param(request, "enable"))
        return;

    int clk = request->getParam("clk")->value().toInt();
    bool enable = request->getParam("enable")->value().toInt();

    if (clk < 0 || clk > 2) {
        request->send(400, "text/plain", "Invalid clock index");
        return;
    }

    // Log the action
    Serial.print("Setting clock ");
    Serial.print(clk);
    Serial.print(enable ? " ON" : " OFF");
    Serial.println();

    // Enable or disable the clock
    si5351.output_enable(static_cast<si5351_clock>(clk), enable ? 1 : 0);

    request->send(201, "text/plain", "OK");
}

void handler_input_voltage_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(power_get_input_volt()));
}

void handler_mac_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", wifi_get_mac());
}

void handler_githash_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", AUTO_VERSION);
}

void handler_heap_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(xPortGetFreeHeapSize()));
}

void handler_ip_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", wifi_get_ip());
}

// TODO: terrible idea to access the file sys    in this handler...
void handler_revision_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", fs_load_setting(HARDWARE_FILE, "hardware_rev"));
}

void handler_serial_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", fs_load_setting(HARDWARE_FILE, "serial_number"));
}

void handler_api_get(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", API_IMPLEMENTED);
}

void handler_debug_post(AsyncWebServerRequest *request) {
    if(!handler_require_param(request, "command"))
        return;

    uint64_t command_num = request->getParam("command")->value().toInt();

    if(command_num == DEBUG_CMD_REBOOT) {
        esp_restart();
    }

    request->send(201, "text/plain", "OK");
}
