#pragma once

// move everything in this file to platformio.ini??

#define TASK_PRIORITY_HIGHEST   5
#define TASK_PRIORITY_NORMAL    3
#define TASK_PRIORITY_LOWEST    1

// audio settings used at startup
#define AUDIO_FILT_DEFAULT      AUDIO_FILT_SSB
#define AUDIO_VOL_DEFAULT       1.0
#define AUDIO_SIDE_DEFAULT      0.1
#define AUDIO_VOL_DURING_CAL    0.1
#define AUDIO_PGA_DEFAULT       true

// signals that the radio and audio modules should expect quadrature sampling detector (QSD) as BFO
#define RX_ARCHITECTURE_QSD