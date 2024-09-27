#include "globals.h"
#include "audio.h"
#include "fir_coeffs_bpf.h"
#include "fir_coeffs_hilbert.h"

#include <Arduino.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"
#include "AudioLibs/VBANStream.h"
#include "Communication/ESPNowStream.h"
#include "Communication/UDPStream.h"

// defines the indices of the audio mixer that combines two input channels and sidetone
#define MIXER_IDX_SIDETONE      0
#define MIXER_IDX_LEFT          1
#define MIXER_IDX_RIGHT         2

// constant used in math [dB]
#define PGA_GAIN                24

#define NOTIFY_PGA_ON           1
#define NOTIFY_PGA_OFF          2
#define NOTIFY_MODE_HF_RXTX_CW  4
#define NOTIFY_MODE_VHF_RX      8
#define NOTIFY_MODE_VHF_TX      16

TwoWire codecI2C = TwoWire(1);  // "Wire" is used in si5351 library. Defined through TwoWire(0), so the other peripheral is still available

TaskHandle_t xAudioTaskHandle;

AudioInfo                     info_stereo(F_AUDIO, 2, 16);              // sampling rate, # channels, bit depth
AudioInfo                     info_mono(F_AUDIO, 1, 16);                // sampling rate, # channels, bit depth
DriverPins                    my_pins;                                  // board pins
AudioBoard                    audio_board(AudioDriverES8388, my_pins);  // audio board
I2SCodecStream                i2s_stream(audio_board);                  // i2s codec

#ifdef AUDIO_EN_OUT_VBAN
VBANStream                    vban;                                     // audio over wifi
#endif
#ifdef AUDIO_EN_OUT_CSV
CsvOutput<int16_t>            csv_stream(Serial);                      // data over serial
#endif
#ifdef AUDIO_EN_OUT_ESPNOW
ESPNowStream now;
const char *peers[] = {"48:CA:43:57:66:98"};    // serial number 1
#endif

SineWaveGenerator<int16_t>    sine_wave;
GeneratedSoundStream<int16_t> sound_stream(sine_wave);

ChannelSplitOutput            input_split;                              // splits the stereo input stream into two mono streams
VolumeStream                  out_vol;                                  // output volume control
VolumeStream                  input_l_vol, input_r_vol;
VolumeMeter                   out_vol_meas;                             // measure the volume of the output
MultiOutput                   multi_output;                             // splits the final output into audio jack, vban output, csv stream
FilteredStream<int16_t, float> audio_filt(out_vol, info_mono.channels); // filter outputting into out_vol volume control
OutputMixer<int16_t>          side_l_r_mix(audio_filt, 3);              // sidetone, left, right, audio mixing into audio_filt
ChannelFormatConverterStreamT<int16_t> mono_to_stereo(i2s_stream);      // turns a mono stream into a stereo stream
// TODO: try using constructor with 3rd argument of default buffer size, currently it's relatively large at 1024
StreamCopy copier_1(side_l_r_mix, sound_stream, 256);                      // move sine wave from sound_stream into the sidetone mixer
StreamCopy copier_2(input_split, i2s_stream, 256);                           // moves data through the streams. To: input_split, from: i2s_stream
#ifdef AUDIO_PATH_IQ
FilteredStream<int16_t, float> hilbert_n45deg(input_l_vol, info_mono.channels);
FilteredStream<int16_t, float> hilbert_p45deg(input_r_vol, info_mono.channels);
#endif

// example of i2s codec for both input and output: https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/streams-audiokit-filter-audiokit/streams-audiokit-filter-audiokit.ino

#ifdef AUDIO_EN_OUT_UDP
WiFiUDP default_udp;
UDPStream udp("", "");    // already connected
// Throttle throttle(udp);
// IPAddress udpAddress(192, 168, 0, 232); // laptop
// IPAddress udpAddress(192, 168, 0, 238); // serial number 1
// IPAddress udpAddress(192, 168, 0, 178); // serial number 2
IPAddress udpAddress(192, 168, 0, 255); // broadcast
const int udpPort = 7000;
#endif

WiFiClient client;
IPAddress clientAddress(192, 168, 0, 178);
const int ip_port = 7000;


float sidetone_vol = 1.0;
float sidetone_freq = F_SIDETONE_DEFAULT;
bool sidetone_en = false;
bool pga_en = false;
float global_vol = 0;
audio_filt_t cur_filt = AUDIO_FILT_DEFAULT;
float last_volume_dB = 0;

void audio_task (void * pvParameter);
void audio_configure_codec(audio_mode_t mode);
void audio_measure_volume();

