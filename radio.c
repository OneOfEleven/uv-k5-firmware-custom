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

#include <string.h>

#include "app/dtmf.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "audio.h"
#include "bsp/dp32g030/gpio.h"
#include "dcs.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "frequencies.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/menu.h"

vfo_info_t     *g_tx_vfo;
vfo_info_t     *g_rx_vfo;
vfo_info_t     *g_current_vfo;
dcs_code_type_t g_selected_code_type;
dcs_code_type_t g_current_code_type;
uint8_t         g_selected_code;
step_setting_t  g_step_setting;
vfo_state_t     g_vfo_state[2];

bool RADIO_CheckValidChannel(uint16_t Channel, bool bCheckScanList, uint8_t VFO)
{	// return true if the channel appears valid

	uint8_t Attributes;
	uint8_t PriorityCh1;
	uint8_t PriorityCh2;

	if (Channel > USER_CHANNEL_LAST)
		return false;

	Attributes = g_user_channel_attributes[Channel];

	if ((Attributes & USER_CH_BAND_MASK) > BAND7_470MHz)
		return false;

	if (bCheckScanList)
	{
		switch (VFO)
		{
			case 0:
				if ((Attributes & USER_CH_SCANLIST1) == 0)
					return false;

				PriorityCh1 = g_eeprom.scan_list_priority_ch1[0];
				PriorityCh2 = g_eeprom.scan_list_priority_ch2[0];
				break;

			case 1:
				if ((Attributes & USER_CH_SCANLIST2) == 0)
					return false;

				PriorityCh1 = g_eeprom.scan_list_priority_ch1[1];
				PriorityCh2 = g_eeprom.scan_list_priority_ch2[1];
				break;

			default:
				return true;
		}

		if (PriorityCh1 == Channel)
			return false;

		if (PriorityCh2 == Channel)
			return false;
	}

	return true;
}

uint8_t RADIO_FindNextChannel(uint8_t Channel, int8_t Direction, bool bCheckScanList, uint8_t VFO)
{
	unsigned int i;

	for (i = 0; i <= USER_CHANNEL_LAST; i++)
	{
		if (Channel == 0xFF)
			Channel = USER_CHANNEL_LAST;
		else
		if (Channel > USER_CHANNEL_LAST)
			Channel = USER_CHANNEL_FIRST;

		if (RADIO_CheckValidChannel(Channel, bCheckScanList, VFO))
			return Channel;

		Channel += Direction;
	}

	return 0xFF;
}

void RADIO_InitInfo(vfo_info_t *pInfo, const uint8_t ChannelSave, const uint32_t Frequency)
{
	memset(pInfo, 0, sizeof(*pInfo));

	pInfo->band                     = FREQUENCY_GetBand(Frequency);
	pInfo->scanlist_1_participation  = true;
	pInfo->scanlist_2_participation  = true;
	pInfo->step_setting             = STEP_12_5kHz;
	pInfo->step_freq            = STEP_FREQ_TABLE[pInfo->step_setting];
	pInfo->channel_save             = ChannelSave;
	pInfo->frequency_reverse         = false;
	pInfo->output_power             = OUTPUT_POWER_LOW;
	pInfo->freq_config_rx.frequency = Frequency;
	pInfo->freq_config_tx.frequency = Frequency;
	pInfo->pRX                      = &pInfo->freq_config_rx;
	pInfo->pTX                      = &pInfo->freq_config_tx;
	pInfo->compander                = 0;  // off

	if (ChannelSave == (FREQ_CHANNEL_FIRST + BAND2_108MHz))
		pInfo->am_mode = 1;

	RADIO_ConfigureSquelchAndOutputPower(pInfo);
}

