#include "globals.h"
#include "radio_hf.h"
#include "audio.h"
#include "file_system.h"

#include <Arduino.h>
#include <si5351.h>

#define NOTIFY_KEY_ON         (1 << 0)
#define NOTIFY_KEY_OFF        (1 << 1)
#define NOTIFY_QSK_EXPIRE     (1 << 2)
#define NOTIFY_FREQ_CHANGE    (1 << 3)
#define NOTIFY_CAL_XTAL       (1 << 4)
#define NOTIFY_CAL_IF         (1 << 5)
#define NOTIFY_CAL_BPF        (1 << 6)

#define SI5351_IDX_BFO    SI5351_CLK0
#define SI5351_IDX_VFO    SI5351_CLK1
#define SI5351_IDX_TX     SI5351_CLK2

Si5351 si5351;

QueueHandle_t xRadioQueue;

TaskHandle_t xRadioTaskHandle;
TimerHandle_t xQskTimer = NULL;

radio_audio_bw_t bw = BW_CW;
radio_rxtx_mode_t rxtx_mode = MODE_STARTUP;
radio_band_t band = BAND_HF_2;
radio_filt_sweep_t sweep_config;
radio_filt_properties_t if_properties, bpf_properties;
radio_band_capability_t band_capability[NUMBER_BANDS];

uint64_t freq_dial = 14040000;
uint64_t freq_if_lower = 9998500;
uint64_t freq_if_upper = 10001500;
uint64_t freq_vfo = 0;
uint64_t freq_bfo = 0;

void radio_si5351_init();
void radio_calc_clocks();
void radio_set_clocks(uint64_t freq_bfo, uint64_t freq_vfo, uint64_t freq_rf);
void radio_set_rxtx_mode(radio_rxtx_mode_t new_mode);
void radio_set_band(radio_band_t new_band);

void radio_task (void * pvParameter);
void radio_qsk_timer_callback(TimerHandle_t timer);

// calibration related functions
void radio_sweep_analyze(radio_filt_sweep_t sweep, float *data, radio_filt_properties_t *properties);
void radio_cal_tx_10MHz();
void radio_cal_if_filt(radio_filt_sweep_t sweep, radio_filt_properties_t *properties);
void radio_cal_bpf_filt(radio_band_t band, radio_filt_sweep_t sweep, radio_filt_properties_t *properties);

void radio_hf_init() {
  // find out what bands are enabled by looking at hardware file
  fs_load_bands(HARDWARE_FILE, band_capability);

  pinMode(BPF_SEL_0, OUTPUT);
  pinMode(BPF_SEL_1, OUTPUT);
  pinMode(LPF_SEL_0, OUTPUT);
  pinMode(LPF_SEL_1, OUTPUT);
  pinMode(TX_RX_SEL, OUTPUT);
  pinMode(PA_VDD_CTRL, OUTPUT);

  digitalWrite(PA_VDD_CTRL, LOW);   // VDD off
  digitalWrite(TX_RX_SEL, LOW);     // RX mode

  radio_si5351_init();

  // create the message queue
  // TODO: parametrize the length of this queue
  xRadioQueue = xQueueCreate(10, sizeof(radio_state_t));

  xTaskCreatePinnedToCore(
    radio_task,
    "Radio Task",
    16384,
    NULL,
    TASK_PRIORITY_RADIO, // priority
    &xRadioTaskHandle,
    TASK_CORE_RADIO // core
  );

  // create the QSK timer
  xQskTimer = xTimerCreate(
    "QskTimer",
    ( 500 / portTICK_PERIOD_MS),
    pdFALSE,                          // one shot timer
    0,
    radio_qsk_timer_callback          // callback function
  );
  xTimerStart(xQskTimer, 0);

  radio_set_dial_freq(14060000);
  radio_set_rxtx_mode(MODE_RX);
  radio_set_band(band);
}

void radio_si5351_init() {
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
  si5351.drive_strength(SI5351_IDX_TX, SI5351_DRIVE_2MA);

  radio_calc_clocks();
  radio_set_clocks(freq_bfo, freq_vfo, freq_dial);
  // the first call to si5351.set_freq() will enable the clocks. Turn them off
  si5351.output_enable(SI5351_IDX_BFO, 0);
  si5351.output_enable(SI5351_IDX_VFO, 0);
  si5351.output_enable(SI5351_IDX_TX, 0);

}