void audio_init() {
    // note: platformio + arduino puts wifi on core 0
    // run on core 1
    xTaskCreatePinnedToCore(
        audio_task,
        "Audio Stream Updater Task",
        32768,
        NULL,
        TASK_PRIORITY_AUDIO, // priority
        &xAudioTaskHandle,
        1 // core
    );
}

void audio_task(void *param) {
    AudioLogger::instance().begin(Serial, AudioLogger::Warning);
    LOGLEVEL_AUDIODRIVER = AudioDriverWarning;    // AudioDriverInfo  // AudioDriverWarning // AudioDriverDebug    // AudioDriverError

    my_pins.addI2C(PinFunction::CODEC, CODEC_SCL, CODEC_SDA, CODEC_ADDR, CODEC_I2C_SPEED, codecI2C);
    my_pins.addI2S(PinFunction::CODEC, CODEC_MCLK, CODEC_BCLK, CODEC_WS, CODEC_DO, CODEC_DI);
    my_pins.begin();
    audio_board.begin();

    // subsequent calls should be to audio_set_mode(), the thread-safe one exposed externally
    // force a configuration of the codec
    audio_configure_codec(AUDIO_HF_RXTX_CW);

    sine_wave.begin(info_mono, sidetone_freq);

#ifdef AUDIO_EN_OUT_VBAN
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
#endif

    // OutputVolume sink to measure amplitude
    // note that this is currently consuming out_vol, which means it'll vary with volume control. Either compensate or measure before scaling
    out_vol_meas.setAudioInfo(info_mono);
    out_vol_meas.begin();

    // set up test stream
#ifdef AUDIO_EN_OUT_CSV
    csv_stream.begin(info_mono);
#endif

    // input_split (stereo) --> two (mono) volume control pathways
    // note that the indices of side_l_r_mix are set by the declaration above, then the following two lines
    // omit the input_x_vol controls if we are not using IQ data
#ifdef AUDIO_PATH_IQ
    input_split.addOutput(input_l_vol, 0);
    input_split.addOutput(input_r_vol, 1);
#else
    input_split.addOutput(side_l_r_mix, 0);
    input_split.addOutput(side_l_r_mix, 1);
#endif
    input_split.begin(info_stereo);


#ifdef AUDIO_PATH_IQ
    // l/r volume pathways feed into hilbert transforms
    input_l_vol.setOutput(hilbert_n45deg);
    input_l_vol.begin(info_mono);
    input_l_vol.setVolume(0);
    input_r_vol.setOutput(hilbert_p45deg);
    input_r_vol.begin(info_mono);
    input_r_vol.setVolume(1.0);

    // hilbert transforms feed into the sidetone+left+right mixer
    hilbert_n45deg.setFilter(0, new FIR<float>(coeff_hilbert_n45deg));
    hilbert_n45deg.setOutput(side_l_r_mix);
    hilbert_p45deg.setFilter(0, new FIR<float>(coeff_hilbert_p45deg));
    hilbert_p45deg.setOutput(side_l_r_mix);
#endif


    // left + right + sidetone --> side_l_r_mix (mono). declaration links to audio_filt
    // HF, VHF, sidetone all set to zero weight. audio_set_mode() will properly apply weights.
    side_l_r_mix.begin();
    side_l_r_mix.setWeight(MIXER_IDX_SIDETONE, 0);      // input 0: sidetone from sound_stream
    side_l_r_mix.setWeight(MIXER_IDX_LEFT, 0);          // input 1: INx-L codec channel
    side_l_r_mix.setWeight(MIXER_IDX_RIGHT, 0);         // input 2: INx-R codec channel

    // audio_filt declaration has already linked it to out_vol (mono)
    audio_set_filt(AUDIO_FILT_DEFAULT);

    // audio_filt (mono) --> out_vol (mono) --> multi_output (mono)
    audio_set_volume(AUDIO_VOL_DEFAULT);
    out_vol.setStream(audio_filt);
    out_vol.setOutput(multi_output);
    out_vol.begin(info_mono);

    // multi_output goes to vban (mono), mono_to_stereo (mono), csv
    multi_output.add(mono_to_stereo);
    multi_output.add(out_vol_meas);
#ifdef AUDIO_EN_OUT_VBAN
    multi_output.add(vban);
#endif
#ifdef AUDIO_EN_OUT_CSV
    multi_output.add(csv_stream);
#endif
#ifdef AUDIO_EN_OUT_ESPNOW
    auto now_cfg = now.defaultConfig();
    now_cfg.mac_address = "48:CA:43:57:66:5C";   // serial number 2
    now_cfg.channel = 2;
    now_cfg.use_send_ack = false;
    now_cfg.write_retry_count = 0;
    now_cfg.delay_after_write_ms = 0;
    now_cfg.delay_after_failed_write_ms = 0;
    now.begin(now_cfg);
    now.addPeers(peers);
    multi_output.add(now);
#endif
#ifdef AUDIO_EN_OUT_UDP
    multi_output.add(udp);

    udp.setUDP(default_udp);
    udp.begin(udpAddress, udpPort);
    // note that I2S buffer total size needs to be >1492, the UDP write size
    // was getting weird UDP packets in Wireshark otherwise
    // worked with 8x256 buffers
#endif


    // TESTING
    multi_output.add(client);
    client.setNoDelay(true);   // doesn't wait to accumulate packets
    if(!client.connected()){
        while (!client.connect(clientAddress, ip_port)) {
            Serial.println("trying to connect...");
            vTaskDelay(pdMS_TO_TICKS(500));
        }    
    }


    // take a mono audio stream and make it stereo
    // declaration links it to i2s stream (stereo output)
    mono_to_stereo.begin(1, 2);

    // use full analog gain in the codec
    audio_en_pga(true);
    audio_en_sidetone(false);
    audio_en_rx_audio(true);

    uint32_t notifiedValue;
    while(true) {
        copier_1.copy();
        copier_2.copy();

        if (!client.connected())
            Serial.println("Lost server");

        // TODO: move everything below here to a lower priority task that runs less frequently
        // update this variable so other modules can more readily consume it
        last_volume_dB = audio_get_rx_db();

        // look for flags. Don't clear on entry; clear on exit
        if(xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifiedValue, 0) == pdTRUE) {
            if(notifiedValue & NOTIFY_PGA_ON) {
                AudioDriver *driver = audio_board.getDriver();
                driver->setInputVolume(100); // changes PGA in the codec
                pga_en = true;
            }
            if(notifiedValue & NOTIFY_PGA_OFF) {
                AudioDriver *driver = audio_board.getDriver();
                driver->setInputVolume(0); // changes PGA in the codec
                pga_en = false;
            }
            if(notifiedValue & NOTIFY_MODE_HF_RXTX_CW)
                audio_configure_codec(AUDIO_HF_RXTX_CW);
            if(notifiedValue & NOTIFY_MODE_VHF_RX)
                audio_configure_codec(AUDIO_VHF_RX);
            if(notifiedValue & NOTIFY_MODE_VHF_TX)
                audio_configure_codec(AUDIO_VHF_TX);
        }

        // TBD if this is needed
        // this is a LOWEST priority task, yield to another LOWEST priority task
        taskYIELD();
    }
}

