
#include "bitmaps.h"

// all these images are on their right sides
// turn your monitor 90-deg anti-clockwise to see the images

const uint8_t BITMAP_POWERSAVE[8] =
{
	#if 0
		// "S"
		__extension__ 0b00000000,
		__extension__ 0b00100110,
		__extension__ 0b01001001,
		__extension__ 0b01001001,
		__extension__ 0b01001001,
		__extension__ 0b01001001,
		__extension__ 0b01001001,
		__extension__ 0b00110010
	#else
		// "PS"
		__extension__ 0b00000000,
		__extension__ 0b01111111,
		__extension__ 0b00010001,
		__extension__ 0b00001110,
		__extension__ 0b00000000,
		__extension__ 0b01000110,
		__extension__ 0b01001001,
		__extension__ 0b00110001
	#endif
};

const uint8_t BITMAP_TX[8] =
{	// "TX"
	__extension__ 0b00000000,
	__extension__ 0b00000001,
	__extension__ 0b00000001,
	__extension__ 0b01111111,
	__extension__ 0b00000001,
	__extension__ 0b00000001,
	__extension__ 0b00000000,
	__extension__ 0b00000000
};

const uint8_t BITMAP_RX[8] =
{	// "RX"
	__extension__ 0b00000000,
	__extension__ 0b01111111,
	__extension__ 0b00001001,
	__extension__ 0b00011001,
	__extension__ 0b01100110,
	__extension__ 0b00000000,
	__extension__ 0b00000000,
	__extension__ 0b00000000
};

#ifndef ENABLE_REVERSE_BAT_SYMBOL
	// Quansheng way (+ pole to the left)
	const uint8_t BITMAP_BATTERY_LEVEL[17] =
	{
		__extension__ 0b00000000,
		__extension__ 0b00111110,
		__extension__ 0b01111111,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01111111
	};
#else
	// reversed (+ pole to the right)
	const uint8_t BITMAP_BATTERY_LEVEL[17] =
	{
		__extension__ 0b00000000,
		__extension__ 0b01111111,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01111111,
		__extension__ 0b00111110
	};
#endif

const uint8_t BITMAP_USB_C[8] =
{	// USB symbol
	__extension__ 0b00000000,
	__extension__ 0b00011100,
	__extension__ 0b00100111,
	__extension__ 0b01000100,
	__extension__ 0b01000100,
	__extension__ 0b01000100,
	__extension__ 0b00100111,
	__extension__ 0b00011100
};

#ifdef ENABLE_KEYLOCK
	const uint8_t BITMAP_KEYLOCK[6] =
	{	// teeny weeny padlock symbol
		__extension__ 0b00000000,
		__extension__ 0b01111100,
		__extension__ 0b01000110,
		__extension__ 0b01000101,
		__extension__ 0b01000110,
		__extension__ 0b01111100
	};
#endif

const uint8_t BITMAP_F_KEY[6] =
{	// F-Key symbol
	__extension__ 0b00000000,
	__extension__ 0b01111110,
	__extension__ 0b01111111,
	__extension__ 0b00011011,
	__extension__ 0b00011011,
	__extension__ 0b00011011
};

#ifdef ENABLE_VOX
	const uint8_t BITMAP_VOX[18] =
	{	// "VOX"
		__extension__ 0b00000000,
		__extension__ 0b00011111,
		__extension__ 0b00100000,
		__extension__ 0b01000000,
		__extension__ 0b00100000,
		__extension__ 0b00011111,
		__extension__ 0b00000000,
		__extension__ 0b00111110,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b01000001,
		__extension__ 0b00111110,
		__extension__ 0b00000000,
		__extension__ 0b01100011,
		__extension__ 0b00010100,
		__extension__ 0b00001000,
		__extension__ 0b00010100,
		__extension__ 0b01100011
	};
#endif

#if 0
	const uint8_t BITMAP_WX[12] =
	{	// "WX"
		__extension__ 0b00000000,
		__extension__ 0b01111111,
		__extension__ 0b00100000,
		__extension__ 0b00011000,
		__extension__ 0b00100000,
		__extension__ 0b01111111,
		__extension__ 0b00000000,
		__extension__ 0b01100011,
		__extension__ 0b00010100,
		__extension__ 0b00001000,
		__extension__ 0b00010100,
		__extension__ 0b01100011
	};
#else
	// 'XB' (cross-band/cross-VFO)
	const uint8_t BITMAP_XB[11] =
	{	// "XB"
		__extension__ 0b00000000,
		__extension__ 0b01100011,
		__extension__ 0b00010100,
		__extension__ 0b00001000,
		__extension__ 0b00010100,
		__extension__ 0b01100011,
		__extension__ 0b00000000,
		__extension__ 0b01111111,
		__extension__ 0b01001001,
		__extension__ 0b01001001,
		__extension__ 0b00110110
	};
#endif

const uint8_t BITMAP_TDR_RUNNING[11] =
{	// "DW"
	__extension__ 0b00000000,
	__extension__ 0b01111111,
	__extension__ 0b01000001,
	__extension__ 0b01000001,
	__extension__ 0b00111110,
	__extension__ 0b00000000,
	__extension__ 0b01111111,
	__extension__ 0b00100000,
	__extension__ 0b00011000,
	__extension__ 0b00100000,
	__extension__ 0b01111111
};

const uint8_t BITMAP_TDR_HOLDING[11] =
{	// "--" .. DW on hold
	__extension__ 0b00000000,
	__extension__ 0b00001000,
	__extension__ 0b00001000,
	__extension__ 0b00001000,
	__extension__ 0b00001000,
	__extension__ 0b00000000,
	__extension__ 0b00001000,
	__extension__ 0b00001000,
	__extension__ 0b00001000,
	__extension__ 0b00001000,
	__extension__ 0b00001000
};

