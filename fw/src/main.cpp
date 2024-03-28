#include <Arduino.h>
#include <Wire.h>
#include <si5351.h>


#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"

#include "wifi_conn.h"

Si5351 si5351;
// TwoWire clockBus = TwoWire(0);
// TwoWire codecBus = TwoWire(1);


AudioInfo                     audio_info(44200, 2, 16);                // sampling rate, # channels, bit depth
SineWaveGenerator<int16_t>    sine_wave(32000);                        // amplitude
GeneratedSoundStream<int16_t> sound_stream(sine_wave);                 // sound generator
DriverPins                    my_pins;                                 // board pins
AudioBoard                    audio_board(AudioDriverES8388, my_pins); // audio board
I2SCodecStream                i2s_out_stream(audio_board);             // i2s coded
StreamCopy                    copier(i2s_out_stream, sound_stream);    // stream copy sound generator to i2s codec
TwoWire                       myWire = TwoWire(0);                     // universal I2C interface



void setup() {
  // put your setup code here, to run once:
  pinMode(LED_GRN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  Serial.begin(SERIAL_SPEED);

  delay(5000);

  // wifi_init();

  // myWire.begin(CODEC_SDA, CODEC_SCL);
  // myWire.beginTransmission(CODEC_ADDR);
  // myWire.write(46);
  // int error = myWire.endTransmission();
  // myWire.requestFrom(CODEC_ADDR, 1);
  // int result = myWire.read();
  // Serial.print("error: ");
  // Serial.println(error);
  // Serial.print("result: ");
  // Serial.println(result);


  AudioLogger::instance().begin(Serial, AudioLogger::Warning);
  // LOGLEVEL_AUDIODRIVER = AudioDriverWarning;
  LOGLEVEL_AUDIODRIVER = AudioDriverDebug;
  // LOGLEVEL_AUDIODRIVER = AudioDriverInfo;

  

  // i2s_out_stream.setVolume(0.1);

  // AudioInfo tmp(44200, 1, 16);
  // i2s_out_stream.setAudioInfo(tmp);

  // myWire.beginTransmission(CODEC_ADDR);
  // myWire.write(46);
  // error = myWire.endTransmission();
  // myWire.requestFrom(CODEC_ADDR, 1);
  // result = myWire.read();
  // Serial.print("error: ");
  // Serial.println(error);
  // Serial.print("result: ");
  // Serial.println(result);


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
  // wait for a second
  delay(1000);
  // turn the LED off by making the voltage LOW
  digitalWrite(LED_GRN, LOW);
   // wait for a second
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
  cfg.output_device = DAC_OUTPUT_LINE2;
  cfg.input_device = ADC_INPUT_LINE1;
  // cfg.output_device = DAC_OUTPUT_ALL;
  // audio_board.setConfig(cfg);
  audio_board.begin(cfg);
*/


  Serial.println("I2S begin ..."); 
  auto i2s_config = i2s_out_stream.defaultConfig(RXTX_MODE);
  i2s_config.copyFrom(audio_info);  
  // i2s_config.output_device = DAC_OUTPUT_LINE1;
  // i2s_config.input_device = ADC_INPUT_LINE1;
  i2s_out_stream.begin(i2s_config); // this should apply I2C and I2S configuration

  // Setup sine wave
  Serial.println("Sine wave begin...");
  sine_wave.begin(audio_info, N_B4); // 493.88 Hz

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
    
  }
}
