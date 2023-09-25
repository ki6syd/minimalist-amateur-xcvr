// useful for flash use debug: "C:\Program Files (x86)\Arduino\hardware\tools\avr\bin\avr-nm.exe" --size-sort -C -r fw.ino.elf

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncWebServer.h>
#include <Updater.h>
#include <SPIFFSEditor.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <PCF8574.h>
#include <si5351.h>
#include <JTEncode.h>
#include <TimeLib.h>
#include "git-version.h"

enum output_pin {
  OUTPUT_RX_MUTE,
  OUTPUT_TX_VDD_EN,
  OUTPUT_ADC_SEL,
  OUTPUT_GREEN_LED,
  OUTPUT_RED_LED,
  OUTPUT_LPF_1,
  OUTPUT_LPF_2,
  OUTPUT_LPF_3,
  OUTPUT_BPF_1,
  OUTPUT_BPF_2,
  OUTPUT_BPF_3,
  OUTPUT_AF_SEL_1,
  OUTPUT_AF_SEL_2,
  OUTPUT_AF_SEL_3,
  OUTPUT_AF_SEL_4,
  OUTPUT_BW_SEL,
  OUTPUT_LNA_SEL,
  OUTPUT_ANT_SEL,
  OUTPUT_SPKR_EN
};

enum input_pin {
  INPUT_VBAT,
  INPUT_AUDIO
};

// enums for output pin states - to help with clarity in situations like open drain outputs or high/low signal driving a relay
enum output_state {
  OUTPUT_HIGH,
  OUTPUT_LOW,
  OUTPUT_ON,
  OUTPUT_OFF,
  OUTPUT_SEL_VBAT,
  OUTPUT_SEL_AUDIO,
  OUTPUT_SEL_CW,
  OUTPUT_SEL_SSB,
  OUTPUT_MUTED,
  OUTPUT_UNMUTED,
  OUTPUT_ANT_DIRECT,
  OUTPUT_ANT_XFMR,
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

const char * preference_file = "/preferences.json";
const char * wifi_file = "/wifi_info.json";
const char * hardware_file = "/hardware_info.json";
String api_base_url = "/api/v1/";
const int led = LED_BUILTIN;

AsyncWebServer server(80);
Si5351 si5351;
JTEncode jtencode;
PCF8574 pcf8574_20(0x20);
PCF8574 pcf8574_21(0x21);


// ------------------------------ FT8 variables ------------------------------
uint8_t ft8_buffer[255];
bool ft8_busy = false;
uint64_t ft8_freq = 14070000;


// ------------------------------ time variables ------------------------------
uint64_t time_offset = 0;



// flag_freq indicates whether frequency OR rx bandwidth need to change
bool flag_freq = false, flag_vol = true, flag_special = false, flag_ft8 = false;
bool dit_flag = false, dah_flag = false;



mode_type tx_rx_mode = MODE_QSK_COUNTDOWN;
int64_t qsk_counter = 0;

// used in OTA
size_t content_len;


// used in io
String hardware_rev = "";
String unit_serial = "";


// TODO: a few of these shouldn't be uint16_t's, figure out better enum strategy
uint16_t rx_bw = OUTPUT_SEL_CW;
uint16_t ant = OUTPUT_ANT_DIRECT;
uint16_t qsk_period = 500;
uint16_t keyer_speed = 0;
uint16_t vol = 0;
int16_t mon_offset = 0;
uint16_t special = 0;
String tx_queue = "";
// TODO: turn this into an enum
bool lna_state = false;
bool speaker_state = false;
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

// TODO: replace this with something that would more easily scale to an arbitrary number of bands
uint64_t f_rf_min_band1 = 0;
uint64_t f_rf_max_band1 = 0;
uint64_t f_rf_min_band2 = 0;
uint64_t f_rf_max_band2 = 0;
uint64_t f_rf_min_band3 = 0;
uint64_t f_rf_max_band3 = 0;

uint16_t keyer_min = 0;
uint16_t keyer_max = 0;
uint16_t vol_min = 0;
uint16_t vol_max = 0;

// TODO - restructure init functions so there's one batch read from JSON before configuring hardware
void setup(void) {
  // short delay to make sure serial monitor catches everything
  // my_delay(2000);

  Serial.begin(115200);
  SPIFFS.begin();

  // comment in for ESP module debug
  // Serial.setDebugOutput(true);

  init_gpio();
  init_audio();
  gpio_write(OUTPUT_GREEN_LED, OUTPUT_ON);

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

  // set antenna impedance relay
  if(load_json_config(preference_file, "ant_default") == "OUTPUT_ANT_DIRECT")
    ant = OUTPUT_ANT_DIRECT;
  if(load_json_config(preference_file, "ant_default") == "OUTPUT_ANT_XFMR")
    ant = OUTPUT_ANT_XFMR;
  gpio_write(OUTPUT_ANT_SEL, (output_state) ant);

  // set lna option
  if(load_json_config(preference_file, "lna_default") == "ON") {
    lna_state = true;
    gpio_write(OUTPUT_LNA_SEL, OUTPUT_ON);
  }
  else {
    lna_state = false;
    gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);
  }    

