#include <Arduino.h>
#include <Wire.h>
#include <si5351.h>


#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"

#include "wifi_conn.h"

Si5351 si5351;
// TwoWire clockBus = TwoWire(0);
// TwoWire codecBus = TwoWire(1);


/*
AudioInfo                     audio_info(44200, 2, 16);                // sampling rate, # channels, bit depth
SineWaveGenerator<int16_t>    sine_wave(32000);                        // amplitude
GeneratedSoundStream<int16_t> sound_stream(sine_wave);                 // sound generator
DriverPins                    my_pins;                                 // board pins
AudioBoard                    audio_board(AudioDriverES8388, my_pins); // audio board
I2SCodecStream                i2s_stream(audio_board);             // i2s codec
StreamCopy                    copier(i2s_stream, sound_stream);    // stream copy sound generator to i2s codec
TwoWire                       myWire = TwoWire(0);                     // universal I2C interface
*/


AudioInfo                     audio_info(44200, 2, 16);                 // sampling rate, # channels, bit depth
DriverPins                    my_pins;                                  // board pins
AudioBoard                    audio_board(AudioDriverES8388, my_pins);  // audio board
I2SCodecStream                i2s_stream(audio_board);                  // i2s codec
VolumeStream                  outVol(i2s_stream);                       // vol outputs to i2s_stream (output)
VolumeStream                  inVol(i2s_stream);                        // vol acceps from i2s_stream
StreamCopy                    copier(outVol, inVol);                    // copy i2s_stream (input) to inVol (output)
TwoWire                       myWire = TwoWire(0);                      // universal I2C interface


// example of i2s codec for both input and output: https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/streams-audiokit-filter-audiokit/streams-audiokit-filter-audiokit.ino



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
  // put your main code here, to run repeatedly:

  /*
  digitalWrite(LED_GRN, HIGH);
  delay(1000);
  digitalWrite(LED_GRN, LOW);
  delay(1000);
  */

  my_pins.addI2C(PinFunction::CODEC, CODEC_SCL, CODEC_SDA, CODEC_ADDR, CODEC_I2C_SPEED, myWire);
  my_pins.addI2S(PinFunction::CODEC, CODEC_MCLK, CODEC_BCLK, CODEC_WS, CODEC_DO, CODEC_DI);
  my_pins.begin();
  audio_board.begin();

/*
  Serial.println("Configure output device ..."); 
  // try changing the output channel with a CodecConfig
  CodecConfig cfg;
  cfg.output_device = DAC_OUTPUT_LINE1;
  //cfg.input_device = ADC_INPUT_LINE1;
  // cfg.output_device = DAC_OUTPUT_ALL;
  // audio_board.setConfig(cfg);
  audio_board.begin(cfg);
*/

  Serial.println("I2S begin ..."); 
  auto i2s_config = i2s_stream.defaultConfig(RXTX_MODE);
  i2s_config.copyFrom(audio_info);
  // i2s_config.input_device = ADC_INPUT_LINE1;  // intend dto use RIN1, not sure 
  // i2s_config.output_device  = DAC_OUTPUT_LINE1;
  i2s_stream.begin(i2s_config); // this should apply I2C and I2S configuration
  

  
  inVol.begin(audio_info);
  inVol.setVolume(1.0);
  outVol.begin(audio_info);
  outVol.setVolume(1.0);


  // Setup sine wave
  Serial.println("Sine wave begin...");
  // sine_wave.begin(audio_info, N_B4); // 493.88 Hz

  Serial.println("Setup completed ...");


  int t = 0;
  int counter = 0;
  while(1) {
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
}
