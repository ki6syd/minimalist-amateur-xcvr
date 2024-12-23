#include "audio_dsp.h"
#include "fir_coeffs_bpf_8kHz.h"
#include "fir_coeffs_hilbert_8kHz.h"
#include <Arduino.h>
#include <Wire.h>

TaskHandle_t xDSPTaskHandle;

AudioInfo info_stereo(F_AUDIO, 2, 16);
AudioInfo info_mono(F_AUDIO, 1, 16);

// codec interfaces
TwoWire codecI2C = TwoWire(1);
DriverPins my_pins;
static AudioBoard audio_board(AudioDriverES8388, my_pins);
I2SCodecStream es8388_stream(audio_board);
// I2SStream pcm1502_stream;

// sine wave source
SineWaveGenerator<int16_t> sidetone_wave(32000);
GeneratedSoundStream<int16_t> sidetone_sound(sidetone_wave);

// filters
FilteredStream<int16_t, float> audio_filt;
FilteredStream<int16_t, float> hilbert;

// volume functions
VolumeStream iq_vol(es8388_stream);             // connect iq_vol to the output of es8388_stream here. No setInput() function for iq_vol.
VolumeMeter vol_meas;

// audio plumbing
OutputMixer<int16_t> *es8388_sidetone_mixer;
MultiOutput rx_tx_audio_mux;

// stream copiers
StreamCopy copier_iq_in(BUFFER_CHUNK);
StreamCopy copier_sidetone_in(BUFFER_CHUNK);


/*
ChannelSplitOutput *input_split;
VolumeStream hp_vol;
VolumeStream input_l_vol, input_r_vol;
MultiOutput *multi_output;
OutputMixer<int16_t> *side_l_r_mix;
ChannelFormatConverterStreamT<int16_t> mono_to_stereo(es8388_stream);
AudioEffectStream effects(mono_to_stereo);
Distortion *vol_limiter;
StreamCopy copier_1(BUFFER_CHUNK);
StreamCopy copier_2(BUFFER_CHUNK * 2);
*/

bool pga_en = false;
float sidetone_vol = AUDIO_SIDE_DEFAULT;
float sidetone_freq = F_SIDETONE_DEFAULT;
bool sidetone_en = false;
float global_vol = AUDIO_VOL_DEFAULT;
audio_filt_t cur_filt = AUDIO_FILT_DEFAULT;
audio_mode_t cur_audio_mode = AUDIO_HF_RXTX_CW;
uint32_t max_safe_vol = 32768;

void audio_dsp_init() {

    my_pins.addI2C(PinFunction::CODEC, CODEC_SCL, CODEC_SDA, CODEC_ADDR, CODEC_I2C_SPEED, codecI2C);
    my_pins.addI2S(PinFunction::CODEC, CODEC_MCLK, CODEC_BCLK, CODEC_WS, CODEC_DO, CODEC_DI);
    my_pins.begin();
    audio_board.begin();

    xTaskCreatePinnedToCore(
        audio_dsp_task,
        "Audio DSP Task",
        65536,
        NULL,
        TASK_PRIORITY_DSP,
        &xDSPTaskHandle,
        TASK_CORE_DSP
    );
}

