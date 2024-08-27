#include "radio_hf.h"

#include <Arduino.h>
#include <si5351.h>

#define NOTIFY_KEY_ON     0
#define NOTIFY_KEY_OFF    1

Si5351 si5351;

QueueHandle_t xRadioQueue;

radio_audio_bw_t bw = BW_CW;
radio_rxtx_mode_t rxtx_mode = MODE_RX;

uint64_t freq_dial = 14040000;
uint64_t freq_xtal_lower = 10000000;
uint64_t freq_xtal_upper = 10003000;
uint64_t freq_vfo = 0;
uint64_t freq_bfo = 0;


void radio_calc_clocks();


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
  digitalWrite(BPF_SEL_0, HIGH);
  digitalWrite(BPF_SEL_1, LOW);


  Wire.begin(CLOCK_SDA, CLOCK_SCL);
  
  Serial.print("[SI5351] Status: ");
  Serial.println(si5351.si5351_read(SI5351_DEVICE_STATUS));
  si5351.init(SI5351_CRYSTAL_LOAD_8PF , 26000000 , 0);
  
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);

  si5351.set_freq(((uint64_t) 10000000) * 100, SI5351_CLK0);
  si5351.set_freq(((uint64_t) 24074500) * 100, SI5351_CLK1);  // should get rid of that 500hz hack, need to cal oscillator and filter
  // si5351.set_freq(((uint64_t) 24060500) * 100, SI5351_CLK1);  // should get rid of that 500hz hack, need to cal oscillator and filter
  si5351.set_freq(((uint64_t) 14040000) * 100, SI5351_CLK2);

  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);

  si5351.output_enable(SI5351_CLK0, 1); // BFO
  si5351.output_enable(SI5351_CLK1, 1); // VFO
  si5351.output_enable(SI5351_CLK2, 0); // TX clock

  // create the message queue
  xRadioQueue = xQueueCreate(10, sizeof(radio_state_t));
}

void radio_task(void *param) {
  radio_state_t tmp;
  uint32_t notifiedValue;

  while(true) {

    // look for frequency/mode change requests
    if(xQueueReceive(xRadioQueue, (void *) &tmp, 0) == pdTRUE) {
      Serial.print("Queue message: ");
      Serial.println(tmp.dial_freq);
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
// frequency limits are NOT enforced at this point
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
  
}