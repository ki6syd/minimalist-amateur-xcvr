#pragma once

// move everything in this file to platformio.ini??

#define TASK_PRIORITY_HIGHEST   5
#define TASK_PRIORITY_NORMAL    3
#define TASK_PRIORITY_LOWEST    1

#define TASK_PRIORITY_AUDIO     TASK_PRIORITY_LOWEST
#define TASK_PRIORITY_RADIO     TASK_PRIORITY_HIGHEST
#define TASK_PRIORITY_POWER     (TASK_PRIORITY_LOWEST + 1)
#define TASK_PRIORITY_BLINK     (TASK_PRIORITY_LOWEST + 1)
#define TASK_PRIORITY_INFO      TASK_PRIORITY_LOWEST
#define TASK_PRIORITY_FS        TASK_PRIORITY_LOWEST

// note: platformio + arduino puts wifi on core 0. server callbacks likely to happen on core 0
#define TASK_CORE_AUDIO         1
#define TASK_CORE_RADIO         1
#define TASK_CORE_POWER         0
#define TASK_CORE_BLINK         0
#define TASK_CORE_INFO          1
#define TASK_CORE_FS            1


// audio settings used at startup
#define AUDIO_FILT_DEFAULT      AUDIO_FILT_SSB
#define AUDIO_VOL_DEFAULT       1.0
#define AUDIO_SIDE_DEFAULT      0.1
#define AUDIO_VOL_DURING_CAL    0.1
#define AUDIO_PGA_DEFAULT       true

#define FILE_SYSTEM             LittleFS

#define API_IMPLEMENTED         "v1"

// signals that the radio and audio modules should expect quadrature sampling detector (QSD) as BFO
// #define RX_ARCHITECTURE_QSD