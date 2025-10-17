#pragma once

// move everything in this file to platformio.ini??

#define TASK_PRIORITY_HIGHEST   5
#define TASK_PRIORITY_NORMAL    3
#define TASK_PRIORITY_LOWEST    1

#define TASK_PRIORITY_AUDIO     TASK_PRIORITY_LOWEST
#define TASK_PRIORITY_DSP       TASK_PRIORITY_NORMAL
// #define TASK_PRIORITY_DSP       TASK_PRIORITY_HIGHEST
#define TASK_PRIORITY_RADIO     (TASK_PRIORITY_HIGHEST - 1)
#define TASK_PRIORITY_ADC       (TASK_PRIORITY_LOWEST + 1)
#define TASK_PRIORITY_BLINK     (TASK_PRIORITY_LOWEST + 1)
#define TASK_PRIORITY_INFO      TASK_PRIORITY_LOWEST
#define TASK_PRIORITY_FS        TASK_PRIORITY_LOWEST
#define TASK_PRIORITY_DIGI      (TASK_PRIORITY_LOWEST + 1)
#define TASK_PRIORITY_KEY_IO    TASK_PRIORITY_HIGHEST
// #define TASK_PRIORITY_KEY_IO    (TASK_PRIORITY_HIGHEST-1)

// note: platformio + arduino puts wifi on core 0. server callbacks likely to happen on core 0
#define TASK_CORE_AUDIO         1
// #define TASK_CORE_AUDIO         0
#define TASK_CORE_DSP           1
// #define TASK_CORE_RADIO         1
#define TASK_CORE_RADIO         0
#define TASK_CORE_ADC           0
#define TASK_CORE_BLINK         0
#define TASK_CORE_INFO          0
#define TASK_CORE_FS            0
#define TASK_CORE_DIGI          0
#define TASK_CORE_KEY_IO        1
// #define TASK_CORE_KEY_IO        0


// #define WIFI_SCAN                // comment in #define to print out scan on connection

#define FILE_SYSTEM             LittleFS

#define API_IMPLEMENTED         "v1"

#define PREFERENCE_FILE         "/preferences.json"
#define HARDWARE_FILE           "/hardware.json"

typedef enum {
    DEBUG_CMD_REBOOT=1,
    DEBUG_CMD_STOP_CLOCKS=5,
    DEBUG_CMD_SET_CLOCKS=6,
    DEBUG_CMD_STOP_BFO=7,
    DEBUG_CMD_STOP_VFO=8
} debug_action_t;
