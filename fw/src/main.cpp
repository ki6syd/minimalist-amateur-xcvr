#include <Arduino.h>
#include <Wire.h>

#include <HardwareSerial.h>

#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"
#include "AudioLibs/VBANStream.h"
#include <si5351.h>

#include "fir_coeffs_bpf.h"
#include "fir_coeffs_lpf.h"
#include "wifi_conn.h"
#include "audio.h"

Si5351 si5351;

TwoWire codecI2C = TwoWire(1);                      // "Wire" is used in si5351 library. Defined through TwoWire(0), so the other peripheral is still available
HardwareSerial VHFserial(1);

AudioInfo                     info_stereo(F_AUDIO, 2, 16);              // sampling rate, # channels, bit depth
AudioInfo                     info_mono(F_AUDIO, 1, 16);                // sampling rate, # channels, bit depth
DriverPins                    my_pins;                                  // board pins
AudioBoard                    audio_board(AudioDriverES8388, my_pins);  // audio board
I2SCodecStream                i2s_stream(audio_board);                  // i2s codec
VBANStream                    vban;                                     // audio over wifi
CsvOutput<int16_t>            test_stream(Serial);                      // data over serial

SineWaveGenerator<int16_t>    sine_wave(8000);
GeneratedSoundStream<int16_t> sound_stream(sine_wave);


ChannelSplitOutput            input_split;                              // splits the stereo input stream into two mono streams
VolumeStream                  out_vol;                                  // output volume control
VolumeOutput                  out_vol_meas;                             // measure the volume of the output
MultiOutput                   multi_output;                             // splits the final output into audio jack, vban output, csv stream
FilteredStream<int16_t, float> audio_filt(out_vol, info_mono.channels); // filter outputting into out_vol volume control
// todo: would be better to rename from hf_vhf mixer --> in1_in2_mixer. Microphone goes through this mixer when selecting LIN2/RIN2.
OutputMixer<int16_t>          hf_vhf_mixer(audio_filt, 3);              // hf, vhf, *and* sidetone audio mixing into audio_filt
ChannelFormatConverterStreamT<int16_t> mono_to_stereo(i2s_stream);      // turns a mono stream into a stereo stream
StreamCopy copier_1(hf_vhf_mixer, sound_stream);                      // move sine wave from sound_stream into the sidetone mixer
StreamCopy copier_2(input_split, i2s_stream);                           // moves data through the streams. To: input_split, from: i2s_stream

// example of i2s codec for both input and output: https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/streams-audiokit-filter-audiokit/streams-audiokit-filter-audiokit.ino


TaskHandle_t audioStreamTaskHandle, blinkTaskHandle, spareTaskHandle, batterySenseTaskHandle, txPulseTaskHandle;

SemaphoreHandle_t btn_semaphore;

int t = 0;
int counter = 0;
int freq_buck = 500e3;


void audioStreamTask(void *param) {
  while(true) {
    copier_1.copy();
    copier_2.copy();
    taskYIELD();
  }
}


