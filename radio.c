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

VFO_Info_t     *gTxVfo;
VFO_Info_t     *gRxVfo;
VFO_Info_t     *gCurrentVfo;
dcs_code_type_t gSelectedcode_type;
dcs_code_type_t gCurrentcode_type;
uint8_t         gSelectedCode;
step_setting_t  gStepSetting;
VfoState_t      VfoState[2];

bool RADIO_CheckValidChannel(uint16_t Channel, bool bCheckScanList, uint8_t VFO)
{	// return true if the channel appears valid

	uint8_t Attributes;
	uint8_t PriorityCh1;
	uint8_t PriorityCh2;

	if (Channel > USER_CHANNEL_LAST)
		return false;

	Attributes = gUSER_ChannelAttributes[Channel];

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

void RADIO_InitInfo(VFO_Info_t *pInfo, const uint8_t ChannelSave, const uint32_t Frequency)
{
	memset(pInfo, 0, sizeof(*pInfo));

	pInfo->band                     = FREQUENCY_GetBand(Frequency);
	pInfo->scanlist_1_participation  = true;
	pInfo->scanlist_2_participation  = true;
	pInfo->step_setting             = STEP_12_5kHz;
	pInfo->step_freq            = StepFrequencyTable[pInfo->step_setting];
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
	VFO_Info_t *pRadio = &g_eeprom.VfoInfo[VFO];

	if (!gSetting_350EN)
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

				gUpdateStatus = true;
				return;
			}
		#endif

		if (Channel <= USER_CHANNEL_LAST)
		{
			Channel = RADIO_FindNextChannel(Channel, RADIO_CHANNEL_UP, false, VFO);
			if (Channel == 0xFF)
			{
				Channel                    = g_eeprom.freq_channel[VFO];
				g_eeprom.screen_channel[VFO] = g_eeprom.freq_channel[VFO];
			}
			else
			{
				g_eeprom.screen_channel[VFO] = Channel;
				g_eeprom.user_channel[VFO]     = Channel;
			}
		}
	}
	else
		Channel = FREQ_CHANNEL_LAST - 1;

	Attributes = gUSER_ChannelAttributes[Channel];
	if (Attributes == 0xFF)
	{	// invalid/unused channel

		uint8_t Index;

		if (Channel <= USER_CHANNEL_LAST)
		{
			Channel                    = g_eeprom.freq_channel[VFO];
			g_eeprom.screen_channel[VFO] = g_eeprom.freq_channel[VFO];
		}

		Index = Channel - FREQ_CHANNEL_FIRST;

		RADIO_InitInfo(pRadio, Channel, frequencyBandTable[Index].lower);
		return;
	}

	Band = Attributes & USER_CH_BAND_MASK;
	if (Band > BAND7_470MHz)
	{
		Band = BAND6_400MHz;
	}

	if (Channel <= USER_CHANNEL_LAST)
	{
		g_eeprom.VfoInfo[VFO].band                    = Band;
		g_eeprom.VfoInfo[VFO].scanlist_1_participation = !!(Attributes & USER_CH_SCANLIST1);
		bParticipation2                              = !!(Attributes & USER_CH_SCANLIST2);
	}
	else
	{
		Band                                         = Channel - FREQ_CHANNEL_FIRST;
		g_eeprom.VfoInfo[VFO].band                    = Band;
		bParticipation2                              = true;
		g_eeprom.VfoInfo[VFO].scanlist_1_participation = true;
	}

	g_eeprom.VfoInfo[VFO].scanlist_2_participation = bParticipation2;
	g_eeprom.VfoInfo[VFO].channel_save            = Channel;

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
		g_eeprom.VfoInfo[VFO].tx_offset_freq_dir = Tmp;
		g_eeprom.VfoInfo[VFO].am_mode = (Data[3] >> 4) & 1u;

		Tmp = Data[6];
		if (Tmp >= ARRAY_SIZE(StepFrequencyTable))
			Tmp = STEP_12_5kHz;
		g_eeprom.VfoInfo[VFO].step_setting  = Tmp;
		g_eeprom.VfoInfo[VFO].step_freq = StepFrequencyTable[Tmp];

		Tmp = Data[7];
		if (Tmp > (ARRAY_SIZE(gSubMenu_SCRAMBLER) - 1))
			Tmp = 0;
		g_eeprom.VfoInfo[VFO].scrambling_type = Tmp;

		g_eeprom.VfoInfo[VFO].freq_config_rx.code_type = (Data[2] >> 0) & 0x0F;
		g_eeprom.VfoInfo[VFO].freq_config_tx.code_type = (Data[2] >> 4) & 0x0F;

		Tmp = Data[0];
		switch (g_eeprom.VfoInfo[VFO].freq_config_rx.code_type)
		{
			default:
			case CODE_TYPE_OFF:
				g_eeprom.VfoInfo[VFO].freq_config_rx.code_type = CODE_TYPE_OFF;
				Tmp = 0;
				break;

			case CODE_TYPE_CONTINUOUS_TONE:
				if (Tmp > (ARRAY_SIZE(CTCSS_Options) - 1))
					Tmp = 0;
				break;

			case CODE_TYPE_DIGITAL:
			case CODE_TYPE_REVERSE_DIGITAL:
				if (Tmp > (ARRAY_SIZE(DCS_Options) - 1))
					Tmp = 0;
				break;
		}
		g_eeprom.VfoInfo[VFO].freq_config_rx.code = Tmp;

		Tmp = Data[1];
		switch (g_eeprom.VfoInfo[VFO].freq_config_tx.code_type)
		{
			default:
			case CODE_TYPE_OFF:
				g_eeprom.VfoInfo[VFO].freq_config_tx.code_type = CODE_TYPE_OFF;
				Tmp = 0;
				break;

			case CODE_TYPE_CONTINUOUS_TONE:
				if (Tmp > (ARRAY_SIZE(CTCSS_Options) - 1))
					Tmp = 0;
				break;

			case CODE_TYPE_DIGITAL:
			case CODE_TYPE_REVERSE_DIGITAL:
				if (Tmp > (ARRAY_SIZE(DCS_Options) - 1))
					Tmp = 0;
				break;
		}
		g_eeprom.VfoInfo[VFO].freq_config_tx.code = Tmp;

		if (Data[4] == 0xFF)
		{
			g_eeprom.VfoInfo[VFO].frequency_reverse  = false;
			g_eeprom.VfoInfo[VFO].channel_bandwidth = BK4819_FILTER_BW_WIDE;
			g_eeprom.VfoInfo[VFO].output_power      = OUTPUT_POWER_LOW;
			g_eeprom.VfoInfo[VFO].busy_channel_lock = false;
		}
		else
		{
			const uint8_t d4 = Data[4];
			g_eeprom.VfoInfo[VFO].frequency_reverse  = !!((d4 >> 0) & 1u);
			g_eeprom.VfoInfo[VFO].channel_bandwidth = !!((d4 >> 1) & 1u);
			g_eeprom.VfoInfo[VFO].output_power      =   ((d4 >> 2) & 3u);
			g_eeprom.VfoInfo[VFO].busy_channel_lock = !!((d4 >> 4) & 1u);
		}

		if (Data[5] == 0xFF)
		{
			g_eeprom.VfoInfo[VFO].DTMF_decoding_enable = false;
			g_eeprom.VfoInfo[VFO].DTMF_ptt_id_tx_mode  = PTT_ID_OFF;
		}
		else
		{
			g_eeprom.VfoInfo[VFO].DTMF_decoding_enable = ((Data[5] >> 0) & 1u) ? true : false;
			g_eeprom.VfoInfo[VFO].DTMF_ptt_id_tx_mode  = ((Data[5] >> 1) & 7u);
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
		g_eeprom.VfoInfo[VFO].tx_offset_freq = info.offset;

		// ***************
	}

	Frequency = pRadio->freq_config_rx.frequency;

