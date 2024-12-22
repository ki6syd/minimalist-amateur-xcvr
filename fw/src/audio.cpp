#include "audio.h"
#include "audio_dsp.h"
#include "globals.h"
#include "file_system.h"
#include <Arduino.h>

#define NOTIFY_PGA_ON           (1 << 0)
#define NOTIFY_PGA_OFF          (1 << 1)
#define NOTIFY_MODE_HF_RXTX_CW  (1 << 2)
#define NOTIFY_MODE_VHF_RX      (1 << 3)
#define NOTIFY_MODE_VHF_TX      (1 << 4)
#define NOTIFY_DBG_MAX_VOL      (1 << 5)


TaskHandle_t xAudioTaskHandle;
static float last_volume_dB = 0;

void audio_logic_task(void *pvParameter);

void audio_init() {
    max_safe_vol = (uint32_t)(fs_load_setting(PREFERENCE_FILE, "max_audio_output").toFloat() * 32768);

    if(fs_setting_exists(PREFERENCE_FILE, "sidetone_level"))
        sidetone_vol = fs_load_setting(PREFERENCE_FILE, "sidetone_level").toFloat();

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
}

void audio_logic_task(void *pvParameter) {
    uint32_t notifiedValue;

    while(true) {
        last_volume_dB = audio_dsp_get_rx_level(20, 1);

        if(xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifiedValue, 0) == pdTRUE) {
            if(notifiedValue & NOTIFY_PGA_ON) {
                pga_en = true;
                audio_dsp_set_input_volume(100);
            }
            if(notifiedValue & NOTIFY_PGA_OFF) {
                pga_en = false;
                audio_dsp_set_input_volume(0);
            }
            // Handle mode changes
            if(notifiedValue & NOTIFY_MODE_HF_RXTX_CW) {
                audio_set_mode(AUDIO_HF_RXTX_CW);
            }
            if(notifiedValue & NOTIFY_MODE_VHF_RX) {
                audio_set_mode(AUDIO_VHF_RX);
            }
            if(notifiedValue & NOTIFY_MODE_VHF_TX) {
                audio_set_mode(AUDIO_VHF_TX);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void audio_set_mode(audio_mode_t mode) {
    cur_audio_mode = mode;
    audio_dsp_task_restart();
    audio_dsp_set_dacs(mode);
}

bool audio_set_volume(float vol) {
    if(vol >= 0.0 && vol <= 1.0) {
        audio_dsp_set_volume(vol);
        return true;
    }
    return false;
}

float audio_get_volume() {
    return global_vol;
}

void audio_en_sidetone(bool enable) {
    audio_dsp_set_sidetone(enable, sidetone_freq, sidetone_vol);
}

bool audio_set_sidetone_volume(float vol) {
    if(vol >= 0.0 && vol <= 1.0) {
        sidetone_vol = vol;
        if(sidetone_en) {
            audio_dsp_set_sidetone(true, sidetone_freq, vol);
        }
        return true;
    }
    return false;
}

float audio_get_sidetone_volume() {
    return sidetone_vol;
}

bool audio_set_sidetone_freq(float freq) {
    if(freq > 0 && freq < 3000) {
        sidetone_freq = freq;
        if(sidetone_en) {
            audio_dsp_set_sidetone(true, freq, sidetone_vol);
        }
        return true;
    }
    return false;
}

float audio_get_sidetone_freq() {
    return sidetone_freq;
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
    }
}

audio_filt_t audio_get_filt() {
    return cur_filt;
}

bool audio_set_filt(audio_filt_t filt) {
    audio_dsp_set_filter(filt);
    return true;
}

