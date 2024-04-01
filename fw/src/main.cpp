#include <Arduino.h>
#include <Wire.h>
#include <si5351.h>


#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"

#include "wifi_conn.h"

Si5351 si5351;
// TwoWire clockBus = TwoWire(0);
// TwoWire codecBus = TwoWire(1);
TwoWire                       myWire = TwoWire(0);                      // universal I2C interface

AudioInfo                     info_stereo(44200, 2, 16);                // sampling rate, # channels, bit depth
AudioInfo                     info_mono(44200, 1, 16);                  // sampling rate, # channels, bit depth
DriverPins                    my_pins;                                  // board pins
AudioBoard                    audio_board(AudioDriverES8388, my_pins);  // audio board
I2SCodecStream                i2s_stream(audio_board);                  // i2s codec


SineWaveGenerator<int16_t>    sine_wave(32000);
GeneratedSoundStream<int16_t> sound_stream(sine_wave);

SineWaveGenerator<int16_t>    sine_wave2(32000);
GeneratedSoundStream<int16_t> sound_stream2(sine_wave2);

// ChannelSplitOutput            input_split;                              // splits the stereo input stream into two mono streams
// ChannelsSelectOutput          input_split;
VolumeStream                  in_vol;                                   // input volume control
VolumeStream                  out_vol;                                  // output volume control
// FilteredStream<int16_t, float> filtered(inVol, audio_info.channels);
InputMerge<int16_t>           output_merge;                             // merges two mono channels into a stereo output
// StreamCopy                    copier(input_split, i2s_stream);          // copier(output, input) drives data from stream to the splitter
StreamCopy copier(i2s_stream, output_merge);

// example of i2s codec for both input and output: https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/streams-audiokit-filter-audiokit/streams-audiokit-filter-audiokit.ino


int t = 0;
int counter = 0;

float coef[] = { 0.021, 0.096, 0.146, 0.096, 0.021};

void setup() {
  // put your setup code here, to run once:
  pinMode(LED_GRN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(SPARE_0, OUTPUT);
  Serial.begin(SERIAL_SPEED);

  delay(5000);

  // wifi_init();

  AudioLogger::instance().begin(Serial, AudioLogger::Warning);
  // LOGLEVEL_AUDIODRIVER = AudioDriverWarning;
  LOGLEVEL_AUDIODRIVER = AudioDriverDebug;
  // LOGLEVEL_AUDIODRIVER = AudioDriverInfo;


  // filtered.setFilter(0, new FIR<float>(coef));
  // filtered.setFilter(1, new FIR<float>(coef));

  

  my_pins.addI2C(PinFunction::CODEC, CODEC_SCL, CODEC_SDA, CODEC_ADDR, CODEC_I2C_SPEED, myWire);
  my_pins.addI2S(PinFunction::CODEC, CODEC_MCLK, CODEC_BCLK, CODEC_WS, CODEC_DO, CODEC_DI);
  my_pins.begin();
  audio_board.begin();


  // Serial.println("Configure output device ..."); 
  // // try changing the output channel with a CodecConfig
  // CodecConfig cfg;
  // // cfg.output_device = DAC_OUTPUT_LINE1;
  // //cfg.input_device = ADC_INPUT_LINE1;
  // cfg.output_device = DAC_OUTPUT_ALL;
  // audio_board.setConfig(cfg);

  Serial.println("I2S begin ..."); 
  auto i2s_config = i2s_stream.defaultConfig(RXTX_MODE);
  i2s_config.copyFrom(info_stereo);
  // i2s_config.input_device = ADC_INPUT_LINE1;  // intend dto use RIN1, not sure 
  // i2s_config.output_device  = DAC_OUTPUT_LINE1;
  i2s_stream.begin(i2s_config); // this should apply I2C and I2S configuration

/*
  input_split.begin(info_stereo);
  input_split.addOutput(in_vol, 0);
  input_split.addOutput(dummy_1, 1);


  input_split.addOutput(in_vol, 0);
  input_split.begin(info_stereo);

*/

  sine_wave.begin(info_mono, N_B4); // 493.88 Hz
  sine_wave2.begin(info_mono, N_C4);

/*
  in_vol.begin(info_mono);
  in_vol.setVolume(1.0);
  in_vol.setOutput(out_vol);

  out_vol.begin(info_mono);
  out_vol.setVolume(1.0);
  out_vol.setOutput(output_merge);
*/
  
  output_merge.add(sound_stream);
  output_merge.add(sound_stream2);
  output_merge.begin(info_stereo);

  // clockBus.begin(CLOCK_SDA, CLOCK_SCL, 100000);
  /*
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
  */

}

void loop() { 
  copier.copy();

  if(millis() - t > 1000) {
    AudioDriver *driver = audio_board.getDriver();
    if(counter % 2 == 0) {
      driver->setMute(false, 0);
      driver->setMute(true, 1);
      digitalWrite(LED_GRN, HIGH);
      Serial.println("on");
    }
    else {
      driver->setMute(true, 0);
      driver->setMute(false, 1);
      digitalWrite(LED_GRN, LOW);
      Serial.println("off");
    }      
    counter++;
    t = millis();
  }

  if(millis() % 2 == 0)
    digitalWrite(SPARE_0, HIGH);
  else 
    digitalWrite(SPARE_0, LOW);
}