#if 1
	// fix previously set incorrect band
	Band = FREQUENCY_GetBand(Frequency);
#endif

	if (Frequency < frequencyBandTable[Band].lower)
		Frequency = frequencyBandTable[Band].lower;
	else
	if (Frequency > frequencyBandTable[Band].upper)
		Frequency = frequencyBandTable[Band].upper;
	else
	if (Channel >= FREQ_CHANNEL_FIRST)
		Frequency = FREQUENCY_FloorToStep(Frequency, g_eeprom.VfoInfo[VFO].step_freq, frequencyBandTable[Band].lower);

	pRadio->freq_config_rx.frequency = Frequency;

	if (Frequency >= 10800000 && Frequency < 13600000)
		g_eeprom.VfoInfo[VFO].tx_offset_freq_dir = TX_OFFSET_FREQ_DIR_OFF;
	else
	if (Channel > USER_CHANNEL_LAST)
		g_eeprom.VfoInfo[VFO].tx_offset_freq = FREQUENCY_FloorToStep(g_eeprom.VfoInfo[VFO].tx_offset_freq, g_eeprom.VfoInfo[VFO].step_freq, 0);

	RADIO_ApplyOffset(pRadio);

	memset(g_eeprom.VfoInfo[VFO].name, 0, sizeof(g_eeprom.VfoInfo[VFO].name));
	if (Channel < USER_CHANNEL_LAST)
	{	// 16 bytes allocated to the channel name but only 10 used, the rest are 0's
		EEPROM_ReadBuffer(0x0F50 + (Channel * 16), g_eeprom.VfoInfo[VFO].name + 0, 8);
		EEPROM_ReadBuffer(0x0F58 + (Channel * 16), g_eeprom.VfoInfo[VFO].name + 8, 2);
	}

	if (!g_eeprom.VfoInfo[VFO].frequency_reverse)
	{
		g_eeprom.VfoInfo[VFO].pRX = &g_eeprom.VfoInfo[VFO].freq_config_rx;
		g_eeprom.VfoInfo[VFO].pTX = &g_eeprom.VfoInfo[VFO].freq_config_tx;
	}
	else
	{
		g_eeprom.VfoInfo[VFO].pRX = &g_eeprom.VfoInfo[VFO].freq_config_tx;
		g_eeprom.VfoInfo[VFO].pTX = &g_eeprom.VfoInfo[VFO].freq_config_rx;
	}

	if (!gSetting_350EN)
	{
		FREQ_Config_t *pConfig = g_eeprom.VfoInfo[VFO].pRX;
		if (pConfig->frequency >= 35000000 && pConfig->frequency < 40000000)
			pConfig->frequency = 43300000;
	}

	if (g_eeprom.VfoInfo[VFO].am_mode)
	{	// freq/chan is in AM mode
		g_eeprom.VfoInfo[VFO].scrambling_type         = 0;
//		g_eeprom.VfoInfo[VFO].DTMF_decoding_enable    = false;  // no reason to disable DTMF decoding, aircraft use it on SSB
		g_eeprom.VfoInfo[VFO].freq_config_rx.code_type = CODE_TYPE_OFF;
		g_eeprom.VfoInfo[VFO].freq_config_tx.code_type = CODE_TYPE_OFF;
	}

	g_eeprom.VfoInfo[VFO].compander = (Attributes & USER_CH_COMPAND) >> 4;

	RADIO_ConfigureSquelchAndOutputPower(pRadio);
}

