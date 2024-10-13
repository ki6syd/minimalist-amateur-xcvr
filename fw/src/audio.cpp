#include "globals.h"
#include "audio.h"
#include "fir_coeffs_bpf_8kHz.h"
#include "fir_coeffs_hilbert_8kHz.h"
#include "file_system.h"

#include <Arduino.h>
#include <Wire.h>
#include <WiFiUdp.h>                    // TODO: move the networking items to wifi_conn
#include <ESPmDNS.h>
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

#define NOTIFY_PGA_ON           (1 << 0)
#define NOTIFY_PGA_OFF          (1 << 1)
#define NOTIFY_MODE_HF_RXTX_CW  (1 << 2)
#define NOTIFY_MODE_VHF_RX      (1 << 3)
#define NOTIFY_MODE_VHF_TX      (1 << 4)
#define NOTIFY_DBG_MAX_VOL      (1 << 5)

#define INT16T_MAX              32767

#define BUFFER_CHUNK            64

TwoWire codecI2C = TwoWire(1);  // "Wire" is used in si5351 library. Defined through TwoWire(0), so the other peripheral is still available

TaskHandle_t xDSPTaskHandle, xAudioTaskHandle, xGainTaskHandle;

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
#ifdef AUDIO_EN_OUT_IP
WiFiClient client;
// IPAddress client_address(192, 168, 0, 178);  // serial number 2
IPAddress client_address(192, 168, 0, 232);  // laptop
const int ip_port = 7000;
#endif

SineWaveGenerator<int16_t>    sine_wave;
GeneratedSoundStream<int16_t> sound_stream(sine_wave);

ChannelSplitOutput            *input_split;                              // splits the stereo input stream into two mono streams
VolumeStream                  out_vol;                                  // output volume control
VolumeStream                  input_l_vol, input_r_vol;
VolumeMeter                   audio_filt_meas(out_vol);                 // measures the volume output of the audio filter, outputs into out_vol volume control
MultiOutput                   *multi_output;                             // splits the final output into audio jack, vban output, csv stream
FilteredStream<int16_t, float> audio_filt(audio_filt_meas, info_mono.channels); // filter outputting into audio_filt_meas volume detection
// TODO: inject sidetone after the audio filter, to avoid delays
OutputMixer<int16_t>          *side_l_r_mix;              // sidetone, left, right, audio mixing into audio_filt
ChannelFormatConverterStreamT<int16_t> mono_to_stereo(i2s_stream);      // turns a mono stream into a stereo stream
AudioEffectStream             effects(mono_to_stereo);                  // effects --> mono_to_stereo             
Distortion                    *volume_limiter;
StreamCopy copier_1(BUFFER_CHUNK);
StreamCopy copier_2(BUFFER_CHUNK * 2);
#ifdef AUDIO_PATH_IQ
FilteredStream<int16_t, float> hilbert_n45deg(input_l_vol, info_mono.channels);
FilteredStream<int16_t, float> hilbert_p45deg(input_r_vol, info_mono.channels);
float i_channel_correction = 1.0;
float q_channel_correction = 0.89;
#endif

// example of i2s codec for both input and output: https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/streams-audiokit-filter-audiokit/streams-audiokit-filter-audiokit.ino

float sidetone_vol = AUDIO_SIDE_DEFAULT;
float sidetone_freq = F_SIDETONE_DEFAULT;
bool sidetone_en = false;
bool pga_en = false;
bool i2s_configured = false;
float global_vol = AUDIO_VOL_DEFAULT;
audio_filt_t cur_filt = AUDIO_FILT_DEFAULT;
audio_mode_t cur_audio_mode = AUDIO_HF_RXTX_CW;
float last_volume_dB = 0;
uint32_t max_safe_vol = 32768;                   // 32768 allows full volume output (int32_t)

void audio_dsp_task(void * pvParameter);
void audio_dsp_task_restart();
void audio_logic_task(void *pvParameter);
void audio_gain_task(void *pvParameter);
void audio_measure_volume();
void audio_set_dacs(audio_mode_t audio_mode);