void RADIO_ConfigureChannel(const unsigned int VFO, const unsigned int configure)
{
	uint8_t     Channel;
	uint8_t     Attributes;
	uint8_t     Band;
	bool        bParticipation2;
	uint16_t    Base;
	uint32_t    Frequency;
	vfo_info_t *pRadio = &g_eeprom.vfo_info[VFO];

	if (!g_setting_350_enable)
	{
		if (g_eeprom.freq_channel[VFO] == (FREQ_CHANNEL_LAST - 2))
			g_eeprom.freq_channel[VFO] = FREQ_CHANNEL_LAST - 1;

		if (g_eeprom.screen_channel[VFO] == (FREQ_CHANNEL_LAST - 2))
			g_eeprom.screen_channel[VFO] = FREQ_CHANNEL_LAST - 1;
	}

	Channel = g_eeprom.screen_channel[VFO];

	if (IS_VALID_CHANNEL(Channel))
	{
		#ifdef ENABLE_NOAA
			if (Channel >= NOAA_CHANNEL_FIRST)
			{
				RADIO_InitInfo(pRadio, g_eeprom.screen_channel[VFO], NoaaFrequencyTable[Channel - NOAA_CHANNEL_FIRST]);

				if (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF)
					return;

				g_eeprom.cross_vfo_rx_tx = CROSS_BAND_OFF;

				g_update_status = true;
				return;
			}
		#endif

		if (Channel <= USER_CHANNEL_LAST)
		{
			Channel = RADIO_FindNextChannel(Channel, RADIO_CHANNEL_UP, false, VFO);
			if (Channel == 0xFF)
			{
				Channel                      = g_eeprom.freq_channel[VFO];
				g_eeprom.screen_channel[VFO] = g_eeprom.freq_channel[VFO];
			}
			else
			{
				g_eeprom.screen_channel[VFO] = Channel;
				g_eeprom.user_channel[VFO]   = Channel;
			}
		}
	}
	else
		Channel = FREQ_CHANNEL_LAST - 1;

	Attributes = g_user_channel_attributes[Channel];
	if (Attributes == 0xFF)
	{	// invalid/unused channel

		uint8_t Index;

		if (Channel <= USER_CHANNEL_LAST)
		{
			Channel                      = g_eeprom.freq_channel[VFO];
			g_eeprom.screen_channel[VFO] = g_eeprom.freq_channel[VFO];
		}

		Index = Channel - FREQ_CHANNEL_FIRST;

		RADIO_InitInfo(pRadio, Channel, FREQ_BAND_TABLE[Index].lower);
		return;
	}

	Band = Attributes & USER_CH_BAND_MASK;
	if (Band > BAND7_470MHz)
	{
		Band = BAND6_400MHz;
	}

	if (Channel <= USER_CHANNEL_LAST)
	{
		g_eeprom.vfo_info[VFO].band                     = Band;
		g_eeprom.vfo_info[VFO].scanlist_1_participation = (Attributes & USER_CH_SCANLIST1) ? true : false;
		bParticipation2                                 = (Attributes & USER_CH_SCANLIST2) ? true : false;
	}
	else
	{
		Band                                            = Channel - FREQ_CHANNEL_FIRST;
		g_eeprom.vfo_info[VFO].band                     = Band;
		bParticipation2                                 = true;
		g_eeprom.vfo_info[VFO].scanlist_1_participation = true;
	}

	g_eeprom.vfo_info[VFO].scanlist_2_participation = bParticipation2;
	g_eeprom.vfo_info[VFO].channel_save             = Channel;

	if (Channel <= USER_CHANNEL_LAST)
		Base = Channel * 16;
	else
		Base = 0x0C80 + ((Channel - FREQ_CHANNEL_FIRST) * 32) + (VFO * 16);

	if (configure == VFO_CONFIGURE_RELOAD || Channel >= FREQ_CHANNEL_FIRST)
	{
		uint8_t Tmp;
		uint8_t Data[8];

		// ***************

		EEPROM_ReadBuffer(Base + 8, Data, sizeof(Data));

		Tmp = Data[3] & 0x0F;
		if (Tmp > TX_OFFSET_FREQ_DIR_SUB)
			Tmp = 0;
		g_eeprom.vfo_info[VFO].tx_offset_freq_dir = Tmp;
		g_eeprom.vfo_info[VFO].am_mode            = (Data[3] >> 4) & 1u;

		Tmp = Data[6];
		if (Tmp >= ARRAY_SIZE(STEP_FREQ_TABLE))
			Tmp = STEP_12_5kHz;
		g_eeprom.vfo_info[VFO].step_setting  = Tmp;
		g_eeprom.vfo_info[VFO].step_freq     = STEP_FREQ_TABLE[Tmp];

		Tmp = Data[7];
		if (Tmp > (ARRAY_SIZE(g_sub_menu_SCRAMBLER) - 1))
			Tmp = 0;
		g_eeprom.vfo_info[VFO].scrambling_type = Tmp;

		g_eeprom.vfo_info[VFO].freq_config_rx.code_type = (Data[2] >> 0) & 0x0F;
		g_eeprom.vfo_info[VFO].freq_config_tx.code_type = (Data[2] >> 4) & 0x0F;

		Tmp = Data[0];
		switch (g_eeprom.vfo_info[VFO].freq_config_rx.code_type)
		{
			default:
			case CODE_TYPE_OFF:
				g_eeprom.vfo_info[VFO].freq_config_rx.code_type = CODE_TYPE_OFF;
				Tmp = 0;
				break;

			case CODE_TYPE_CONTINUOUS_TONE:
				if (Tmp > (ARRAY_SIZE(CTCSS_OPTIONS) - 1))
					Tmp = 0;
				break;

			case CODE_TYPE_DIGITAL:
			case CODE_TYPE_REVERSE_DIGITAL:
				if (Tmp > (ARRAY_SIZE(DCS_OPTIONS) - 1))
					Tmp = 0;
				break;
		}
		g_eeprom.vfo_info[VFO].freq_config_rx.code = Tmp;

		Tmp = Data[1];
		switch (g_eeprom.vfo_info[VFO].freq_config_tx.code_type)
		{
			default:
			case CODE_TYPE_OFF:
				g_eeprom.vfo_info[VFO].freq_config_tx.code_type = CODE_TYPE_OFF;
				Tmp = 0;
				break;

			case CODE_TYPE_CONTINUOUS_TONE:
				if (Tmp > (ARRAY_SIZE(CTCSS_OPTIONS) - 1))
					Tmp = 0;
				break;

			case CODE_TYPE_DIGITAL:
			case CODE_TYPE_REVERSE_DIGITAL:
				if (Tmp > (ARRAY_SIZE(DCS_OPTIONS) - 1))
					Tmp = 0;
				break;
		}
		g_eeprom.vfo_info[VFO].freq_config_tx.code = Tmp;

		if (Data[4] == 0xFF)
		{
			g_eeprom.vfo_info[VFO].frequency_reverse = false;
			g_eeprom.vfo_info[VFO].channel_bandwidth = BK4819_FILTER_BW_WIDE;
			g_eeprom.vfo_info[VFO].output_power      = OUTPUT_POWER_LOW;
			g_eeprom.vfo_info[VFO].busy_channel_lock = false;
		}
		else
		{
			const uint8_t d4 = Data[4];
			g_eeprom.vfo_info[VFO].frequency_reverse = ((d4 >> 0) & 1u) ? true : false;
			g_eeprom.vfo_info[VFO].channel_bandwidth = ((d4 >> 1) & 1u) ? true : false;
			g_eeprom.vfo_info[VFO].output_power      =  (d4 >> 2) & 3u;
			g_eeprom.vfo_info[VFO].busy_channel_lock = ((d4 >> 4) & 1u) ? true : false;
		}

		if (Data[5] == 0xFF)
		{
			g_eeprom.vfo_info[VFO].dtmf_decoding_enable = false;
			g_eeprom.vfo_info[VFO].dtmf_ptt_id_tx_mode  = PTT_ID_OFF;
		}
		else
		{
			g_eeprom.vfo_info[VFO].dtmf_decoding_enable = ((Data[5] >> 0) & 1u) ? true : false;
			g_eeprom.vfo_info[VFO].dtmf_ptt_id_tx_mode  = ((Data[5] >> 1) & 7u);
		}

		// ***************

		struct
		{
			uint32_t frequency;
			uint32_t offset;
		} __attribute__((packed)) info;

		EEPROM_ReadBuffer(Base, &info, sizeof(info));

		pRadio->freq_config_rx.frequency = info.frequency;

		if (info.offset >= 100000000)
			info.offset = 1000000;
		g_eeprom.vfo_info[VFO].tx_offset_freq = info.offset;

		// ***************
	}

	Frequency = pRadio->freq_config_rx.frequency;

#if 1
	// fix previously set incorrect band
	Band = FREQUENCY_GetBand(Frequency);
#endif

	if (Frequency < FREQ_BAND_TABLE[Band].lower)
		Frequency = FREQ_BAND_TABLE[Band].lower;
	else
	if (Frequency > FREQ_BAND_TABLE[Band].upper)
		Frequency = FREQ_BAND_TABLE[Band].upper;
	else
	if (Channel >= FREQ_CHANNEL_FIRST)
		Frequency = FREQUENCY_FloorToStep(Frequency, g_eeprom.vfo_info[VFO].step_freq, FREQ_BAND_TABLE[Band].lower);

	pRadio->freq_config_rx.frequency = Frequency;

	if (Frequency >= 10800000 && Frequency < 13600000)
		g_eeprom.vfo_info[VFO].tx_offset_freq_dir = TX_OFFSET_FREQ_DIR_OFF;
	else
	if (Channel > USER_CHANNEL_LAST)
		g_eeprom.vfo_info[VFO].tx_offset_freq = FREQUENCY_FloorToStep(g_eeprom.vfo_info[VFO].tx_offset_freq, g_eeprom.vfo_info[VFO].step_freq, 0);

	RADIO_ApplyOffset(pRadio);

	memset(g_eeprom.vfo_info[VFO].name, 0, sizeof(g_eeprom.vfo_info[VFO].name));
	if (Channel < USER_CHANNEL_LAST)
	{	// 16 bytes allocated to the channel name but only 10 used, the rest are 0's
		EEPROM_ReadBuffer(0x0F50 + (Channel * 16), g_eeprom.vfo_info[VFO].name + 0, 8);
		EEPROM_ReadBuffer(0x0F58 + (Channel * 16), g_eeprom.vfo_info[VFO].name + 8, 2);
	}

	if (!g_eeprom.vfo_info[VFO].frequency_reverse)
	{
		g_eeprom.vfo_info[VFO].pRX = &g_eeprom.vfo_info[VFO].freq_config_rx;
		g_eeprom.vfo_info[VFO].pTX = &g_eeprom.vfo_info[VFO].freq_config_tx;
	}
	else
	{
		g_eeprom.vfo_info[VFO].pRX = &g_eeprom.vfo_info[VFO].freq_config_tx;
		g_eeprom.vfo_info[VFO].pTX = &g_eeprom.vfo_info[VFO].freq_config_rx;
	}

	if (!g_setting_350_enable)
	{
		freq_config_t *pConfig = g_eeprom.vfo_info[VFO].pRX;
		if (pConfig->frequency >= 35000000 && pConfig->frequency < 40000000) // not allowed in this range
			pConfig->frequency = 43300000;      // hop onto the ham band
	}

	if (g_eeprom.vfo_info[VFO].am_mode)
	{	// freq/chan is in AM mode
		g_eeprom.vfo_info[VFO].scrambling_type         = 0;
//		g_eeprom.vfo_info[VFO].dtmf_decoding_enable    = false;  // no reason to disable DTMF decoding, aircraft use it on SSB
		g_eeprom.vfo_info[VFO].freq_config_rx.code_type = CODE_TYPE_OFF;
		g_eeprom.vfo_info[VFO].freq_config_tx.code_type = CODE_TYPE_OFF;
	}

	g_eeprom.vfo_info[VFO].compander = (Attributes & USER_CH_COMPAND) >> 4;

	RADIO_ConfigureSquelchAndOutputPower(pRadio);
}

