/* Copyright 2023 Dual Tachyon
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

#ifndef FREQUENCIES_H
#define FREQUENCIES_H

#include <stdint.h>

#include "frequencies.h"

enum frequency_band_e {
	BAND_NONE   = -1,
	BAND1_50MHz =  0,
	BAND2_108MHz,
	BAND3_136MHz,
	BAND4_174MHz,
	BAND5_350MHz,
	BAND6_400MHz,
	BAND7_470MHz
};
typedef enum frequency_band_e frequency_band_t;

typedef struct {
	const uint32_t lower;
	const uint32_t upper;
} freq_band_table_t;

extern uint32_t g_aircopy_freq;

extern const freq_band_table_t FM_RADIO_BAND;

extern const freq_band_table_t BX4819_BAND1;
extern const freq_band_table_t BX4819_BAND2;

extern const freq_band_table_t FREQ_BAND_TABLE[7];

#ifdef ENABLE_1250HZ_STEP
	// includes 1.25kHz step
	enum step_setting_e {
		STEP_1_25kHz = 0,
		STEP_2_5kHz,
		STEP_6_25kHz,
		STEP_10_0kHz,
		STEP_12_5kHz,
		STEP_25_0kHz,
		STEP_8_33kHz,
//		STEP_100Hz,
//		STEP_500Hz
	};
#else
	// QS steps
	enum step_setting_e {
		STEP_2_5kHz = 0,
		STEP_5_0kHz,
		STEP_6_25kHz,
		STEP_10_0kHz,
		STEP_12_5kHz,
		STEP_25_0kHz,
		STEP_8_33kHz,
//		STEP_100Hz,
//		STEP_500Hz
	};
#endif
typedef enum step_setting_e step_setting_t;

extern const uint16_t STEP_FREQ_TABLE[7];
//extern const uint16_t STEP_FREQ_TABLE[9];

#ifdef ENABLE_NOAA
	extern const uint32_t NOAA_FREQUENCY_TABLE[10];
#endif

frequency_band_t FREQUENCY_GetBand(uint32_t Frequency);
uint8_t          FREQUENCY_CalculateOutputPower(uint8_t TxpLow, uint8_t TxpMid, uint8_t TxpHigh, int32_t LowerLimit, int32_t Middle, int32_t UpperLimit, int32_t Frequency);
uint32_t         FREQUENCY_FloorToStep(uint32_t Upper, uint32_t Step, uint32_t Lower);

int              TX_freq_check(const uint32_t Frequency);
int              RX_freq_check(const uint32_t Frequency);

#endif