void audio_init() {
    // load maximum volume (a float from 0-1.0) and scale by maximum int32_t
    // TODO: make sure this ended up between 0-32768
    max_safe_vol = (uint32_t) (fs_load_setting(PREFERENCE_FILE, "max_audio_output").toFloat() * 32768); 

    // load sidetone volume
    // TODO: enforce bounds
    if(fs_setting_exists(PREFERENCE_FILE, "sidetone_level"))
        sidetone_vol = fs_load_setting(PREFERENCE_FILE, "sidetone_level").toInt();

    // note: platformio + arduino puts wifi on core 0
    // run on core 1
    xTaskCreatePinnedToCore(
        audio_dsp_task,
        "Audio Stream Updater Task",
        65536,
        NULL,
        TASK_PRIORITY_DSP, // priority
        &xDSPTaskHandle,
        TASK_CORE_DSP // core
    );

    xTaskCreatePinnedToCore(
        audio_logic_task,
        "Audio Housekeeping Task",
        16384,
        NULL,
        TASK_PRIORITY_AUDIO, // priority
        &xAudioTaskHandle,
        TASK_CORE_AUDIO // core
    );

    // working as designed but commented out, should implement AGC which actually controls PGA level
    /*
    xTaskCreatePinnedToCore(
        audio_gain_task,
        "Audio Gain Task",
        16384,
        NULL,
        TASK_PRIORITY_AUDIO, // priority
        &xGainTaskHandle,
        TASK_CORE_AUDIO // core
    );
    */
}

