
#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>

extern const uint8_t BITMAP_POWERSAVE[8];
extern const uint8_t BITMAP_TX[8];
extern const uint8_t BITMAP_RX[8];

extern const uint8_t BITMAP_BATTERY_LEVEL[17];

extern const uint8_t BITMAP_USB_C[8];

#ifdef ENABLE_KEYLOCK
	extern const uint8_t BITMAP_KEYLOCK[6];
#endif

extern const uint8_t BITMAP_F_KEY[6];

#ifdef ENABLE_VOX
	extern const uint8_t BITMAP_VOX[18];
#endif

#if 0
	extern const uint8_t BITMAP_WX[12];
#else
	extern const uint8_t BITMAP_XB[11];
#endif

extern const uint8_t BITMAP_TDR_RUNNING[11];
extern const uint8_t BITMAP_TDR_HOLDING[11];

#ifdef ENABLE_VOICE
	extern const uint8_t BITMAP_VOICE_PROMPT[7];
#endif

extern const uint8_t BITMAP_MONITOR[6];

#ifdef ENABLE_FMRADIO
	extern const uint8_t BITMAP_FM[11];
#endif

#ifdef ENABLE_NOAA
	extern const uint8_t BITMAP_NOAA[10];
#endif

extern const uint8_t BITMAP_ANTENNA[5];
extern const uint8_t BITMAP_ANTENNA_LEVEL1[3];
extern const uint8_t BITMAP_ANTENNA_LEVEL2[3];
extern const uint8_t BITMAP_ANTENNA_LEVEL3[3];
extern const uint8_t BITMAP_ANTENNA_LEVEL4[3];
extern const uint8_t BITMAP_ANTENNA_LEVEL5[3];
extern const uint8_t BITMAP_ANTENNA_LEVEL6[3];

extern const uint8_t BITMAP_MARKER[8];

extern const uint8_t BITMAP_VFO_DEFAULT[8];
extern const uint8_t BITMAP_VFO_NOT_DEFAULT[8];

extern const uint8_t BITMAP_SCANLIST1[6];
extern const uint8_t BITMAP_SCANLIST2[6];

#endif