void RADIO_ConfigureSquelchAndOutputPower(VFO_Info_t *pInfo)
{
	uint8_t          TX_power[3];
	FREQUENCY_Band_t Band;

	// *******************************
	// squelch

	Band = FREQUENCY_GetBand(pInfo->pRX->frequency);
	uint16_t Base = (Band < BAND4_174MHz) ? 0x1E60 : 0x1E00;

	if (g_eeprom.squelch_level == 0)
	{	// squelch == 0 (off)
		pInfo->squelch_open_RSSI_thresh    = 0;     // 0 ~ 255
		pInfo->squelch_open_noise_thresh   = 127;   // 127 ~ 0
		pInfo->squelch_close_glitch_thresh = 255;   // 255 ~ 0

		pInfo->squelch_close_RSSI_thresh   = 0;     // 0 ~ 255
		pInfo->squelch_close_noise_thresh  = 127;   // 127 ~ 0
		pInfo->squelch_open_glitch_thresh  = 255;   // 255 ~ 0
	}
	else
	{	// squelch >= 1
		Base += g_eeprom.squelch_level;                                        // my eeprom squelch-1
																			  // VHF   UHF
		EEPROM_ReadBuffer(Base + 0x00, &pInfo->squelch_open_RSSI_thresh,    1);  //  50    10
		EEPROM_ReadBuffer(Base + 0x10, &pInfo->squelch_close_RSSI_thresh,   1);  //  40     5

		EEPROM_ReadBuffer(Base + 0x20, &pInfo->squelch_open_noise_thresh,   1);  //  65    90
		EEPROM_ReadBuffer(Base + 0x30, &pInfo->squelch_close_noise_thresh,  1);  //  70   100

		EEPROM_ReadBuffer(Base + 0x40, &pInfo->squelch_close_glitch_thresh, 1);  //  90    90
		EEPROM_ReadBuffer(Base + 0x50, &pInfo->squelch_open_glitch_thresh,  1);  // 100   100

		uint16_t rssi_open    = pInfo->squelch_open_RSSI_thresh;
		uint16_t rssi_close   = pInfo->squelch_close_RSSI_thresh;
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

		pInfo->squelch_open_RSSI_thresh    = (rssi_open    > 255) ? 255 : rssi_open;
		pInfo->squelch_close_RSSI_thresh   = (rssi_close   > 255) ? 255 : rssi_close;
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
		 frequencyBandTable[Band].lower,
		(frequencyBandTable[Band].lower + frequencyBandTable[Band].upper) / 2,
		 frequencyBandTable[Band].upper,
		pInfo->pTX->frequency);

	// *******************************
}