void RADIO_ConfigureSquelchAndOutputPower(vfo_info_t *pInfo)
{
	uint8_t          TX_power[3];
	FREQUENCY_Band_t Band;

	// *******************************
	// squelch

	Band = FREQUENCY_GetBand(pInfo->pRX->frequency);
	uint16_t Base = (Band < BAND4_174MHz) ? 0x1E60 : 0x1E00;

	if (g_eeprom.squelch_level == 0)
	{	// squelch == 0 (off)
		pInfo->squelch_open_rssi_thresh    = 0;     // 0 ~ 255
		pInfo->squelch_open_noise_thresh   = 127;   // 127 ~ 0
		pInfo->squelch_close_glitch_thresh = 255;   // 255 ~ 0

		pInfo->squelch_close_rssi_thresh   = 0;     // 0 ~ 255
		pInfo->squelch_close_noise_thresh  = 127;   // 127 ~ 0
		pInfo->squelch_open_glitch_thresh  = 255;   // 255 ~ 0
	}
	else
	{	// squelch >= 1
		Base += g_eeprom.squelch_level;                                        // my eeprom squelch-1
																			  // VHF   UHF
		EEPROM_ReadBuffer(Base + 0x00, &pInfo->squelch_open_rssi_thresh,    1);  //  50    10
		EEPROM_ReadBuffer(Base + 0x10, &pInfo->squelch_close_rssi_thresh,   1);  //  40     5

		EEPROM_ReadBuffer(Base + 0x20, &pInfo->squelch_open_noise_thresh,   1);  //  65    90
		EEPROM_ReadBuffer(Base + 0x30, &pInfo->squelch_close_noise_thresh,  1);  //  70   100

		EEPROM_ReadBuffer(Base + 0x40, &pInfo->squelch_close_glitch_thresh, 1);  //  90    90
		EEPROM_ReadBuffer(Base + 0x50, &pInfo->squelch_open_glitch_thresh,  1);  // 100   100

		uint16_t rssi_open    = pInfo->squelch_open_rssi_thresh;
		uint16_t rssi_close   = pInfo->squelch_close_rssi_thresh;
		uint16_t noise_open   = pInfo->squelch_open_noise_thresh;
		uint16_t noise_close  = pInfo->squelch_close_noise_thresh;
		uint16_t glitch_open  = pInfo->squelch_open_glitch_thresh;
		uint16_t glitch_close = pInfo->squelch_close_glitch_thresh;

		#if ENABLE_SQUELCH_MORE_SENSITIVE
			// make squelch a little more sensitive
			//
			// getting the best setting here is still experimental, bare with me
			//
			// note that 'noise' and 'glitch' values are inverted compared to 'rssi' values

			#if 0
				rssi_open   = (rssi_open   * 8) / 9;
				noise_open  = (noise_open  * 9) / 8;
				glitch_open = (glitch_open * 9) / 8;
			#else
				// even more sensitive .. use when RX bandwidths are fixed (no weak signal auto adjust)
				rssi_open   = (rssi_open   * 1) / 2;
				noise_open  = (noise_open  * 2) / 1;
				glitch_open = (glitch_open * 2) / 1;
			#endif

		#else
			// more sensitive .. use when RX bandwidths are fixed (no weak signal auto adjust)
			rssi_open   = (rssi_open   * 3) / 4;
			noise_open  = (noise_open  * 4) / 3;
			glitch_open = (glitch_open * 4) / 3;
		#endif

		rssi_close   = (rssi_open   *  9) / 10;
		noise_close  = (noise_open  * 10) / 9;
		glitch_close = (glitch_open * 10) / 9;

		// ensure the 'close' threshold is lower than the 'open' threshold
		if (rssi_close   == rssi_open   && rssi_close   >= 2)
			rssi_close -= 2;
		if (noise_close  == noise_open  && noise_close  <= 125)
			noise_close += 2;
		if (glitch_close == glitch_open && glitch_close <= 253)
			glitch_close += 2;

		pInfo->squelch_open_rssi_thresh    = (rssi_open    > 255) ? 255 : rssi_open;
		pInfo->squelch_close_rssi_thresh   = (rssi_close   > 255) ? 255 : rssi_close;
		pInfo->squelch_open_noise_thresh   = (noise_open   > 127) ? 127 : noise_open;
		pInfo->squelch_close_noise_thresh  = (noise_close  > 127) ? 127 : noise_close;
		pInfo->squelch_open_glitch_thresh  = (glitch_open  > 255) ? 255 : glitch_open;
		pInfo->squelch_close_glitch_thresh = (glitch_close > 255) ? 255 : glitch_close;
	}

	// *******************************
	// output power

	// my calibration data
	//
	// 1ED0    32 32 32   64 64 64   8C 8C 8C   FF FF FF FF FF FF FF ..  50 MHz 
	// 1EE0    32 32 32   64 64 64   8C 8C 8C   FF FF FF FF FF FF FF .. 108 MHz
	// 1EF0    5F 5F 5F   69 69 69   91 91 8F   FF FF FF FF FF FF FF .. 136 MHz
	// 1F00    32 32 32   64 64 64   8C 8C 8C   FF FF FF FF FF FF FF .. 174 MHz
	// 1F10    5A 5A 5A   64 64 64   82 82 82   FF FF FF FF FF FF FF .. 350 MHz
	// 1F20    5A 5A 5A   64 64 64   8F 91 8A   FF FF FF FF FF FF FF .. 400 MHz
	// 1F30    32 32 32   64 64 64   8C 8C 8C   FF FF FF FF FF FF FF .. 470 MHz

	Band = FREQUENCY_GetBand(pInfo->pTX->frequency);

	EEPROM_ReadBuffer(0x1ED0 + (Band * 16) + (pInfo->output_power * 3), TX_power, 3);

	pInfo->txp_calculated_setting = FREQUENCY_CalculateOutputPower(
		TX_power[0],
		TX_power[1],
		TX_power[2],
		 FREQ_BAND_TABLE[Band].lower,
		(FREQ_BAND_TABLE[Band].lower + FREQ_BAND_TABLE[Band].upper) / 2,
		 FREQ_BAND_TABLE[Band].upper,
		pInfo->pTX->frequency);

	// *******************************
}

