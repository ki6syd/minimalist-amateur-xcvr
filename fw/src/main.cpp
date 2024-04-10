#include <Arduino.h>
#include <Wire.h>
#include <si5351.h>

#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"
#include "AudioLibs/VBANStream.h"

#include "fir_coeffs_bpf.h"
#include "wifi_conn.h"
#include "audio.h"

Si5351 si5351;
// TwoWire clockBus = TwoWire(0);
// TwoWire codecBus = TwoWire(1);
TwoWire codecI2C = TwoWire(1);                      // "Wire" is used in si5351 library. Defined through TwoWire(0), so the other peripheral is still available
// TwoWire clockI2C = TwoWire(1);

AudioInfo                     info_stereo(F_AUDIO, 2, 16);              // sampling rate, # channels, bit depth
AudioInfo                     info_mono(F_AUDIO, 1, 16);                // sampling rate, # channels, bit depth
DriverPins                    my_pins;                                  // board pins
AudioBoard                    audio_board(AudioDriverES8388, my_pins);  // audio board
I2SCodecStream                i2s_stream(audio_board);                  // i2s codec
VBANStream                    vban;                                     // audio over wifi

// SineWaveGenerator<int16_t>    sine_wave(32000);
// GeneratedSoundStream<int16_t> sound_stream(sine_wave);

ChannelSplitOutput            input_split;                              // splits the stereo input stream into two mono streams
VolumeStream                  input_hf_sel, input_vhf_sel;              // gives ability to mute left or right channel (HF or VHF)
VolumeStream                  input_vol;                                // gain on the input, also allows some linking between hf_vhf_mixer and audio_filt
InputMixer<int16_t>           hf_vhf_mixer;
VolumeStream                  out_vol;                                  // output volume control
MultiOutput                   multi_output;                             // splits the final output into audio jack and vban output
FilteredStream<int16_t, float> audio_filt(out_vol, info_mono.channels);
ChannelFormatConverterStreamT<int16_t> mono_to_stereo(i2s_stream);      // turns a mono stream into a stereo stream
StreamCopy copier_1(input_split, i2s_stream);                           // moves data through the streams. To: input_split, from: i2s_stream

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

  my_pins.addI2C(PinFunction::CODEC, CODEC_SCL, CODEC_SDA, CODEC_ADDR, CODEC_I2C_SPEED, codecI2C);
  my_pins.addI2S(PinFunction::CODEC, CODEC_MCLK, CODEC_BCLK, CODEC_WS, CODEC_DO, CODEC_DI);
  my_pins.begin();
  audio_board.begin();

  // setup i2s input/output
  Serial.println("I2S begin ..."); 
  auto i2s_config = i2s_stream.defaultConfig(RXTX_MODE);
  i2s_config.copyFrom(info_stereo);
  i2s_config.buffer_size = 512;
  i2s_config.buffer_count = 2;
  i2s_config.port_no = 0;
  i2s_stream.begin(i2s_config); // this should apply I2C and I2S configuration

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

  // sine_wave.begin(info_mono, N_B4);
  // csvStream.begin(info_stereo);

  // input_split (stereo) --> input_hf_sel (mono from right channel), input_vhf_sel (mono from left channel)
  input_split.addOutput(input_vhf_sel, 0);
  input_split.addOutput(input_hf_sel, 1);
  input_split.begin(info_stereo);
  Serial.println("Done creating input_split");

  // input_hf_sel (mono) --> audio_filt (mono)
  input_hf_sel.setVolume(1.0);
  // input_hf_sel.setOutput(audio_filt);
  input_hf_sel.begin(info_mono);

  
  // input_vhf_sel (mono) --> audio_filt (mono)
  input_vhf_sel.setVolume(1.0);
  // input_vhf_sel.setOutput(audio_filt);
  input_vhf_sel.begin(info_mono);

  // hf + vhf --> hf_vhf_mixer (mono)
  hf_vhf_mixer.add(input_hf_sel);
  hf_vhf_mixer.add(input_vhf_sel);
  hf_vhf_mixer.begin(info_mono);
  

  // input mixer (mono) --> input vol (mono) --> audio_filt (mono);
  input_vol.setVolume(1.0);
  // input_vol.setStream(hf_vhf_mixer);
  input_vol.setOutput(audio_filt);
  input_vol.begin(info_mono);

  // audio_filt declaration links it to out_vol (mono)
  audio_filt.setFilter(0, new FIR<float>(coeff_bandpass));

  // audio_filt (mono) --> out_vol (mono)
  out_vol.setVolume(1.0);
  out_vol.setOutput(multi_output);
  out_vol.begin(info_mono);

  // multi_output goes to vban (mono) and mono_to_stereo (mono)
  multi_output.add(vban);
  multi_output.add(mono_to_stereo);
  
  // declaration links it to i2s stream (stereo output)
  mono_to_stereo.begin(1, 2);




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
      audio_filt.setFilter(0, new FIR<float>(coeff_bandpass));  // definitely creating a memory issue by creating new filters repeatedly...
    }
    else {
      // driver->setMute(true, 0);
      // driver->setMute(false, 1);
      // driver->setInputVolume(100);
      audio_filt.setFilter(0, new FIR<float>(coeff_lowpass));  // definitely creating a memory issue by creating new filters repeatedly...
    }      
    counter++;
    t = millis();
  } 
}