void RADIO_ApplyOffset(VFO_Info_t *pInfo)
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

	if (Frequency < frequencyBandTable[0].lower)
		Frequency = frequencyBandTable[0].lower;
	else
	if (Frequency > frequencyBandTable[ARRAY_SIZE(frequencyBandTable) - 1].upper)
		Frequency = frequencyBandTable[ARRAY_SIZE(frequencyBandTable) - 1].upper;

	pInfo->freq_config_tx.frequency = Frequency;
}

static void RADIO_SelectCurrentVfo(void)
{
 	gCurrentVfo = (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF) ? gRxVfo : &g_eeprom.VfoInfo[g_eeprom.tx_vfo];
}

void RADIO_SelectVfos(void)
{
	g_eeprom.tx_vfo = get_tx_VFO();
	g_eeprom.rx_vfo = (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF) ? g_eeprom.tx_vfo : (g_eeprom.tx_vfo + 1) & 1u;

	gTxVfo = &g_eeprom.VfoInfo[g_eeprom.tx_vfo];
	gRxVfo = &g_eeprom.VfoInfo[g_eeprom.rx_vfo];

	RADIO_SelectCurrentVfo();
}

void RADIO_SetupRegisters(bool bSwitchToFunction0)
{
	BK4819_FilterBandwidth_t Bandwidth = gRxVfo->channel_bandwidth;
	uint16_t                 InterruptMask;
	uint32_t                 Frequency;

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

	gEnableSpeaker = false;

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
//				BK4819_SetFilterBandwidth(Bandwidth, gRxVfo->am_mode && gSetting_AM_fix);
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
		if (IS_NOT_NOAA_CHANNEL(gRxVfo->channel_save) || !gIsNoaaMode)
			Frequency = gRxVfo->pRX->frequency;
		else
			Frequency = NoaaFrequencyTable[gNoaaChannel];
	#else
		Frequency = gRxVfo->pRX->frequency;
	#endif
	BK4819_SetFrequency(Frequency);

	BK4819_SetupSquelch(
		gRxVfo->squelch_open_RSSI_thresh,    gRxVfo->squelch_close_RSSI_thresh,
		gRxVfo->squelch_open_noise_thresh,   gRxVfo->squelch_close_noise_thresh,
		gRxVfo->squelch_close_glitch_thresh, gRxVfo->squelch_open_glitch_thresh);

	BK4819_PickRXFilterPathBasedOnFrequency(Frequency);

	// what does this in do ?
	BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2, true);

	// AF RX Gain and DAC
	BK4819_WriteRegister(BK4819_REG_48, 0xB3A8);  // 1011 00 111010 1000

	InterruptMask = BK4819_REG_3F_SQUELCH_FOUND | BK4819_REG_3F_SQUELCH_LOST;

	#ifdef ENABLE_NOAA
		if (IS_NOT_NOAA_CHANNEL(gRxVfo->channel_save))
	#endif
	{
		if (gRxVfo->am_mode == 0)
		{	// FM
			uint8_t code_type = gSelectedcode_type;
			uint8_t Code     = gSelectedCode;
			if (gCssScanMode == CSS_SCAN_MODE_OFF)
			{
				code_type = gRxVfo->pRX->code_type;
				Code     = gRxVfo->pRX->code;
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
					BK4819_SetCTCSSFrequency(CTCSS_Options[Code]);

					//#ifndef ENABLE_CTCSS_TAIL_PHASE_SHIFT
						BK4819_SetTailDetection(550);		// QS's 55Hz tone method
					//#else
					//	BK4819_SetTailDetection(CTCSS_Options[Code]);
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

			if (gRxVfo->scrambling_type > 0 && gSetting_ScrambleEnable)
				BK4819_EnableScramble(gRxVfo->scrambling_type - 1);
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
				if (g_eeprom.vox_switch && !gFmRadioMode && IS_NOT_NOAA_CHANNEL(gCurrentVfo->channel_save) && gCurrentVfo->am_mode == 0)
			#else
				if (g_eeprom.vox_switch && IS_NOT_NOAA_CHANNEL(gCurrentVfo->channel_save) && gCurrentVfo->am_mode == 0)
			#endif
		#else
			#ifdef ENABLE_FMRADIO
				if (g_eeprom.vox_switch && !gFmRadioMode && gCurrentVfo->am_mode == 0)
			#else
				if (g_eeprom.vox_switch && gCurrentVfo->am_mode == 0)
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
	BK4819_SetCompander((gRxVfo->am_mode == 0 && gRxVfo->compander >= 2) ? gRxVfo->compander : 0);

	#if 0
		if (!gRxVfo->DTMF_decoding_enable && !gSetting_KILLED)
		{
			BK4819_DisableDTMF();
		}
		else
		{
			BK4819_EnableDTMF();
			InterruptMask |= BK4819_REG_3F_DTMF_5TONE_FOUND;
		}
	#else
		if (gCurrentFunction != FUNCTION_TRANSMIT)
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

		gUpdateStatus = true;

		if (g_eeprom.NOAA_auto_scan)
		{
			if (g_eeprom.dual_watch != DUAL_WATCH_OFF)
			{
				if (IS_NOT_NOAA_CHANNEL(g_eeprom.screen_channel[0]))
				{
					if (IS_NOT_NOAA_CHANNEL(g_eeprom.screen_channel[1]))
					{
						gIsNoaaMode = false;
						return;
					}
					ChanAB = 1;
				}
				else
					ChanAB = 0;

				if (!gIsNoaaMode)
					gNoaaChannel = g_eeprom.VfoInfo[ChanAB].channel_save - NOAA_CHANNEL_FIRST;

				gIsNoaaMode = true;
				return;
			}

			if (gRxVfo->channel_save >= NOAA_CHANNEL_FIRST)
			{
				gIsNoaaMode          = true;
				gNoaaChannel         = gRxVfo->channel_save - NOAA_CHANNEL_FIRST;
				gNOAA_Countdown_10ms = NOAA_countdown_2_10ms;
				gScheduleNOAA        = false;
			}
			else
				gIsNoaaMode = false;
		}
		else
			gIsNoaaMode = false;
	}
#endif

void RADIO_SetTxParameters(void)
{
	BK4819_FilterBandwidth_t Bandwidth = gCurrentVfo->channel_bandwidth;

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

	gEnableSpeaker = false;

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
//				BK4819_SetFilterBandwidth(Bandwidth, gCurrentVfo->am_mode && gSetting_AM_fix);
				BK4819_SetFilterBandwidth(Bandwidth, true);
			#else
				BK4819_SetFilterBandwidth(Bandwidth, false);
			#endif
			break;
	}

	#pragma GCC diagnostic pop

	BK4819_SetFrequency(gCurrentVfo->pTX->frequency);

	// TX compressor
	BK4819_SetCompander((gRxVfo->am_mode == 0 && (gRxVfo->compander == 1 || gRxVfo->compander >= 3)) ? gRxVfo->compander : 0);

	BK4819_PrepareTransmit();

	SYSTEM_DelayMs(10);

	BK4819_PickRXFilterPathBasedOnFrequency(gCurrentVfo->pTX->frequency);

	BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1, true);

	SYSTEM_DelayMs(5);

	BK4819_SetupPowerAmplifier(gCurrentVfo->txp_calculated_setting, gCurrentVfo->pTX->frequency);

	SYSTEM_DelayMs(10);

	switch (gCurrentVfo->pTX->code_type)
	{
		default:
		case CODE_TYPE_OFF:
			BK4819_ExitSubAu();
			break;

		case CODE_TYPE_CONTINUOUS_TONE:
			BK4819_SetCTCSSFrequency(CTCSS_Options[gCurrentVfo->pTX->code]);
			break;

		case CODE_TYPE_DIGITAL:
		case CODE_TYPE_REVERSE_DIGITAL:
			BK4819_SetCDCSSCodeWord(DCS_GetGolayCodeWord(gCurrentVfo->pTX->code_type, gCurrentVfo->pTX->code));
			break;
	}
}

