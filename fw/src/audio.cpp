#include "audio.h"
#include "audio_dsp.h"
#include "globals.h"
#include "file_system.h"
#include <Arduino.h>

#define NOTIFY_PGA_ON           (1 << 0)
#define NOTIFY_PGA_OFF          (1 << 1)
#define NOTIFY_MODE_HF_RX_CW    (1 << 2)
#define NOTIFY_MODE_HF_TX_CW    (1 << 3)
#define NOTIFY_MODE_HF_TX_SSB   (1 << 4)
#define NOTIFY_MODE_VHF_RX      (1 << 5)
#define NOTIFY_MODE_VHF_TX      (1 << 6)
#define NOTIFY_DBG_MAX_VOL      (1 << 7)
#define NOTIFY_DBG_MIN_VOL      (1 << 8)
#define NOTIFY_DBG_IMD_TEST     (1 << 9)

TaskHandle_t xAudioTaskHandle;
static float last_volume_dB = 0;

void audio_logic_task(void *pvParameter);

void audio_init() {
    // TODO: write a load_setting_uint function
    if(fs_setting_exists(PREFERENCE_FILE, "max_audio_output"))
        max_safe_vol = (uint32_t) (fs_load_setting_float(PREFERENCE_FILE, "max_audio_output", 0.0, 1.0) * INT16T_MAX);

    if(fs_setting_exists(PREFERENCE_FILE, "sidetone_level"))
        sidetone_vol = fs_load_setting_float(PREFERENCE_FILE, "sidetone_level", 0.0, 1.0);

    // TODO: delete me
    // if(fs_setting_exists(PREFERENCE_FILE, "tx_power"))
    //     tx_power = fs_load_setting(PREFERENCE_FILE, "tx_power").toFloat();

    if(fs_setting_exists(HARDWARE_FILE, "i_rx_gain"))
        i_rx_gain = fs_load_setting_float(HARDWARE_FILE, "i_rx_gain", 0.0, 1.0);
    if(fs_setting_exists(HARDWARE_FILE, "i_tx_gain"))
        i_tx_gain = fs_load_setting_float(HARDWARE_FILE, "i_tx_gain", 0.0, 1.0);
    if(fs_setting_exists(HARDWARE_FILE, "q_rx_gain"))
        q_rx_gain = fs_load_setting_float(HARDWARE_FILE, "q_rx_gain", 0.0, 1.0);
    if(fs_setting_exists(HARDWARE_FILE, "q_tx_gain"))
        q_tx_gain = fs_load_setting_float(HARDWARE_FILE, "q_tx_gain", 0.0, 1.0);


    // Initialize DSP subsystem
    audio_dsp_init();

    // Create audio logic task
    xTaskCreatePinnedToCore(
        audio_logic_task,
        "Audio Logic Task",
        16384,
        NULL,
        TASK_PRIORITY_AUDIO,
        &xAudioTaskHandle,
        TASK_CORE_AUDIO
    );

    // TODO: move this elsewhere
    audio_en_pga(true);
}