  // TODO - bulk read from JSON, move everything out of init files
  min_vbat = load_json_config(preference_file, "v_bat_min_cutoff").toFloat();
  max_vbat = load_json_config(preference_file, "v_bat_max_cutoff").toFloat();

  qsk_period = load_json_config(preference_file, "qsk_delay_ms").toFloat();
  mon_offset = load_json_config(preference_file, "sidetone_level").toFloat();

  // initial read of battery voltage
  last_vbat = analog_read(INPUT_VBAT);

  // load serial number
  unit_serial = load_json_config(hardware_file, "serial_number");
}


void loop(void) {
  uint8_t i = 0;
  MDNS.update();

  // reconfig clocks if frequency has changed
  if (flag_freq) {
    // don't update frequency if we are still in TX mode - derisks hardware damage
    if (tx_rx_mode == MODE_RX) {
      flag_freq = false;

      gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
      update_relays(f_rf);
      gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);

      gpio_write(OUTPUT_RED_LED, OUTPUT_ON);

      // update antenna connection
      gpio_write(OUTPUT_ANT_SEL, (output_state) ant);

      // set LNA hardware
      if(lna_state)
        gpio_write(OUTPUT_LNA_SEL, OUTPUT_ON);
      else
        gpio_write(OUTPUT_LNA_SEL, OUTPUT_OFF);

      // set speaker
      if(speaker_state)
        gpio_write(OUTPUT_SPKR_EN, OUTPUT_ON);
      else
        gpio_write(OUTPUT_SPKR_EN, OUTPUT_OFF);
      
      // TODO: this logic might make more sense elsewhere. Also should consider the concept of USB/LSB, dial freq, and mode rather than this mess of if statements.
      if(rx_bw == OUTPUT_SEL_CW)
        set_clocks(f_bfo, f_vfo, f_rf);
      else {
        if(f_rf < 10e6)
          set_clocks(f_bfo, f_vfo, f_rf - f_audio);
        else
          set_clocks(f_bfo, f_vfo, f_rf + f_audio);
      }
      gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
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

  // todo - queues 
  if(flag_ft8) {
    send_ft8();
  }

  // handle key inputs
  if (dit_flag)
    dit();
  if (dah_flag)
    dah();

  // send a letter from the queue if there is anything
  update_keyer_queue();

  // decrement QSK timer if needed
  update_qsk_timer();

  // do analog read once per loop. vbat during tx, smeter during rx. 
  // noise comes from toggling adc mux, so reading both types doesn't work well
  if(tx_rx_mode == MODE_TX || tx_rx_mode == MODE_QSK_COUNTDOWN)
    last_vbat = analog_read(INPUT_VBAT);
  else {
    // do this every 10th loop. Frequent ADC reads disrupt wifi
    /*
    if(i % 10 == 0)
      update_smeter();
      */
  }

  // TODO - have some better logic for this, and extract the 2.0v rationality threshold into a settings file
  if (last_vbat > max_vbat || (last_vbat < min_vbat && last_vbat > 2.0)) {
    // turn off TX clock and set VDD off
    gpio_write(OUTPUT_TX_VDD_EN, OUTPUT_OFF);
    my_delay(100);
    si5351.output_enable(SI5351_CLK2, 0);

    Serial.print("[SAFETY] VBat out of range ");
    Serial.println(last_vbat);
    
    set_mode(MODE_RX);
    for (int i = 0; i < 10; i++) {
      gpio_write(OUTPUT_RED_LED, OUTPUT_ON);
      my_delay(50);
      gpio_write(OUTPUT_RED_LED, OUTPUT_OFF);
      my_delay(50);
    }

    // force another ADC read so we don't get stuck in an undervoltage state. Only samples during TX normally.
    last_vbat = analog_read(INPUT_VBAT);
  }

  my_delay(5);
  i++;
}