void RADIO_SetVfoState(VfoState_t State)
{
	if (State == VFO_STATE_NORMAL)
	{
		VfoState[0] = VFO_STATE_NORMAL;
		VfoState[1] = VFO_STATE_NORMAL;

		#ifdef ENABLE_FMRADIO
			gFM_ResumeCountdown_500ms = 0;
		#endif
	}
	else
	{
		if (State == VFO_STATE_VOLTAGE_HIGH)
		{
			VfoState[0] = VFO_STATE_VOLTAGE_HIGH;
			VfoState[1] = VFO_STATE_TX_DISABLE;
		}
		else
		{	// 1of11
			const unsigned int vfo = (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF) ? g_eeprom.rx_vfo : g_eeprom.tx_vfo;
			VfoState[vfo] = State;
		}

		#ifdef ENABLE_FMRADIO
			gFM_ResumeCountdown_500ms = fm_resume_countdown_500ms;
		#endif
	}

	gUpdateDisplay = true;
}

void RADIO_PrepareTX(void)
{
	VfoState_t State = VFO_STATE_NORMAL;  // default to OK to TX

	if (g_eeprom.dual_watch != DUAL_WATCH_OFF)
	{	// dual-RX is enabled

		gDualWatchCountdown_10ms = dual_watch_count_after_tx_10ms;
		gScheduleDualWatch       = false;

#if 0
		if (gRxVfoIsActive)
		{	// use the TX vfo
			g_eeprom.rx_vfo = g_eeprom.tx_vfo;
			gRxVfo         = &g_eeprom.VfoInfo[g_eeprom.tx_vfo];
			gRxVfoIsActive = false;
		}
		gCurrentVfo = gRxVfo;
#else
		if (!gRxVfoIsActive)
		{	// use the current RX vfo
			g_eeprom.rx_vfo = g_eeprom.tx_vfo;
			gRxVfo         = &g_eeprom.VfoInfo[g_eeprom.tx_vfo];
			gRxVfoIsActive = true;
		}
		gCurrentVfo = gRxVfo;
#endif
	
		// let the user see that DW is not active '><' symbol
		gDualWatchActive = false;
		gUpdateStatus    = true;
	}

	RADIO_SelectCurrentVfo();

	#ifndef ENABLE_TX_WHEN_AM
		if (gCurrentVfo->am_mode)
		{	// not allowed to TX if in AM mode
			State = VFO_STATE_TX_DISABLE;
		}
		else
	#endif
	if (!gSetting_TX_EN || gSerialConfigCountDown_500ms > 0)
	{	// TX is disabled or config upload/download in progress
		State = VFO_STATE_TX_DISABLE;
	}
	else
	if (TX_freq_check(gCurrentVfo->pTX->frequency) == 0)
	{	// TX frequency is allowed
		if (gCurrentVfo->busy_channel_lock && gCurrentFunction == FUNCTION_RECEIVE)
			State = VFO_STATE_BUSY;          // busy RX'ing a station
		else
		if (gBatteryDisplayLevel == 0)
			State = VFO_STATE_BAT_LOW;       // charge your battery !
		else
		if (gBatteryDisplayLevel >= 6)
			State = VFO_STATE_VOLTAGE_HIGH;  // over voltage (no doubt to protect the PA) .. this is being a pain
	}
	else
		State = VFO_STATE_TX_DISABLE;        // TX frequency not allowed

	if (State != VFO_STATE_NORMAL)
	{	// TX not allowed

		RADIO_SetVfoState(State);

		#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
			gAlarmState = ALARM_STATE_OFF;
		#endif

		gDTMF_ReplyState = DTMF_REPLY_NONE;

		AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
		return;
	}

	// TX is allowed

	if (gDTMF_ReplyState == DTMF_REPLY_ANI)
	{
		if (gDTMF_CallMode == DTMF_CALL_MODE_DTMF)
		{
			gDTMF_IsTx                  = true;
			gDTMF_CallState             = DTMF_CALL_STATE_NONE;
			gDTMF_TxStopCountdown_500ms = DTMF_txstop_countdown_500ms;
		}
		else
		{
			gDTMF_CallState = DTMF_CALL_STATE_CALL_OUT;
			gDTMF_IsTx      = false;
		}
	}

	FUNCTION_Select(FUNCTION_TRANSMIT);

	gTxTimerCountdown_500ms = 0;            // no timeout

	#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
		if (gAlarmState == ALARM_STATE_OFF)
	#endif
	{
		if (g_eeprom.tx_timeout_timer == 0)
			gTxTimerCountdown_500ms = 60;   // 30 sec
		else
		if (g_eeprom.tx_timeout_timer < (ARRAY_SIZE(gSubMenu_TOT) - 1))
			gTxTimerCountdown_500ms = 120 * g_eeprom.tx_timeout_timer;  // minutes
		else
			gTxTimerCountdown_500ms = 120 * 15;  // 15 minutes
	}
	gTxTimeoutReached    = false;

	gFlagEndTransmission = false;
	gRTTECountdown       = 0;
	gDTMF_ReplyState     = DTMF_REPLY_NONE;
}

