#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <PCF8574.h>
#include <si5351.h>

enum output_pin {
  OUTPUT_RX_MUTE,
  OUTPUT_TX_VDD_EN,
  OUTPUT_ADC_SEL,
  OUTPUT_USER_LED,
  OUTPUT_DEBUG_LED,
  OUTPUT_LPF_1,
  OUTPUT_LPF_2,
  OUTPUT_BPF_1,
  OUTPUT_BPF_2,
  OUTPUT_AF_SEL_1,
  OUTPUT_AF_SEL_2,
  OUTPUT_AF_SEL_3,
  OUTPUT_BW_SEL
};

enum input_pin {
  INPUT_VBAT,
  INPUT_AUDIO
};

// enums for output pin states - to help with clarity in situations like driving a relay
enum output_state {
  OUTPUT_HIGH,
  OUTPUT_LOW,
  OUTPUT_ON,
  OUTPUT_OFF,
  OUTPUT_SEL_VBAT,
  OUTPUT_SEL_AUDIO,
  OUTPUT_SEL_CW,
  OUTPUT_SEL_SSB,
  OUTPUT_AUDIO_RX,
  OUTPUT_AUDIO_SIDETONE
};

enum mode_type {
  MODE_RX,
  MODE_TX,
  MODE_QSK_COUNTDOWN
};

enum key_types {
  KEY_STRAIGHT,
  KEY_PADDLE
};

const char * hw_config_file = "/hardware_config.json";
const char * credential_file = "/credentials.json";
const int led = LED_BUILTIN;

AsyncWebServer server(80);
Si5351 si5351;
PCF8574 pcf8574(0x20);

// flag_freq indicates whether frequency OR rx bandwidth need to change
bool flag_freq = false, flag_vol = true, flag_special = false;
bool dit_flag = false, dah_flag = false;

mode_type tx_rx_mode = MODE_QSK_COUNTDOWN;
int64_t qsk_counter = 0;

key_types key_type = KEY_PADDLE;

output_pin lpf_relay = OUTPUT_LPF_1, bpf_relay = OUTPUT_BPF_1;

float last_vbat = 0, last_smeter = 0;
float min_vbat, max_vbat;

// TODO: replace f_rf with a concept of dial frequency, and USB/LSB
uint64_t f_rf = 0;
uint64_t f_bfo = 0;
uint64_t f_audio = 0;
uint64_t f_vfo = 0;
uint64_t f_if = 0;

// TOOD: replace this with something that would more easily scale to an arbitrary number of bands
uint64_t f_rf_min_band1 = 0;
uint64_t f_rf_max_band1 = 0;
uint64_t f_rf_min_band2 = 0;
uint64_t f_rf_max_band2 = 0;


uint16_t rx_bw = OUTPUT_SEL_CW;
uint16_t keyer_speed = 0;
uint16_t vol = 0;
uint16_t special = 0;
String tx_queue = "";

uint64_t last_loop_time = 0;

float dbg_1 = 0;
float dbg_2 = 0;

// TODO - restructure init functions so there's one batch read from JSON before configuring hardware

void setup(void) {
  // short delay to make sure serial monitor catches everything
  my_delay(2000);

  Serial.begin(115200);
  SPIFFS.begin();

  // comment in for ESP module debug
  // Serial.setDebugOutput(true);

  init_gpio();
  gpio_write(OUTPUT_USER_LED, OUTPUT_ON);

  // start up wifi and MDNS
  // looks for home network, start AP if it's not found
  init_wifi_mdns();

  // start web server, attach handlers
  init_web_server();

  // start radio hardware
  init_radio();

  // start keyer
  init_keyer();

  // turn off TX power
  gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_OFF);

  // turn off LED after config finishes
  gpio_write(OUTPUT_USER_LED, OUTPUT_OFF);

  update_volume(vol);

  // TODO - bulk read from JSON, move everything out of init files
  min_vbat = load_json_config(hw_config_file, "v_bat_min_cutoff").toFloat();
  max_vbat = load_json_config(hw_config_file, "v_bat_max_cutoff").toFloat();
}


void loop(void) {
  // update loop timer and debug variable
  last_loop_time = micros();

  MDNS.update();

  // reconfig clocks if frequency has changed
  if (flag_freq) {
    // don't update frequency if we are still in TX mode - derisks hardware damage
    if (tx_rx_mode == MODE_RX) {
      flag_freq = false;

      gpio_write(OUTPUT_DEBUG_LED, OUTPUT_ON);
      update_relays(f_rf);
      gpio_write(OUTPUT_DEBUG_LED, OUTPUT_OFF);

      gpio_write(OUTPUT_DEBUG_LED, OUTPUT_ON);
      set_clocks(f_bfo, f_vfo, f_rf);
      gpio_write(OUTPUT_DEBUG_LED, OUTPUT_OFF);
    }
    else
      Serial.println("[SAFETY] Did not update frequency because radio was transmitting.");

    // update AF filter routing, regardless of whether tx_rx_mode changed
    // TODO - add a flag for this?
    gpio_write(OUTPUT_BW_SEL, (output_state) rx_bw);
  }

  // perform volume update if change detected
  if (flag_vol) {
    flag_vol = false;
    update_volume(vol);
  }

  // take action based on special functions
  if (flag_special) {
    flag_special = false;
    special_mode(special);
  }

  // handle key inputs
  if (dit_flag)
    dit();
  if (dah_flag)
    dah();

  // send a letter from the queue if there is anything
  handle_keyer_queue();

  // decrement QSK timer if needed
  handle_qsk_timer();

  // analog reads so server has fresh info
  last_vbat = analog_read(INPUT_VBAT);

  // TODO - have some better logic for this, and extract the 1.0v rationality threshold
  if (last_vbat > max_vbat || (last_vbat < min_vbat && last_vbat > 1.0)) {
    Serial.print("[SAFETY] VBat out of range ");
    Serial.println(last_vbat);
    set_mode(MODE_RX);
    for (int i = 0; i < 10; i++) {
      gpio_write(OUTPUT_USER_LED, OUTPUT_ON);
      my_delay(50);
      gpio_write(OUTPUT_USER_LED, OUTPUT_OFF);
      my_delay(50);
    }
  }



  // commented out because it's causing a hum, suspect due to mux switching affecting audio signal
  // last_smeter = analog_read(INPUT_AUDIO);

  // update debug values
  // light up LED if loop took longer than expected.
  dbg_1 = micros() - last_loop_time;
  if (dbg_1 > 250)
    gpio_write(OUTPUT_DEBUG_LED, OUTPUT_ON);
  else
    gpio_write(OUTPUT_DEBUG_LED, OUTPUT_OFF);
  dbg_2 = WiFi.RSSI();
}
