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

#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
#include "frequencies.h"
#include "misc.h"
#include "settings.h"

// the default AIRCOPY frequency
uint32_t g_aircopy_freq = 41002500;

const freq_band_table_t AIR_BAND = {10800000, 13700000};

// the BK4819 has 2 bands it covers, 18MHz ~ 630MHz and 760MHz ~ 1300MHz
const freq_band_table_t BX4819_BAND1 = { 1800000,  63000000};
const freq_band_table_t BX4819_BAND2 = {84000000, 130000000};

const freq_band_table_t FREQ_BAND_TABLE[7] =
{
	#ifdef ENABLE_WIDE_RX
		// extended range
		{BX4819_BAND1.lower, 10800000},             // band 1
		{AIR_BAND.lower,     AIR_BAND.upper},       // band 2
		{AIR_BAND.upper,     17400000},             // band 3
		{17400000,           35000000},             // band 4
		{35000000,           40000000},             // band 5
		{40000000,           47000000},             // band 6
		{47000000,           BX4819_BAND2.upper}    // band 7
	#else
		// QS original
		{ 5000000,       7600000},         // band 1
		{AIR_BAND.lower, AIR_BAND.upper},  // band 2
		{AIR_BAND.upper, 17400000},        // band 3
		{17400000,       35000000},        // band 4
		{35000000,       40000000},        // band 5
		{40000000,       47000000},        // band 6
		{47000000,       60000000}         // band 7
	#endif
};

#ifdef ENABLE_NOAA
	const uint32_t NOAA_FREQUENCY_TABLE[10] =
	{
		16255000,
		16240000,
		16247500,
		16242500,
		16245000,
		16250000,
		16252500,
		16152500,
		16177500,
		16327500
	};
#endif

// the first 7 values MUST remain in those same positions
// so as to remain compatible with the QS config software
//
const uint16_t STEP_FREQ_TABLE[21] = {
	250, 500, 625, 1000, 1250, 2500, 833,
	1, 5, 10, 25, 50, 100, 125, 1500, 3000, 5000, 10000, 12500, 25000, 50000
};

// the above step sizes will be sorted to appear to be in order to the user
uint16_t step_freq_table_sorted[ARRAY_SIZE(STEP_FREQ_TABLE)];

unsigned int FREQUENCY_get_step_index(const unsigned int step_size)
{	// return the index into 'STEP_FREQ_TABLE' for the supplied step size
	unsigned int i;
	for (i = 0; i < ARRAY_SIZE(step_freq_table_sorted); i++)
		if (STEP_FREQ_TABLE[step_freq_table_sorted[i]] == step_size)
			return i;
	// not found, so default to 12.5kHz
	return 11;
}

void FREQUENCY_init(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(step_freq_table_sorted); i++)
		step_freq_table_sorted[i] = i;

	// sort according to step size
	for (i = 0; i < ARRAY_SIZE(step_freq_table_sorted) - 1; i++)
	{
		uint16_t step1 = STEP_FREQ_TABLE[step_freq_table_sorted[i]];
		unsigned int k;
		for (k = i + 1; k < ARRAY_SIZE(step_freq_table_sorted); k++)
		{
			const uint16_t step2 = STEP_FREQ_TABLE[step_freq_table_sorted[k]];
			if (step2 < step1)
			{	// swap
				const uint16_t temp = step_freq_table_sorted[i];
				step_freq_table_sorted[i] = step_freq_table_sorted[k];
				step_freq_table_sorted[k] = temp;
				step1 = STEP_FREQ_TABLE[step_freq_table_sorted[i]];
			}
		}
	}
/*
	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		UART_SendText("step ..\r\n");
		for (i = 0; i < ARRAY_SIZE(step_freq_table_sorted); i++)
			UART_printf("%2u %2u %5u\r\n", i, step_freq_table_sorted[i], STEP_FREQ_TABLE[step_freq_table_sorted[i]]);
		UART_SendText("\r\n");
	#endif
*/
}