void RADIO_EnableCxCSS(void)
{
	switch (gCurrentVfo->pTX->code_type)
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
	if (g_eeprom.roger_mode == ROGER_MODE_ROGER)
		BK4819_PlayRoger();
	else
	if (g_eeprom.roger_mode == ROGER_MODE_MDC)
		BK4819_PlayRogerMDC();

	if (gCurrentVfo->DTMF_ptt_id_tx_mode == PTT_ID_APOLLO)
		BK4819_PlaySingleTone(2475, 250, 28, g_eeprom.DTMF_side_tone);

	if (gDTMF_CallState == DTMF_CALL_STATE_NONE &&
	   (gCurrentVfo->DTMF_ptt_id_tx_mode == PTT_ID_TX_DOWN || gCurrentVfo->DTMF_ptt_id_tx_mode == PTT_ID_BOTH))
	{	// end-of-tx
		if (g_eeprom.DTMF_side_tone)
		{
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
			gEnableSpeaker = true;
			SYSTEM_DelayMs(60);
		}

		BK4819_EnterDTMF_TX(g_eeprom.DTMF_side_tone);

		BK4819_PlayDTMFString(
				g_eeprom.DTMF_down_code,
				0,
				g_eeprom.DTMF_first_code_persist_time,
				g_eeprom.DTMF_hash_code_persist_time,
				g_eeprom.DTMF_code_persist_time,
				g_eeprom.DTMF_code_interval_time);

		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
		gEnableSpeaker = false;
	}

	BK4819_ExitDTMF_TX(true);
}
