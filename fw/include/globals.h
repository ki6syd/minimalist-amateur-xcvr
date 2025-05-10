#pragma once

// move everything in this file to platformio.ini??

#define TASK_PRIORITY_HIGHEST   5
#define TASK_PRIORITY_NORMAL    3
#define TASK_PRIORITY_LOWEST    1

#define TASK_PRIORITY_DSP       TASK_PRIORITY_NORMAL
#define TASK_PRIORITY_AUDIO     TASK_PRIORITY_LOWEST
#define TASK_PRIORITY_RADIO     (TASK_PRIORITY_HIGHEST - 1)
#define TASK_PRIORITY_ADC       (TASK_PRIORITY_LOWEST + 1)
#define TASK_PRIORITY_BLINK     (TASK_PRIORITY_LOWEST + 1)
#define TASK_PRIORITY_INFO      TASK_PRIORITY_LOWEST
#define TASK_PRIORITY_FS        TASK_PRIORITY_LOWEST
#define TASK_PRIORITY_DIGI      (TASK_PRIORITY_LOWEST + 1)
#define TASK_PRIORITY_KEY_IO    TASK_PRIORITY_HIGHEST

// note: platformio + arduino puts wifi on core 0. server callbacks likely to happen on core 0
#define TASK_CORE_AUDIO         1
#define TASK_CORE_DSP           1
#define TASK_CORE_RADIO         1
#define TASK_CORE_ADC           0
#define TASK_CORE_BLINK         0
#define TASK_CORE_INFO          1
#define TASK_CORE_FS            1
#define TASK_CORE_DIGI          1
#define TASK_CORE_KEY_IO        1

#define F_AUDIO                 8000
#define F_SIDETONE_DEFAULT      499
#define BFO_CLOCK_DIV           1
#define AUDIO_FILT_DEFAULT      AUDIO_FILT_NARROW
#define SIDEBAND_DEFAULT        SIDEBAND_USB
#define MODULATION_DEFAULT      MOD_SSB
#define AUDIO_VOL_DEFAULT       0.75
#define AUDIO_SIDE_DEFAULT      0.8
#define TX_POWER_DEFAULT        0.8
#define AUDIO_VOL_DURING_CAL    0.01
#define AUDIO_PGA_DEFAULT       true
// #define AUDIO_EN_OUT_VBAN            // flakey
// #define AUDIO_EN_OUT_CSV             // too slow to print over serial
// #define AUDIO_EN_OUT_ESPNOW          // doesn't work
// #define AUDIO_EN_OUT_UDP             // works except for dropped packets causing choppy audio
#define AUDIO_EN_OUT_IP              // works fairly well. Commented out normally because the audio module has no graceful to handle losing connection to rcvr after gaining it

// comment out to prevent accidental TX
#define RADIO_ALLOW_TX

#define DIGI_QUEUE_LEN          10

#define VHF_DEFAULT_FREQ        146580000
#define HF_DEFAULT_FREQ         14060000

#define MORSE_SPEED_MIN         5
#define MORSE_SPEED_MAX         30

// #define WIFI_SCAN                // comment in #define to print out scan on connection

#define FILE_SYSTEM             LittleFS

#define API_IMPLEMENTED         "v1"

#define PREFERENCE_FILE         "/preferences.json"
#define HARDWARE_FILE           "/hardware.json"

typedef enum {
    DEBUG_CMD_REBOOT=1,
    DEBUG_CMD_CAL_XTAL=2,
    DEBUG_CMD_MAX_VOL=3,
    DEBUG_CMD_SPOT=4,
    DEBUG_CMD_STOP_CLOCKS=5,
    DEBUG_CMD_SET_CLOCKS=6,
    DEBUG_CMD_STOP_BFO=7,
    DEBUG_CMD_STOP_VFO=8,
    DEBUG_CMD_IMD_TEST=9,
    DEBUG_CMD_MIN_VOL=10,
    DEBUG_CMD_CAL_LPF=12,
    DEBUG_CMD_PA_VDD=13
} debug_action_t;

typedef enum {
    AUDIO_HF_RX_CW,
    AUDIO_HF_TX_CW,
    AUDIO_HF_TX_SSB,
    AUDIO_VHF_RX,
    AUDIO_VHF_TX
} audio_mode_t;

typedef enum {
    MOD_CW,
    MOD_SSB,
    MOD_FM
} radio_modulation_t;

typedef enum {
    AUDIO_FILT_NARROW,
    AUDIO_FILT_WIDE
} audio_filt_t;

typedef enum {
    SIDEBAND_LSB,
    SIDEBAND_USB,
    SIDEBAND_NA
} sideband_t;