void RADIO_ApplyOffset(vfo_info_t *pInfo)
{
	uint32_t Frequency = pInfo->freq_config_rx.frequency;

	switch (pInfo->tx_offset_freq_dir)
	{
		case TX_OFFSET_FREQ_DIR_OFF:
			break;
		case TX_OFFSET_FREQ_DIR_ADD:
			Frequency += pInfo->tx_offset_freq;
			break;
		case TX_OFFSET_FREQ_DIR_SUB:
			Frequency -= pInfo->tx_offset_freq;
			break;
	}

	if (Frequency < FREQ_BAND_TABLE[0].lower)
		Frequency = FREQ_BAND_TABLE[0].lower;
	else
	if (Frequency > FREQ_BAND_TABLE[ARRAY_SIZE(FREQ_BAND_TABLE) - 1].upper)
		Frequency = FREQ_BAND_TABLE[ARRAY_SIZE(FREQ_BAND_TABLE) - 1].upper;

	pInfo->freq_config_tx.frequency = Frequency;
}

static void RADIO_SelectCurrentVfo(void)
{
 	g_current_vfo = (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF) ? g_rx_vfo : &g_eeprom.vfo_info[g_eeprom.tx_vfo];
}

void RADIO_SelectVfos(void)
{
	g_eeprom.tx_vfo = get_TX_VFO();
	g_eeprom.rx_vfo = (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF) ? g_eeprom.tx_vfo : (g_eeprom.tx_vfo + 1) & 1u;

	g_tx_vfo = &g_eeprom.vfo_info[g_eeprom.tx_vfo];
	g_rx_vfo = &g_eeprom.vfo_info[g_eeprom.rx_vfo];

	RADIO_SelectCurrentVfo();
}