frequency_band_t FREQUENCY_GetBand(uint32_t Frequency)
{
	int band;
	for (band = ARRAY_SIZE(FREQ_BAND_TABLE) - 1; band >= 0; band--)
		if (Frequency >= FREQ_BAND_TABLE[band].lower && Frequency < FREQ_BAND_TABLE[band].upper)
//		if (Frequency >= FREQ_BAND_TABLE[band].lower)
			return (frequency_band_t)band;

	return BAND1_50MHz;
//	return BAND_NONE;
}

uint8_t FREQUENCY_CalculateOutputPower(uint8_t TxpLow, uint8_t TxpMid, uint8_t TxpHigh, int32_t LowerLimit, int32_t Middle, int32_t UpperLimit, int32_t Frequency)
{
	uint8_t pwr = TxpMid;

	if (Frequency <= LowerLimit)
		return TxpLow;

	if (Frequency >= UpperLimit)
		return TxpHigh;

	// linear interpolation
	if (Frequency <= Middle)
		pwr += ((TxpMid  - TxpLow) * (Frequency - LowerLimit)) / (Middle - LowerLimit);
	else
		pwr += ((TxpHigh - TxpMid) * (Frequency - Middle))     / (UpperLimit - Middle);
	return pwr;
}

uint32_t FREQUENCY_floor_to_step(uint32_t freq, const uint32_t step_size, const uint32_t lower, const uint32_t upper)
{
	uint32_t delta;

	if (upper > lower && upper != 0xffffffff)
		if (freq > (upper - 1))
			freq =  upper - 1;

	if (freq <= lower)
		return lower;

	delta = freq - lower;

	if (step_size == 833)  // 8.33 ~ 25/3
	{	// long winded because 8.33 is not exactly 25/3
		uint32_t base  =  (delta / 2500) * 2500;
		uint32_t index = ((delta - base) % 2500) / step_size;
		if (index == 2)
			base++;
		freq = lower + base + (step_size * index);
	}
	else
	{
		freq = lower + ((delta / step_size) * step_size);
	}

	return freq;
}

#ifdef ENABLE_SCAN_RANGES
	const freq_scan_range_table_t FREQ_SCAN_RANGE_TABLE[] =
	{
		{ 2760125,  2760125 + (1000 * 40), 1000},
		{ 2696500,  2696500 + (1000 * 80), 1000},
		{ 2600000,  2800000, 1000},
		{ 2800000,  2970000, 1000},
		{ 5000000,  5200000, 1000},
		{ 5000000,  5400000, 1000},
		{ 7000000,  7050000, 1250},
		{10800000, 11800000, 2500},
//		{11800000, 13700000,  833},
		{11800000, 13700000, 2500},
		{14400000, 14600000, 1250},
		{14400000, 14800000, 1250},
		{15600000, 15800000, 2500},
		{16200000, 17400000, 1250},
		{21900000, 22500000, 1500},
		{24000000, 39000000, 2500},
		{43000000, 44000000, 1250},
		{44600625, 44600625 + (1250 * 16), 1250},
		{44000000, 47000000, 1250}
	};

	void FREQUENCY_scan_range(const uint32_t freq, uint32_t *lower, uint32_t *upper, uint32_t *step_size)
	{
		const frequency_band_t band = FREQUENCY_GetBand(freq);
		unsigned int i;

		for (i = 0; i < ARRAY_SIZE(FREQ_SCAN_RANGE_TABLE); i++)
		{
			const uint32_t _upper = FREQ_SCAN_RANGE_TABLE[i].upper;
			const uint32_t _lower = FREQ_SCAN_RANGE_TABLE[i].lower;
			if (freq >= _lower && freq < _upper)
			{
				if (upper)     *upper     = _upper;
				if (lower)     *lower     = _lower;
				if (step_size) *step_size = FREQ_SCAN_RANGE_TABLE[i].step_size;
				return;
			}
		}

		if (upper)     *upper     = FREQ_BAND_TABLE[band].upper;
		if (lower)     *lower     = FREQ_BAND_TABLE[band].lower;
//		if (step_size) *step_size = FREQ_BAND_TABLE[band].step_size;
	}