void radio_task(void *param) {
  radio_state_t tmp;
  uint32_t notifiedValue;

  while(true) {
    // look for flags. Don't clear on entry; clear on exit
    // example: https://freertos.org/Documentation/02-Kernel/02-Kernel-features/03-Direct-to-task-notifications/04-As-event-group
    if(xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifiedValue, pdMS_TO_TICKS(10)) == pdTRUE) {
      if(notifiedValue & NOTIFY_KEY_OFF) {
        // initiate mode change
        radio_set_rxtx_mode(MODE_QSK_COUNTDOWN);

        // turn off sidetone, LED, TX power amp rail
        audio_en_sidetone(false);
        digitalWrite(LED_RED, LOW);
        digitalWrite(PA_VDD_CTRL, LOW);
      }
      if(notifiedValue & NOTIFY_KEY_ON) {
        // initiate mode change
        radio_set_rxtx_mode(MODE_TX);

        // turn off sidetone, LED, TX power amp rail
        audio_en_sidetone(true);
        digitalWrite(LED_RED, HIGH);
        // turn on power amp rail if build flags allow
#ifdef RADIO_ALLOW_TX
        digitalWrite(PA_VDD_CTRL, HIGH);
#endif
      }
      if(notifiedValue & NOTIFY_QSK_EXPIRE) {
        radio_set_rxtx_mode(MODE_RX);
      }
      if(notifiedValue & NOTIFY_FREQ_CHANGE) {
        // wait for zero ticks, don't want to block here
        if(xQueueReceive(xRadioQueue, (void *) &tmp, 0) == pdTRUE) {
          Serial.print("Queue message: ");
          Serial.println(tmp.dial_freq);

          // TODO: block the frequncy change if it's a different band, and we are already transmitting

          freq_dial = tmp.dial_freq;
          radio_calc_clocks();
          radio_set_clocks(freq_bfo, freq_vfo, freq_dial);

          // figure out which band we should be on
          radio_band_t new_band = radio_get_band(freq_dial);
          
          // force a relay update if needed
          if(new_band != band)
            radio_set_band(new_band);

          // TODO: unpack any bandwidth changes from tmp.bw
          // radio_set_clocks() needs to know the audio filter to account for sidetone offset?
        }
      }
      if(notifiedValue & NOTIFY_CAL_XTAL) {
        radio_cal_tx_10MHz();
      }
      if(notifiedValue & NOTIFY_CAL_IF) {
        radio_cal_if_filt(sweep_config, &if_properties);
        freq_if_lower = if_properties.f_lower;
        freq_if_upper = if_properties.f_upper;
      }
      if(notifiedValue & NOTIFY_CAL_BPF) {
        radio_band_t band_to_sweep = radio_get_band(sweep_config.f_center);
        radio_cal_bpf_filt(band_to_sweep, sweep_config, &bpf_properties);
      }
    }
  }
}

// when the QSK timer expires, transition into RX mode
// TODO: test this
void radio_qsk_timer_callback(TimerHandle_t timer) {
  xTaskNotify(xRadioTaskHandle, NOTIFY_QSK_EXPIRE, eSetBits);
}


void radio_key_on() {
  xTaskNotify(xRadioTaskHandle, NOTIFY_KEY_ON, eSetBits);
}

void radio_key_off() {
  xTaskNotify(xRadioTaskHandle, NOTIFY_KEY_OFF, eSetBits);
}

// helper function to REQUEST a frequency change
bool radio_set_dial_freq(uint64_t freq) {
  if(!radio_freq_valid(freq))
    return false;

  radio_state_t tmp = { .dial_freq = freq, .bw = radio_get_bw()};

  if(xQueueSend(xRadioQueue, (void *) &tmp, 0) != pdTRUE) {
    Serial.println("Unable to change frequency, queue full");
    return false;
  }

  xTaskNotify(xRadioTaskHandle, NOTIFY_FREQ_CHANGE, eSetBits);

  return true;
}

uint64_t radio_get_dial_freq() {
  return freq_dial;
}

