#include <Arduino.h>
#include <HardwareSerial.h>

#include "globals.h"
#include "wifi_conn.h"
#include "audio.h"
#include "radio_hf.h"

HardwareSerial VHFserial(1);

TaskHandle_t xSpareTaskHandle0, xSpareTaskHandle1;
TaskHandle_t blinkTaskHandle, batterySenseTaskHandle, txPulseTaskHandle; 

SemaphoreHandle_t btn_semaphore;

int t = 0;
int counter = 0;
int freq_buck = 510e3;


// if LED_DBG_x is *not* illuminated, spareTask is starved
void spare_task_core_0(void *param) {
  while(true) {
    digitalWrite(LED_DBG_0, HIGH);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    digitalWrite(LED_DBG_0, LOW);
    taskYIELD();
  }
}
void spare_task_core_1(void *param) {
  while(true) {
    digitalWrite(LED_DBG_1, HIGH);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    digitalWrite(LED_DBG_1, LOW);
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
    // Serial.print("VDD read: ");
    // Serial.println(analogRead(ADC_VDD) * ADC_MAX_VOLT / ADC_VDD_SCALE / ADC_FS_COUNTS);

    // Serial.print("S-meter: ");
    // Serial.println(audio_get_rx_db());

    vTaskDelay(1000 / portTICK_PERIOD_MS);
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
      if(digitalRead(BOOT_BTN) == LOW)
        radio_key_on();
      if(digitalRead(BOOT_BTN) == HIGH)
        radio_key_off();


      attachInterrupt(digitalPinToInterrupt(BOOT_BTN), buttonISR, CHANGE);
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

  pinMode(BOOT_BTN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOOT_BTN), buttonISR, CHANGE);
  pinMode(MIC_PTT, INPUT);
  attachInterrupt(digitalPinToInterrupt(MIC_PTT), buttonISR, FALLING);
  pinMode(KEY_DAH, INPUT);
  attachInterrupt(digitalPinToInterrupt(KEY_DAH), buttonISR, FALLING);
  pinMode(KEY_DIT, INPUT);
  attachInterrupt(digitalPinToInterrupt(KEY_DIT), buttonISR, FALLING);
  pinMode(VHF_EN, OUTPUT);
  pinMode(VHF_PTT, OUTPUT);
  pinMode(SPARE_0, OUTPUT);

  // digitalWrite(VHF_EN, HIGH);   // enabled
  digitalWrite(VHF_EN, LOW);
  digitalWrite(VHF_PTT, HIGH);  // RX

  digitalWrite(SPARE_0, LOW);

  digitalWrite(LED_DBG_0, LOW);
  digitalWrite(LED_DBG_1, LOW);

  // set up a PWM channel that drives buck converter sync pin
  // feature for the future: select this frequency based on the dial frequency
  ledcSetup(PWM_CHANNEL_SYNC, freq_buck, 2);
  ledcAttachPin(BUCK_SYNC, PWM_CHANNEL_SYNC);
  ledcWrite(PWM_CHANNEL_SYNC, 2);

  // only start the HWCDC if something is connected
  // fixes bad behavior where audio output from codec was corrupt if USB unplugged
  if(Serial.isConnected())
    Serial.begin(DEBUG_SERIAL_SPEED);

  VHFserial.begin(VHF_SERIAL_SPEED, SERIAL_8N1, VHF_TX_ESP_RX, VHF_RX_ESP_TX);

  delay(5000);

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
  audio_set_sidetone_volume(AUDIO_SIDE_DEFAULT);
  audio_set_volume(AUDIO_VOL_DEFAULT);

  radio_init();

  // run on core 0
  xTaskCreatePinnedToCore(
    spare_task_core_0,
    "Debug LED spare time task",
    4096,
    NULL,
    TASK_PRIORITY_LOWEST, // priority
    &xSpareTaskHandle0,
    0 // core
  );

  // run on core 1
  xTaskCreatePinnedToCore(
    spare_task_core_1,
    "Debug LED spare time task",
    4096,
    NULL,
    TASK_PRIORITY_LOWEST, // priority
    &xSpareTaskHandle1,
    1 // core
  );

  // run on core 0
  xTaskCreatePinnedToCore(
    blinkTask,
    "Blinky light",
    4096,
    NULL,
    TASK_PRIORITY_LOWEST + 1, // priority
    &blinkTaskHandle,
    0 // core
  );

  xTaskCreatePinnedToCore(
    batterySenseTask,
    "VDD Sensing",
    4096,
    NULL,
    TASK_PRIORITY_NORMAL, // priority
    &batterySenseTaskHandle,
    0 // core
  );

  btn_semaphore = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(
    txPulseTask,
    "TX pulse generator",
    4096,
    NULL,
    TASK_PRIORITY_HIGHEST, // priority
    &txPulseTaskHandle,
    1 // core
  );

  // set VHFserial timeout (milliseconds)
  VHFserial.setTimeout(100);

  // send serial commands
  Serial.println("VHF connecting...");
  VHFserial.println("AT+DMOCONNECT");

  delay(1000);

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

// code in loop() is just for testing, don't want to actually do anything here
void loop() { 
  if(millis() - t > 1000) {
    if(counter % 2 == 0) {
      audio_test(true);
    }
    else {
      radio_set_dial_freq(14060000);

      audio_test(false);

    }      
    counter++;
    t = millis();
  } 
}
