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
#include "misc.h"

enum frequency_band_e {
	BAND_NONE   = -1,
	BAND1_50MHz =  0,
	BAND2_108MHz,
	BAND3_137MHz,
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

typedef struct {
	const uint32_t lower;
	const uint32_t upper;
	const uint16_t step_size;
} __attribute__((packed)) freq_scan_range_table_t;

extern uint32_t g_aircopy_freq;

extern const freq_band_table_t AIR_BAND;

extern const freq_band_table_t BX4819_BAND1;
extern const freq_band_table_t BX4819_BAND2;

extern const freq_band_table_t FREQ_BAND_TABLE[7];

// 250, 500, 625, 1000, 1250, 2500, 833, 1, 5, 10, 25, 50, 100, 125, 1500, 3000, 5000, 10000, 12500, 25000, 50000
enum step_setting_e {
	STEP_2_5kHz = 0,
	STEP_5_0kHz,
	STEP_6_25kHz,
	STEP_10_0kHz,
	STEP_12_5kHz,
	STEP_25_0kHz,
	STEP_8_33kHz,

	STEP_10Hz,
	STEP_50Hz,
	STEP_100Hz,
	STEP_250Hz,
	STEP_500Hz,
	STEP_1kHz,
	STEP_1_25kHz,
	STEP_15kHz,
	STEP_30kHz,
	STEP_50kHz,
	STEP_100kHz,
	STEP_125kHz,
	STEP_250kHz,
	STEP_500kHz
};
typedef enum step_setting_e step_setting_t;

extern const uint16_t STEP_FREQ_TABLE[21];
extern uint16_t       step_freq_table_sorted[ARRAY_SIZE(STEP_FREQ_TABLE)];

#ifdef ENABLE_NOAA
	extern const uint32_t NOAA_FREQUENCY_TABLE[10];
#endif

// ***********

unsigned int     FREQUENCY_get_step_index(const unsigned int step_size);
void             FREQUENCY_init(void);

frequency_band_t FREQUENCY_GetBand(uint32_t Frequency);
uint8_t          FREQUENCY_CalculateOutputPower(uint8_t TxpLow, uint8_t TxpMid, uint8_t TxpHigh, int32_t LowerLimit, int32_t Middle, int32_t UpperLimit, int32_t Frequency);

uint32_t         FREQUENCY_floor_to_step(uint32_t freq, const uint32_t step_size, const uint32_t lower, const uint32_t upper);

int              FREQUENCY_tx_freq_check(const uint32_t Frequency);
int              FREQUENCY_rx_freq_check(const uint32_t Frequency);

#ifdef ENABLE_SCAN_RANGES
	void FREQUENCY_scan_range(const uint32_t freq, uint32_t *lower, uint32_t *upper, uint32_t *step_size);
#endif

// ***********

#endif
