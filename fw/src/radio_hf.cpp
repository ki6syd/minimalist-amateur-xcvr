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

uint64_t freq_if_lower = 9998500;
uint64_t freq_if_upper = 10001500;
uint64_t freq_vfo = 0;
uint64_t freq_bfo = 0;

// variable to track what value of audio_get_rx_db() corresponds to the #define'd S_UNIT_REF above
float audio_level_sREF = -49.4;

void hf_si5351_init();
void hf_set_clocks(uint64_t freq_bfo, uint64_t freq_vfo, uint64_t freq_rf);
void radio_sweep_analyze(radio_filt_sweep_t sweep, float *data, radio_filt_properties_t *properties);

void hf_init() {
  pinMode(BPF_SEL_0, OUTPUT);
  pinMode(BPF_SEL_1, OUTPUT);
  pinMode(LPF_SEL_0, OUTPUT);
  pinMode(LPF_SEL_1, OUTPUT);
  pinMode(TX_RX_SEL, OUTPUT);
  pinMode(PA_VDD_CTRL, OUTPUT);

  digitalWrite(PA_VDD_CTRL, LOW);   // VDD off
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

  // TODO: error checking. Maybe add a function for enforcing bounds on settings loaded from json
  freq_if_lower = fs_load_setting(HARDWARE_FILE, "freq_if_lower").toInt();
  freq_if_upper = fs_load_setting(HARDWARE_FILE, "freq_if_upper").toInt();

  Wire.begin(CLOCK_SDA, CLOCK_SCL);

  Serial.print("[SI5351] Status: ");
  Serial.println(si5351.si5351_read(SI5351_DEVICE_STATUS));

  // TODO: parse and have logic based on SI5351 status
  // expect to see a "0" when ANDing with (1<<3), this would indicate that there is no LOS related to the XTAL. 

  si5351.init(SI5351_CRYSTAL_LOAD_8PF , si5351_xtal_freq, 0);

  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);

  si5351.drive_strength(SI5351_IDX_BFO, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_IDX_VFO, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_IDX_TX, SI5351_DRIVE_8MA);

  hf_set_dial_freq(radio_get_dial_freq());

  // the first call to si5351.set_freq() will enable the clocks. Turn them off
  si5351.output_enable(SI5351_IDX_BFO, 0);
  si5351.output_enable(SI5351_IDX_VFO, 0);
  si5351.output_enable(SI5351_IDX_TX, 0);
}

// helper function that only radio.cpp or radio_hf.cpp should call
// inputs: dial frequency, crystal edge frequencies, sidetone frequency, CW vs SSB
// result: sets the VFO and BFO frequencies
// assumes typical logic of USB above 10MHz and LSB below 10MHz. Will need an update for FT8
// note that we use F_SIDETONE_DEFAULT rather than get_sidetone_freq() because sidetone freq() is changing during FT8
void hf_set_dial_freq(uint64_t freq_dial) {
  if(freq_dial < 10000000) {
    freq_vfo = freq_if_lower - freq_dial;
    freq_bfo = freq_if_lower - ((uint64_t) F_SIDETONE_DEFAULT);
  }
  else {
    freq_vfo = freq_dial + freq_if_upper;
    freq_bfo = freq_if_upper + ((uint64_t) F_SIDETONE_DEFAULT);
  }
#ifdef RX_ARCHITECTURE_QSD
  // multiple BFO frequency by 4x if we are using a QSD and 90deg divider circuit
  freq_bfo = 4 * (freq_dial + ((uint64_t) audio_get_sidetone_freq()));
#endif

  hf_set_clocks(freq_bfo, freq_vfo, freq_dial);
}


void hf_set_clocks(uint64_t freq_bfo, uint64_t freq_vfo, uint64_t freq_rf) {
  si5351.set_freq(freq_bfo * 100, SI5351_IDX_BFO);
  si5351.set_freq(freq_vfo * 100, SI5351_IDX_VFO);
  si5351.set_freq(freq_rf * 100, SI5351_IDX_TX);

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
  result += "BFO: ";
  result += String(freq_bfo);
  result += "\tVFO: ";
  result += String(freq_vfo);
  result += "\tTX: ";
  result += String(radio_get_dial_freq());
  return result;
}

// turn on TX for 5 seconds so nearby radio can listen for 10MHz carrier
void hf_cal_tx_10MHz() {
  // ensure VDD is turned off
  digitalWrite(PA_VDD_CTRL, LOW);
  vTaskDelay(pdMS_TO_TICKS(50));

  si5351.set_freq(((uint64_t) 10000000) * 100, SI5351_IDX_TX);
  si5351.output_enable(SI5351_IDX_BFO, 0);
  si5351.output_enable(SI5351_IDX_VFO, 0);
  si5351.output_enable(SI5351_IDX_TX, 1);
  vTaskDelay(pdMS_TO_TICKS(10000));

  radio_set_rxtx_mode(MODE_QSK_COUNTDOWN);
}