void audio_logic_task(void *pvParameter) {
    uint32_t notifiedValue;
    audio_mode_t prev_audio_mode;

    while(true) {
        last_volume_dB = audio_dsp_get_rx_level(10, 1);
        prev_audio_mode = cur_audio_mode;

        if(xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifiedValue, 0) == pdTRUE) {
            if(notifiedValue & NOTIFY_PGA_ON) {
                pga_en = true;
                audio_dsp_set_pga_gain(100);
            }
            if(notifiedValue & NOTIFY_PGA_OFF) {
                pga_en = false;
                audio_dsp_set_pga_gain(0);
            }
            // Handle mode changes
            if(notifiedValue & NOTIFY_MODE_HF_RX_CW) {
                iq_rx_balance.setVolume(q_rx_gain, 0);
                iq_rx_balance.setVolume(i_rx_gain, 1);

                // don't 
                tx_vol.setVolume(0.0);

                hp_vol.setVolume(global_vol);

                cur_audio_mode = AUDIO_HF_RX;

                // special case (a bit of a hack): don't update the codec if previous mode was HF_TX_CW. Speeds up entry into CW RX, avoids memory issues
                if(prev_audio_mode != AUDIO_HF_TX_CW)
                    audio_dsp_request_codec_update();

                // exits WITHOUT changing sidetone volume. That is handled by key on/off function. This just changes "modes"
            }
            if(notifiedValue & NOTIFY_MODE_HF_TX_CW) {
                iq_rx_balance.setVolume(0, 0);
                iq_rx_balance.setVolume(0, 1);

                /*
                todo: delete this if it just works to call audio_set_tx_power
                Serial.print("Setting tx_power: ");
                Serial.println(tx_power);
                tx_vol.setVolume(tx_power * q_tx_gain, 0);
                tx_vol.setVolume(tx_power * i_tx_gain, 1);
                */
                audio_set_tx_power(radio_get_power());

                // TODO: consider deleting this from the audio mode change. Needs low latency so also exists in the sidetone enabling.
                hp_vol.setVolume(sidetone_vol * global_vol);

                cur_audio_mode = AUDIO_HF_TX_CW;

                // special case (a bit of a hack): don't update the codec if previous mode was HF_RX. Speeds up entry into CW TX, avoids memory issues
                if(prev_audio_mode != AUDIO_HF_RX)
                    audio_dsp_request_codec_update();

                // exits WITHOUT changing sidetone volume. That is handled by key on/off function. This just changes "modes"
            }
            if(notifiedValue & NOTIFY_MODE_HF_TX_SSB) {
                iq_rx_balance.setVolume(q_rx_gain, 0);
                iq_rx_balance.setVolume(q_rx_gain, 1);

                /*
                TODO: just delete this if the call below works
                Serial.print("Setting tx_power: ");
                Serial.println(tx_power);
                tx_vol.setVolume(tx_power * q_tx_gain, 0);
                tx_vol.setVolume(tx_power * i_tx_gain, 1);
                */
                audio_set_tx_power(radio_get_power());

                // TODO: consider deleting this from the audio mode change. Needs low latency so also exists in the sidetone enabling.
                hp_vol.setVolume(sidetone_vol * global_vol);

                cur_audio_mode = AUDIO_HF_TX_SSB;

                audio_dsp_request_codec_update();

                // exits WITHOUT changing sidetone volume. That is handled by key on/off function. This just changes "modes"
            }
            if(notifiedValue & NOTIFY_MODE_VHF_RX) {
                // TODO
            }
            if(notifiedValue & NOTIFY_MODE_VHF_TX) {
                // TODO
            }
            if(notifiedValue & NOTIFY_DBG_MAX_VOL) {
                // TODO:
            }
            if(notifiedValue & NOTIFY_DBG_MIN_VOL) {
                sidetone_wave.setAmplitude(0);
                imd_test_wave.setAmplitude(0);
            }
            if(notifiedValue & NOTIFY_DBG_IMD_TEST) {
                // create a two-tone test waveform by outputting half amplitude at two different frequencies
                imd_test_wave.setAmplitude(INT16_MAX/2);
                sidetone_wave.setAmplitude(INT16_MAX/2);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void audio_set_mode(audio_mode_t new_audio_mode) {
    Serial.print("audio_set_mode(): ");
    Serial.println(audio_mode_to_string(new_audio_mode));

    // don't do anything if requested mode is the same
    // example: radio_set_rxtx_mode() may repeatedly call this when going from TX to QSK_COUNTDOWN during keying
    if(new_audio_mode == cur_audio_mode) {
        Serial.println("Skipping - no change to cur_audio_mode");
        return;
    }

    if(new_audio_mode == AUDIO_HF_RX)
        xTaskNotify(xAudioTaskHandle, NOTIFY_MODE_HF_RX_CW, eSetBits);
    else if(new_audio_mode == AUDIO_HF_TX_CW)
        xTaskNotify(xAudioTaskHandle, NOTIFY_MODE_HF_TX_CW, eSetBits);
    else if(new_audio_mode == AUDIO_HF_TX_SSB)
        xTaskNotify(xAudioTaskHandle, NOTIFY_MODE_HF_TX_SSB, eSetBits);
    else if(new_audio_mode == AUDIO_VHF_RX)
        xTaskNotify(xAudioTaskHandle, NOTIFY_MODE_VHF_RX, eSetBits);
    else if(new_audio_mode == AUDIO_VHF_TX)
        xTaskNotify(xAudioTaskHandle, NOTIFY_MODE_VHF_TX, eSetBits);
}

String audio_mode_to_string(audio_mode_t mode) {
    switch(mode) {
        case AUDIO_HF_RX:
            return "HF RX";
        case AUDIO_HF_TX_CW:
            return "HF TX CW";
        case AUDIO_HF_TX_SSB:
            return "HF TX SSB";
        case AUDIO_VHF_RX:
            return "VHF RX";
        case AUDIO_VHF_TX:
            return "VHF TX";
        default:
            return "UNKNOWN";
    }
}

bool audio_set_hp_volume(float vol) {
    if(vol >= 0.0 && vol <= 1.0) {
        global_vol = vol;
        
        // force an update to the audio controls by setting the mode again. TX vs RX volume handled there.
        Serial.println("Setting HP volume");
        audio_set_mode(cur_audio_mode);

        return true;
    }
    return false;
}

float audio_get_volume() {
    return global_vol;
}

bool audio_set_sidetone_volume(float vol) {
    if(vol >= 0.0 && vol <= 1.0) {
        sidetone_vol = vol;

        // force an update to the audio controls by setting the mode again. TX vs RX volume handled there.
        Serial.println("Setting sidetone volume");
        audio_set_mode(cur_audio_mode);

        return true;
    }
    return false;
}

float audio_get_sidetone_volume() {
    return sidetone_vol;
}

void audio_en_sidetone(bool en) {
    Serial.print("Audio sidetone: ");
    Serial.println(en);

    int16_t amp;
    if(en)
        amp = INT16T_MAX;
    else
        amp = 0;

    sidetone_wave.setAmplitude(amp);

    // this call also happens during mode change. Added here to avoid loud volume before mode fully changes.
    hp_vol.setVolume(sidetone_vol * global_vol);
}

bool audio_set_sidetone_freq(float freq) {
    if(freq > 0 && freq < 3000) {
        sidetone_freq = freq;
        sidetone_wave.setFrequency(freq);
        return true;
    }
    return false;
}

float audio_get_sidetone_freq() {
    return sidetone_freq;
}

bool audio_set_tx_power(float power) {
    if(power < 0.0 || power > 1.0)
        return false;
    
    // NOTE: does not set power yet, it'll update volume control on the next entry to TX mdoe
    Serial.print("Setting TX power: ");
    Serial.println(power);

    // update power variable and also make an adjustment to the volume control
    tx_vol.setVolume(power * q_tx_gain, 0);
    tx_vol.setVolume(power * i_tx_gain, 1);
    return true;
}

// update all the gains for the IQ balance, both TX and RX
bool audio_set_iq_gains(float i_tx, float q_tx, float i_rx, float q_rx) {
    if(i_tx < 0 || i_tx > 1 || q_tx < 0 || q_tx > 1 || 
       i_rx < 0 || i_rx > 1 || q_rx < 0 || q_rx > 1) {
        return false;
    }
    
    i_tx_gain = i_tx;
    q_tx_gain = q_tx; 
    i_rx_gain = i_rx;
    q_rx_gain = q_rx;

    // force an update to gains by setting the mode again
    audio_set_mode(cur_audio_mode);

    return true;
}
void audio_en_pga(bool enable) {
    xTaskNotify(xAudioTaskHandle, 
                enable ? NOTIFY_PGA_ON : NOTIFY_PGA_OFF, 
                eSetBits);
}

bool audio_get_pga() {
    return pga_en;
}

float audio_get_rx_db(uint16_t num_to_avg, uint16_t delay_ms) {
    return audio_dsp_get_rx_level(num_to_avg, delay_ms);
}

float audio_get_loudness() {
    return last_volume_dB;
}

void audio_debug(debug_action_t command_num) {
    switch(command_num) {
        case DEBUG_CMD_MAX_VOL:
            xTaskNotify(xAudioTaskHandle, NOTIFY_DBG_MAX_VOL, eSetBits);
            break;
        case DEBUG_CMD_IMD_TEST:
            xTaskNotify(xAudioTaskHandle, NOTIFY_DBG_IMD_TEST, eSetBits);
            break;
        case DEBUG_CMD_MIN_VOL:
            xTaskNotify(xAudioTaskHandle, NOTIFY_DBG_MIN_VOL, eSetBits);        
            break;
    }
}