#endif

int FREQUENCY_tx_freq_check(const uint32_t Frequency)
{	// return '0' if TX frequency is allowed
	// otherwise return '-1'

	if (Frequency < BX4819_BAND1.lower || Frequency > BX4819_BAND2.upper)
		return -1;  // BX radio chip does not work out this range

	if (Frequency >= BX4819_BAND1.upper && Frequency < BX4819_BAND2.lower)
		return -1;  // BX radio chip does not work in this range

	if (Frequency >= AIR_BAND.lower && Frequency < AIR_BAND.upper)
		return -1;  // TX not allowed in the airband

	if (Frequency < FREQ_BAND_TABLE[0].lower || Frequency > FREQ_BAND_TABLE[ARRAY_SIZE(FREQ_BAND_TABLE) - 1].upper)
		return -1;  // TX not allowed outside this range

	switch (g_eeprom.config.setting.freq_lock)
	{
		default:
		case FREQ_LOCK_NORMAL:
			if (Frequency >= AIR_BAND.upper && Frequency < 17400000)
				return 0;
			if (Frequency >= 17400000 && Frequency < 35000000)
				if (g_eeprom.config.setting.enable_tx_200)
					return 0;
			if (Frequency >= 35000000 && Frequency < 40000000)
				if (g_eeprom.config.setting.enable_tx_350 && g_eeprom.config.setting.enable_350)
					return 0;
			if (Frequency >= 40000000 && Frequency < 47000000)
				return 0;
			if (Frequency >= 47000000 && Frequency <= 60000000)
				if (g_eeprom.config.setting.enable_tx_470)
					return 0;
			break;

		case FREQ_LOCK_FCC:
			if (Frequency >= 14400000 && Frequency < 14800000)
				return 0;
			if (Frequency >= 42000000 && Frequency < 45000000)
				return 0;
			break;

		case FREQ_LOCK_CE:
			if (Frequency >= 14400000 && Frequency < 14600000)
				return 0;
			if (Frequency >= 43000000 && Frequency < 44000000)
				return 0;
			break;

		case FREQ_LOCK_GB:
			if (Frequency >= 14400000 && Frequency < 14800000)
				return 0;
			if (Frequency >= 43000000 && Frequency < 44000000)
				return 0;
			break;

		case FREQ_LOCK_430:
			if (Frequency >= AIR_BAND.lower && Frequency < 17400000)
				return 0;
			if (Frequency >= 40000000 && Frequency < 43000000)
				return 0;
			break;

		case FREQ_LOCK_438:
			if (Frequency >= AIR_BAND.lower && Frequency < 17400000)
				return 0;
			if (Frequency >= 40000000 && Frequency < 43800000)
				return 0;
			break;

		case FREQ_LOCK_446:
			if (Frequency >= 446.00625 && Frequency <= 446.19375)
				return 0;
			break;

		#ifdef ENABLE_TX_UNLOCK
			case FREQ_LOCK_TX_UNLOCK:
			{
				unsigned int i;
				for (i = 0; i < ARRAY_SIZE(FREQ_BAND_TABLE); i++)
					if (Frequency >= FREQ_BAND_TABLE[i].lower && Frequency < FREQ_BAND_TABLE[i].upper)
						return 0;
				break;
			}
		#endif
	}

	// dis-allowed TX frequency
	return -1;
}

int FREQUENCY_rx_freq_check(const uint32_t Frequency)
{	// return '0' if RX frequency is allowed
	// otherwise return '-1'

	if (Frequency < FREQ_BAND_TABLE[0].lower || Frequency > FREQ_BAND_TABLE[ARRAY_SIZE(FREQ_BAND_TABLE) - 1].upper)
		return -1;

	if (Frequency >= BX4819_BAND1.upper && Frequency < BX4819_BAND2.lower)
		return -1;

	return 0;   // OK frequency
}