radio_audio_bw_t radio_get_bw() {
  return bw;
}

// inputs: dial frequency, crystal edge frequencies, sidetone frequency, CW vs SSB
// result: sets the VFO and BFO frequencies
// assumes typical logic of USB above 10MHz and LSB below 10MHz. Will need an update for FT8
void radio_calc_clocks() {
  if(freq_dial < 10000000) {
    freq_vfo = freq_dial - freq_if_lower;
    freq_bfo = freq_if_lower - ((uint64_t) audio_get_sidetone_freq());
  }
  else {
    freq_vfo = freq_dial + freq_if_upper;
    freq_bfo = freq_if_upper + ((uint64_t) audio_get_sidetone_freq());
  }
#ifdef RX_ARCHITECTURE_QSD
  // multiple BFO frequency by 4x if we are using a QSD and 90deg divider circuit
  freq_bfo *= 4;
#endif

}

void radio_set_clocks(uint64_t freq_bfo, uint64_t freq_vfo, uint64_t freq_rf) {
  si5351.set_freq(freq_bfo * 100, SI5351_IDX_BFO);
  si5351.set_freq(freq_vfo * 100, SI5351_IDX_VFO);
  si5351.set_freq(freq_rf * 100, SI5351_IDX_TX);

  // Serial.println(radio_freq_string());
}

void radio_set_rxtx_mode(radio_rxtx_mode_t new_mode) {
  // return immediately if there was no change requested
  if(new_mode == rxtx_mode)
    return;

  // intervene on invalid request: going from TX direct to RX
  if(rxtx_mode == MODE_TX && new_mode == MODE_RX)
    new_mode = MODE_QSK_COUNTDOWN;

  switch(new_mode) {
    case MODE_RX:
      // stop QSK counter, in case it was still running
      xTimerStop(xQskTimer, 0);

      // update mode so the next function calls assume TX
      rxtx_mode = MODE_RX;

      // set up clocks
      si5351.output_enable(SI5351_IDX_BFO, 1);
      si5351.output_enable(SI5351_IDX_VFO, 1);
      si5351.output_enable(SI5351_IDX_TX, 0);

      // change over relays if needed. Add some settling time
      // TODO: rework the radio_set_band(band) fundtion so it is "radio_set_relays(freq)" and looks up band from dial freq
      radio_set_band(band);

      // enable RX audio
      audio_en_rx_audio(true);
      audio_en_sidetone(false);

      break;
    case MODE_QSK_COUNTDOWN:
      // restart QSK counter upon entry to QSK_COUNTDOWN mode
      if(xTimerReset(xQskTimer, 0) != pdPASS) {
        Serial.println("**** failed to restart qsk timer");
      }
      // update mode 
      rxtx_mode = MODE_QSK_COUNTDOWN;

      // turn off RX audio
      // TX power amp rail, sidetone, and TX LED are handled elsewhere
      audio_en_rx_audio(false);

      // no need to update clocks, was just in TX
      // no need to update relays when mode changes to QSK, was just in TX

      // delay so VDD can discharge
      // implemented on entry to QSK_COUNTDOWN to guarantee that we can't immediately transition to RX 
      vTaskDelay(pdMS_TO_TICKS(5));

      break;

    case MODE_TX:
      // stop QSK counter
      xTimerStop(xQskTimer, 0);

      // update mode so the next function calls assume TX
      rxtx_mode = MODE_TX;

      // turn off RX audio
      // TX power amp rail, sidetone, and TX LED are handled elsewhere
      audio_en_rx_audio(false);

      // change over relays if needed. Add some settling time
      // TODO: rework the radio_set_band(band) fundtion so it is "radio_set_relays(freq)" and looks up band from dial freq
      radio_set_band(band);

      // set up clocks. TX clock always running in TX mode
      si5351.output_enable(SI5351_IDX_BFO, 0);
      si5351.output_enable(SI5351_IDX_VFO, 0);
      si5351.output_enable(SI5351_IDX_TX, 1);

      break;
    case MODE_SELF_TEST:
      rxtx_mode = new_mode;
      return;
  }
}