// stop the VFO, enable BFO, sweep TX-CLK injection and observe amplitude
// assumes that si5351 crystal is already calibrated
void hf_cal_if_filt(radio_filt_sweep_t sweep, radio_filt_properties_t *properties) {
  // ensure VDD is turned off
  digitalWrite(PA_VDD_CTRL, LOW);
  vTaskDelay(pdMS_TO_TICKS(50));
  
  bool pga_init = audio_get_pga();
  float volume_init = audio_get_volume();
  
  Serial.println("\n\nCalibrating IF filter...");
  digitalWrite(LED_RED, HIGH);

  audio_en_pga(false);
  audio_en_sidetone(false);
  audio_set_volume(AUDIO_VOL_DURING_CAL);  // can mute for quiet startup
  audio_en_rx_audio(true);
  audio_set_filt(AUDIO_FILT_SSB);
  
  // order of these two function calls matters
  radio_set_rxtx_mode(MODE_SELF_TEST);
  radio_set_band(BAND_SELFTEST_LOOPBACK);

  vTaskDelay(100 / portTICK_PERIOD_MS);

  // turn on TX-CLK, turn off other clocks
  si5351.output_enable(SI5351_IDX_BFO, 1);
  si5351.output_enable(SI5351_IDX_VFO, 0);
  si5351.output_enable(SI5351_IDX_TX, 1);

  // wait to let audio settle
  vTaskDelay(500 / portTICK_PERIOD_MS);

  // sweep TX-CLK
  int16_t step_size = sweep.f_span / sweep.num_steps;
  float measurements[sweep.num_steps];
  for(int16_t i = 0; i < sweep.num_steps; i++)
  {
    uint64_t f_tx = sweep.f_center + (uint64_t) step_size * (i - sweep.num_steps/2);
    si5351.set_freq(f_tx * 100, SI5351_IDX_TX);
    si5351.set_freq(((uint64_t) f_tx - audio_get_sidetone_freq()) * 100, SI5351_IDX_BFO);
    vTaskDelay(pdMS_TO_TICKS(100));

    measurements[i] = audio_get_rx_db(sweep.num_to_avg, 10);
  }

  // analyze sweep and save findings
  radio_sweep_analyze(sweep, measurements, properties);
  freq_if_lower = properties->f_lower;
  freq_if_upper = properties->f_upper;

  // TODO: logic to broaden the search if the upper or lower index are at the edges of the sweep
  digitalWrite(LED_RED, LOW);
  radio_set_band(radio_get_band(radio_get_dial_freq()));
  radio_set_rxtx_mode(MODE_RX);
  audio_set_filt(AUDIO_FILT_DEFAULT);
  audio_en_pga(pga_init);
  audio_set_volume(volume_init);
  audio_en_rx_audio(true);
  Serial.println("Routine complete.\n\n");
}

// data matches less well than the IF filter calibration. Better to do VNA and testpoints.
void hf_cal_bpf_filt(radio_band_t band, radio_filt_sweep_t sweep, radio_filt_properties_t *properties) {
  // ensure VDD is turned off
  digitalWrite(PA_VDD_CTRL, LOW);
  vTaskDelay(pdMS_TO_TICKS(50));
  
  bool pga_init = audio_get_pga();
  float volume_init = audio_get_volume();

  Serial.println("\n\nCalibrating Bandpass filter...");
  digitalWrite(LED_RED, HIGH);

  // order of these two function calls matters
  radio_set_rxtx_mode(MODE_SELF_TEST);
  radio_set_band(band);

  audio_en_pga(false);
  audio_en_sidetone(false);
  audio_en_vol_clipping(false);
  audio_set_volume(AUDIO_VOL_DURING_CAL);  // can mute for quiet startup
  audio_en_rx_audio(true);
  audio_set_filt(AUDIO_FILT_SSB);

  // turn on all clocks for this self-test
  si5351.output_enable(SI5351_IDX_BFO, 1);
  si5351.output_enable(SI5351_IDX_VFO, 1);
  si5351.output_enable(SI5351_IDX_TX, 1);

  // reduce clock strength
  si5351.drive_strength(SI5351_IDX_TX, SI5351_DRIVE_2MA);

  // wait to let audio settle
  vTaskDelay(100 / portTICK_PERIOD_MS);

  int64_t step_size = ((int64_t) sweep.f_span) / sweep.num_steps;
  float measurements[sweep.num_steps];
  for(uint16_t i = 0; i < sweep.num_steps; i++)
  {
    int64_t dF = step_size * (((int64_t) i) - (int64_t) sweep.num_steps/2);
    uint64_t freq_dial = (uint64_t) (sweep.f_center + dF);
    hf_set_dial_freq(freq_dial);
    vTaskDelay(pdMS_TO_TICKS(50));

    measurements[i] = audio_get_rx_db(sweep.num_to_avg, 10);
  }

  radio_sweep_analyze(sweep, measurements, properties);

  digitalWrite(LED_RED, LOW);
  // radio_set_dial_freq(sweep.f_center);
  radio_set_rxtx_mode(MODE_RX);
  audio_set_filt(AUDIO_FILT_DEFAULT);
  audio_en_pga(pga_init);
  audio_en_vol_clipping(true);
  audio_set_volume(volume_init);
  audio_en_rx_audio(true);
  Serial.println("Routine complete.\n\n");
}