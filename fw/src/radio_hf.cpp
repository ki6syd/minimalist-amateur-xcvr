#include "globals.h"
#include "radio_hf.h"
#include "audio.h"

#include <Arduino.h>
#include <si5351.h>

#define NOTIFY_KEY_ON         0
#define NOTIFY_KEY_OFF        1
#define NOTIFY_QSK_EXPIRE     2
#define NOTIFY_FREQ_CHANGE    3

#define SI5351_IDX_BFO    SI5351_CLK0
#define SI5351_IDX_VFO    SI5351_CLK1
#define SI5351_IDX_TX     SI5351_CLK2

// #define CAL_FREQ_ON_STARTUP
// #define CAL_IF_FILT_ON_STARTUP
// #define CAL_BPF_FILT_ON_STARTUP
#define CAL_GRAPHICS

Si5351 si5351;

QueueHandle_t xRadioQueue;

TaskHandle_t xRadioTaskHandle;
TimerHandle_t xQskTimer = NULL;

radio_audio_bw_t bw = BW_CW;
radio_rxtx_mode_t rxtx_mode = MODE_RX;
radio_band_t band = BAND_HF_2;

uint64_t freq_dial = 14040000;
uint64_t freq_xtal_lower = 9997800;
uint64_t freq_xtal_upper = 10002600;
uint64_t freq_vfo = 0;
uint64_t freq_bfo = 0;

void radio_si5351_init();
void radio_calc_clocks();
void radio_set_clocks(uint64_t freq_bfo, uint64_t freq_vfo, uint64_t freq_rf);
void radio_set_rxtx_mode(radio_rxtx_mode_t new_mode);
void radio_set_band(radio_band_t new_band);
bool radio_freq_valid(uint64_t freq);

void radio_task (void * pvParameter);
void radio_qsk_timer_callback(TimerHandle_t timer);

// calibration related functions
void radio_sweep_analyze(radio_filt_sweep_t sweep, float *data, radio_filt_properties_t *properties);
void radio_cal_tx_10MHz();
void radio_cal_if_filt(radio_filt_sweep_t sweep, radio_filt_properties_t *properties);
void radio_cal_bpf_filt(radio_band_t band, radio_filt_sweep_t sweep, radio_filt_properties_t *properties);

void radio_hf_init() {
  pinMode(BPF_SEL_0, OUTPUT);
  pinMode(BPF_SEL_1, OUTPUT);
  pinMode(LPF_SEL_0, OUTPUT);
  pinMode(LPF_SEL_1, OUTPUT);
  pinMode(TX_RX_SEL, OUTPUT);
  pinMode(PA_VDD_CTRL, OUTPUT);

  digitalWrite(PA_VDD_CTRL, LOW);   // VDD off
  digitalWrite(TX_RX_SEL, LOW);     // RX mode

  radio_si5351_init();

  radio_set_rxtx_mode(MODE_RX);

  // create the message queue
  xRadioQueue = xQueueCreate(10, sizeof(radio_state_t));

  // note: platformio + arduino puts wifi on core 0
  xTaskCreatePinnedToCore(
    radio_task,
    "Radio Task",
    16384,
    NULL,
    TASK_PRIORITY_HIGHEST, // priority
    &xRadioTaskHandle,
    1 // core
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
}

void radio_si5351_init() {
  Wire.begin(CLOCK_SDA, CLOCK_SCL);

  Serial.print("[SI5351] Status: ");
  Serial.println(si5351.si5351_read(SI5351_DEVICE_STATUS));
  si5351.init(SI5351_CRYSTAL_LOAD_8PF , 25999740, 0);

  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);

  si5351.drive_strength(SI5351_IDX_BFO, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_IDX_VFO, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_IDX_TX, SI5351_DRIVE_2MA);


#ifdef CAL_FREQ_ON_STARTUP
  radio_cal_tx_10MHz();
#endif
#ifdef CAL_IF_FILT_ON_STARTUP
  radio_filt_sweep_t if_sweep = {
    .f_center = 10000000,
    .f_span = 20000,
    .num_steps = 100,
    .num_to_avg = 5,
    .rolloff = 3
    };
  radio_filt_properties_t if_properties;
  radio_cal_if_filt(if_sweep, &if_properties);
  freq_xtal_lower = if_properties.f_lower;
  freq_xtal_upper = if_properties.f_upper;
#endif

  freq_dial = 14060000;
  radio_calc_clocks();
  radio_set_clocks(freq_bfo, freq_vfo, freq_dial);

  // do this routine only after calibrating the crystal frequencies
#ifdef CAL_BPF_FILT_ON_STARTUP
  radio_filt_properties_t bpf_properties;
  radio_filt_sweep_t bpf_sweep = {
    .f_center = 7000,
    .f_span = 4000000,
    .num_steps = 50,
    .num_to_avg = 3,
    .rolloff = 6
    };
  radio_cal_bpf_filt(BAND_HF_1, bpf_sweep, &bpf_properties);

  bpf_sweep.f_center = 14000000;
  radio_cal_bpf_filt(BAND_HF_2, bpf_sweep, &bpf_properties);

  bpf_sweep.f_center = 21000000;
  radio_cal_bpf_filt(BAND_HF_3, bpf_sweep, &bpf_properties);

#endif

}