void RADIO_SetupRegisters(bool bSwitchToFunction0)
{
	BK4819_filter_bandwidth_t Bandwidth = g_rx_vfo->channel_bandwidth;
	uint16_t                 InterruptMask;
	uint32_t                 Frequency;

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

	g_enable_speaker = false;

	BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_GREEN, false);

	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

	switch (Bandwidth)
	{
		default:
			Bandwidth = BK4819_FILTER_BW_WIDE;
		case BK4819_FILTER_BW_WIDE:
		case BK4819_FILTER_BW_NARROW:
			#ifdef ENABLE_AM_FIX
//				BK4819_SetFilterBandwidth(Bandwidth, g_rx_vfo->am_mode && g_setting_am_fix);
				BK4819_SetFilterBandwidth(Bandwidth, true);
			#else
				BK4819_SetFilterBandwidth(Bandwidth, false);
			#endif
			break;
	}

	#pragma GCC diagnostic pop

	BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_RED, false);

	BK4819_SetupPowerAmplifier(0, 0);

	BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1, false);

	while (1)
	{
		const uint16_t Status = BK4819_ReadRegister(BK4819_REG_0C);
		if ((Status & 1u) == 0) // INTERRUPT REQUEST
			break;

		BK4819_WriteRegister(BK4819_REG_02, 0);
		SYSTEM_DelayMs(1);
	}
	BK4819_WriteRegister(BK4819_REG_3F, 0);

	// mic gain 0.5dB/step 0 to 31
	BK4819_WriteRegister(BK4819_REG_7D, 0xE940 | (g_eeprom.mic_sensitivity_tuning & 0x1f));

	#ifdef ENABLE_NOAA
		if (IS_NOT_NOAA_CHANNEL(g_rx_vfo->channel_save) || !g_is_noaa_mode)
			Frequency = g_rx_vfo->pRX->frequency;
		else
			Frequency = NoaaFrequencyTable[g_noaa_channel];
	#else
		Frequency = g_rx_vfo->pRX->frequency;
	#endif
	BK4819_SetFrequency(Frequency);

	BK4819_SetupSquelch(
		g_rx_vfo->squelch_open_rssi_thresh,    g_rx_vfo->squelch_close_rssi_thresh,
		g_rx_vfo->squelch_open_noise_thresh,   g_rx_vfo->squelch_close_noise_thresh,
		g_rx_vfo->squelch_close_glitch_thresh, g_rx_vfo->squelch_open_glitch_thresh);

	BK4819_PickRXFilterPathBasedOnFrequency(Frequency);

	// what does this in do ?
	BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2, true);

	// AF RX Gain and DAC
	BK4819_WriteRegister(BK4819_REG_48, 0xB3A8);  // 1011 00 111010 1000

	InterruptMask = BK4819_REG_3F_SQUELCH_FOUND | BK4819_REG_3F_SQUELCH_LOST;

	#ifdef ENABLE_NOAA
		if (IS_NOT_NOAA_CHANNEL(g_rx_vfo->channel_save))
	#endif
	{
		if (g_rx_vfo->am_mode == 0)
		{	// FM
			uint8_t code_type = g_selected_code_type;
			uint8_t Code     = g_selected_code;
			if (g_css_scan_mode == CSS_SCAN_MODE_OFF)
			{
				code_type = g_rx_vfo->pRX->code_type;
				Code     = g_rx_vfo->pRX->code;
			}

			switch (code_type)
			{
				default:
				case CODE_TYPE_OFF:
					BK4819_SetCTCSSFrequency(670);

					//#ifndef ENABLE_CTCSS_TAIL_PHASE_SHIFT
						BK4819_SetTailDetection(550);		// QS's 55Hz tone method
					//#else
					//	BK4819_SetTailDetection(670);       // 67Hz
					//#endif

					InterruptMask = BK4819_REG_3F_CxCSS_TAIL | BK4819_REG_3F_SQUELCH_FOUND | BK4819_REG_3F_SQUELCH_LOST;
					break;

				case CODE_TYPE_CONTINUOUS_TONE:
					BK4819_SetCTCSSFrequency(CTCSS_OPTIONS[Code]);

					//#ifndef ENABLE_CTCSS_TAIL_PHASE_SHIFT
						BK4819_SetTailDetection(550);		// QS's 55Hz tone method
					//#else
					//	BK4819_SetTailDetection(CTCSS_OPTIONS[Code]);
					//#endif

					InterruptMask = 0
						| BK4819_REG_3F_CxCSS_TAIL
						| BK4819_REG_3F_CTCSS_FOUND
						| BK4819_REG_3F_CTCSS_LOST
						| BK4819_REG_3F_SQUELCH_FOUND
						| BK4819_REG_3F_SQUELCH_LOST;

					break;

				case CODE_TYPE_DIGITAL:
				case CODE_TYPE_REVERSE_DIGITAL:
					BK4819_SetCDCSSCodeWord(DCS_GetGolayCodeWord(code_type, Code));
					InterruptMask = 0
						| BK4819_REG_3F_CxCSS_TAIL
						| BK4819_REG_3F_CDCSS_FOUND
						| BK4819_REG_3F_CDCSS_LOST
						| BK4819_REG_3F_SQUELCH_FOUND
						| BK4819_REG_3F_SQUELCH_LOST;
					break;
			}

			if (g_rx_vfo->scrambling_type > 0 && g_setting_scramble_enable)
				BK4819_EnableScramble(g_rx_vfo->scrambling_type - 1);
			else
				BK4819_DisableScramble();
		}
	}
	#ifdef ENABLE_NOAA
		else
		{
			BK4819_SetCTCSSFrequency(2625);
			InterruptMask = 0
				| BK4819_REG_3F_CTCSS_FOUND
				| BK4819_REG_3F_CTCSS_LOST
				| BK4819_REG_3F_SQUELCH_FOUND
				| BK4819_REG_3F_SQUELCH_LOST;
		}
	#endif

	#ifdef ENABLE_VOX
		#ifdef ENABLE_NOAA
			#ifdef ENABLE_FMRADIO
				if (g_eeprom.vox_switch && !g_fm_radio_mode && IS_NOT_NOAA_CHANNEL(g_current_vfo->channel_save) && g_current_vfo->am_mode == 0)
			#else
				if (g_eeprom.vox_switch && IS_NOT_NOAA_CHANNEL(g_current_vfo->channel_save) && g_current_vfo->am_mode == 0)
			#endif
		#else
			#ifdef ENABLE_FMRADIO
				if (g_eeprom.vox_switch && !g_fm_radio_mode && g_current_vfo->am_mode == 0)
			#else
				if (g_eeprom.vox_switch && g_current_vfo->am_mode == 0)
			#endif
		#endif
		{
			BK4819_EnableVox(g_eeprom.vox1_threshold, g_eeprom.vox0_threshold);
			InterruptMask |= BK4819_REG_3F_VOX_FOUND | BK4819_REG_3F_VOX_LOST;
		}
		else
	#endif
		BK4819_DisableVox();

	// RX expander
	BK4819_SetCompander((g_rx_vfo->am_mode == 0 && g_rx_vfo->compander >= 2) ? g_rx_vfo->compander : 0);

	#if 0
		if (!g_rx_vfo->dtmf_decoding_enable && !g_setting_killed)
		{
			BK4819_DisableDTMF();
		}
		else
		{
			BK4819_EnableDTMF();
			InterruptMask |= BK4819_REG_3F_DTMF_5TONE_FOUND;
		}
	#else
		if (g_current_function != FUNCTION_TRANSMIT)
		{
			BK4819_DisableDTMF();
			BK4819_EnableDTMF();
			InterruptMask |= BK4819_REG_3F_DTMF_5TONE_FOUND;
		}
		else
		{
			BK4819_DisableDTMF();
		}
	#endif

	// enable/disable BK4819 selected interrupts
	BK4819_WriteRegister(BK4819_REG_3F, InterruptMask);

	FUNCTION_Init();

	if (bSwitchToFunction0)
		FUNCTION_Select(FUNCTION_FOREGROUND);
}