#ifdef ENABLE_VOICE
	const uint8_t BITMAP_VOICE_PROMPT[7] =
	{
		__extension__ 0b00000000,
		__extension__ 0b00011000,
		__extension__ 0b00011000,
		__extension__ 0b00100100,
		__extension__ 0b01000010,
		__extension__ 0b11111111,
		__extension__ 0b00011000
	};
#endif

const uint8_t BITMAP_MONITOR[6] =
{	// "M"
	__extension__ 0b00000000,
	__extension__ 0b01111111,
	__extension__ 0b00000010,
	__extension__ 0b00001100,
	__extension__ 0b00000010,
	__extension__ 0b01111111
};

#ifdef ENABLE_FMRADIO
	const uint8_t BITMAP_FM[11] =
	{	// "FM"
		__extension__ 0b00000000,
		__extension__ 0b01111111,
		__extension__ 0b00001001,
		__extension__ 0b00001001,
		__extension__ 0b00000001,
		__extension__ 0b00000000,
		__extension__ 0b01111111,
		__extension__ 0b00000010,
		__extension__ 0b00001100,
		__extension__ 0b00000010,
		__extension__ 0b01111111
	};
#endif

#ifdef ENABLE_NOAA
	const uint8_t BITMAP_NOAA[10] =
	{	// "NS"
		__extension__ 0b00000000,
		__extension__ 0b01111111,
		__extension__ 0b00000100,
		__extension__ 0b00001000,
		__extension__ 0b00010000,
		__extension__ 0b01111111,
		__extension__ 0b00000000,
		__extension__ 0b01000110,
		__extension__ 0b01001001,
		__extension__ 0b00110001
	};
#endif

const uint8_t BITMAP_ANTENNA[5] =
{
	__extension__ 0b00000011,
	__extension__ 0b00000101,
	__extension__ 0b01111111,
	__extension__ 0b00000101,
	__extension__ 0b00000011
};

const uint8_t BITMAP_ANTENNA_LEVEL1[3] =
{
	__extension__ 0b01100000,
	__extension__ 0b01100000,
	__extension__ 0b00000000
};

const uint8_t BITMAP_ANTENNA_LEVEL2[3] =
{
	__extension__ 0b01110000,
	__extension__ 0b01110000,
	__extension__ 0b00000000
};

const uint8_t BITMAP_ANTENNA_LEVEL3[3] =
{
	__extension__ 0b01111000,
	__extension__ 0b01111000,
	__extension__ 0b00000000
};

const uint8_t BITMAP_ANTENNA_LEVEL4[3] =
{
	__extension__ 0b01111100,
	__extension__ 0b01111100,
	__extension__ 0b00000000
};

const uint8_t BITMAP_ANTENNA_LEVEL5[3] =
{
	__extension__ 0b01111110,
	__extension__ 0b01111110,
	__extension__ 0b00000000
};

const uint8_t BITMAP_ANTENNA_LEVEL6[3] =
{
	__extension__ 0b01111111,
	__extension__ 0b01111111,
	__extension__ 0b00000000
};

const uint8_t BITMAP_MARKER[8] =
{
	__extension__ 0b11111111,
	__extension__ 0b11111111,
	__extension__ 0b01111110,
	__extension__ 0b01111110,
	__extension__ 0b00111100,
	__extension__ 0b00111100,
	__extension__ 0b00011000,
	__extension__ 0b00011000
};

const uint8_t BITMAP_VFO_DEFAULT[8] =
{
	__extension__ 0b00000000,
	__extension__ 0b01111111,
	__extension__ 0b01111111,
	__extension__ 0b00111110,
	__extension__ 0b00111110,
	__extension__ 0b00011100,
	__extension__ 0b00011100,
	__extension__ 0b00001000
};

const uint8_t BITMAP_VFO_NOT_DEFAULT[8] =
{
	__extension__ 0b00000000,
	__extension__ 0b01000001,
	__extension__ 0b01000001,
	__extension__ 0b00100010,
	__extension__ 0b00100010,
	__extension__ 0b00010100,
	__extension__ 0b00010100,
	__extension__ 0b00001000
};

#if 0
	const uint8_t BITMAP_SCANLIST1[6] =
	{	// 'I' symbol
		__extension__ 0b00000000,
		__extension__ 0b00100001,
		__extension__ 0b00111111,
		__extension__ 0b00100001,
		__extension__ 0b00000000,
		__extension__ 0b00000000
	};

	const uint8_t BITMAP_SCANLIST2[6] =
	{	// 'II' symbol
		__extension__ 0b00100001,
		__extension__ 0b00111111,
		__extension__ 0b00100001,
		__extension__ 0b00100001,
		__extension__ 0b00111111,
		__extension__ 0b00100001
	};
#else
	const uint8_t BITMAP_SCANLIST1[6] =
	{	// 'I' symbol
		__extension__ 0b00000000,
		__extension__ 0b00111111,
		__extension__ 0b00111111,
		__extension__ 0b00000000,
		__extension__ 0b00000000,
		__extension__ 0b00000000
	};

	const uint8_t BITMAP_SCANLIST2[6] =
	{	// 'II' symbol
		__extension__ 0b00000000,
		__extension__ 0b00111111,
		__extension__ 0b00111111,
		__extension__ 0b00000000,
		__extension__ 0b00111111,
		__extension__ 0b00111111
	};
#endif