void radio_task(void *param) {
  radio_state_t tmp;
  uint32_t notifiedValue;

  while(true) {
    // look for flags
    // Don't clear flag on entry. Clear on exit. Don't wait, the task will yield at the end
    // example: https://freertos.org/Documentation/02-Kernel/02-Kernel-features/03-Direct-to-task-notifications/04-As-event-group
    if(xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifiedValue, 0) == pdTRUE) {
      if(notifiedValue == NOTIFY_KEY_OFF) {
        Serial.println("Key off");
        // initiate mode change
        radio_set_rxtx_mode(MODE_QSK_COUNTDOWN);

        // turn off sidetone, LED, TX power amp rail
        audio_en_sidetone(false);
        digitalWrite(LED_RED, LOW);
        digitalWrite(PA_VDD_CTRL, LOW);
      }
      if(notifiedValue == NOTIFY_KEY_ON) {
        Serial.println("Key on");
        // initiate mode change
        radio_set_rxtx_mode(MODE_TX);

        // turn off sidetone, LED, TX power amp rail
        audio_en_sidetone(true);
        digitalWrite(LED_RED, HIGH);
        // digitalWrite(PA_VDD_CTRL, HIGH); // comment out for testing
      }
      if(notifiedValue == NOTIFY_QSK_EXPIRE) {
        radio_set_rxtx_mode(MODE_RX);
      }
      if(notifiedValue == NOTIFY_FREQ_CHANGE) {
        // wait for zero ticks, don't want to block here
        if(xQueueReceive(xRadioQueue, (void *) &tmp, 0) == pdTRUE) {
          Serial.print("Queue message: ");
          Serial.println(tmp.dial_freq);

          freq_dial = tmp.dial_freq;
          radio_calc_clocks();
          radio_set_clocks(freq_bfo, freq_vfo, freq_dial);

          // TODO: unpack any bandwidth changes from tmp.bw
        }
      }
    }

    // todo why was notifywait not blocking
    vTaskDelay(1 / portTICK_PERIOD_MS);
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
void radio_set_dial_freq(uint64_t freq) {
  if(!radio_freq_valid(freq))
    return;

  radio_state_t tmp = { .dial_freq = freq, .bw = radio_get_bw()};

  if(xQueueSend(xRadioQueue, (void *) &tmp, 0) != pdTRUE) {
    // TODO: consider replacing this debug statement with returning false
    Serial.println("Unable to change frequency, queue full");
  }

  xTaskNotify(xRadioTaskHandle, NOTIFY_FREQ_CHANGE, eSetBits);
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
    freq_vfo = freq_dial - freq_xtal_lower;
    freq_bfo = freq_xtal_lower - ((uint64_t) audio_get_sidetone_freq());
  }
  else {
    freq_vfo = freq_dial + freq_xtal_upper;
    freq_bfo = freq_xtal_upper + ((uint64_t) audio_get_sidetone_freq());
  }
}

