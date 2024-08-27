#include <Arduino.h>
#include <HardwareSerial.h>

#include "wifi_conn.h"
#include "audio.h"
#include <si5351.h>

Si5351 si5351;

HardwareSerial VHFserial(1);

TaskHandle_t audioStreamTaskHandle, blinkTaskHandle, spareTaskHandle, batterySenseTaskHandle, txPulseTaskHandle;
SemaphoreHandle_t btn_semaphore;

int t = 0;
int counter = 0;
int freq_buck = 510e3;



void spareTask(void *param) {
  while(true) {
    digitalWrite(LED_RED, LOW);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    digitalWrite(LED_RED, HIGH);
    taskYIELD();
  }
}

void blinkTask(void *param) {
  while(true) {
    digitalWrite(LED_GRN, HIGH);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    digitalWrite(LED_GRN, LOW);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void batterySenseTask(void *param) {
  while(true) {
    // Serial.print("Analog read: ");
    // Serial.println(analogRead(ADC_VDD) * ADC_MAX_VOLT / ADC_VDD_SCALE / ADC_FS_COUNTS);

    // Serial.print("S-meter: ");
    // Serial.println(out_vol_meas.volume());

    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

ICACHE_RAM_ATTR void buttonISR() {
  // detach interrupt here, reattaches after taking semaphore
  detachInterrupt(digitalPinToInterrupt(BOOT_BTN));
  detachInterrupt(digitalPinToInterrupt(MIC_PTT));
  detachInterrupt(digitalPinToInterrupt(KEY_DIT));
  detachInterrupt(digitalPinToInterrupt(KEY_DAH));

  xSemaphoreGiveFromISR(btn_semaphore, NULL);
}


void txPulseTask(void *param) {
  while(true) {
    if(xSemaphoreTake(btn_semaphore, portMAX_DELAY) == pdPASS) {
      Serial.println("TX Pulsing");

      // HF TX test

      si5351.output_enable(SI5351_CLK2, 1); // TX clock
      digitalWrite(PA_VDD_CTRL, HIGH);
      vTaskDelay(25 / portTICK_PERIOD_MS);      
      vTaskDelay(10000 / portTICK_PERIOD_MS);      
      digitalWrite(PA_VDD_CTRL, LOW);
      vTaskDelay(25 / portTICK_PERIOD_MS);
      si5351.output_enable(SI5351_CLK2, 0); // TX clock


      // VHF TX test
      /*
      // change to LOUT1 and ROUT1
      AudioDriver *driver = audio_board.getDriver();
      Serial.println();
      Serial.println("Powering up DAC 1");
      driver->setMute(true, 0);
      driver->setMute(false, 1);

      // change to LIN2
      // HACK: reconfigure the codec with a different input_device
      // AudioDriver doesn't have a way to set input source besides the i2s_config used at startup. passed through to es8388_init()
      auto i2s_config = i2s_stream.defaultConfig(RXTX_MODE);
      i2s_config.input_device = ADC_INPUT_LINE2;
      i2s_config.copyFrom(info_stereo);
      i2s_config.buffer_size = 512;
      i2s_config.buffer_count = 2;
      i2s_config.port_no = 0;
      i2s_stream.begin(i2s_config);

      // turn on sidetone
      hf_vhf_mixer.setWeight(0, 1.0);

      vTaskDelay(100 / portTICK_PERIOD_MS);
      digitalWrite(VHF_PTT, LOW);
      // descending frequency sweep
      for(uint16_t i=0; i < 20; i++) {
        sine_wave.setFrequency(700);
        vTaskDelay(250 / portTICK_PERIOD_MS);
        sine_wave.setFrequency(1400);
        vTaskDelay(250 / portTICK_PERIOD_MS);
      }
      sine_wave.setFrequency(700);
      // vTaskDelay(1500 / portTICK_PERIOD_MS);
      digitalWrite(VHF_PTT, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);

      // restore previous state (LOUT2 and ROUT2 as output, LIN1 and RIN1 as input)
      Serial.println();
      Serial.println("Powering up DAC 2");
      driver->setMute(false, 0);
      driver->setMute(true, 1);
      hf_vhf_mixer.setWeight(0, 0);

      */



      attachInterrupt(digitalPinToInterrupt(BOOT_BTN), buttonISR, FALLING);
      attachInterrupt(digitalPinToInterrupt(MIC_PTT), buttonISR, FALLING);
      attachInterrupt(digitalPinToInterrupt(KEY_DIT), buttonISR, FALLING);
      attachInterrupt(digitalPinToInterrupt(KEY_DAH), buttonISR, FALLING);
    }
  }
}



void setup() {
  // put your setup code here, to run once:
  pinMode(LED_GRN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_DBG_0, OUTPUT);
  pinMode(LED_DBG_1, OUTPUT);
  pinMode(BPF_SEL_0, OUTPUT);
  pinMode(BPF_SEL_1, OUTPUT);
  pinMode(LPF_SEL_0, OUTPUT);
  pinMode(LPF_SEL_1, OUTPUT);
  pinMode(TX_RX_SEL, OUTPUT);
  pinMode(PA_VDD_CTRL, OUTPUT);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOOT_BTN), buttonISR, FALLING);
  pinMode(MIC_PTT, INPUT);
  attachInterrupt(digitalPinToInterrupt(MIC_PTT), buttonISR, FALLING);
  pinMode(KEY_DAH, INPUT);
  attachInterrupt(digitalPinToInterrupt(KEY_DAH), buttonISR, FALLING);
  pinMode(KEY_DIT, INPUT);
  attachInterrupt(digitalPinToInterrupt(KEY_DIT), buttonISR, FALLING);
  pinMode(VHF_EN, OUTPUT);
  pinMode(VHF_PTT, OUTPUT);
  pinMode(SPARE_0, OUTPUT);

  digitalWrite(PA_VDD_CTRL, LOW);

  // digitalWrite(VHF_EN, HIGH);   // enabled
  digitalWrite(VHF_EN, LOW);
  digitalWrite(VHF_PTT, HIGH);  // RX

  digitalWrite(TX_RX_SEL, LOW); // RX mode

  digitalWrite(SPARE_0, LOW);

  digitalWrite(LED_DBG_0, LOW);
  digitalWrite(LED_DBG_1, LOW);

  // set up a PWM channel that drives buck converter sync pin
  // feature for the future: select this frequency based on the dial frequency
  ledcSetup(PWM_CHANNEL_SYNC, freq_buck, 2);
  ledcAttachPin(BUCK_SYNC, PWM_CHANNEL_SYNC);
  ledcWrite(PWM_CHANNEL_SYNC, 2);

  Serial.begin(DEBUG_SERIAL_SPEED);

  VHFserial.begin(VHF_SERIAL_SPEED, SERIAL_8N1, VHF_TX_ESP_RX, VHF_RX_ESP_TX);

  delay(5000);
  digitalWrite(SPARE_0, HIGH);

  // https://community.platformio.org/t/how-to-use-psram-on-esp32-s3-devkitc-1-under-esp-idf/32127/17
  if(psramInit())
  Serial.println("\nPSRAM is correctly initialized");
  else
    Serial.println("PSRAM not available");
  Serial.println(psramFound());

  // check PSRAM
  Serial.print("Size PSRAM: ");
  Serial.println(ESP.getPsramSize());
  Serial.print("Free PSRAM: ");
  Serial.println(ESP.getFreePsram());

  Serial.println("allocating to PSRAM...");
  byte* psram_buffer = (byte*) ps_malloc(1024);

  Serial.print("Free PSRAM: ");
  Serial.println(ESP.getFreePsram());
  

  // wifi_init();

  audio_init();



  // note: platformio + arduino puts wifi on core 0

  // run on core 1
  xTaskCreatePinnedToCore(
    audio_stream_task,
    "Audio Stream Updater Task",
    16384,
    NULL,
    1, // priority
    &audioStreamTaskHandle,
    1 // core
  );

  // run on core 1
  xTaskCreatePinnedToCore(
    spareTask,
    "Red LED spare time task",
    4096,
    NULL,
    4, // priority
    &spareTaskHandle,
    1 // core
  );

  // run on core 0
  xTaskCreatePinnedToCore(
    blinkTask,
    "Blinky light",
    4096,
    NULL,
    3, // priority
    &blinkTaskHandle,
    0 // core
  );

  xTaskCreatePinnedToCore(
    batterySenseTask,
    "VDD Sensing",
    4096,
    NULL,
    3, // priority
    &batterySenseTaskHandle,
    0 // core
  );

  btn_semaphore = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(
    txPulseTask,
    "TX pulse generator",
    4096,
    NULL,
    2, // priority
    &txPulseTaskHandle,
    1 // core
  );

  

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

  // select 20m band
  digitalWrite(BPF_SEL_0, HIGH);
  digitalWrite(BPF_SEL_1, LOW);

  // select RX
  digitalWrite(TX_RX_SEL, LOW);

  // select TX on band#2
  /*
  digitalWrite(TX_RX_SEL, HIGH);
  digitalWrite(LPF_SEL_0, HIGH);
  digitalWrite(LPF_SEL_1, HIGH);  
  */

  // set VHFserial timeout (milliseconds)
  VHFserial.setTimeout(100);

  // send serial commands
  Serial.println("VHF connecting...");
  VHFserial.println("AT+DMOCONNECT");

  delay(1000);

  /*
  Serial.println("VHF Response: ");
  while(VHFserial.available() > 1) {
    Serial.print((char) VHFserial.read());
  }
  */

  // todo: parametrize length
  char buf[64];
  int read_len = VHFserial.readBytes(buf, 64);

  Serial.println("VHF Response: ");
  for(int i=0; i < read_len; i++) {
    Serial.print(buf[i]);
  }
  // todo: parse this response and make sure it's good.


  // set frequency
  // see: https://www.dorji.com/docs/data/DRA818V.pdf
  VHFserial.println("AT+DMOSETGROUP=0,146.5400,146.5400,0000,1,0000");
  delay(1000);
  Serial.println("VHF Response: ");
  while(VHFserial.available() > 1) {
    Serial.print((char) VHFserial.read());
  }

  Serial.println();

}


void loop() { 
  if(millis() - t > 2000) {
    // AudioDriver *driver = audio_board.getDriver();
    if(counter % 2 == 0) {
      audio_en_sidetone(true);
      // driver->setMute(false, 0);
      // driver->setMute(true, 1);    // turns off DAC output
      // // driver->setInputVolume(10);   // changes PGA
      // audio_filt.setFilter(0, new FIR<float>(coeff_bandpass));  // definitely creating a memory issue by creating new filters repeatedly...

      // hf_vhf_mixer.setWeight(0, 0);
    }
    else {
      audio_en_sidetone(false);
      // driver->setMute(true, 0);
      // driver->setMute(false, 1);
      // // driver->setInputVolume(80); // changes PGA
      // audio_filt.setFilter(0, new FIR<float>(coeff_lowpass));  // definitely creating a memory issue by creating new filters repeatedly...

      // hf_vhf_mixer.setWeight(0, 1.0);
    }      
    counter++;
    t = millis();
  } 
}
