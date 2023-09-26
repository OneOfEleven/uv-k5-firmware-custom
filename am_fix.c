
/* Copyright 2023 OneOfEleven
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

// code to 'try' and reduce the AM demodulator saturation problem
//
// that is until someone works out how to properly configure the BK chip !

#include <string.h>

#include "am_fix.h"
#include "app/main.h"
#include "board.h"
#include "driver/bk4819.h"
#include "external/printf/printf.h"
#include "frequencies.h"
#include "functions.h"
#include "misc.h"
#include "ui/rssi.h"

// original QS front end register settings
const uint8_t orig_lna_short = 3;   //   0dB
const uint8_t orig_lna       = 2;   // -14dB
const uint8_t orig_mixer     = 3;   //   0dB
const uint8_t orig_pga       = 6;   //  -3dB

#ifdef ENABLE_AM_FIX

	typedef struct
	{
		#if 1
			// bitfields take up less flash bytes
			uint8_t lna_short:2;   // 0 ~ 3
			uint8_t       lna:3;   // 0 ~ 7
			uint8_t     mixer:2;   // 0 ~ 3
			uint8_t       pga:3;   // 0 ~ 7
		#else
			uint8_t lna_short;     // 0 ~ 3
			uint8_t       lna;     // 0 ~ 7
			uint8_t     mixer;     // 0 ~ 3
			uint8_t       pga;     // 0 ~ 7
		#endif
	} t_am_fix_gain_table;
	//} __attribute__((packed)) t_am_fix_gain_table;

	// REG_10 AGC gain table
	//
	// <15:10> ???
	//
	//   <9:8> = LNA Gain Short
	//           3 =   0dB   < original value
	//           2 = -24dB   // was -11
	//           1 = -30dB   // was -16
	//           0 = -33dB   // was -19
	//
	//   <7:5> = LNA Gain
	//           7 =   0dB
	//           6 =  -2dB
	//           5 =  -4dB
	//           4 =  -6dB
	//           3 =  -9dB
	//           2 = -14dB   < original value
	//           1 = -19dB
	//           0 = -24dB
	//
	//   <4:3> = MIXER Gain
	//           3 =   0dB   < original value
	//           2 =  -3dB
	//           1 =  -6dB
	//           0 =  -8dB
	//
	//   <2:0> = PGA Gain
	//           7 =   0dB
	//           6 =  -3dB   < original value
	//           5 =  -6dB
	//           4 =  -9dB
	//           3 = -15dB
	//           2 = -21dB
	//           1 = -27dB
	//           0 = -33dB

	// front end register dB values
	//
	// these values need to be accurate for the code to properly/reliably switch
	// between table entries when adjusting the front end registers.
	//
	// these 4 tables need a measuring/calibration update
	//
	// QUESTION: why do I have to surround the negative numbers in brackets ??? .. if I don't then the table contains wrong numbers
	//
//	static const int16_t lna_short_dB[] = {  -19,   -16,   -11,     0};   // was (but wrong)
	static const int16_t lna_short_dB[] = { (-33), (-30), (-24),    0};   // corrected'ish
	static const int16_t lna_dB[]       = { (-24), (-19), (-14), ( -9), (-6), (-4), (-2), 0};
	static const int16_t mixer_dB[]     = { ( -8), ( -6), ( -3),    0};
	static const int16_t pga_dB[]       = { (-33), (-27), (-21), (-15), (-9), (-6), (-3), 0};

	// lookup table is hugely easier than writing code to do the same
	//
	static const t_am_fix_gain_table am_fix_gain_table[] =
	{
		{.lna_short = 3, .lna = 2, .mixer = 3, .pga = 6},  //  0 0dB -14dB  0dB  -3dB .. -17dB original

#ifdef ENABLE_AM_FIX_TEST1

		// test table that lets me manually set the lna-short register
		// to measure it's actual dB change using an RF signal generator

		{0, 2, 3, 6},         // 1 .. -33dB  -14dB   0dB  -3dB .. -50dB
		{1, 2, 3, 6},         // 2 .. -30dB  -14dB   0dB  -3dB .. -47dB
		{2, 2, 3, 6},         // 3 .. -24dB  -14dB   0dB  -3dB .. -41dB
		{3, 2, 3, 6}          // 4 ..   0dB  -14dB   0dB  -3dB .. -17dB
	};

	const unsigned int original_index = 1;

#elif 0

		//  in this table the 'lna-short' register I leave unchanged

		{3, 0, 0, 0},         //   1 .. 0dB  -24dB  -8dB -33dB .. -65dB
		{3, 0, 1, 0},         //   2 .. 0dB  -24dB  -6dB -33dB .. -63dB
		{3, 0, 2, 0},         //   3 .. 0dB  -24dB  -3dB -33dB .. -60dB
		{3, 0, 0, 1},         //   4 .. 0dB  -24dB  -8dB -27dB .. -59dB
		{3, 1, 1, 0},         //   5 .. 0dB  -19dB  -6dB -33dB .. -58dB
		{3, 0, 1, 1},         //   6 .. 0dB  -24dB  -6dB -27dB .. -57dB
		{3, 1, 2, 0},         //   7 .. 0dB  -19dB  -3dB -33dB .. -55dB
		{3, 1, 0, 1},         //   8 .. 0dB  -19dB  -8dB -27dB .. -54dB
		{3, 0, 0, 2},         //   9 .. 0dB  -24dB  -8dB -21dB .. -53dB
		{3, 1, 1, 1},         //  10 .. 0dB  -19dB  -6dB -27dB .. -52dB
		{3, 0, 1, 2},         //  11 .. 0dB  -24dB  -6dB -21dB .. -51dB
		{3, 2, 2, 0},         //  12 .. 0dB  -14dB  -3dB -33dB .. -50dB
		{3, 2, 0, 1},         //  13 .. 0dB  -14dB  -8dB -27dB .. -49dB
		{3, 0, 2, 2},         //  14 .. 0dB  -24dB  -3dB -21dB .. -48dB
		{3, 2, 3, 0},         //  15 .. 0dB  -14dB   0dB -33dB .. -47dB
		{3, 1, 3, 1},         //  16 .. 0dB  -19dB   0dB -27dB .. -46dB
		{3, 0, 3, 2},         //  17 .. 0dB  -24dB   0dB -21dB .. -45dB
		{3, 3, 0, 1},         //  18 .. 0dB   -9dB  -8dB -27dB .. -44dB
		{3, 1, 2, 2},         //  19 .. 0dB  -19dB  -3dB -21dB .. -43dB
		{3, 0, 2, 3},         //  20 .. 0dB  -24dB  -3dB -15dB .. -42dB
		{3, 0, 0, 4},         //  21 .. 0dB  -24dB  -8dB  -9dB .. -41dB
		{3, 1, 1, 3},         //  22 .. 0dB  -19dB  -6dB -15dB .. -40dB
		{3, 0, 1, 4},         //  23 .. 0dB  -24dB  -6dB  -9dB .. -39dB
		{3, 0, 0, 5},         //  24 .. 0dB  -24dB  -8dB  -6dB .. -38dB
		{3, 1, 2, 3},         //  25 .. 0dB  -19dB  -3dB -15dB .. -37dB
		{3, 0, 2, 4},         //  26 .. 0dB  -24dB  -3dB  -9dB .. -36dB
		{3, 4, 0, 2},         //  27 .. 0dB   -6dB  -8dB -21dB .. -35dB
		{3, 1, 1, 4},         //  28 .. 0dB  -19dB  -6dB  -9dB .. -34dB
		{3, 1, 0, 5},         //  29 .. 0dB  -19dB  -8dB  -6dB .. -33dB
		{3, 3, 0, 3},         //  30 .. 0dB   -9dB  -8dB -15dB .. -32dB
		{3, 5, 1, 2},         //  31 .. 0dB   -4dB  -6dB -21dB .. -31dB
		{3, 1, 0, 6},         //  32 .. 0dB  -19dB  -8dB  -3dB .. -30dB
		{3, 2, 3, 3},         //  33 .. 0dB  -14dB   0dB -15dB .. -29dB
		{3, 1, 1, 6},         //  34 .. 0dB  -19dB  -6dB  -3dB .. -28dB
		{3, 4, 1, 3},         //  35 .. 0dB   -6dB  -6dB -15dB .. -27dB
		{3, 2, 2, 4},         //  36 .. 0dB  -14dB  -3dB  -9dB .. -26dB
		{3, 1, 2, 6},         //  37 .. 0dB  -19dB  -3dB  -3dB .. -25dB
		{3, 3, 1, 4},         //  38 .. 0dB   -9dB  -6dB  -9dB .. -24dB
		{3, 2, 1, 6},         //  39 .. 0dB  -14dB  -6dB  -3dB .. -23dB
		{3, 5, 2, 3},         //  40 .. 0dB   -4dB  -3dB -15dB .. -22dB
		{3, 4, 1, 4},         //  41 .. 0dB   -6dB  -6dB  -9dB .. -21dB
		{3, 4, 0, 5},         //  42 .. 0dB   -6dB  -8dB  -6dB .. -20dB
		{3, 5, 1, 4},         //  43 .. 0dB   -4dB  -6dB  -9dB .. -19dB
		{3, 3, 3, 4},         //  44 .. 0dB   -9dB   0dB  -9dB .. -18dB
		{3, 2, 3, 6},         //  45 .. 0dB  -14dB   0dB  -3dB .. -17dB original
		{3, 5, 1, 5},         //  46 .. 0dB   -4dB  -6dB  -6dB .. -16dB
		{3, 3, 1, 7},         //  47 .. 0dB   -9dB  -6dB   0dB .. -15dB
		{3, 2, 3, 7},         //  48 .. 0dB  -14dB   0dB   0dB .. -14dB
		{3, 5, 1, 6},         //  49 .. 0dB   -4dB  -6dB  -3dB .. -13dB
		{3, 4, 2, 6},         //  50 .. 0dB   -6dB  -3dB  -3dB .. -12dB
		{3, 5, 2, 6},         //  51 .. 0dB   -4dB  -3dB  -3dB .. -10dB
		{3, 4, 3, 6},         //  52 .. 0dB   -6dB   0dB  -3dB ..  -9dB
		{3, 5, 2, 7},         //  53 .. 0dB   -4dB  -3dB   0dB ..  -7dB
		{3, 4, 3, 7},         //  54 .. 0dB   -6dB   0dB   0dB ..  -6dB
		{3, 5, 3, 7}          //  55 .. 0dB   -4dB   0dB   0dB ..  -4dB
	};

	const unsigned int original_index = 45;

#else

		// in this table I include adjusting the 'lna-short' register ~
		// more gain adjustment range from doing so

		{0, 0, 0, 0},         //   1 .. -33dB -24dB -8dB -33dB .. -98dB
		{0, 0, 1, 0},         //   2 .. -33dB -24dB -6dB -33dB .. -96dB
		{1, 0, 0, 0},         //   3 .. -30dB -24dB -8dB -33dB .. -95dB
		{0, 1, 0, 0},         //   4 .. -33dB -19dB -8dB -33dB .. -93dB
		{0, 0, 0, 1},         //   5 .. -33dB -24dB -8dB -27dB .. -92dB
		{0, 1, 1, 0},         //   6 .. -33dB -19dB -6dB -33dB .. -91dB
		{0, 0, 1, 1},         //   7 .. -33dB -24dB -6dB -27dB .. -90dB
		{1, 0, 0, 1},         //   8 .. -30dB -24dB -8dB -27dB .. -89dB
		{0, 1, 2, 0},         //   9 .. -33dB -19dB -3dB -33dB .. -88dB
		{1, 0, 3, 0},         //  10 .. -30dB -24dB  0dB -33dB .. -87dB
		{0, 0, 0, 2},         //  11 .. -33dB -24dB -8dB -21dB .. -86dB
		{1, 1, 2, 0},         //  12 .. -30dB -19dB -3dB -33dB .. -85dB
		{0, 0, 3, 1},         //  13 .. -33dB -24dB  0dB -27dB .. -84dB
		{0, 3, 0, 0},         //  14 .. -33dB  -9dB -8dB -33dB .. -83dB
		{1, 1, 3, 0},         //  15 .. -30dB -19dB  0dB -33dB .. -82dB
		{1, 0, 3, 1},         //  16 .. -30dB -24dB  0dB -27dB .. -81dB
		{0, 2, 3, 0},         //  17 .. -33dB -14dB  0dB -33dB .. -80dB
		{1, 2, 0, 1},         //  18 .. -30dB -14dB -8dB -27dB .. -79dB
		{0, 3, 2, 0},         //  19 .. -33dB  -9dB -3dB -33dB .. -78dB
		{1, 4, 0, 0},         //  20 .. -30dB  -6dB -8dB -33dB .. -77dB
		{1, 1, 3, 1},         //  21 .. -30dB -19dB  0dB -27dB .. -76dB
		{0, 0, 2, 3},         //  22 .. -33dB -24dB -3dB -15dB .. -75dB
		{1, 3, 0, 1},         //  23 .. -30dB  -9dB -8dB -27dB .. -74dB
		{1, 6, 0, 0},         //  24 .. -30dB  -2dB -8dB -33dB .. -73dB
		{0, 7, 1, 0},         //  25 .. -33dB   0dB -6dB -33dB .. -72dB
		{0, 6, 2, 0},         //  26 .. -33dB  -2dB -3dB -33dB .. -71dB
		{2, 1, 3, 1},         //  27 .. -24dB -19dB  0dB -27dB .. -70dB
		{0, 3, 1, 2},         //  28 .. -33dB  -9dB -6dB -21dB .. -69dB
		{0, 0, 0, 6},         //  29 .. -33dB -24dB -8dB  -3dB .. -68dB
		{0, 5, 2, 1},         //  30 .. -33dB  -4dB -3dB -27dB .. -67dB
		{0, 0, 1, 6},         //  31 .. -33dB -24dB -6dB  -3dB .. -66dB
		{1, 2, 3, 2},         //  32 .. -30dB -14dB  0dB -21dB .. -65dB
		{2, 1, 1, 3},         //  33 .. -24dB -19dB -6dB -15dB .. -64dB
		{1, 7, 3, 0},         //  34 .. -30dB   0dB  0dB -33dB .. -63dB
		{1, 3, 0, 3},         //  35 .. -30dB  -9dB -8dB -15dB .. -62dB
		{0, 1, 2, 5},         //  36 .. -33dB -19dB -3dB  -6dB .. -61dB
		{2, 0, 2, 4},         //  37 .. -24dB -24dB -3dB  -9dB .. -60dB
		{1, 6, 3, 1},         //  38 .. -30dB  -2dB  0dB -27dB .. -59dB
		{1, 2, 0, 5},         //  39 .. -30dB -14dB -8dB  -6dB .. -58dB
		{2, 5, 0, 2},         //  40 .. -24dB  -4dB -8dB -21dB .. -57dB
		{2, 6, 2, 1},         //  41 .. -24dB  -2dB -3dB -27dB .. -56dB
		{0, 5, 2, 3},         //  42 .. -33dB  -4dB -3dB -15dB .. -55dB
		{2, 0, 2, 6},         //  43 .. -24dB -24dB -3dB  -3dB .. -54dB
		{0, 3, 0, 6},         //  44 .. -33dB  -9dB -8dB  -3dB .. -53dB
		{0, 6, 0, 4},         //  45 .. -33dB  -2dB -8dB  -9dB .. -52dB
		{0, 3, 1, 6},         //  46 .. -33dB  -9dB -6dB  -3dB .. -51dB
		{1, 2, 3, 5},         //  47 .. -30dB -14dB  0dB  -6dB .. -50dB
		{0, 5, 1, 5},         //  48 .. -33dB  -4dB -6dB  -6dB .. -49dB
		{0, 3, 3, 5},         //  49 .. -33dB  -9dB  0dB  -6dB .. -48dB
		{0, 6, 2, 4},         //  50 .. -33dB  -2dB -3dB  -9dB .. -47dB
		{1, 5, 2, 4},         //  51 .. -30dB  -4dB -3dB  -9dB .. -46dB
		{3, 0, 1, 3},         //  52 ..   0dB -24dB -6dB -15dB .. -45dB
		{0, 6, 1, 6},         //  53 .. -33dB  -2dB -6dB  -3dB .. -44dB
		{1, 5, 2, 5},         //  54 .. -30dB  -4dB -3dB  -6dB .. -43dB
		{0, 4, 2, 7},         //  55 .. -33dB  -6dB -3dB   0dB .. -42dB
		{2, 2, 2, 7},         //  56 .. -24dB -14dB -3dB   0dB .. -41dB
		{2, 5, 2, 4},         //  57 .. -24dB  -4dB -3dB  -9dB .. -40dB
		{2, 3, 3, 5},         //  58 .. -24dB  -9dB  0dB  -6dB .. -39dB
		{1, 6, 3, 5},         //  59 .. -30dB  -2dB  0dB  -6dB .. -38dB
		{2, 5, 1, 6},         //  60 .. -24dB  -4dB -6dB  -3dB .. -37dB
		{3, 3, 3, 1},         //  61 ..   0dB  -9dB  0dB -27dB .. -36dB
		{3, 2, 3, 2},         //  62 ..   0dB -14dB  0dB -21dB .. -35dB
		{2, 5, 2, 6},         //  63 .. -24dB  -4dB -3dB  -3dB .. -34dB
		{3, 0, 1, 6},         //  64 ..   0dB -24dB -6dB  -3dB .. -33dB
		{3, 0, 0, 7},         //  65 ..   0dB -24dB -8dB   0dB .. -32dB
		{2, 5, 3, 6},         //  66 .. -24dB  -4dB  0dB  -3dB .. -31dB
		{3, 3, 3, 2},         //  67 ..   0dB  -9dB  0dB -21dB .. -30dB
		{2, 6, 3, 6},         //  68 .. -24dB  -2dB  0dB  -3dB .. -29dB
		{3, 2, 0, 5},         //  69 ..   0dB -14dB -8dB  -6dB .. -28dB
		{3, 5, 0, 3},         //  70 ..   0dB  -4dB -8dB -15dB .. -27dB
		{3, 3, 0, 4},         //  71 ..   0dB  -9dB -8dB  -9dB .. -26dB
		{3, 1, 1, 7},         //  72 ..   0dB -19dB -6dB   0dB .. -25dB
		{3, 4, 2, 3},         //  73 ..   0dB  -6dB -3dB -15dB .. -24dB
		{3, 4, 0, 4},         //  74 ..   0dB  -6dB -8dB  -9dB .. -23dB
		{3, 2, 0, 7},         //  75 ..   0dB -14dB -8dB   0dB .. -22dB
		{3, 7, 1, 3},         //  76 ..   0dB   0dB -6dB -15dB .. -21dB
		{3, 6, 2, 3},         //  77 ..   0dB  -2dB -3dB -15dB .. -20dB
		{3, 5, 3, 3},         //  78 ..   0dB  -4dB  0dB -15dB .. -19dB
		{3, 3, 3, 4},         //  79 ..   0dB  -9dB  0dB  -9dB .. -18dB
		{3, 2, 3, 6},         //  80 ..   0dB -14dB  0dB  -3dB .. -17dB original
		{3, 6, 0, 5},         //  81 ..   0dB  -2dB -8dB  -6dB .. -16dB
		{3, 7, 1, 4},         //  82 ..   0dB   0dB -6dB  -9dB .. -15dB
		{3, 2, 3, 7},         //  83 ..   0dB -14dB  0dB   0dB .. -14dB
		{3, 6, 0, 6},         //  84 ..   0dB  -2dB -8dB  -3dB .. -13dB
		{3, 3, 2, 7},         //  85 ..   0dB  -9dB -3dB   0dB .. -12dB
		{3, 7, 0, 6},         //  86 ..   0dB   0dB -8dB  -3dB .. -11dB
		{3, 5, 3, 5},         //  87 ..   0dB  -4dB  0dB  -6dB .. -10dB
		{3, 7, 2, 5},         //  88 ..   0dB   0dB -3dB  -6dB ..  -9dB
		{3, 6, 3, 5},         //  89 ..   0dB  -2dB  0dB  -6dB ..  -8dB
		{3, 5, 2, 7},         //  90 ..   0dB  -4dB -3dB   0dB ..  -7dB
		{3, 7, 2, 6},         //  91 ..   0dB   0dB -3dB  -3dB ..  -6dB
		{3, 6, 2, 7},         //  92 ..   0dB  -2dB -3dB   0dB ..  -5dB
		{3, 5, 3, 7},         //  93 ..   0dB  -4dB  0dB   0dB ..  -4dB
		{3, 7, 2, 7},         //  94 ..   0dB   0dB -3dB   0dB ..  -3dB
		{3, 6, 3, 7},         //  95 ..   0dB  -2dB  0dB   0dB ..  -2dB
		{3, 7, 3, 7}          //  96 ..   0dB   0dB  0dB   0dB ..   0dB
	};

	const unsigned int original_index = 80;

#endif

	unsigned int counter = 0;
	
	int16_t orig_dB_gain = -17;

	#ifdef ENABLE_AM_FIX_TEST1
		unsigned int am_fix_gain_table_index[2] = {1 + gSetting_AM_fix_test1, 1 + gSetting_AM_fix_test1};
	#else
		unsigned int am_fix_gain_table_index[2] = {original_index, original_index};
	#endif

	// used to simply detect we've changed our table index/register settings
	unsigned int am_fix_gain_table_index_prev[2] = {0, 0};

	// holds the previous RSSI level
	int16_t prev_rssi[2] = {0, 0};

	// to help reduce gain hunting, provides a peak hold time delay
	unsigned int am_gain_hold_counter[2] = {0, 0};

	// used to correct the RSSI readings after our front end gain adjustments
	int16_t rssi_db_gain_diff[2] = {0, 0};

	// used to limit the max front end gain
	unsigned int max_index = ARRAY_SIZE(am_fix_gain_table) - 1;
	
	void AM_fix_init(void)
	{
/*		unsigned int i;
		for (i = 0; i < 2; i++)
		{
			#ifdef ENABLE_AM_FIX_TEST1
				am_fix_gain_table_index[i] = 1 + gSetting_AM_fix_test1;
			#else
				am_fix_gain_table_index[i] = original_index;  // re-start with original QS setting
			#endif
		}
*/
		orig_dB_gain = lna_short_dB[orig_lna_short] + lna_dB[orig_lna] + mixer_dB[orig_mixer] + pga_dB[orig_pga];

		#if 0
		{	// set the maximum gain we want to use
//			const int16_t max_gain_dB = orig_dB_gain;
			const int16_t max_gain_dB = -10;
			max_index = ARRAY_SIZE(am_fix_gain_table);
			while (--max_index > 1)
			{
				const uint8_t lna_short = am_fix_gain_table[max_index].lna_short;
				const uint8_t lna       = am_fix_gain_table[max_index].lna;
				const uint8_t mixer     = am_fix_gain_table[max_index].mixer;
				const uint8_t pga       = am_fix_gain_table[max_index].pga;
				const int16_t gain_dB   = lna_short_dB[lna_short] + lna_dB[lna] + mixer_dB[mixer] + pga_dB[pga];
				if (gain_dB <= max_gain_dB)
					break;
			}
		}
		#else
			max_index = ARRAY_SIZE(am_fix_gain_table) - 1;
		#endif
	}

	void AM_fix_reset(const int vfo)
	{	// reset the AM fixer

		prev_rssi[vfo] = 0;

		am_gain_hold_counter[vfo] = 0;

		rssi_db_gain_diff[vfo] = 0;

		#ifdef ENABLE_AM_FIX_TEST1
			am_fix_gain_table_index[vfo] = 1 + gSetting_AM_fix_test1;
		#else
			am_fix_gain_table_index[vfo] = original_index;  // re-start with original QS setting
		#endif

		am_fix_gain_table_index_prev[vfo] = 0;
		
//		AM_fix_init();  // bootup calls this
	}

	// adjust the RX RF gain to try and prevent the AM demodulator from
	// saturating/overloading/clipping (distorted AM audio)
	//
	// we're actually doing the BK4819's job for it here, but as the chip
	// won't/don't do it for itself, we're left to bodging it ourself by
	// playing with the RF front end gain settings
	//
	void AM_fix_adjust_frontEnd_10ms(const int vfo)
	{
		int16_t diff_dB;
		int16_t rssi;

		#ifndef ENABLE_AM_FIX_TEST1
			// -89dBm, any higher and the AM demodulator starts to saturate/clip/distort
			const int16_t desired_rssi = (-89 + 160) * 2;   // dBm to ADC sample
		#endif

		counter++;
		
		// but we're not in FM mode, we're in AM mode

		switch (gCurrentFunction)
		{
			case FUNCTION_TRANSMIT:
			case FUNCTION_BAND_SCOPE:
			case FUNCTION_POWER_SAVE:
				return;

			case FUNCTION_FOREGROUND:
//				return;

			// only adjust stuff if we're in one of these modes
			case FUNCTION_RECEIVE:
			case FUNCTION_MONITOR:
			case FUNCTION_INCOMING:
				break;
		}

		{	// sample the current RSSI level
			// average it with the previous rssi (a bit of noise/spike immunity)
			const int16_t new_rssi = BK4819_GetRSSI();
			rssi                   = (prev_rssi[vfo] > 0) ? (prev_rssi[vfo] + new_rssi) / 2 : new_rssi;
//			rssi                   = (new_rssi > prev_rssi[vfo] && prev_rssi[vfo] > 0) ? (prev_rssi[vfo] + new_rssi) / 2 : new_rssi;
			prev_rssi[vfo]         = new_rssi;
		}

		{	// save the corrected RSSI level
			const int16_t new_rssi = rssi - (rssi_db_gain_diff[vfo] * 2);
			if (gCurrentRSSI[vfo] != new_rssi)
			{
				gCurrentRSSI[vfo] = new_rssi;
				gUpdateDisplay    = ((counter & 15u) == 0);  // limit the screen update rate to once every 150ms
			}
		}
		
#ifdef ENABLE_AM_FIX_TEST1
		// user is manually adjusting a gain register - don't do anything automatically

		{		
			int i = 1 + (int)gSetting_AM_fix_test1;
			i = (i < 1) ? 1 : (i > ((int)ARRAY_SIZE(am_fix_gain_table) - 1) ? ARRAY_SIZE(am_fix_gain_table) - 1 : i;
			
			if (am_fix_gain_table_index[vfo] == i)
				return;    // no change
		}

#else
		// automatically adjust the RF RX gain

		if (am_gain_hold_counter[vfo] > 0)
			am_gain_hold_counter[vfo]--;

		// dB difference between actual and desired RSSI level
		diff_dB = (rssi - desired_rssi) / 2;

		if (diff_dB > 0)
		{	// decrease gain

			unsigned int index = am_fix_gain_table_index[vfo];   // current position we're at

			if (diff_dB >= 10)
			{	// jump immediately to a new gain setting
				// this greatly speeds up initial gain reduction (but reduces noise/spike immunity)

				uint8_t lna_short  = am_fix_gain_table[index].lna_short;
				uint8_t lna        = am_fix_gain_table[index].lna;
				uint8_t mixer      = am_fix_gain_table[index].mixer;
				uint8_t pga        = am_fix_gain_table[index].pga;

//				const int16_t desired_gain_dB = lna_short_dB[lna_short] + lna_dB[lna] + mixer_dB[mixer] + pga_dB[pga] - diff_dB;
				const int16_t desired_gain_dB = lna_short_dB[lna_short] + lna_dB[lna] + mixer_dB[mixer] + pga_dB[pga] - diff_dB + 8; // get no closer than 8dB (bit of noise/spike immunity)

				// scan the table to see what index to jump straight too
				while (index > 1)
				{
					index--;

					lna_short = am_fix_gain_table[index].lna_short;
					lna       = am_fix_gain_table[index].lna;
					mixer     = am_fix_gain_table[index].mixer;
					pga       = am_fix_gain_table[index].pga;

					{
						const int16_t gain_dB = lna_short_dB[lna_short] + lna_dB[lna] + mixer_dB[mixer] + pga_dB[pga];
						if (gain_dB <= desired_gain_dB)
							break;
					}
				}

				//index = (am_fix_gain_table_index[vfo] + index) / 2;  // easy does it
			}
			else
			{	// incrementally reduce the gain .. taking it slow improves noise/spike immunity

//				if (index >= (1 + 3) && diff_dB >= 3)
//					index -= 3;  // faster gain reduction
//				else
				if (index > 1)
					index--;     // slow step-by-step gain reduction
			}

			index = (index < 1) ? 1 : (index > max_index) ? max_index : index;
			
			if (am_fix_gain_table_index[vfo] != index)
			{
				am_fix_gain_table_index[vfo] = index;
				am_gain_hold_counter[vfo]    = 30;   // 300ms hold
			}
		}

		if (diff_dB >= -4)                    // 4dB hysterisis (help reduce gain hunting)
			am_gain_hold_counter[vfo] = 30;   // 300ms hold

		if (am_gain_hold_counter[vfo] == 0)
		{	// hold has been released, we're free to increase gain
			const unsigned int index = am_fix_gain_table_index[vfo] + 1;
			am_fix_gain_table_index[vfo] = (index > max_index) ? max_index : index;      // limit the gain
		}

		if (am_fix_gain_table_index[vfo] == am_fix_gain_table_index_prev[vfo])
			return;     // no gain changes have been made

#endif

		{	// apply the new settings to the front end registers

			const unsigned int index   = am_fix_gain_table_index[vfo];

			const uint8_t lna_short = am_fix_gain_table[index].lna_short;
			const uint8_t lna       = am_fix_gain_table[index].lna;
			const uint8_t mixer     = am_fix_gain_table[index].mixer;
			const uint8_t pga       = am_fix_gain_table[index].pga;
			const int16_t gain_dB   = lna_short_dB[lna_short] + lna_dB[lna] + mixer_dB[mixer] + pga_dB[pga];

			BK4819_WriteRegister(BK4819_REG_13, (lna_short << 8) | (lna << 5) | (mixer << 3) | (pga << 0));

			// offset the RSSI reading to the rest of the firmware to cancel out the gain adjustments we make

			// gain difference from original QS setting
			rssi_db_gain_diff[vfo] = gain_dB - orig_dB_gain;

			// remember the new table index
			am_fix_gain_table_index_prev[vfo] = index;
			
			{	// save the corrected RSSI level
				const int16_t new_rssi = rssi - (rssi_db_gain_diff[vfo] * 2);
				if (gCurrentRSSI[vfo] != new_rssi)
				{
					gCurrentRSSI[vfo] = new_rssi;
					gUpdateDisplay    = ((counter & 15u) == 0);  // limit the screen update rate to once every 150ms
				}
			}
		}

		#ifdef ENABLE_AM_FIX_SHOW_DATA
			gUpdateDisplay = ((counter & 15u) == 0);  // limit the screen update rate to once every 150ms
		#endif
	}

	#ifdef ENABLE_AM_FIX_SHOW_DATA

		void AM_fix_print_data(const int vfo, char *s)
		{
			// fetch current register settings
			const unsigned int index = am_fix_gain_table_index[vfo];
			const uint8_t lna_short  = am_fix_gain_table[index].lna_short;
			const uint8_t lna        = am_fix_gain_table[index].lna;
			const uint8_t mixer      = am_fix_gain_table[index].mixer;
			const uint8_t pga        = am_fix_gain_table[index].pga;
			const int16_t gain_dB    = lna_short_dB[lna_short] + lna_dB[lna] + mixer_dB[mixer] + pga_dB[pga];
			if (s != NULL)
				sprintf(s, "idx %2d %4ddB %3u", index, gain_dB, prev_rssi[vfo]);
		}

	#endif

#endif