#ifdef ENABLE_NOAA
	void RADIO_ConfigureNOAA(void)
	{
		uint8_t ChanAB;

		g_update_status = true;

		if (g_eeprom.noaa_auto_scan)
		{
			if (g_eeprom.dual_watch != DUAL_WATCH_OFF)
			{
				if (IS_NOT_NOAA_CHANNEL(g_eeprom.screen_channel[0]))
				{
					if (IS_NOT_NOAA_CHANNEL(g_eeprom.screen_channel[1]))
					{
						g_is_noaa_mode = false;
						return;
					}
					ChanAB = 1;
				}
				else
					ChanAB = 0;

				if (!g_is_noaa_mode)
					g_noaa_channel = g_eeprom.vfo_info[ChanAB].channel_save - NOAA_CHANNEL_FIRST;

				g_is_noaa_mode = true;
				return;
			}

			if (g_rx_vfo->channel_save >= NOAA_CHANNEL_FIRST)
			{
				g_is_noaa_mode          = true;
				g_noaa_channel         = g_rx_vfo->channel_save - NOAA_CHANNEL_FIRST;
				g_noaa_count_down_10ms = noaa_count_down_2_10ms;
				g_schedule_noaa        = false;
			}
			else
				g_is_noaa_mode = false;
		}
		else
			g_is_noaa_mode = false;
	}