void audio_configure_codec(audio_mode_t mode) {
    Serial.println("Configuring codec...");
    auto i2s_config = i2s_stream.defaultConfig(RXTX_MODE);
    i2s_config.copyFrom(info_stereo);
    
    i2s_config.buffer_size = 256;       // working with ip
    i2s_config.buffer_count = 16;
    // i2s_config.buffer_size = 256;       // working with udp
    // i2s_config.buffer_count = 8;
    // i2s_config.buffer_size = 256;       // good compromise
    // i2s_config.buffer_count = 8;
    // i2s_config.buffer_size = 1024;   // works but sidetone is choppy
    // i2s_config.buffer_count = 2;
    // i2s_config.buffer_size = 512;    // long delay
    // i2s_config.buffer_count = 4;
    i2s_config.port_no = 0;

    AudioDriver *driver = audio_board.getDriver();

    // configure the input line using the i2s_stream.begin()
    // then configure output line using driver->setMute()
    // TBD why i2s_stream.begin() needs to be called first.
    if(mode == AUDIO_HF_RXTX_CW) {
        i2s_config.input_device = ADC_INPUT_LINE1;
        i2s_stream.begin(i2s_config); // this applies both I2C and I2S configuration

        driver->setMute(false, 0);
        driver->setMute(true, 1);
    }
    else if(mode == AUDIO_VHF_RX) {
        i2s_config.input_device = ADC_INPUT_LINE2;
        i2s_stream.begin(i2s_config); // this applies both I2C and I2S configuration

        driver->setMute(false, 0);
        driver->setMute(true, 1);
    }
    else if(mode == AUDIO_VHF_TX) {
        i2s_config.input_device = ADC_INPUT_LINE2;
        i2s_stream.begin(i2s_config); // this applies both I2C and I2S configuration

        driver->setMute(true, 0);
        driver->setMute(false, 1);
    }
}

// TODO: think about thread safety with this function. It's the only one in the file that accesses the hardware
void audio_set_mode(audio_mode_t mode) {
    if(mode == AUDIO_HF_RXTX_CW)
        xTaskNotify(xAudioTaskHandle, NOTIFY_MODE_HF_RXTX_CW, eSetBits);
    else if(mode == AUDIO_VHF_RX)
        xTaskNotify(xAudioTaskHandle, NOTIFY_MODE_VHF_RX, eSetBits);
    else if(mode == AUDIO_VHF_TX)
        xTaskNotify(xAudioTaskHandle, NOTIFY_MODE_VHF_TX, eSetBits);
}

