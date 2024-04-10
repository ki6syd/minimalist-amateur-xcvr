#include <Arduino.h>
#include <Wire.h>
#include <si5351.h>


#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"
#include "AudioLibs/VBANStream.h"

#include "fir_coeffs_bpf.h"
#include "wifi_conn.h"

Si5351 si5351;
// TwoWire clockBus = TwoWire(0);
// TwoWire codecBus = TwoWire(1);
TwoWire codecI2C = TwoWire(1);                      // "Wire" is used in si5351 library. Defined through TwoWire(0), so the other peripheral is still available
// TwoWire clockI2C = TwoWire(1);

AudioInfo                     info_stereo(F_AUDIO, 2, 16);                // sampling rate, # channels, bit depth
AudioInfo                     info_mono(F_AUDIO, 1, 16);                  // sampling rate, # channels, bit depth
DriverPins                    my_pins;                                  // board pins
AudioBoard                    audio_board(AudioDriverES8388, my_pins);  // audio board
I2SCodecStream                i2s_stream(audio_board);                  // i2s codec
VBANStream vban;
// CsvOutput<int16_t> csvStream(Serial);

// SineWaveGenerator<int16_t>    sine_wave(32000);
// GeneratedSoundStream<int16_t> sound_stream(sine_wave);

ChannelSplitOutput            input_split;                              // splits the stereo input stream into two mono streams
VolumeStream                  out_vol;                                  // output volume control
FilteredStream<int16_t, float> filtered(out_vol, info_mono.channels);
// ChannelFormatConverterStreamT<int16_t> mono_to_stereo(i2s_stream);
StreamCopy copier_1(input_split, i2s_stream);

TaskHandle_t audioStreamTaskHandle, blinkTaskHandle, spareTaskHandle;

// example of i2s codec for both input and output: https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/streams-audiokit-filter-audiokit/streams-audiokit-filter-audiokit.ino
int t = 0;
int counter = 0;



void audioStreamTask(void *param) {
  while(true) {
    copier_1.copyAll();
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
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    digitalWrite(LED_GRN, LOW);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(LED_GRN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(SPARE_0, OUTPUT);
  pinMode(SPARE_1, OUTPUT);
  Serial.begin(SERIAL_SPEED);

  delay(5000);

  // wifi_init();

  AudioLogger::instance().begin(Serial, AudioLogger::Warning);
  // LOGLEVEL_AUDIODRIVER = AudioDriverWarning;
  LOGLEVEL_AUDIODRIVER = AudioDriverDebug;
  // LOGLEVEL_AUDIODRIVER = AudioDriverInfo;
  
  // setup wav server
  // setup output
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


  my_pins.addI2C(PinFunction::CODEC, CODEC_SCL, CODEC_SDA, CODEC_ADDR, CODEC_I2C_SPEED, codecI2C);
  my_pins.addI2S(PinFunction::CODEC, CODEC_MCLK, CODEC_BCLK, CODEC_WS, CODEC_DO, CODEC_DI);
  my_pins.begin();
  audio_board.begin();

  Serial.println("I2S begin ..."); 
  auto i2s_config = i2s_stream.defaultConfig(RXTX_MODE);
  i2s_config.copyFrom(info_stereo);
  i2s_config.buffer_size = 512;
  i2s_config.buffer_count = 2;
  i2s_config.port_no = 0;
  i2s_stream.begin(i2s_config); // this should apply I2C and I2S configuration

  // sine_wave.begin(info_mono, N_B4);
  // csvStream.begin(info_stereo);

  filtered.setFilter(0, new FIR<float>(coeff_bandpass));

  // input_split (stereo) --> out_vol (mono)
  input_split.addOutput(filtered, 1);
  input_split.begin(info_stereo);
  Serial.println("Done creating input_split");

  // input_split (mono) --> out_vol (mono)
  out_vol.setVolume(1.0);
  // out_vol.setOutput(mono_to_stereo);
  out_vol.setOutput(vban);
  out_vol.begin(info_mono);

  // mono_to_stereo.begin(1, 2);

  // run on core 1
  xTaskCreatePinnedToCore(
    audioStreamTask,
    "Audio Stream Updater Task",
    4096,
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
    2, // priority
    &spareTaskHandle,
    1 // core
  );

  xTaskCreatePinnedToCore(
    blinkTask,
    "LED Blinking Task",
    4096,
    NULL,
    1, // priority
    &blinkTaskHandle,
    0 // core
  );

  Wire.begin(CLOCK_SDA, CLOCK_SCL);
  
  Serial.print("[SI5351] Status: ");
  Serial.println(si5351.si5351_read(SI5351_DEVICE_STATUS));
  si5351.init(SI5351_CRYSTAL_LOAD_8PF , 26000000 , 0);
  
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);

  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA);
  
  si5351.set_freq(10000000 * 100, SI5351_CLK0);
  si5351.set_freq(20000000 * 100, SI5351_CLK1);
  si5351.set_freq(30000000 * 100, SI5351_CLK2);

  si5351.output_enable(SI5351_CLK0, 1);
  si5351.output_enable(SI5351_CLK1, 1);
  si5351.output_enable(SI5351_CLK2, 0);

}


void loop() {  
  if(millis() - t > 5000) {
    AudioDriver *driver = audio_board.getDriver();
    if(counter % 2 == 0) {
      // driver->setMute(false, 0);
      // driver->setMute(true, 1);    // turns off DAC output
      // driver->setInputVolume(0);   // changes PGA
      filtered.setFilter(0, new FIR<float>(coeff_bandpass));  // definitely creating a memory issue by creating new filters repeatedly...
    }
    else {
      // driver->setMute(true, 0);
      // driver->setMute(false, 1);
      // driver->setInputVolume(100);
      filtered.setFilter(0, new FIR<float>(coeff_lowpass));  // definitely creating a memory issue by creating new filters repeatedly...
    }      
    counter++;
    t = millis();
  } 
}