// this function does NOT care whether transmit is active currently. Blindly sets the correct relays/mux based on the band selection
// mode change safety is handled by dial frequency updates
// TODO: this is actually a relay setting function, break out into something that accepts a band and a mode
void radio_set_band(radio_band_t new_band) {
  if(rxtx_mode == MODE_RX || rxtx_mode == MODE_QSK_COUNTDOWN) {
    digitalWrite(TX_RX_SEL, LOW);
    switch(new_band) {
      case BAND_HF_1:
        digitalWrite(BPF_SEL_0, LOW);
        digitalWrite(BPF_SEL_1, LOW);
        break;
      case BAND_HF_2:
        digitalWrite(BPF_SEL_0, HIGH);
        digitalWrite(BPF_SEL_1, LOW);
        break;
      case BAND_HF_3:
        digitalWrite(BPF_SEL_0, LOW);
        digitalWrite(BPF_SEL_1, HIGH);
        break;
      default:
        Serial.print("Invalid band request: ");
        Serial.println(new_band);
        return;
    }
    digitalWrite(LPF_SEL_0, LOW);
    digitalWrite(LPF_SEL_1, LOW);
  }
  else if(rxtx_mode == MODE_TX) {
    digitalWrite(TX_RX_SEL, HIGH);
    switch(new_band) {
      case BAND_HF_1:
        digitalWrite(LPF_SEL_0, LOW);
        digitalWrite(LPF_SEL_1, HIGH);
        break;
      case BAND_HF_2:
        digitalWrite(LPF_SEL_0, HIGH);
        digitalWrite(LPF_SEL_1, LOW);
        break;
      case BAND_HF_3:
        digitalWrite(LPF_SEL_0, HIGH);
        digitalWrite(LPF_SEL_1, HIGH);
        break;
      default:
        Serial.print("Invalid band request: ");
        Serial.println(new_band);
        return;
      digitalWrite(BPF_SEL_0, HIGH);
      digitalWrite(BPF_SEL_1, HIGH);
    }
  }
  else if(rxtx_mode == MODE_SELF_TEST) {
    // turn this off, just to be safe in selftest mode
    digitalWrite(PA_VDD_CTRL, LOW);
    // want to engage both BPF and LPF pathways properly
    digitalWrite(TX_RX_SEL, LOW);
    switch(new_band) {
      case BAND_HF_1:
        digitalWrite(BPF_SEL_0, LOW);
        digitalWrite(BPF_SEL_1, LOW);
        digitalWrite(LPF_SEL_0, LOW);
        digitalWrite(LPF_SEL_1, HIGH);
        break;
      case BAND_HF_2:
        digitalWrite(BPF_SEL_0, HIGH);
        digitalWrite(BPF_SEL_1, LOW);
        digitalWrite(LPF_SEL_0, HIGH);
        digitalWrite(LPF_SEL_1, LOW);
        break;
      case BAND_HF_3:
        digitalWrite(BPF_SEL_0, LOW);
        digitalWrite(BPF_SEL_1, HIGH);
        digitalWrite(LPF_SEL_0, HIGH);
        digitalWrite(LPF_SEL_1, HIGH);
        break;
      case BAND_SELFTEST_LOOPBACK:
        digitalWrite(BPF_SEL_0, HIGH);
        digitalWrite(BPF_SEL_1, HIGH);
        digitalWrite(LPF_SEL_0, LOW);
        digitalWrite(LPF_SEL_1, LOW);
        break;
      default:
        Serial.print("Invalid band request: ");
        Serial.println(new_band);
        return;
    }
  }

  // add a delay for relay settling
  // TODO: parametrize this
  vTaskDelay(pdMS_TO_TICKS(2));

  // update band
  band = new_band;
}

radio_band_t radio_get_band(uint64_t freq) {
  for(uint8_t i = 0; i < NUMBER_BANDS; i++) {
    if(freq < band_capability[i].max_freq && freq > band_capability[i].min_freq) {
      return band_capability[i].band_name;
    }
  }
  return BAND_UNKNOWN;
}

bool radio_freq_valid(uint64_t freq) {
  for(uint8_t i = 0; i < NUMBER_BANDS; i++) {
    if(freq < band_capability[i].max_freq && freq > band_capability[i].min_freq)
      return true;
  }

  return false;
}


