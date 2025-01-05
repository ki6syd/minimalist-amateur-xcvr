#include "globals.h"
#include "radio.h"
#include "radio_hf.h"
#include "audio.h"
#include "file_system.h"

#include <Arduino.h>
#include <si5351.h>

// TODO: pull these into the .ini file, or JSON?
#define S_UNIT_PER_DB         (1.0 / 6.0)
#define S_UNIT_REF            7

Si5351 si5351;

uint64_t freq_if = 45000000;
uint64_t freq_vfo = 0;
uint64_t freq_bfo = 0;

// variable to track what value of audio_get_rx_db() corresponds to the #define'd S_UNIT_REF above
float audio_level_sREF = -49.4;

void hf_si5351_init();
void hf_set_clocks(uint64_t freq_bfo, uint64_t freq_vfo, uint64_t freq_rf);
void radio_sweep_analyze(radio_filt_sweep_t sweep, float *data, radio_filt_properties_t *properties);

void hf_init() {
  pinMode(LPF_SEL_0, OUTPUT);
  pinMode(LPF_SEL_1, OUTPUT);
  pinMode(TX_RX_SEL, OUTPUT);
  digitalWrite(TX_RX_SEL, LOW);     // RX mode

  hf_si5351_init();
}

void hf_si5351_init() {
  uint64_t si5351_xtal_freq = fs_load_setting(HARDWARE_FILE, "xtal_freq_hz").toInt();
  // rough error checking. TODO: move magic numbers elsewhere
  if(si5351_xtal_freq < 25900000 || si5351_xtal_freq > 26100000) {
    si5351_xtal_freq = 26000000;
    Serial.println("Wrong config file, setting to 26.000MHz");
  }

  Wire.begin(CLOCK_SDA, CLOCK_SCL);

  Serial.print("[SI5351] Status: ");
  Serial.println(si5351.si5351_read(SI5351_DEVICE_STATUS));

  // TODO: parse and have logic based on SI5351 status
  // expect to see a "0" when ANDing with (1<<3), this would indicate that there is no LOS related to the XTAL. 

  // TODO: there are currently spurs from fractional dividers. Eliminate these

  si5351.init(SI5351_CRYSTAL_LOAD_8PF , si5351_xtal_freq, 0);

  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);

  si5351.set_ms_source(SI5351_IDX_VFO, SI5351_PLLA);
  si5351.set_ms_source(SI5351_IDX_BFO_I, SI5351_PLLB);
  si5351.set_ms_source(SI5351_IDX_BFO_Q, SI5351_PLLB);

  si5351.drive_strength(SI5351_IDX_VFO, SI5351_DRIVE_8MA);
  si5351.drive_strength(SI5351_IDX_BFO_I, SI5351_DRIVE_8MA);
  si5351.drive_strength(SI5351_IDX_BFO_Q, SI5351_DRIVE_8MA);

  hf_set_dial_freq(radio_get_dial_freq(), SIDEBAND_USB);

  si5351.output_enable(SI5351_IDX_VFO, 1);
  si5351.output_enable(SI5351_IDX_BFO_I, 1);
  si5351.output_enable(SI5351_IDX_BFO_Q, 1);
}

// helper function that only radio.cpp or radio_hf.cpp should call
// inputs: dial frequency, crystal edge frequencies, sidetone frequency, CW vs SSB
// result: sets the VFO and BFO frequencies
// note that we use F_SIDETONE_DEFAULT rather than get_sidetone_freq() because sidetone freq() is changing during FT8
void hf_set_dial_freq(uint64_t freq_dial, sideband_t sideband) {
  freq_vfo = freq_if + freq_dial;

  // TODO: check this logic for CW sidetone offset
  if(radio_get_bw() == BW_CW) {
    if(sideband == SIDEBAND_USB)
      freq_bfo = freq_if - ((uint64_t) F_SIDETONE_DEFAULT);
    else
      freq_bfo = freq_if + ((uint64_t) F_SIDETONE_DEFAULT);
  }
  else if (radio_get_bw() == BW_SSB) {
    freq_bfo = freq_if;
  }
  else {
    Serial.println("ERROR: unknown bandwidth");
  }

  hf_set_clocks(freq_bfo, freq_vfo, freq_dial);
}


void hf_set_clocks(uint64_t freq_bfo, uint64_t freq_vfo, uint64_t freq_rf) {
  // TODO: remove freq_rf
  si5351.set_freq(freq_vfo * 100, SI5351_IDX_VFO);
  si5351.set_freq(freq_bfo * 100 * BFO_CLOCK_DIV, SI5351_IDX_BFO_I);
  si5351.set_freq(freq_bfo * 100 * BFO_CLOCK_DIV, SI5351_IDX_BFO_Q);

  // TODO: set phase and BFO frequency a single time at startup? Eliminates resets at a later time
  // TODO: why is the hard-coded delay necessary?
  uint16_t phase_delay = (uint16_t) (SI5351_PLL_FIXED / (freq_bfo * BFO_CLOCK_DIV / 2));
  phase_delay = 20;
  si5351.set_phase(SI5351_IDX_BFO_I, 0);
  si5351.set_phase(SI5351_IDX_BFO_Q, phase_delay);
  si5351.pll_reset(SI5351_PLLB);              // see: https://groups.io/g/EdenDSP/topic/si5351_at_70mhz/9274649

  // Serial.println(radio_freq_string());
}

float hf_get_s_meter() {
  float audio_level = audio_get_loudness();
  float s_unit_delta = (audio_level_sREF - audio_level) * S_UNIT_PER_DB;
  // Serial.print("audio: ");
  // Serial.print(audio_level);
  // Serial.print("\tdelta: ");
  // Serial.print(s_unit_delta);
  // Serial.print("\tsnapshot: ");
  // Serial.println(audio_get_rx_vol());
  float s_output = S_UNIT_REF - s_unit_delta;
  if(s_output < 0)
    s_output = 0;
  if(s_output > 9)
    s_output = 9;

  return s_output;
}

String hf_freq_string() {
  String result = "";
  result += "VFO: ";
  result += String(freq_vfo);
  result += "\tBFO: ";
  result += String(freq_bfo);
  result += "\tDial Frequency: ";
  result += String(radio_get_dial_freq());
  return result;
}

// turn on TX for 5 seconds so nearby radio can listen for 10MHz carrier
void hf_cal_tx_10MHz() {
  // ensure VDD is turned off
  digitalWrite(PA_VDD_CTRL, LOW);
  vTaskDelay(pdMS_TO_TICKS(50));

  si5351.set_freq(((uint64_t) 10000000) * 100, SI5351_IDX_VFO);
  si5351.output_enable(SI5351_IDX_BFO_I, 0);
  si5351.output_enable(SI5351_IDX_BFO_Q, 0);
  si5351.output_enable(SI5351_IDX_VFO, 1);
  vTaskDelay(pdMS_TO_TICKS(10000));

  radio_set_rxtx_mode(MODE_QSK_COUNTDOWN);
}