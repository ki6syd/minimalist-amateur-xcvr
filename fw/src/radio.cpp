#include "globals.h"
#include "radio.h"
#include "radio_hf.h"
#include "radio_vhf.h"
#include "audio.h"
#include "io.h"
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
radio_band_t band = BAND_UNKNOWN;
radio_filt_sweep_t sweep_config;
radio_band_capability_t band_capability[NUMBER_BANDS];
radio_filt_properties_t if_properties, bpf_properties;

QueueHandle_t xRadioQueue;
TaskHandle_t xRadioTaskHandle;
TimerHandle_t xQskTimer = NULL;

bool ok_to_tx = false;

uint64_t freq_dial = 14040000;

void radio_task(void * pvParameter);
void qsk_timer_callback(TimerHandle_t timer);
bool radio_freq_is_hf(uint64_t freq_dial);

void radio_init() {
  // find out what bands are enabled by looking at hardware file
  fs_load_bands(HARDWARE_FILE, band_capability);

  hf_init();
  vhf_init();

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
    &xRadioTaskHandle,
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
    if(xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifiedValue, pdMS_TO_TICKS(5)) == pdTRUE) {
      if(notifiedValue & NOTIFY_KEY_OFF) {
        // set sidetone, before doing anything else. Minimize audio delay.
        audio_en_sidetone(false);

        // initiate mode change
        radio_set_rxtx_mode(MODE_QSK_COUNTDOWN);

        // turn off sidetone, LED, TX power amp rail, VHF tx_en, etc
        digitalWrite(PA_VDD_CTRL, LOW);
        digitalWrite(VHF_PTT, HIGH);
        digitalWrite(LED_RED, LOW);
      }
      if(notifiedValue & NOTIFY_KEY_ON) {
        // set sidetone, before doing anything else. Minimize audio delay.
        if(ok_to_tx && radio_freq_is_hf(freq_dial))
          audio_en_sidetone(true);

        // initiate mode change
        radio_set_rxtx_mode(MODE_TX);

        // turn off sidetone, LED, TX power amp rail, VHF tx_en, etc
        digitalWrite(LED_RED, HIGH);
        
        if(ok_to_tx) {
          if(radio_freq_is_hf(freq_dial))
            digitalWrite(PA_VDD_CTRL, HIGH);
          else
            digitalWrite(VHF_PTT, LOW);          
        }
      }
      if(notifiedValue & NOTIFY_QSK_EXPIRE) {
        radio_set_rxtx_mode(MODE_RX);
      }
      if(notifiedValue & NOTIFY_FREQ_CHANGE) {
        // wait for zero ticks, don't want to block here
        if(xQueueReceive(xRadioQueue, (void *) &tmp, 0) == pdTRUE) {
            Serial.print("Queue message: ");
            Serial.println(tmp.dial_freq);

            // double check that frequency is valid.
            if(!radio_freq_valid(tmp.dial_freq))
                continue;

            // reject frequency changes while transmitting if they force a band change
            radio_band_t new_band = radio_get_band(tmp.dial_freq);
            if(new_band != band && rxtx_mode == MODE_TX) {
                Serial.println("***Couldn't change freq while TX");
                // TODO: put the frequency change request back in the queue?
                continue;
            }

            // different logic depending on HF or VHF requested frequency
            if(radio_freq_is_hf(tmp.dial_freq)) {
                freq_dial = tmp.dial_freq;
                hf_set_dial_freq(freq_dial);
                
                // force a relay update if needed
                if(new_band != band)
                    radio_set_band(new_band);

                // TODO: unpack any bandwidth changes from tmp.bw
                // hf_set_clocks() needs to know the audio filter to account for sidetone offset?
            }
            else {
              // update relays, enable module (if needed)
              if(band != BAND_VHF)
                  radio_set_band(BAND_VHF);

              // only update frequency and reprogram module if configuration success
              if(vhf_is_configured()) {
                freq_dial = tmp.dial_freq;
                vhf_set_freq(freq_dial);
              }
            }

            // call to set_rxtx_mode() to force an audio path change if needed
            radio_set_rxtx_mode(MODE_QSK_COUNTDOWN);
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
void qsk_timer_callback(TimerHandle_t timer) {
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

        if(radio_freq_is_hf(freq_dial)) {
          // change audio mode, function will ignore if there's no change
          audio_set_mode(AUDIO_HF_RXTX_CW);

          // set up clocks
          // TODO: don't expose si5351 directly to this module
          si5351.output_enable(SI5351_IDX_BFO, 1);
          si5351.output_enable(SI5351_IDX_VFO, 1);
          si5351.output_enable(SI5351_IDX_TX, 0);

          // allow audio to pass through
          audio_en_rx_audio(true);
        }
        else {
            audio_set_mode(AUDIO_VHF_RX);
            
            // TODO: turn off si5351 clocks for VHF mode
        }
        // no sidetone in RX mode, regardless of VHF or HF
        // TODO: delete this? sidetone controls happen at key_on and key_off
        audio_en_sidetone(false);

        // change over relays if needed
        // TODO: rework the radio_set_band(band) function so it is "radio_set_relays(freq)" and looks up band from dial freq
        radio_set_band(band);

        break;
    case MODE_QSK_COUNTDOWN:
        // restart QSK counter upon entry to QSK_COUNTDOWN mode
        if(xTimerReset(xQskTimer, 0) != pdPASS) {
            Serial.println("**** failed to restart qsk timer");
        }
        // update mode 
        rxtx_mode = MODE_QSK_COUNTDOWN;

        if(radio_freq_is_hf(freq_dial)) {
            // turn off RX audio
            // TX power amp rail, sidetone, and TX LED are handled elsewhere
            audio_en_rx_audio(false);

            // no need to update clocks, was just in TX
            // no need to update relays when mode changes to QSK, was just in TX

            // delay so VDD can discharge
            // implemented on entry to QSK_COUNTDOWN to guarantee that we can't immediately transition to RX 
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        else {
            // no action needed for VHF, QSK_COUNTDOWN timer could be zero.
        }

        break;
    case MODE_TX:
        // stop QSK counter
        xTimerStop(xQskTimer, 0);

        // update mode so the next function calls assume TX
        rxtx_mode = MODE_TX;

        if(radio_freq_is_hf(freq_dial)) {
          // change audio mode, function will ignore if there's no change
          audio_set_mode(AUDIO_HF_RXTX_CW);

          // turn off RX audio
          // TX power amp rail, sidetone, and TX LED are handled elsewhere
          audio_en_rx_audio(false);

          // set up clocks. TX clock always running in TX mode
          si5351.output_enable(SI5351_IDX_BFO, 0);
          si5351.output_enable(SI5351_IDX_VFO, 0);
          si5351.output_enable(SI5351_IDX_TX, 1);
        }
        else {
          audio_set_mode(AUDIO_VHF_TX);

          // TODO: turn off si5351 clocks for VHF mode
        }

        // change over relays if needed. Add some settling time
        // TODO: rework the radio_set_band(band) fundtion so it is "radio_set_relays(freq)" and looks up band from dial freq
        radio_set_band(band);        

        break;
    case MODE_SELF_TEST:
      rxtx_mode = new_mode;
      return;
  }
}

// this function does NOT care whether transmit is active currently. Blindly sets the correct relays/mux based on the band selection
// mode change safety is handled by dial frequency updates
// TODO: this is actually a relay setting function, break out into something that accepts a band and a mode
// setting band to anything but BAND_VHF will result in powerdown
void radio_set_band(radio_band_t new_band) {
    if(rxtx_mode == MODE_RX || rxtx_mode == MODE_QSK_COUNTDOWN) {
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
            case BAND_VHF:
                digitalWrite(BPF_SEL_0, HIGH);
                digitalWrite(BPF_SEL_1, HIGH);
                break;
            default:
                Serial.print("Invalid band request: ");
                Serial.println(new_band);
                return;
        }
        digitalWrite(LPF_SEL_0, LOW);
        digitalWrite(LPF_SEL_1, LOW);
        digitalWrite(TX_RX_SEL, LOW);
    }
    else if(rxtx_mode == MODE_TX) {
        switch(new_band) {
            case BAND_HF_1:
                digitalWrite(LPF_SEL_0, LOW);
                digitalWrite(LPF_SEL_1, LOW);
                digitalWrite(TX_RX_SEL, HIGH);
                break;
            case BAND_HF_2:
                digitalWrite(LPF_SEL_0, HIGH);
                digitalWrite(LPF_SEL_1, LOW);
                digitalWrite(TX_RX_SEL, HIGH);
                break;
            case BAND_HF_3:
                digitalWrite(LPF_SEL_0, LOW);
                digitalWrite(LPF_SEL_1, HIGH);
                digitalWrite(TX_RX_SEL, HIGH);
                break;
            case BAND_VHF:
                digitalWrite(LPF_SEL_0, LOW);
                digitalWrite(LPF_SEL_1, LOW);
                digitalWrite(TX_RX_SEL, LOW);
                break;
            default:
                Serial.print("Invalid band request: ");
                Serial.println(new_band);
                return;
        }
        digitalWrite(BPF_SEL_0, HIGH);
        digitalWrite(BPF_SEL_1, HIGH);
    }
    else if(rxtx_mode == MODE_SELF_TEST) {
      Serial.println(radio_band_to_string(new_band));
        // turn this off, just to be safe in selftest mode
        digitalWrite(PA_VDD_CTRL, LOW);
        // set TX_RX_SEL to LOW to get more signal
        digitalWrite(TX_RX_SEL, HIGH);    
        switch(new_band) {
            case BAND_HF_1:
              digitalWrite(BPF_SEL_0, LOW);
              digitalWrite(BPF_SEL_1, LOW);
              digitalWrite(LPF_SEL_0, LOW);
              digitalWrite(LPF_SEL_1, LOW);
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
              digitalWrite(LPF_SEL_0, LOW);
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

  // delay if there's a transition in bands (to let relays settle)
  if(band != new_band) 
    vTaskDelay(pdMS_TO_TICKS(5));

  // VHF --> HF
  if(band == BAND_VHF && new_band != BAND_VHF)
    vhf_powerdown();

  // HF --> VHF
  if(band != BAND_VHF && new_band == BAND_VHF) {
    // connect to VHF module if it's not already configured
    if(!vhf_is_configured()) {
      io_set_blink_mode(BLINK_STARTUP);
      // attempt to connect, powerdown if it doesn't work
      if(vhf_handshake()) {
        io_set_blink_mode(BLINK_NORMAL);
      }
      else {
        io_set_blink_mode(BLINK_ERROR);
        vTaskDelay(pdMS_TO_TICKS(1000));
        vhf_powerdown();
        // return now, before "band" can be updated
        return;
      }
    }
  }

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
  if(band == BAND_VHF) {
    String result = "TX: ";
    result += String(freq_dial);
    return result;
  }
  else
    return hf_freq_string();
}

float radio_get_s_meter() {
  if(radio_freq_is_hf(freq_dial))
    return hf_get_s_meter();
  else
    return vhf_get_s_meter();
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

// TODO: add a flag for enabling TX, this can only disable (until next boot)
void radio_enable_tx(bool en) {
  if(!en && ok_to_tx)
    xTaskNotify(xRadioTaskHandle, NOTIFY_LOW_BAT, eSetBits);
}

bool radio_freq_is_hf(uint64_t dial_freq) {
    radio_band_t checking = radio_get_band(dial_freq);

    if(checking == BAND_VHF)
        return false;
    else
        return true;
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
    case DEBUG_STOP_CLOCKS: {
      si5351.output_enable(SI5351_IDX_TX, 0);
      si5351.output_enable(SI5351_IDX_BFO, 0);
      si5351.output_enable(SI5351_IDX_VFO, 0);
    }
  }
}