String radio_band_to_string(radio_band_t band) {
    switch (band) {
        case BAND_HF_1: return "BAND_HF_1";
        case BAND_HF_2: return "BAND_HF_2";
        case BAND_HF_3: return "BAND_HF_3";
        case BAND_VHF: return "BAND_VHF";
        case BAND_SELFTEST_LOOPBACK: return "BAND_SELFTEST_LOOPBACK";
        default: return "UNKNOWN_BAND";
    }
}

// Convert radio_audio_bw_t enum to string
String radio_bandwidth_to_string(radio_audio_bw_t bw) {
    switch (bw) {
        case BW_CW: return "CW";
        case BW_SSB: return "SSB";
        case BW_FM: return "FM";
        default: return "UNKNOWN_BW";
    }
}


String radio_freq_string() {
  String result = "";

  result += "BFO: ";
  result += String(freq_bfo);
  result += "\tVFO: ";
  result += String(freq_vfo);
  result += "\tTX: ";
  result += String(freq_dial);
  return result;
}

// inputs: sweep setup, dataset
// outputs: analyzed properties
void radio_sweep_analyze(radio_filt_sweep_t sweep, float *data, radio_filt_properties_t *properties) {
  uint64_t step_size = sweep.f_span / sweep.num_steps;

  String result = "";
  for(uint16_t i=0; i < sweep.num_steps; i++) {
    int64_t dF = (int64_t) step_size * ((int64_t) i - sweep.num_steps/2);
    result += "dF: ";
    result += String(dF);

    // hack: add a tab if needed
    if(dF >= 0 && dF < 1000)
      result += "\t";

    result += "\t\tVol: ";
    result += String(data[i]);
    result += "\t\t";

    uint16_t bar_size = (uint16_t) abs(data[i]);
    for(uint16_t j = 0; j < bar_size; j++) {
      result += ".";
    }
    result += "\n";
  }
  Serial.println(result);

  // find center of filter (highest signal)
  uint16_t idx_max = 0;
  float meas_max = -1000;
  for(uint16_t i = 0; i < sweep.num_steps; i++) {
    if(data[i] > meas_max) {
      meas_max = data[i];
      idx_max = i;
    }
  }
  properties->f_center = sweep.f_center - ((int16_t) sweep.num_steps/2 - idx_max);

  Serial.print("idx_max: ");
  Serial.print(idx_max);
  Serial.print("\tfreq_max: ");
  Serial.print(properties->f_center);
  Serial.print("\tmeas_max: ");
  Serial.println(meas_max);

  // find lower corner
  uint16_t idx_lower = 0;
  float meas_lower = meas_max;
  for(uint16_t i=idx_max; i > 0; i--) {
    if(abs(meas_max - data[i]) > sweep.rolloff) {
      idx_lower = i;
      meas_lower = data[i];
      break;
    }
  }
  properties->f_lower = sweep.f_center - (sweep.num_steps/2 - idx_lower) * step_size;

  Serial.print("idx_lower: ");
  Serial.print(idx_lower);
  Serial.print("\tfreq_lower: ");
  Serial.print(properties->f_lower);
  Serial.print("\tmeas_lower: ");
  Serial.println(meas_lower);


  // find upper corner
  uint16_t idx_upper = 0;
  float meas_upper = meas_max;
  for(uint16_t i=idx_max; i < sweep.num_steps; i++) {
    if(abs(meas_max - data[i]) > sweep.rolloff) {
      idx_upper = i;
      meas_upper = data[i];
      break;
    }
  }
  properties->f_upper = sweep.f_center + (idx_upper - sweep.num_steps/2) * step_size;

  Serial.print("idx_upper: ");
  Serial.print(idx_upper);
  Serial.print("\tfreq_upper: ");
  Serial.print(properties->f_upper);
  Serial.print("\tmeas_upper: ");
  Serial.println(meas_upper);
}

