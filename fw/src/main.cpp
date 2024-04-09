#include <Arduino.h>
#include <Wire.h>
#include <si5351.h>


#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"
#include "AudioLibs/VBANStream.h"

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

// fiiir.com
float coeff_bandpass[] = {
    -0.000049667891008862,
    -0.000082505495149508,
    -0.000082419725815841,
    -0.000042564605141018,
    0.000031750886731946,
    0.000123628830704511,
    0.000208683928576606,
    0.000261700248992016,
    0.000264044309628554,
    0.000209692356454297,
    0.000107990398357052,
    -0.000017907552011543,
    -0.000136787319490411,
    -0.000217938496299906,
    -0.000240237949491669,
    -0.000199124134982942,
    -0.000109232010858879,
    -0.000001527119759525,
    0.000084736252823148,
    0.000113390071481858,
    0.000063089300699269,
    -0.000063958305670638,
    -0.000239138791031772,
    -0.000411995056264291,
    -0.000521881217561321,
    -0.000514437759131790,
    -0.000359097494486809,
    -0.000063164337554578,
    0.000321579280524773,
    0.000702318619586611,
    0.000959079945854949,
    0.000966951820742127,
    0.000622464464258271,
    -0.000131307217960568,
    -0.001286398735524864,
    -0.002762650578780128,
    0.002830378530404382,
    -0.002519970638830962,
    -0.008923262182541074,
    -0.014740624153076236,
    -0.018331764976970623,
    -0.018516117685207851,
    -0.014932120797064536,
    -0.008185525148043878,
    0.000268012410851050,
    0.008487653030118019,
    0.014569017602758257,
    0.017182073296091019,
    0.015971559538961982,
    0.011696240489674676,
    0.006050238002762203,
    0.001196867984785052,
    -0.000870561751374628,
    0.001030647709703133,
    0.006815017628835920,
    0.015000984997418349,
    0.022978295097917122,
    0.027631775960962515,
    0.026181557330637265,
    0.017029869365863586,
    0.000386325021703308,
    -0.021515141340570039,
    -0.044701374113571932,
    -0.064217527677578934,
    -0.075204531528552832,
    -0.074053292757130351,
    -0.059358640520375372,
    -0.032430104508599761,
    0.002787410432163656,
    0.040394064931157714,
    0.073684408281848995,
    0.096518060010860357,
    0.104605886405147855,
    0.096518060010861578,
    0.073684408281849204,
    0.040394064931157297,
    0.002787410432163698,
    -0.032430104508599664,
    -0.059358640520375344,
    -0.074053292757130046,
    -0.075204531528552748,
    -0.064217527677579059,
    -0.044701374113571862,
    -0.021515141340569949,
    0.000386325021703313,
    0.017029869365863644,
    0.026181557330637185,
    0.027631775960962331,
    0.022978295097917097,
    0.015000984997418327,
    0.006815017628835875,
    0.001030647709703207,
    -0.000870561751374611,
    0.001196867984785021,
    0.006050238002762238,
    0.011696240489674690,
    0.015971559538961975,
    0.017182073296091092,
    0.014569017602758260,
    0.008487653030117972,
    0.000268012410851050,
    -0.008185525148043885,
    -0.014932120797064498,
    -0.018516117685207729,
    -0.018331764976970581,
    -0.014740624153076257,
    -0.008923262182541058,
    -0.002519970638830967,
    0.002830378530404359,
    -0.002762650578780139,
    -0.001286398735524867,
    -0.000131307217960562,
    0.000622464464258268,
    0.000966951820742125,
    0.000959079945854949,
    0.000702318619586618,
    0.000321579280524779,
    -0.000063164337554574,
    -0.000359097494486804,
    -0.000514437759131772,
    -0.000521881217561316,
    -0.000411995056264300,
    -0.000239138791031777,
    -0.000063958305670631,
    0.000063089300699271,
    0.000113390071481856,
    0.000084736252823151,
    -0.000001527119759520,
    -0.000109232010858877,
    -0.000199124134982944,
    -0.000240237949491669,
    -0.000217938496299902,
    -0.000136787319490406,
    -0.000017907552011532,
    0.000107990398357060,
    0.000209692356454315,
    0.000264044309628568,
    0.000261700248992012,
    0.000208683928576599,
    0.000123628830704517,
    0.000031750886731944,
    -0.000042564605141022,
    -0.000082419725815837,
    -0.000082505495149500,
    -0.000049667891008860
};

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
  if(millis() - t > 2000) {
    AudioDriver *driver = audio_board.getDriver();
    if(counter % 2 == 0) {
      // driver->setMute(false, 0);
      // driver->setMute(true, 1);
      driver->setInputVolume(0);
      
    }
    else {
      // driver->setMute(true, 0);
      // driver->setMute(false, 1);
      driver->setInputVolume(100);
    }      
    counter++;
    t = millis();
  } 
}