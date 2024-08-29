#include "radio_hf.h"
#include "audio.h"

#include <Arduino.h>
#include <si5351.h>

#define NOTIFY_KEY_ON     0
#define NOTIFY_KEY_OFF    1

#define SI5351_IDX_BFO    SI5351_CLK0
#define SI5351_IDX_VFO    SI5351_CLK1
#define SI5351_IDX_TX     SI5351_CLK2

// #define CAL_FREQ_ON_STARTUP
#define CAL_IF_FILT_ON_STARTUP
#define CAL_BPF_FILT_ON_STARTUP
#define CAL_GRAPHICS

Si5351 si5351;

QueueHandle_t xRadioQueue;

radio_audio_bw_t bw = BW_CW;
radio_rxtx_mode_t rxtx_mode = MODE_RX;

uint64_t freq_dial = 14040000;
uint64_t freq_xtal_lower = 10000000;
uint64_t freq_xtal_upper = 10003000;
uint64_t freq_vfo = 0;
uint64_t freq_bfo = 0;

void radio_si5351_init();
void radio_calc_clocks();
void radio_set_clocks(uint64_t freq_bfo, uint64_t freq_vfo, uint64_t freq_rf);
void radio_cal_tx_10MHz();
void radio_cal_if_filt();
void radio_cal_bpf_filt();

void radio_init() {
  pinMode(BPF_SEL_0, OUTPUT);
  pinMode(BPF_SEL_1, OUTPUT);
  pinMode(LPF_SEL_0, OUTPUT);
  pinMode(LPF_SEL_1, OUTPUT);
  pinMode(TX_RX_SEL, OUTPUT);
  pinMode(PA_VDD_CTRL, OUTPUT);

  digitalWrite(PA_VDD_CTRL, LOW);   // VDD off
  digitalWrite(TX_RX_SEL, LOW);     // RX mode
  
  // select 20m band
  digitalWrite(BPF_SEL_0, LOW);
  digitalWrite(BPF_SEL_1, HIGH);

  radio_si5351_init();

  // create the message queue
  xRadioQueue = xQueueCreate(10, sizeof(radio_state_t));

  // note: platformio + arduino puts wifi on core 0
  xTaskCreatePinnedToCore(
    radio_task,
    "Radio Task",
    16384,
    NULL,
    1, // priority
    &xRadioTaskHandle,
    1 // core
  );
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
  radio_cal_if_filt();
#endif

  freq_dial = 14060000;
  radio_calc_clocks();
  radio_set_clocks(freq_bfo, freq_vfo, freq_dial);

  // do this routine only after calibrating the crystal frequencies
#ifdef CAL_BPF_FILT_ON_STARTUP
  radio_cal_bpf_filt();
#endif

  si5351.output_enable(SI5351_IDX_BFO, 1);
  si5351.output_enable(SI5351_IDX_VFO, 1);
  si5351.output_enable(SI5351_IDX_TX, 0);

}

void radio_task(void *param) {
  radio_state_t tmp;
  uint32_t notifiedValue;

  while(true) {

    // look for frequency/mode change requests
    if(xQueueReceive(xRadioQueue, (void *) &tmp, 0) == pdTRUE) {
      Serial.print("Queue message: ");
      Serial.println(tmp.dial_freq);

      // freq_dial = tmp.dial_freq;
      // radio_calc_clocks();
      // radio_set_clocks(freq_bfo, freq_vfo, freq_dial);
      // TODO: setting the clocks would have enabled some. Need to go back to RX mode
    }

    // Check for notification about key up or key down
    // Don't clear on entry. Clear on exit. Don't wait, the task will yield at the end
    // example: https://freertos.org/Documentation/02-Kernel/02-Kernel-features/03-Direct-to-task-notifications/04-As-event-group
    if(xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifiedValue, 0) == pdTRUE) {
      if(notifiedValue == NOTIFY_KEY_OFF)
        Serial.println("Key off");
      if(notifiedValue == NOTIFY_KEY_ON)
        Serial.println("Key on");
    }

    // check if QSK timer has expired
    
    taskYIELD();
  }
}

void radio_key_on() {
  xTaskNotify(xRadioTaskHandle, NOTIFY_KEY_ON, eSetBits);
}