void spareTask(void *param) {
  while(true) {
    digitalWrite(LED_RED, HIGH);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    digitalWrite(LED_RED, LOW);
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

  xSemaphoreGiveFromISR(btn_semaphore, NULL);
}


void txPulseTask(void *param) {
  while(true) {
    if(xSemaphoreTake(btn_semaphore, portMAX_DELAY) == pdPASS) {
      Serial.println("TX Pulsing");

      // HF TX test

      // si5351.output_enable(SI5351_CLK2, 1); // TX clock
      digitalWrite(PA_VDD_CTRL, HIGH);
      vTaskDelay(25 / portTICK_PERIOD_MS);      
      digitalWrite(PA_VDD_CTRL, LOW);
      vTaskDelay(25 / portTICK_PERIOD_MS);
      si5351.output_enable(SI5351_CLK2, 0); // TX clock


      // VHF TX test

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

      attachInterrupt(digitalPinToInterrupt(BOOT_BTN), buttonISR, FALLING);
      attachInterrupt(digitalPinToInterrupt(MIC_PTT), buttonISR, FALLING);
    }
  }
}



void setup() {
  // put your setup code here, to run once:
  pinMode(LED_GRN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BPF_SEL_1, OUTPUT);
  pinMode(BPF_SEL_2, OUTPUT);
  pinMode(LPF_SEL_1, OUTPUT);
  pinMode(LPF_SEL_2, OUTPUT);
  pinMode(TX_RX_SEL, OUTPUT);
  pinMode(PA_VDD_CTRL, OUTPUT);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOOT_BTN), buttonISR, FALLING);
  pinMode(MIC_PTT, INPUT);
  attachInterrupt(digitalPinToInterrupt(MIC_PTT), buttonISR, FALLING);
  pinMode(VHF_EN, OUTPUT);
  pinMode(VHF_PTT, OUTPUT);

  digitalWrite(PA_VDD_CTRL, LOW);

  digitalWrite(VHF_EN, HIGH);   // enabled
  digitalWrite(VHF_PTT, HIGH);  // RX


  // set up a PWM channel that drives buck converter sync pin
  // feature for the future: select this frequency based on the dial frequency
  ledcSetup(PWM_CHANNEL_SYNC, freq_buck, 2);
  ledcAttachPin(BUCK_SYNC, PWM_CHANNEL_SYNC);
  ledcWrite(PWM_CHANNEL_SYNC, 2);

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

  AudioLogger::instance().begin(Serial, AudioLogger::Warning);
  // LOGLEVEL_AUDIODRIVER = AudioDriverWarning;
  LOGLEVEL_AUDIODRIVER = AudioDriverDebug;
  // LOGLEVEL_AUDIODRIVER = AudioDriverInfo;

  my_pins.addI2C(PinFunction::CODEC, CODEC_SCL, CODEC_SDA, CODEC_ADDR, CODEC_I2C_SPEED, codecI2C);
  my_pins.addI2S(PinFunction::CODEC, CODEC_MCLK, CODEC_BCLK, CODEC_WS, CODEC_DO, CODEC_DI);
  my_pins.begin();
  audio_board.begin();

  // setup i2s input/output
  Serial.println("I2S begin ...");
  // call to defaultConfig() sets input/output channels. Otherwise need es8388_config_input_device() 
  auto i2s_config = i2s_stream.defaultConfig(RXTX_MODE);
  // i2s_config.input_device = ADC_INPUT_LINE2;    // TEMPORARY!! for LIN2 test. AudioDriver doesn't have a way to set input source besides the i2s_config used at startup
  i2s_config.copyFrom(info_stereo);
  i2s_config.buffer_size = 512;
  i2s_config.buffer_count = 2;
  i2s_config.port_no = 0;
  i2s_stream.begin(i2s_config); // this should apply I2C and I2S configuration

  sine_wave.begin(info_mono, 700);

  // setup vban output (mono)
  auto cfg = vban.defaultConfig(TX_MODE);
  cfg.copyFrom(info_mono);
  cfg.ssid = WIFI_STA_SSID;
  cfg.password = WIFI_STA_PASS;
  cfg.stream_name = "Stream1";
  // cfg.target_ip = IPAddress{192,168,1,37}; 
  cfg.throttle_active = true;
  cfg.throttle_correction_us = -1000; // optimize overload and underrun
  if (!vban.begin(cfg)) stop();
  digitalWrite(LED_GRN, HIGH);

  // OutputVolume sink to measure amplitude
  // note that this is currently consuming out_vol, which means it'll vary with volume control. Either compensate or measure before scaling
  out_vol_meas.setAudioInfo(info_mono);
  out_vol_meas.begin();

  // set up test stream
  test_stream.begin(info_mono);

  // input_split (stereo) --> two (mono) channels of the audio mixer
  input_split.addOutput(hf_vhf_mixer, 0);
  input_split.addOutput(hf_vhf_mixer, 1);
  input_split.begin(info_stereo);
  
  // hf + vhf --> hf_vhf_mixer (mono). declaration links to audio_filt
  // HF vs VHF select done through setWeight()
  hf_vhf_mixer.begin();
  hf_vhf_mixer.setWeight(0, 0);   // input 0: sidetone
  hf_vhf_mixer.setWeight(1, 0.5);   // input 1, 2: HF and VHF
  hf_vhf_mixer.setWeight(2, 0.5);

  // audio_filt declaration links it to out_vol (mono)
  // audio_filt.setFilter(0, new FIR<float>(coeff_bpf_400_600));
  audio_filt.setFilter(0, new FIR<float>(coeff_lpf_2500));

  // audio_filt (mono) --> out_vol (mono) --> multi_output (mono)
  out_vol.setVolume(1.0);
  out_vol.setStream(audio_filt);
  out_vol.setOutput(multi_output);
  out_vol.begin(info_mono);

  // multi_output goes to vban (mono), mono_to_stereo (mono), csv
  // multi_output.add(vban);
  multi_output.add(mono_to_stereo);
  // multi_output.add(test_stream);
  multi_output.add(out_vol_meas);
  
  // take a mono audio stream and make it stereo
  // declaration links it to i2s stream (stereo output)
  mono_to_stereo.begin(1, 2);

  // turn on DAC that goes to headphones. turn off DAC that goes to VHF/HF audio inputs
  AudioDriver *driver = audio_board.getDriver();
  driver->setMute(false, 0);
  driver->setMute(true, 1);


  // note: platformio + arduino puts wifi on core 0

  // run on core 1
  xTaskCreatePinnedToCore(
    audioStreamTask,
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
  si5351.set_freq(((uint64_t) 24060500) * 100, SI5351_CLK1);  // should get rid of that 500hz hack, need to cal oscillator and filter
  si5351.set_freq(((uint64_t) 14040000) * 100, SI5351_CLK2);

  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);

  si5351.output_enable(SI5351_CLK0, 1); // BFO
  si5351.output_enable(SI5351_CLK1, 1); // VFO
  si5351.output_enable(SI5351_CLK2, 0); // TX clock

  // select 20m band
  digitalWrite(BPF_SEL_1, HIGH);
  digitalWrite(BPF_SEL_2, LOW);

  // select RX
  // digitalWrite(TX_RX_SEL, LOW);

  // select TX on band#2
  digitalWrite(TX_RX_SEL, HIGH);
  digitalWrite(LPF_SEL_1, HIGH);
  digitalWrite(LPF_SEL_2, HIGH);  


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
    AudioDriver *driver = audio_board.getDriver();
    if(counter % 2 == 0) {
      // driver->setMute(false, 0);
      // driver->setMute(true, 1);    // turns off DAC output
      // // driver->setInputVolume(10);   // changes PGA
      // audio_filt.setFilter(0, new FIR<float>(coeff_bandpass));  // definitely creating a memory issue by creating new filters repeatedly...

      hf_vhf_mixer.setWeight(0, 0);
    }
    else {
      // driver->setMute(true, 0);
      // driver->setMute(false, 1);
      // // driver->setInputVolume(80);
      // audio_filt.setFilter(0, new FIR<float>(coeff_lowpass));  // definitely creating a memory issue by creating new filters repeatedly...

      hf_vhf_mixer.setWeight(0, 1.0);
    }      
    counter++;
    t = millis();
  } 
}