void radio_set_clocks(uint64_t freq_bfo, uint64_t freq_vfo, uint64_t freq_rf) {
  si5351.set_freq(freq_bfo * 100, SI5351_IDX_BFO);
  si5351.set_freq(freq_vfo * 100, SI5351_IDX_VFO);
  si5351.set_freq(freq_rf * 100, SI5351_IDX_TX);

  // Serial.print("bfo: ");
  // Serial.print(freq_bfo);
  // Serial.print("\tvfo: ");
  // Serial.print(freq_vfo);
  // Serial.print("\ttx: ");
  // Serial.println(freq_rf);
}

void radio_set_rxtx_mode(radio_rxtx_mode_t new_mode) {
  // return immediately if there was no change requested
  if(new_mode == rxtx_mode)
    return;

  // intervene on invalid request: going from TX direct to RX
  if(rxtx_mode == MODE_TX && new_mode == MODE_RX)
    new_mode = MODE_QSK_COUNTDOWN;

  Serial.print("Mode change: ");
  Serial.println(new_mode);

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
      vTaskDelay(5 / portTICK_PERIOD_MS);

      // enable RX audio
      audio_en_pga(true);
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

      // mute all audio sources
      audio_en_pga(false);
      audio_en_rx_audio(true);

      // no need to update clocks, was just in TX
      // no need to update relays when mode changes to QSK, was just in TX

      // delay so VDD can discharge
      // implemented on entry to QSK_COUNTDOWN to guarantee that we can't immediately transition to RX 
      vTaskDelay(25 / portTICK_PERIOD_MS);

      break;

    case MODE_TX:
      // stop QSK counter
      xTimerStop(xQskTimer, 0);

      // update mode so the next function calls assume TX
      rxtx_mode = MODE_TX;

      // turn off RX audio
      // TX power amp rail, sidetone, and TX LED are handled elsewhere
      audio_en_pga(false);
      audio_en_rx_audio(false);

      // change over relays if needed. Add some settling time
      // TODO: rework the radio_set_band(band) fundtion so it is "radio_set_relays(freq)" and looks up band from dial freq
      radio_set_band(band);
      vTaskDelay(5 / portTICK_PERIOD_MS);

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
void radio_set_band(radio_band_t new_band) {
  if(rxtx_mode == MODE_RX || rxtx_mode == MODE_QSK_COUNTDOWN) {
    digitalWrite(TX_RX_SEL, LOW);
    switch(new_band) {
      case BAND_HF_1:
        digitalWrite(BPF_SEL_0, HIGH);
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
      case BAND_SELF_TEST:
        digitalWrite(BPF_SEL_0, HIGH);
        digitalWrite(BPF_SEL_1, HIGH);
        break;
      default:
        Serial.print("Invalid band request: ");
        Serial.println(new_band);
        return;
      digitalWrite(LPF_SEL_0, LOW);
      digitalWrite(LPF_SEL_1, LOW);
    }
  }
  else if(rxtx_mode == MODE_SELF_TEST) {
    // want to engage both BPF and LPF pathways properly
    digitalWrite(TX_RX_SEL, LOW);
    // turn this off, just to be safe in selftest mode
    digitalWrite(PA_VDD_CTRL, LOW);
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
      case BAND_SELF_TEST:
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
  else if(rxtx_mode == MODE_TX) {
    digitalWrite(TX_RX_SEL, HIGH);
    switch(new_band) {
      case BAND_HF_1:
        digitalWrite(BPF_SEL_0, HIGH);
        digitalWrite(BPF_SEL_1, HIGH);
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
        digitalWrite(BPF_SEL_0, HIGH);
        digitalWrite(BPF_SEL_1, HIGH);
        digitalWrite(LPF_SEL_0, HIGH);
        digitalWrite(LPF_SEL_1, HIGH);
        break;
      case BAND_SELF_TEST:
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
  band = new_band;
}

// 
// TODO: load frequency limits for each band from NVM
bool radio_freq_valid(uint64_t freq) {
  if(freq > 7000000 && freq < 7250000)
    return true;
  if(freq > 14000000 && freq < 14300000)
    return true;
  if(freq > 21000000 && freq < 21300000)
    return true;

  return false;
}

// inputs: sweep setup, dataset
// outputs: analyzed properties
void radio_sweep_analyze(radio_filt_sweep_t sweep, float *data, radio_filt_properties_t *properties) {
  uint64_t step_size = sweep.f_span / sweep.num_steps;

  // graphics
#ifdef CAL_GRAPHICS
  for(uint16_t i=0; i < sweep.num_steps; i++) {
    int64_t dF = (int64_t) step_size * ((int64_t) i - sweep.num_steps/2);
    Serial.print("dF: ");
    Serial.print(dF);

    // hack: add a tab if needed
    if(dF >= 0 && dF < 1000)
      Serial.print("\t");

    Serial.print("\t\tVol: ");
    Serial.print(data[i]);
    Serial.print("\t\t");

    uint16_t bar_size = (uint16_t) abs(data[i]);
    for(uint16_t j = 0; j < bar_size; j++) {
      Serial.print(".");
    }
    Serial.println();
  }
#endif

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
  si5351.set_freq(((uint64_t) 10000000) * 100, SI5351_IDX_TX);
  si5351.output_enable(SI5351_IDX_BFO, 0);
  si5351.output_enable(SI5351_IDX_VFO, 0);
  si5351.output_enable(SI5351_IDX_TX, 1);
  vTaskDelay(5000 / portTICK_PERIOD_MS);
}

// stop the VFO, enable BFO, sweep TX-CLK injection and observe amplitude
// assumes that si5351 crystal is already calibrated
void radio_cal_if_filt(radio_filt_sweep_t sweep, radio_filt_properties_t *properties) {
  Serial.println("\n\nCalibrating Crystal filter...");
  digitalWrite(LED_RED, HIGH);

  audio_en_pga(false);
  audio_en_sidetone(false);
  audio_set_volume(AUDIO_VOL_DURING_CAL);  // can mute for quiet startup
  audio_en_rx_audio(true);
  audio_set_filt(AUDIO_FILT_SSB);
  
  radio_set_band(BAND_SELF_TEST);
  radio_set_rxtx_mode(MODE_SELF_TEST);

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
    si5351.set_freq(((uint64_t) f_tx + audio_get_sidetone_freq()) * 100, SI5351_IDX_BFO);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    measurements[i] = audio_get_rx_db(sweep.num_to_avg, 1);
  }

  radio_sweep_analyze(sweep, measurements, properties);

  // TODO: logic to broaden the search if the upper or lower index are at the edges of the sweep
  // TODO: logic to enable/disable PGA if it's always railed

  digitalWrite(LED_RED, LOW);
  audio_set_filt(AUDIO_FILT_DEFAULT);
  audio_en_pga(AUDIO_PGA_DEFAULT);
  audio_set_volume(AUDIO_VOL_DEFAULT);
  audio_en_rx_audio(true);
}

void radio_cal_bpf_filt(radio_band_t band, radio_filt_sweep_t sweep, radio_filt_properties_t *properties) {
  Serial.println("\n\nCalibrating Bandpass filter...");
  digitalWrite(LED_RED, HIGH);

  radio_set_band(band);
  radio_set_rxtx_mode(MODE_SELF_TEST);

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
    vTaskDelay(10 / portTICK_PERIOD_MS);

    measurements[i] = audio_get_rx_db(sweep.num_to_avg, 10);
  }

  radio_sweep_analyze(sweep, measurements, properties);

  digitalWrite(LED_RED, LOW);
  audio_set_filt(AUDIO_FILT_DEFAULT);
  audio_en_pga(AUDIO_PGA_DEFAULT);
  audio_set_volume(AUDIO_VOL_DEFAULT);
  audio_en_rx_audio(true);
}