// turn on TX for 5 seconds so nearby radio can listen for 10MHz carrier
void radio_cal_tx_10MHz() {
  // ensure VDD is turned off
  digitalWrite(PA_VDD_CTRL, LOW);
  vTaskDelay(pdMS_TO_TICKS(50));

  si5351.set_freq(((uint64_t) 10000000) * 100, SI5351_IDX_TX);
  si5351.output_enable(SI5351_IDX_BFO, 0);
  si5351.output_enable(SI5351_IDX_VFO, 0);
  si5351.output_enable(SI5351_IDX_TX, 1);
  vTaskDelay(pdMS_TO_TICKS(5000));

  radio_set_rxtx_mode(MODE_QSK_COUNTDOWN);
}

// stop the VFO, enable BFO, sweep TX-CLK injection and observe amplitude
// assumes that si5351 crystal is already calibrated
void radio_cal_if_filt(radio_filt_sweep_t sweep, radio_filt_properties_t *properties) {
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

  vTaskDelay(500 / portTICK_PERIOD_MS);

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

  radio_sweep_analyze(sweep, measurements, properties);

  // TODO: logic to broaden the search if the upper or lower index are at the edges of the sweep
  digitalWrite(LED_RED, LOW);
  radio_set_rxtx_mode(MODE_RX);
  radio_set_dial_freq(freq_dial);
  audio_set_filt(AUDIO_FILT_DEFAULT);
  audio_en_pga(pga_init);
  audio_set_volume(volume_init);
  audio_en_rx_audio(true);
}

void radio_cal_bpf_filt(radio_band_t band, radio_filt_sweep_t sweep, radio_filt_properties_t *properties) {
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
  audio_set_volume(AUDIO_VOL_DURING_CAL);  // can mute for quiet startup
  audio_en_rx_audio(true);
  audio_set_filt(AUDIO_FILT_SSB);

  // turn on all clocks for this self-test
  si5351.output_enable(SI5351_IDX_BFO, 1);
  si5351.output_enable(SI5351_IDX_VFO, 1);
  si5351.output_enable(SI5351_IDX_TX, 1);

  // wait to let audio settle
  vTaskDelay(500 / portTICK_PERIOD_MS);

  int64_t step_size = ((int64_t) sweep.f_span) / sweep.num_steps;
  float measurements[sweep.num_steps];
  for(uint16_t i = 0; i < sweep.num_steps; i++)
  {
    int64_t dF = step_size * (((int64_t) i) - (int64_t) sweep.num_steps/2);
    freq_dial = (uint64_t) sweep.f_center + dF;
    radio_calc_clocks();
    radio_set_clocks(freq_bfo, freq_vfo, freq_dial);
    vTaskDelay(pdMS_TO_TICKS(50));

    measurements[i] = audio_get_rx_db(sweep.num_to_avg, 10);
  }

  radio_sweep_analyze(sweep, measurements, properties);

  digitalWrite(LED_RED, LOW);
  radio_set_rxtx_mode(MODE_RX);
  radio_set_dial_freq(sweep.f_center);
  audio_set_filt(AUDIO_FILT_DEFAULT);
  audio_en_pga(pga_init);
  audio_set_volume(volume_init);
  audio_en_rx_audio(true);
}

void radio_debug(debug_action_t action, void *value) {
  switch(action) {
    case DEBUG_CMD_TXCLK: {
      bool on_off = *((bool *) value);
      if(on_off)
        si5351.output_enable(SI5351_IDX_TX, 1);
      else
        si5351.output_enable(SI5351_IDX_TX, 0);
      break;
    }
    case DEBUG_CMD_CAL_XTAL: {
      xTaskNotify(xRadioTaskHandle, NOTIFY_CAL_XTAL, eSetBits);
      break;
    }
    case DEBUG_CMD_CAL_IF: {
      sweep_config = {
        .f_center = 10000000,
        .f_span = 8000,
        .num_steps = 30,
        .num_to_avg = 10,
        .rolloff = 3
        };

      xTaskNotify(xRadioTaskHandle, NOTIFY_CAL_IF, eSetBits);
      break;
    }
    case DEBUG_CMD_CAL_BPF: {
      sweep_config = {
        .f_center = radio_get_dial_freq(),
        .f_span = radio_get_dial_freq() / 2,
        .num_steps = 50,
        .num_to_avg = 5,
        .rolloff = 3
        };

      xTaskNotify(xRadioTaskHandle, NOTIFY_CAL_BPF, eSetBits);
      break;
    }
  }
}