// changes the audio FIR in use
// TODO: definitely creating a memory issue by creating new filters repeatedly...
bool audio_set_filt(audio_filt_t filt) {
    switch(filt) {
        case AUDIO_FILT_CW:
            // audio_filt.setFilter(0, new FIR<float>(coeff_bpf_400_600));
            audio_filt.setFilter(0, new FIR<float>(coeff_bpf_300_700));
            break;

        case AUDIO_FILT_SSB:
            // basically a LPF, but should filter out some amount of 60hz noise
            audio_filt.setFilter(0, new FIR<float>(coeff_bpf_400_2000));
            break;
        default:
            return false;
    }
    cur_filt = filt;
    return true;
}

audio_filt_t audio_get_filt() {
    return cur_filt;
}

// debug fuction just to see the phase shift toggling
void audio_test(bool swap) {
#ifdef AUDIO_PATH_IQ
    if(swap) {
        hilbert_n45deg.setFilter(0, new FIR<float>(coeff_hilbert_p45deg));
        hilbert_p45deg.setFilter(0, new FIR<float>(coeff_hilbert_n45deg));
    }
    else {
        hilbert_n45deg.setFilter(0, new FIR<float>(coeff_hilbert_n45deg));
        hilbert_p45deg.setFilter(0, new FIR<float>(coeff_hilbert_p45deg));
    }
#endif
}

void audio_en_pga(bool gain) {
    if(gain)
        xTaskNotify(xAudioTaskHandle, NOTIFY_PGA_ON, eSetBits);
    else
        xTaskNotify(xAudioTaskHandle, NOTIFY_PGA_OFF, eSetBits);
}

// ONLY affects sidetone, does not affect muting of rx audio
void audio_en_sidetone(bool tone) {
    sidetone_en = tone;

    if(tone) {
        side_l_r_mix.setWeight(MIXER_IDX_SIDETONE, sidetone_vol);
    }
    else {
        side_l_r_mix.setWeight(MIXER_IDX_SIDETONE, 0.05);
    }

}

// ONLY affects muting of rx audio, does not affect sidetone
void audio_en_rx_audio(bool en) {
    if(en) {
        // unmute input mixer channels. LEFT channel depends on whether IQ audio is in use
        side_l_r_mix.setWeight(MIXER_IDX_RIGHT, 1.0);
#ifdef AUDIO_PATH_IQ
        side_l_r_mix.setWeight(MIXER_IDX_LEFT, 1.0);
#endif

    }
    else {
        // mute all input mixer channels, regardless of IQ audio
        side_l_r_mix.setWeight(MIXER_IDX_LEFT, 0.0);
        side_l_r_mix.setWeight(MIXER_IDX_RIGHT, 0.0);
    }
}

bool audio_set_volume(float vol) {
    if(vol >= 0.0 && vol <= 1.0) {
        global_vol = vol;
        out_vol.setVolume(vol);
        return true;
    }
    return false;
}

bool audio_set_sidetone_volume(float vol) {
    if(vol >= 0.0 && vol <= 1.0) {
        sidetone_vol = vol;

        // call to audio_en_sidetone() with its current state will adjust weights
        audio_en_sidetone(sidetone_en);
        return true;
    }
    return false;
}

float audio_get_sidetone_volume() {
    return sidetone_vol;
}

bool audio_set_sidetone_freq(float freq) {
    // enforce 0-2kHz range 
    if(freq > 0 && freq < 2000) {
        sidetone_freq = freq;
        sine_wave.setFrequency(sidetone_freq);
        return true;
    }
    return false;
}

// compensate for volume control here
// TODO: change audio chain to measure before applying volume control
float audio_get_rx_db(uint16_t num_to_avg, uint16_t delay_ms) {
    if(sidetone_en)
        return -1001;
    else {
        float volume_dB = 0;
        for(uint16_t i = 0; i < num_to_avg; i++) {
            volume_dB += out_vol_meas.volumeDB();

            // only delay between samples if we are averaging
            if(num_to_avg > 0)
                vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
        volume_dB /= num_to_avg;

        // correct for volume
        volume_dB += 20 * log10(1 / global_vol);

        // correct for PGA
        if(pga_en)
            volume_dB -= PGA_GAIN;

        return volume_dB;
    }
}

// TODO: report this in terms of S-units
float audio_get_s_meter() {
    return last_volume_dB;
}

float audio_get_sidetone_freq() {
    return sidetone_freq;
}

float audio_get_volume() {
    return global_vol;
}