#endif

void RADIO_SetTxParameters(void)
{
	BK4819_filter_bandwidth_t Bandwidth = g_current_vfo->channel_bandwidth;

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

	g_enable_speaker = false;

	BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2, false);

	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

	switch (Bandwidth)
	{
		default:
			Bandwidth = BK4819_FILTER_BW_WIDE;
		case BK4819_FILTER_BW_WIDE:
		case BK4819_FILTER_BW_NARROW:
			#ifdef ENABLE_AM_FIX
//				BK4819_SetFilterBandwidth(Bandwidth, g_current_vfo->am_mode && g_setting_am_fix);
				BK4819_SetFilterBandwidth(Bandwidth, true);
			#else
				BK4819_SetFilterBandwidth(Bandwidth, false);
			#endif
			break;
	}

	#pragma GCC diagnostic pop

	BK4819_SetFrequency(g_current_vfo->pTX->frequency);

	// TX compressor
	BK4819_SetCompander((g_rx_vfo->am_mode == 0 && (g_rx_vfo->compander == 1 || g_rx_vfo->compander >= 3)) ? g_rx_vfo->compander : 0);

	BK4819_PrepareTransmit();

	SYSTEM_DelayMs(10);

	BK4819_PickRXFilterPathBasedOnFrequency(g_current_vfo->pTX->frequency);

	BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1, true);

	SYSTEM_DelayMs(5);

	BK4819_SetupPowerAmplifier(g_current_vfo->txp_calculated_setting, g_current_vfo->pTX->frequency);

	SYSTEM_DelayMs(10);

	switch (g_current_vfo->pTX->code_type)
	{
		default:
		case CODE_TYPE_OFF:
			BK4819_ExitSubAu();
			break;

		case CODE_TYPE_CONTINUOUS_TONE:
			BK4819_SetCTCSSFrequency(CTCSS_OPTIONS[g_current_vfo->pTX->code]);
			break;

		case CODE_TYPE_DIGITAL:
		case CODE_TYPE_REVERSE_DIGITAL:
			BK4819_SetCDCSSCodeWord(DCS_GetGolayCodeWord(g_current_vfo->pTX->code_type, g_current_vfo->pTX->code));
			break;
	}
}

void RADIO_Setg_vfo_state(vfo_state_t State)
{
	if (State == VFO_STATE_NORMAL)
	{
		g_vfo_state[0] = VFO_STATE_NORMAL;
		g_vfo_state[1] = VFO_STATE_NORMAL;

		#ifdef ENABLE_FMRADIO
			g_fm_resume_count_down_500ms = 0;
		#endif
	}
	else
	{
		if (State == VFO_STATE_VOLTAGE_HIGH)
		{
			g_vfo_state[0] = VFO_STATE_VOLTAGE_HIGH;
			g_vfo_state[1] = VFO_STATE_TX_DISABLE;
		}
		else
		{	// 1of11
			const unsigned int vfo = (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF) ? g_eeprom.rx_vfo : g_eeprom.tx_vfo;
			g_vfo_state[vfo] = State;
		}

		#ifdef ENABLE_FMRADIO
			g_fm_resume_count_down_500ms = fm_resume_countdown_500ms;
		#endif
	}

	g_update_display = true;
}