void audio_dsp_task(void *pvParameter) {

    // initialize ES8388 codec
    auto i2s_config = es8388_stream.defaultConfig(RXTX_MODE);
    i2s_config.copyFrom(info_stereo);
    i2s_config.buffer_size = BUFFER_CHUNK*4;
    i2s_config.buffer_count = 4;
    i2s_config.port_no = 0;
    i2s_config.input_device = (cur_audio_mode == AUDIO_HF_RXTX_CW) ? ADC_INPUT_LINE1 : ADC_INPUT_LINE2;
    es8388_stream.begin(i2s_config);
    audio_dsp_set_dacs(cur_audio_mode);

    // initialize PCM1502 codec
    /*
    auto cfg_tx = pcm1502_stream.defaultConfig(TX_MODE);
    cfg_tx.copyFrom(info_stereo);
    cfg_tx.port_no = 1;
    cfg_tx.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;  // comment out this line (and .channels=1) for stereo. 
    cfg_tx.channels = 1;
    cfg_tx.buffer_count = 4;
    cfg_tx.buffer_size = BUFFER_CHUNK;
    cfg_tx.pin_bck = HP_DAC_BCLK;
    cfg_tx.pin_data = HP_DAC_DO;
    cfg_tx.pin_ws = HP_DAC_LRCLK;
    pcm1502_stream.begin(cfg_tx);
    */

    es8388_sidetone_mixer = new OutputMixer<int16_t>(hilbert, 2);

    copier_iq_in.begin(*es8388_sidetone_mixer, iq_vol);
    copier_sidetone_in.begin(*es8388_sidetone_mixer, sidetone_sound);


    // hilbert_n45deg.begin(info_mono);
    // hilbert_n45deg.setFilter(0, new FIR<float>(coeff_hilbert_n45deg));
    // hilbert_p45deg.begin(info_mono);
    // hilbert_p45deg.setFilter(0, new FIR<float>(coeff_hilbert_p45deg));

    sidetone_wave.begin(info_stereo, sidetone_freq);
    Serial.println("sidetone_freq: ");
    Serial.println(sidetone_freq);

    iq_vol.begin(info_stereo);
    iq_vol.setVolume(1.0, 0);       // replace this with actual I/Q gain correction, for both RX and TX
    iq_vol.setVolume(1.0, 1);

    es8388_sidetone_mixer->begin();
    es8388_sidetone_mixer->setWeight(0, 1.0);
    es8388_sidetone_mixer->setWeight(1, 1.0);

    // hilbert.setStream(es8388_stream);
    hilbert.setOutput(rx_tx_audio_mux);     // output of hilbert transform used in both RX and TX audio pathways
    hilbert.begin(info_stereo);
    hilbert.setFilter(0, new FIR<float>(coeff_hilbert_n45deg));
    hilbert.setFilter(1, new FIR<float>(coeff_hilbert_p45deg));

    rx_tx_audio_mux.add(es8388_stream);


    size_t bytes_copied_in = 0;
    size_t bytes_copied_sidetone = 0;
    while(true) {
        bytes_copied_in = copier_iq_in.copy();
        bytes_copied_sidetone = copier_sidetone_in.copy();

        Serial.print("Bytes copied (IQ): ");
        Serial.println(bytes_copied_in);
        Serial.print("Bytes copied (Sidetone): ");
        Serial.println(bytes_copied_sidetone);

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
void audio_dsp_task_restart() {
    vTaskDelete(xDSPTaskHandle);

    xTaskCreatePinnedToCore(
        audio_dsp_task,
        "Audio DSP Task",
        65536,
        NULL,
        TASK_PRIORITY_DSP,
        &xDSPTaskHandle,
        TASK_CORE_DSP
    );
}

void audio_dsp_set_filter(audio_filt_t filt) {
    switch(filt) {
        case AUDIO_FILT_CW:
            audio_filt.setFilter(0, new FIR<float>(coeff_bpf_300_700));
            break;
        case AUDIO_FILT_SSB:
            audio_filt.setFilter(0, new FIR<float>(coeff_bpf_400_2000));
            break;
    }
    cur_filt = filt;
}

void audio_dsp_set_volume(float vol) {
    if(vol >= 0.0 && vol <= 1.0) {
        global_vol = vol;
        // hp_vol.setVolume(vol);
    }
}

void audio_dsp_set_sidetone(bool enable, float freq, float vol) {
    // commenting out for testing. Other functions in the audio modules call this, delete them?
    // sidetone was getting turned off
    /*
    sidetone_en = enable;
    sidetone_freq = freq;
    sidetone_vol = vol;

    if (vol < 0.0) {
        vol = 0.0;
    } else if (vol > 1.0) {
        vol = 1.0;
    }

    if(enable) {
        int16_t amp = (int16_t)(vol * INT16T_MAX);
        sidetone_wave.setAmplitude(amp);
        sidetone_wave.setFrequency(freq);
    } else {
        sidetone_wave.setAmplitude(0);
    }
    */
}

void audio_dsp_set_dacs(audio_mode_t mode) {
    if(mode == AUDIO_HF_RXTX_CW || mode == AUDIO_VHF_RX) {
        audio_dsp_set_mute(false, 0);
        audio_dsp_set_mute(true, 1);
    }
    else if(mode == AUDIO_VHF_TX) {
        audio_dsp_set_mute(true, 0);
        audio_dsp_set_mute(false, 1);
    }
}
float audio_dsp_get_rx_level(uint16_t num_avg, uint16_t delay_ms) {
    if(sidetone_en) return -1001;
    
    float rx_dB = 0;
    for(uint16_t i = 0; i < num_avg; i++) {
        rx_dB += vol_meas.volumeDB();
        if(num_avg > 1) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }
    rx_dB /= num_avg;
    
    if(pga_en) rx_dB -= PGA_GAIN;
    
    return rx_dB;
}

void audio_en_rx_audio(bool en) {
    if(en) {

    }
    else {

    }
}

void audio_en_vol_clipping(bool enable) {
    /*
    if(enable)
        vol_limiter->setClipThreashold(max_safe_vol);
    else
        vol_limiter->setClipThreashold(INT16T_MAX);
    */
}

void audio_dsp_set_input_volume(uint8_t volume) {
    AudioDriver *driver = audio_board.getDriver();
    driver->setInputVolume(volume);
}

void audio_dsp_set_mute(bool mute, uint8_t channel) {
    AudioDriver *driver = audio_board.getDriver();
    driver->setMute(mute, channel);
}