void audio_dsp_task(void *param) {
    Serial.println("Starting Audio DSP task");

    AudioLogger::instance().begin(Serial, AudioLogger::Warning);
    LOGLEVEL_AUDIODRIVER = AudioDriverWarning;    // AudioDriverInfo  // AudioDriverWarning // AudioDriverDebug    // AudioDriverError

    // allocate side_l_r_mix, memory will free up when task gets deleted
    side_l_r_mix = new OutputMixer<int16_t>(audio_filt, 3);
    input_split = new ChannelSplitOutput();

    // tie side_l_r_mix to copier now that it's allocated. Puts sidetone as first index of side_l_r_mix
    copier_1.begin(*side_l_r_mix, sound_stream);
    copier_2.begin(*input_split, i2s_stream);                      // moves data through the streams. To: input_split, from: i2s_stream

    my_pins.addI2C(PinFunction::CODEC, CODEC_SCL, CODEC_SDA, CODEC_ADDR, CODEC_I2C_SPEED, codecI2C);
    my_pins.addI2S(PinFunction::CODEC, CODEC_MCLK, CODEC_BCLK, CODEC_WS, CODEC_DO, CODEC_DI);
    my_pins.begin();
    audio_board.begin();

    // codec setup
    auto i2s_config = i2s_stream.defaultConfig(RXTX_MODE);
    i2s_config.copyFrom(info_stereo);
    i2s_config.buffer_size = BUFFER_CHUNK * 2;
    i2s_config.buffer_count = 4;
    i2s_config.port_no = 0;
    if(cur_audio_mode == AUDIO_HF_RXTX_CW)
        i2s_config.input_device = ADC_INPUT_LINE1;
    else if(cur_audio_mode == AUDIO_VHF_RX || cur_audio_mode == AUDIO_VHF_TX)
        i2s_config.input_device = ADC_INPUT_LINE2;
    i2s_stream.begin(i2s_config); // this applies both I2C and I2S configuration

    // mute specific channels (by powering DAC on/off) based on audio mode
    audio_set_dacs(cur_audio_mode);

    // sidetone audio source
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
#ifdef AUDIO_EN_OUT_CSV
    csv_stream.begin(info_mono);
#endif

    // input_split (stereo) --> two (mono) volume control pathways
    // note that the indices of side_l_r_mix are set by the declaration above, then the following two lines
    // omit the input_x_vol controls if we are not using IQ data
#ifdef AUDIO_PATH_IQ
    // order of these will determine which side_l_r_mix is working
    input_split.addOutput(input_l_vol, 0);
    input_split.addOutput(input_r_vol, 1);
    
#else
    input_split->addOutput(*side_l_r_mix, 0);
    input_split->addOutput(*side_l_r_mix, 1);
#endif
    input_split->begin(info_stereo);


#ifdef AUDIO_PATH_IQ
    // l/r volume pathways feed into hilbert transforms
    input_l_vol.setOutput(hilbert_n45deg);
    input_l_vol.begin(info_mono);
    input_l_vol.setVolume(q_channel_correction);

    input_r_vol.setOutput(hilbert_p45deg);
    input_r_vol.begin(info_mono);
    input_r_vol.setVolume(i_channel_correction);

    // hilbert transforms feed into the sidetone+left+right mixer
    hilbert_n45deg.setFilter(0, new FIR<float>(coeff_hilbert_n45deg));
    hilbert_n45deg.setOutput(side_l_r_mix);

    hilbert_p45deg.setFilter(0, new FIR<float>(coeff_hilbert_p45deg));
    hilbert_p45deg.setOutput(side_l_r_mix);
#endif

    // left + right + sidetone --> side_l_r_mix (mono). declaration links to audio_filt
    // HF I, Q, sidetone all set to zero weight. audio_set_mode() will properly apply weights.
    side_l_r_mix->begin();
    side_l_r_mix->setWeight(MIXER_IDX_SIDETONE, 1);      // input 0: sidetone from sound_stream. Set to 1.0, sidetone volume control happens on the sine wave
    side_l_r_mix->setWeight(MIXER_IDX_LEFT, 0);          // input 1: INx-L codec channel
    side_l_r_mix->setWeight(MIXER_IDX_RIGHT, 0);         // input 2: INx-R codec channel


    // audio_filt declaration has already linked it to out_vol (mono)
    audio_set_filt(cur_filt);

    // audio_filt (mono) --> audio_filt_meas (mono) --> out_vol (mono) --> multi_output (mono)
    audio_set_volume(global_vol);
    multi_output = new MultiOutput();
    out_vol.setOutput(*multi_output);
    out_vol.begin(info_mono);
    audio_filt_meas.setAudioInfo(info_mono);
    audio_filt_meas.begin();

    // multi_output goes to mono_to_stereo (mono, via the clipping effect), and any optional outputs
    multi_output->add(effects);    
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
#ifdef AUDIO_EN_OUT_IP
    client.setNoDelay(true);   // doesn't wait to accumulate packets
    client.setTimeout(1);       // 2 second timeout

    // attempt connection repeatedly
    bool connected = false;
    IPAddress search_address;
    for(uint16_t i = 0; i < 3; i++) {
        // try to find the client. assumes MDNS has started
        // TODO: move this sort of logicto wifi_conn.cpp, don't hard-code
        search_address = MDNS.queryHost("key", 250);    //500ms timeout
        if(search_address != IPAddress(0, 0, 0, 0)) {
            Serial.print("Found address: ");
            Serial.println(search_address.toString());
            client_address = search_address;
        }
        else { 
            Serial.print("Audio receiver not yet found on MDNS, will also try: ");
            Serial.println(client_address.toString());
        }
            
        if(client.connect(client_address, ip_port)) {
            connected = true;
            Serial.print("Successfully connected to server at: ");
            Serial.println(client_address.toString());
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(250));        
    }

    // only proceed if client connected
    if(connected) {
        Serial.println("IP Key Connection success");
        multi_output->add(client);
    }
    else {
        Serial.println("Unable to connect to IP key");
        client.stop();
    }
#endif

    // Distortion (clipping) operates *after* volume control is applied, making it the same threshold regardless of volume setting
    volume_limiter = new Distortion(max_safe_vol, max_safe_vol);
    effects.addEffect(*volume_limiter);
    effects.begin(info_mono);

    // take a mono audio stream and make it stereo
    // declaration links it to i2s stream (stereo output)
    mono_to_stereo.begin(1, 2);

    // use full analog gain in the codec
    audio_en_pga(true);
    audio_en_sidetone(false);
    audio_en_rx_audio(true);
    audio_set_sidetone_volume(AUDIO_SIDE_DEFAULT);
    audio_set_volume(AUDIO_VOL_DEFAULT);

    uint32_t start_tick, stop_tick, c1_processed, c2_processed;
    while(true) {
        start_tick = xTaskGetTickCount();
        // TODO (for IP): the .copy() calls will block if the client disconnects
        c1_processed = copier_1.copy();
        c2_processed = copier_2.copy();
        stop_tick = xTaskGetTickCount();

        /*
        Serial.print(stop_tick - start_tick);
        Serial.print("\t");
        Serial.print(c1_processed);
        Serial.print("\t");
        Serial.println(c2_processed);
        */

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// properly disables and restarts the DSP task 
void audio_dsp_task_restart() {
    // disable audio to avoid any noise during restart
    audio_en_rx_audio(false);

    // Are any of these even needed? 
    // copier_1.end();
    // copier_2.end();
    audio_board.end();
    i2s_stream.end();
    sine_wave.end();
    // side_l_r_mix.end();      // library has problematic use of malloc/free? reason for malloc'ing this element.
    // audio_filt_meas.end();
    // input_split.end();
    // multi_output.end();
    // out_vol.end();
    effects.clear();
    // mono_to_stereo.end();
    delete side_l_r_mix;
    delete input_split;
    delete volume_limiter;

    Serial.println("\nDeleting existing DSP task...");

    vTaskDelete(xDSPTaskHandle);

    xTaskCreatePinnedToCore(
        audio_dsp_task,
        "Audio Stream Updater Task",
        65536,
        NULL,
        TASK_PRIORITY_DSP, // priority
        &xDSPTaskHandle,
        TASK_CORE_DSP // core
    );
}

void audio_logic_task(void *pvParameter) {
    uint32_t notifiedValue;

    while(true) {
        // update this variable so other modules can more readily consume it
        last_volume_dB = audio_get_rx_db(20, 1);

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
            if(notifiedValue & NOTIFY_MODE_HF_RXTX_CW && cur_audio_mode != AUDIO_HF_RXTX_CW) {
                cur_audio_mode = AUDIO_HF_RXTX_CW;
                cur_filt = AUDIO_FILT_DEFAULT;
                audio_dsp_task_restart();
            }
            if(notifiedValue & NOTIFY_MODE_VHF_RX && cur_audio_mode != AUDIO_VHF_RX) {
                // HF --> VHF: need to switch to other audio inputs, requires codec input reconfiguration
                if(cur_audio_mode == AUDIO_HF_RXTX_CW) {
                    cur_audio_mode = AUDIO_VHF_RX;
                    cur_filt = AUDIO_FILT_SSB;
                    audio_dsp_task_restart();
                }
                // VHF TX --> VHF RX: only need to adjust DACs, don't need to reconfigured pathways
                else if(cur_audio_mode == AUDIO_VHF_TX) {
                    cur_audio_mode = AUDIO_VHF_RX;
                    audio_set_dacs(cur_audio_mode);
                    audio_en_rx_audio(true);
                    // in TX->RX transition, restore volume
                    audio_set_volume(global_vol);
                    // restore volume limiter
                    audio_en_vol_clipping(true);
                }
            }
            if(notifiedValue & NOTIFY_MODE_VHF_TX && cur_audio_mode != AUDIO_VHF_TX) {
                // HF --> VHF: need to switch to other audio inputs, requires codec input reconfiguration
                if(cur_audio_mode == AUDIO_HF_RXTX_CW) {
                    cur_audio_mode = AUDIO_VHF_TX;
                    cur_filt = AUDIO_FILT_SSB;
                    audio_dsp_task_restart();
                }
                // VHF RX --> VHF TX: only need to adjust DACs, don't need to reconfigured pathways
                else if(cur_audio_mode == AUDIO_VHF_RX) {
                    cur_audio_mode = AUDIO_VHF_TX;
                    audio_set_dacs(cur_audio_mode);
                    // turn on audio. TODO: clean up naming, we need "rx" audio on because it controls the side_l_r mixer
                    audio_en_rx_audio(true);
                    // TODO: some sort of mic gain control set in json file
                    out_vol.setVolume(1.0);     // equivalent to calling audio_set_volume(), but doesn't override global_vol
                    // allow full volume output
                    audio_en_vol_clipping(false);
                }
            }
            if(notifiedValue & NOTIFY_DBG_MAX_VOL) {
                Serial.println("Testing max volume");

                audio_set_sidetone_volume(1.0);
                audio_set_volume(0);
                audio_en_sidetone(true);
                audio_en_rx_audio(false);

                // ramp up volume slowly
                for(float i=0; i < 1; i += 0.01) {
                    Serial.print("Volume: ");
                    Serial.println(i);
                    audio_set_volume(i);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }

                audio_set_sidetone_volume(AUDIO_SIDE_DEFAULT);
                audio_set_volume(AUDIO_VOL_DEFAULT);
                audio_en_sidetone(false);
                audio_en_rx_audio(true);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// responsible for adjusting PGA gain based on signal strength
// TODO: turn this into an AGC loop, not just binary PGA on/off
void audio_gain_task(void *pvParameter) {
    uint32_t counter = 0;
    float cur_s_meter = 0;
    float last_s_meter = 0;
    while(true) {
        // check current S-meter value
        cur_s_meter = radio_get_s_meter();

        // turn off PGA if s-meter is high AND pga was already turned on
        if(cur_s_meter > 7 && pga_en) {
            audio_en_pga(false);
            vTaskDelay(pdTICKS_TO_MS(100));
        }
        
        // increment counter if s-meter is low AND pga is turned off currently
        if(cur_s_meter < 6 && !pga_en)
            counter++;

        // turn on PGA if counter expires
        if(counter > 10) {
            audio_en_pga(true);
            counter = 0;
            vTaskDelay(10);
        }

        last_s_meter = cur_s_meter;
    }
}

void audio_set_mode(audio_mode_t mode) {
    if(mode == AUDIO_HF_RXTX_CW)
        xTaskNotify(xAudioTaskHandle, NOTIFY_MODE_HF_RXTX_CW, eSetBits);
    else if(mode == AUDIO_VHF_RX)
        xTaskNotify(xAudioTaskHandle, NOTIFY_MODE_VHF_RX, eSetBits);
    else if(mode == AUDIO_VHF_TX)
        xTaskNotify(xAudioTaskHandle, NOTIFY_MODE_VHF_TX, eSetBits);
}

// changes the audio FIR in use
// note that calling setFilter() will free memory associated with the previuosly registered filter, so this isn't causing memory leaks
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
    Serial.println(swap);

    if(swap) {
        hilbert_n45deg.setFilter(0, new FIR<float>(coeff_hilbert_n45deg));
        // input_l_vol.setVolume(1.0);
        // input_r_vol.setVolume(0);
    }
    else  {
        hilbert_n45deg.setFilter(0, new FIR<float>(coeff_hilbert_n45deg_negated));
        // input_l_vol.setVolume(0);
        // input_r_vol.setVolume(1.0);
    }

    /*
    if(swap) {
        hilbert_n45deg.setFilter(0, new FIR<float>(coeff_hilbert_p45deg));
        hilbert_p45deg.setFilter(0, new FIR<float>(coeff_hilbert_n45deg));
    }
    else {
        hilbert_n45deg.setFilter(0, new FIR<float>(coeff_hilbert_n45deg));
        hilbert_p45deg.setFilter(0, new FIR<float>(coeff_hilbert_p45deg));
    }
    */
#else
    if(swap)
        audio_en_pga(true);
    else
        audio_en_pga(false);
    
    // time for PGA flag to get addressed
    vTaskDelay(pdMS_TO_TICKS(100));

    Serial.print("\nen: ");
    Serial.print(swap);

    Serial.print("\tRX sample: ");
    Serial.println(audio_get_rx_db(50, 1));
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
    if(side_l_r_mix == nullptr)
        return;

    if(tone) {
        int16_t tmp = (int16_t) (sidetone_vol * INT16T_MAX);
        sine_wave.setAmplitude(tmp);
    }
    else
        sine_wave.setAmplitude(0);

    sidetone_en = tone;
}

// ONLY affects muting of rx audio, does not affect sidetone
// TODO: remove "rx" from the name, kind of confusing, we also set en_rx_audio(true) during VHF TX
void audio_en_rx_audio(bool en) {
    if(side_l_r_mix == nullptr)
        return;

    if(en) {
        // unmute input mixer channels. LEFT channel depends on whether IQ audio is in use
        if(cur_audio_mode == AUDIO_VHF_TX) {
            side_l_r_mix->setWeight(MIXER_IDX_RIGHT, 0.0);
        }
        else
            side_l_r_mix->setWeight(MIXER_IDX_RIGHT, 1.0);
        
#ifdef AUDIO_PATH_IQ
        side_l_r_mix.setWeight(MIXER_IDX_LEFT, 1.0);
#else
        // use LEFT input for VHF audio, only in TX mode
        if(cur_audio_mode == AUDIO_VHF_TX) {
            side_l_r_mix->setWeight(MIXER_IDX_LEFT, 1.0);
        }
        else
            side_l_r_mix->setWeight(MIXER_IDX_LEFT, 0.0);
#endif

    }
    else {
        // mute all input mixer channels, regardless of IQ audio
        side_l_r_mix->setWeight(MIXER_IDX_LEFT, 0.0);
        side_l_r_mix->setWeight(MIXER_IDX_RIGHT, 0.0);
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

        // make a call to sidetone_en() to update the sine wave's amplitude
        audio_en_sidetone(sidetone_en);

        return true;
    }
    return false;
}

float audio_get_sidetone_volume() {
    return sidetone_vol;
}

bool audio_set_sidetone_freq(float freq) {
    // enforce 0-3kHz range 
    if(freq > 0 && freq < 3000) {
        sidetone_freq = freq;
        sine_wave.setFrequency(sidetone_freq);
        return true;
    }
    return false;
}

bool audio_get_pga() {
    return pga_en;
}

float audio_get_rx_db(uint16_t num_to_avg, uint16_t delay_ms) {
    if(sidetone_en)
        return -1001;
    else {
        float rx_dB = 0;
        for(uint16_t i = 0; i < num_to_avg; i++) {
            rx_dB += audio_filt_meas.volumeDB();

            // only delay between samples if we are averaging
            if(num_to_avg > 1)
                vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
        rx_dB /= num_to_avg;

        // correct for PGA
        if(pga_en)
            rx_dB -= PGA_GAIN;

        return rx_dB;
    }
}

float audio_get_rx_vol() {
    return audio_filt_meas.volumePercent();
}

float audio_get_loudness() {
    return last_volume_dB;
}

float audio_get_sidetone_freq() {
    return sidetone_freq;
}

float audio_get_volume() {
    return global_vol;
}

void audio_set_dacs(audio_mode_t mode) {
    AudioDriver *driver = audio_board.getDriver();
    if(mode == AUDIO_HF_RXTX_CW || mode == AUDIO_VHF_RX) {
        driver->setMute(false, 0);
        driver->setMute(true, 1);
    }
    else if(mode == AUDIO_VHF_TX) {
        // driver->setMute(false, 0);       // set to false --> TX audio monitor feature (hear what you're saying). No volume control, this would adjust outgoing audio too.
        driver->setMute(true, 0);
        driver->setMute(false, 1);
    }
}

void audio_en_vol_clipping(bool enable) {
    if(enable)
        volume_limiter->setClipThreashold(max_safe_vol);
    else
        volume_limiter->setClipThreashold(INT16T_MAX);
}

void audio_debug(debug_action_t command_num) {
    switch(command_num) {
        case DEBUG_CMD_MAX_VOL:
            xTaskNotify(xAudioTaskHandle, NOTIFY_DBG_MAX_VOL, eSetBits);
            break;
    }
}