void RADIO_PrepareTX(void)
{
	vfo_state_t State = VFO_STATE_NORMAL;  // default to OK to TX

	if (g_eeprom.dual_watch != DUAL_WATCH_OFF)
	{	// dual-RX is enabled

		g_dual_watch_count_down_10ms = dual_watch_count_after_tx_10ms;
		g_schedule_dual_watch       = false;

#if 0
		if (g_rx_vfo_is_active)
		{	// use the TX vfo
			g_eeprom.rx_vfo = g_eeprom.tx_vfo;
			g_rx_vfo         = &g_eeprom.vfo_info[g_eeprom.tx_vfo];
			g_rx_vfo_is_active = false;
		}
		g_current_vfo = g_rx_vfo;
#else
		if (!g_rx_vfo_is_active)
		{	// use the current RX vfo
			g_eeprom.rx_vfo = g_eeprom.tx_vfo;
			g_rx_vfo         = &g_eeprom.vfo_info[g_eeprom.tx_vfo];
			g_rx_vfo_is_active = true;
		}
		g_current_vfo = g_rx_vfo;
#endif
	
		// let the user see that DW is not active '><' symbol
		g_dual_watch_active = false;
		g_update_status    = true;
	}

	RADIO_SelectCurrentVfo();

	#ifndef ENABLE_TX_WHEN_AM
		if (g_current_vfo->am_mode)
		{	// not allowed to TX if in AM mode
			State = VFO_STATE_TX_DISABLE;
		}
		else
	#endif
	if (!g_setting_tx_enable || g_serial_config_count_down_500ms > 0)
	{	// TX is disabled or config upload/download in progress
		State = VFO_STATE_TX_DISABLE;
	}
	else
	if (TX_freq_check(g_current_vfo->pTX->frequency) == 0)
	{	// TX frequency is allowed
		if (g_current_vfo->busy_channel_lock && g_current_function == FUNCTION_RECEIVE)
			State = VFO_STATE_BUSY;          // busy RX'ing a station
		else
		if (g_battery_display_level == 0)
			State = VFO_STATE_BAT_LOW;       // charge your battery !
		else
		if (g_battery_display_level >= 6)
			State = VFO_STATE_VOLTAGE_HIGH;  // over voltage (no doubt to protect the PA) .. this is being a pain
	}
	else
		State = VFO_STATE_TX_DISABLE;        // TX frequency not allowed

	if (State != VFO_STATE_NORMAL)
	{	// TX not allowed

		RADIO_Setg_vfo_state(State);

		#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
			g_alarm_state = ALARM_STATE_OFF;
		#endif

		g_dtmf_reply_state = DTMF_REPLY_NONE;

		AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
		return;
	}

	// TX is allowed

	if (g_dtmf_reply_state == DTMF_REPLY_ANI)
	{
		if (g_dtmf_call_mode == DTMF_CALL_MODE_DTMF)
		{
			g_dtmf_is_tx                  = true;
			g_dtmf_call_state             = DTMF_CALL_STATE_NONE;
			g_dtmf_tx_stop_count_down_500ms = dtmf_txstop_countdown_500ms;
		}
		else
		{
			g_dtmf_call_state = DTMF_CALL_STATE_CALL_OUT;
			g_dtmf_is_tx      = false;
		}
	}

	FUNCTION_Select(FUNCTION_TRANSMIT);

	g_tx_timer_count_down_500ms = 0;            // no timeout

	#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
		if (g_alarm_state == ALARM_STATE_OFF)
	#endif
	{
		if (g_eeprom.tx_timeout_timer == 0)
			g_tx_timer_count_down_500ms = 60;   // 30 sec
		else
		if (g_eeprom.tx_timeout_timer < (ARRAY_SIZE(g_sub_menu_TOT) - 1))
			g_tx_timer_count_down_500ms = 120 * g_eeprom.tx_timeout_timer;  // minutes
		else
			g_tx_timer_count_down_500ms = 120 * 15;  // 15 minutes
	}
	g_tx_timeout_reached    = false;

	g_flag_end_tx = false;
	g_rtte_count_down       = 0;
	g_dtmf_reply_state     = DTMF_REPLY_NONE;
}

void RADIO_EnableCxCSS(void)
{
	switch (g_current_vfo->pTX->code_type)
	{
		default:
		case CODE_TYPE_OFF:
			break;

		case CODE_TYPE_CONTINUOUS_TONE:
			BK4819_EnableCTCSS();
			SYSTEM_DelayMs(200);
			break;

		case CODE_TYPE_DIGITAL:
		case CODE_TYPE_REVERSE_DIGITAL:
			BK4819_EnableCDCSS();
			SYSTEM_DelayMs(200);
			break;
	}
}

void RADIO_PrepareCssTX(void)
{
	RADIO_PrepareTX();

	SYSTEM_DelayMs(200);

	RADIO_EnableCxCSS();
	RADIO_SetupRegisters(true);
}

void RADIO_SendEndOfTransmission(void)
{
	if (g_eeprom.roger_mode == ROGER_MODE_ROGER) {
		#if defined(ENABLE_QUINDAR)
			BK4819_PlaySingleTone(2475, 250, true);
		#else
			BK4819_PlayRoger();
		#endif
	}
	else {
		if (g_eeprom.roger_mode == ROGER_MODE_MDC)
			BK4819_PlayRogerMDC();
	}

	if (g_current_vfo->dtmf_ptt_id_tx_mode == PTT_ID_APOLLO)
		BK4819_PlaySingleTone(2475, 250, 28, g_eeprom.dtmf_side_tone);

	if (g_dtmf_call_state == DTMF_CALL_STATE_NONE &&
	   (g_current_vfo->dtmf_ptt_id_tx_mode == PTT_ID_TX_DOWN || g_current_vfo->dtmf_ptt_id_tx_mode == PTT_ID_BOTH))
	{	// end-of-tx
		if (g_eeprom.dtmf_side_tone)
		{
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
			g_enable_speaker = true;
			SYSTEM_DelayMs(60);
		}

		BK4819_EnterDTMF_TX(g_eeprom.dtmf_side_tone);

		BK4819_PlayDTMFString(
				g_eeprom.dtmf_down_code,
				0,
				g_eeprom.dtmf_first_code_persist_time,
				g_eeprom.dtmf_hash_code_persist_time,
				g_eeprom.dtmf_code_persist_time,
				g_eeprom.dtmf_code_interval_time);

		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
		g_enable_speaker = false;
	}

	BK4819_ExitDTMF_TX(true);
}
