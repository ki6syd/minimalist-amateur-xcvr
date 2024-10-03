#include "globals.h"
#include "radio.h"
#include "radio_hf.h"
#include "radio_vhf.h"
#include "audio.h"
#include "file_system.h"

#include <Arduino.h>

#define NOTIFY_KEY_ON         (1 << 0)
#define NOTIFY_KEY_OFF        (1 << 1)
#define NOTIFY_QSK_EXPIRE     (1 << 2)
#define NOTIFY_FREQ_CHANGE    (1 << 3)
#define NOTIFY_CAL_XTAL       (1 << 4)
#define NOTIFY_CAL_IF         (1 << 5)
#define NOTIFY_CAL_BPF        (1 << 6)
#define NOTIFY_LOW_BAT        (1 << 7)



radio_audio_bw_t bw = BW_CW;
radio_rxtx_mode_t rxtx_mode = MODE_STARTUP;
radio_band_t band = BAND_HF_2;
radio_filt_sweep_t sweep_config;
radio_band_capability_t band_capability[NUMBER_BANDS];
radio_filt_properties_t if_properties, bpf_properties;

QueueHandle_t xRadioQueue;
TaskHandle_t xHFTaskHandle;
TimerHandle_t xQskTimer = NULL;

bool ok_to_tx = false;

uint64_t freq_dial = 14040000;

void radio_task(void * pvParameter);
void qsk_timer_callback(TimerHandle_t timer);



void radio_init() {
  // find out what bands are enabled by looking at hardware file
  fs_load_bands(HARDWARE_FILE, band_capability);

  hf_init();
//   vhf_init();

#ifdef RADIO_ALLOW_TX
  ok_to_tx = true;
#endif

  // create the message queue
  // TODO: parametrize the length of this queue
  xRadioQueue = xQueueCreate(10, sizeof(radio_state_t));

  xTaskCreatePinnedToCore(
    radio_task,
    "Radio Task",
    16384,
    NULL,
    TASK_PRIORITY_RADIO, // priority
    &xHFTaskHandle,
    TASK_CORE_RADIO // core
  );

  // create the QSK timer
  xQskTimer = xTimerCreate(
    "QskTimer",
    ( 500 / portTICK_PERIOD_MS),
    pdFALSE,                          // one shot timer
    0,
    qsk_timer_callback          // callback function
  );
  xTimerStart(xQskTimer, 0);

  radio_set_dial_freq(14060000);
  radio_set_rxtx_mode(MODE_RX);
  radio_set_band(band);
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
        // turn on power amp rail if flag allows
        if(ok_to_tx)
          digitalWrite(PA_VDD_CTRL, HIGH);
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
          hf_set_dial_freq(freq_dial);
          
          // figure out which band we should be on
          radio_band_t new_band = radio_get_band(freq_dial);
          
          // force a relay update if needed
          if(new_band != band)
            radio_set_band(new_band);

          // TODO: unpack any bandwidth changes from tmp.bw
          // hf_set_clocks() needs to know the audio filter to account for sidetone offset?
        }
      }
      if(notifiedValue & NOTIFY_CAL_XTAL) {
        hf_cal_tx_10MHz();
      }
      if(notifiedValue & NOTIFY_CAL_IF) {
        hf_cal_if_filt(sweep_config, &if_properties);
      }
      if(notifiedValue & NOTIFY_CAL_BPF) {
        radio_band_t band_to_sweep = radio_get_band(sweep_config.f_center);
        hf_cal_bpf_filt(band_to_sweep, sweep_config, &bpf_properties);
      }
      if(notifiedValue & NOTIFY_LOW_BAT) {
        ok_to_tx = false;
      }
    }
  }
}

// when the QSK timer expires, transition into RX mode
// TODO: test this
void qsk_timer_callback(TimerHandle_t timer) {
  xTaskNotify(xHFTaskHandle, NOTIFY_QSK_EXPIRE, eSetBits);
}


void radio_key_on() {
  xTaskNotify(xHFTaskHandle, NOTIFY_KEY_ON, eSetBits);
}

void radio_key_off() {
  xTaskNotify(xHFTaskHandle, NOTIFY_KEY_OFF, eSetBits);
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

  xTaskNotify(xHFTaskHandle, NOTIFY_FREQ_CHANGE, eSetBits);

  return true;
}

uint64_t radio_get_dial_freq() {
  return freq_dial;
}

radio_audio_bw_t radio_get_bw() {
  return bw;
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
    // TODO: check HF vs VHF
    return hf_freq_string();
}

float radio_get_s_meter() {
    // TODO: check HF vs VHF
    return hf_get_s_meter();
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



void radio_enable_tx(bool en) {
  if(!en && ok_to_tx)
    xTaskNotify(xHFTaskHandle, NOTIFY_LOW_BAT, eSetBits);
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
      xTaskNotify(xHFTaskHandle, NOTIFY_CAL_XTAL, eSetBits);
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

      xTaskNotify(xHFTaskHandle, NOTIFY_CAL_IF, eSetBits);
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

      xTaskNotify(xHFTaskHandle, NOTIFY_CAL_BPF, eSetBits);
      break;
    }
    case DEBUG_STOP_CLOCKS: {
      si5351.output_enable(SI5351_IDX_TX, 0);
      si5351.output_enable(SI5351_IDX_BFO, 0);
      si5351.output_enable(SI5351_IDX_VFO, 0);
    }
  }
}