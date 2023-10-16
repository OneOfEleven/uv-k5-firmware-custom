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

#include "frequencies.h"
#include "misc.h"
#include "settings.h"

// the default AIRCOPY frequency to use
uint32_t g_aircopy_freq = 41002500;

// FM broadcast band lower/upper limit
const freq_band_table_t FM_RADIO_BAND = {760, 1080};

// the BK4819 has 2 bands it covers, 18MHz ~ 630MHz and 760MHz ~ 1300MHz
const freq_band_table_t BX4819_BAND1 = { 1800000,  63000000};
const freq_band_table_t BX4819_BAND2 = {84000000, 130000000};

const freq_band_table_t FREQ_BAND_TABLE[7] =
{
	#ifdef ENABLE_WIDE_RX
		// extended range
		{ 1800000,  10800000},  // band 1
		{10800000,  13600000},  // band 2
		{13600000,  17400000},  // band 3
		{17400000,  35000000},  // band 4
		{35000000,  40000000},  // band 5
		{40000000,  47000000},  // band 6
		{47000000, 130000000}   // band 7
	#else
		// QS original
		{ 5000000,   7600000},  // band 1
		{10800000,  13600000},  // band 2
		{13600000,  17400000},  // band 3
		{17400000,  35000000},  // band 4
		{35000000,  40000000},  // band 5 
		{40000000,  47000000},  // band 6
		{47000000,  60000000}   // band 7
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

#ifdef ENABLE_1250HZ_STEP
	// includes 1.25kHz step
	const uint16_t STEP_FREQ_TABLE[7] = {125, 250, 625, 1000, 1250, 2500, 833};
#else
	// QS steps (*10 Hz)
	const uint16_t STEP_FREQ_TABLE[7] = {250, 500, 625, 1000, 1250, 2500, 833};
#endif

frequency_band_t FREQUENCY_GetBand(uint32_t Frequency)
{
	int band;
	for (band = ARRAY_SIZE(FREQ_BAND_TABLE) - 1; band >= 0; band--)
		if (Frequency >= FREQ_BAND_TABLE[band].lower)
//		if (Frequency <  FREQ_BAND_TABLE[band].upper)
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

uint32_t FREQUENCY_FloorToStep(uint32_t Upper, uint32_t Step, uint32_t Lower)
{
	#if 1
		uint32_t Index;
	
		if (Step == 833)
		{
			const uint32_t Delta = Upper - Lower;
			uint32_t       Base  = (Delta / 2500) * 2500;
			const uint32_t Index = ((Delta - Base) % 2500) / 833;
	
			if (Index == 2)
				Base++;
	
			return Lower + Base + (Index * 833);
		}
	
		Index = (Upper - Lower) / Step;
	
		return Lower + (Step * Index);
	#else
		return Lower + (((Upper - Lower) / Step) * Step);
	#endif
}

int TX_freq_check(const uint32_t Frequency)
{	// return '0' if TX frequency is allowed
	// otherwise return '-1'

	if (Frequency < BX4819_BAND1.lower || Frequency > BX4819_BAND2.upper)
		return -1;  // BX radio chip does not work out this range

	if (Frequency >= BX4819_BAND1.upper && Frequency < BX4819_BAND2.lower)
		return -1;  // BX radio chip does not work in this range

	if (Frequency >= 10800000 && Frequency < 13600000)
		return -1;  // TX not allowed in the airband

	if (Frequency < FREQ_BAND_TABLE[0].lower || Frequency > FREQ_BAND_TABLE[ARRAY_SIZE(FREQ_BAND_TABLE) - 1].upper)
		return -1;  // TX not allowed outside this range

	switch (g_setting_freq_lock)
	{
		case FREQ_LOCK_NORMAL:
			if (Frequency >= 13600000 && Frequency < 17400000) 	//Frequency Between 136-174 Mhz
				return 0;
			if (Frequency >= 17400000 && Frequency < 35000000) 	//Frequency Between 174-350 Mhz + 174 TX Enabled
				if (g_setting_174_tx_enable)
					return 0;
			if (Frequency >= 35000000 && Frequency < 40000000) 	//Frequency Between 350-400 Mhz + 350 TX Enabled
				if (g_setting_350_tx_enable && g_setting_350_enable)
					return 0;
			if (Frequency >= 40000000 && Frequency < 47000000) 	//Frequency Between 400-470 Mhz
				return 0;
			if (Frequency >= 47000000 && Frequency <= 60000000)	//Frequency Between 470-600 Mhz + 470 TX Enabled
				if (g_setting_470_tx_enable)
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
			if (Frequency >= 13600000 && Frequency < 17400000)
				return 0;
			if (Frequency >= 40000000 && Frequency < 43000000)
				return 0;
			break;

		case FREQ_LOCK_438:
			if (Frequency >= 13600000 && Frequency < 17400000)
				return 0;
			if (Frequency >= 40000000 && Frequency < 43800000)
				return 0;
			break;
			
		#ifdef ENABLE_TX_UNLOCK
			case FREQ_LOCK_TX_UNLOCK:
				return 0;
		#endif
	}

	// dis-allowed TX frequency
	return -1;
}

int RX_freq_check(const uint32_t Frequency)
{	// return '0' if RX frequency is allowed
	// otherwise return '-1'

	if (Frequency < FREQ_BAND_TABLE[0].lower || Frequency > FREQ_BAND_TABLE[ARRAY_SIZE(FREQ_BAND_TABLE) - 1].upper)
		return -1;

	if (Frequency >= BX4819_BAND1.upper && Frequency < BX4819_BAND2.lower)
		return -1;

	return 0;   // OK frequency
}