void radio_key_off() {
  xTaskNotify(xRadioTaskHandle, NOTIFY_KEY_OFF, eSetBits);
}

// helper function to REQUEST a frequency change. Adds to the queue.
// TODO: enforce frequency limits based on band
void radio_set_dial_freq(uint64_t freq) {
  radio_state_t tmp = { .dial_freq = freq, .bw = radio_get_bw()};

  if(xQueueSend(xRadioQueue, (void *) &tmp, 0) != pdTRUE) {
    // TODO: consider replacing this debug statement with returning false
    Serial.println("Unable to change frequency, queue full");
  }
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

  /*
  Serial.print("bfo: ");
  Serial.print(freq_bfo);
  Serial.print("\tvfo: ");
  Serial.print(freq_vfo);
  Serial.print("\ttx: ");
  Serial.println(freq_rf);
  */
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
void radio_cal_if_filt() {
  Serial.println("\n\nCalibrating Crystal filter...");
  digitalWrite(LED_RED, HIGH);

  // turn on TX-CLK, turn off other clocks
  si5351.set_freq(((uint64_t) 10000000) * 100, SI5351_IDX_BFO);
  si5351.output_enable(SI5351_IDX_BFO, 1);
  si5351.output_enable(SI5351_IDX_VFO, 0);
  si5351.output_enable(SI5351_IDX_TX, 1);

  audio_en_pga(false);
  audio_en_sidetone(false);
  audio_set_volume(0.1);  // can mute for quiet startup
  audio_en_rx_audio(true);
  audio_set_filt(AUDIO_FILT_SSB);
  
  // pipe TX-CLK into the RX pathway
  digitalWrite(BPF_SEL_0, HIGH);
  digitalWrite(BPF_SEL_1, HIGH);

  // wait to let audio settle
  vTaskDelay(500 / portTICK_PERIOD_MS);

  // sweep TX-CLK
  int16_t step_size = 200;
  int16_t num_steps = 100;
  uint64_t center_freq = 10000000;
  uint16_t num_avg = 5;
  float filter_rolloff = 3;    // dB determining when the filter has rolled off
  float measurements[num_steps];
  for(int16_t i = 0; i < num_steps; i++)
  {
    uint64_t f_tx = center_freq + (uint64_t) step_size * (i - num_steps/2);
    si5351.set_freq(f_tx * 100, SI5351_IDX_TX);
    si5351.set_freq(((uint64_t) f_tx + audio_get_sidetone_freq()) * 100, SI5351_IDX_BFO);
    vTaskDelay(5 / portTICK_PERIOD_MS);

    // measure average volume
    float avg_vol = 0;
    
    for(int16_t j = 0; j < num_avg; j++) {
      avg_vol += audio_get_rx_db();
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    avg_vol /= num_avg;
    measurements[i] = avg_vol;

    // graphics
#ifdef CAL_GRAPHICS
    Serial.print("dF: ");
    Serial.print(step_size * (i - num_steps/2));
    Serial.print("\t\tVol: ");
    Serial.print(avg_vol);
    Serial.print("\t\t");

    uint16_t bar_size = (uint16_t) abs(measurements[i]);
    for(uint16_t j = 0; j < bar_size; j++) {
      Serial.print(".");
    }
    Serial.println();
#endif
  }

  // find center of filter (highest signal)
  uint16_t idx_max = 0;
  float meas_max = -1000;
  for(uint16_t i = 0; i < num_steps; i++) {
    if(measurements[i] > meas_max) {
      meas_max = measurements[i];
      idx_max = i;
    }
  }

  Serial.print("idx_max: ");
  Serial.print(idx_max);
  Serial.print("\tmeas_max: ");
  Serial.println(meas_max);

  // find lower corner
  uint16_t idx_lower = 0;
  float meas_lower = meas_max;
  for(uint16_t i=idx_max; i > 0; i--) {
    if(abs(meas_max - measurements[i]) > filter_rolloff) {
      idx_lower = i;
      meas_lower = measurements[i];
      break;
    }
  }
  Serial.print("idx_lower: ");
  Serial.print(idx_lower);
  Serial.print("\tfreq_lower: ");
  Serial.print(center_freq - (num_steps/2 - idx_lower) * step_size);
  Serial.print("\tmeas_lower: ");
  Serial.println(meas_lower);


  // find upper corner
  uint16_t idx_upper = 0;
  float meas_upper = meas_max;
  for(uint16_t i=idx_max; i < num_steps; i++) {
    if(abs(meas_max - measurements[i]) > filter_rolloff) {
      idx_upper = i;
      meas_upper = measurements[i];
      break;
    }
  }
  Serial.print("idx_upper: ");
  Serial.print(idx_upper);
  Serial.print("\tfreq_upper: ");
  Serial.print(center_freq + (idx_upper - num_steps/2) * step_size);
  Serial.print("\tmeas_upper: ");
  Serial.println(meas_upper);

  // TODO: logic to broaden the search if the upper or lower index are at the edges of the sweep
  // TODO: logic to enable/disable PGA if it's always railed

  // TODO: sanity check the detected upper/lower crystal frequencies before assigning
  freq_xtal_lower = center_freq - (num_steps/2 - idx_lower) * step_size;
  freq_xtal_upper = center_freq + (idx_upper - num_steps/2) * step_size;

  digitalWrite(LED_RED, LOW);
  audio_set_filt(AUDIO_FILT_CW);
  audio_en_pga(true);
  audio_set_volume(0.1);
  audio_en_rx_audio(true);
}


void radio_cal_bpf_filt() {
  Serial.println("\n\nCalibrating Bandpass filter...");
  digitalWrite(LED_RED, HIGH);

  // select TX on band # 2 (20m)
  // TODO: band selection by enum
  digitalWrite(PA_VDD_CTRL, LOW);   // VDD off
  vTaskDelay(500 / portTICK_PERIOD_MS);
  
  digitalWrite(LPF_SEL_0, HIGH);
  digitalWrite(LPF_SEL_1, LOW);  

  digitalWrite(TX_RX_SEL, LOW);


  // select band # 2
  digitalWrite(BPF_SEL_0, HIGH);
  digitalWrite(BPF_SEL_1, LOW);

  // turn on all clocks for this self-test
  si5351.output_enable(SI5351_IDX_BFO, 1);
  si5351.output_enable(SI5351_IDX_VFO, 1);
  si5351.output_enable(SI5351_IDX_TX, 1);

  audio_en_pga(false);
  audio_en_sidetone(false);
  audio_set_volume(0.1);  // can mute for quiet startup
  audio_en_rx_audio(true);
  audio_set_filt(AUDIO_FILT_SSB);

  // wait to let audio settle
  vTaskDelay(500 / portTICK_PERIOD_MS);

  // sweep dial frequency
  int64_t step_size = 25000;
  int64_t num_steps = 100;
  uint64_t center_freq = 14000000;
  uint16_t num_avg = 3;
  float measurements[num_steps];
  for(int64_t i = 0; i < num_steps; i++)
  {
    int64_t dF = step_size * (i - num_steps/2);
    Serial.println(dF);
    freq_dial = (uint64_t) center_freq + dF;
    radio_calc_clocks();
    radio_set_clocks(freq_bfo, freq_vfo, freq_dial);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    // measure average volume
    float avg_vol = 0;
    
    for(int16_t j = 0; j < num_avg; j++) {
      avg_vol += audio_get_rx_db();
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    avg_vol /= num_avg;
    measurements[i] = avg_vol;

    // graphics
#ifdef CAL_GRAPHICS
    Serial.print("dF: ");
    Serial.print(step_size * (i - num_steps/2));
    Serial.print("\t\tVol: ");
    Serial.print(avg_vol);
    Serial.print("\t\t");

    uint16_t bar_size = (uint16_t) abs(measurements[i]);
    for(uint16_t j = 0; j < bar_size; j++) {
      Serial.print(".");
    }
    Serial.println();
#endif
  }



  digitalWrite(TX_RX_SEL, LOW);
  digitalWrite(LPF_SEL_0, LOW);
  digitalWrite(LPF_SEL_1, LOW); 

  digitalWrite(LED_RED, LOW);
  audio_set_filt(AUDIO_FILT_CW);
  audio_en_pga(true);
  audio_set_volume(0.1);
  audio_en_rx_audio(true);
}