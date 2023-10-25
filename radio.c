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

#include "app/app.h"
#include "app/dtmf.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "audio.h"
#include "board.h"
#include "bsp/dp32g030/gpio.h"
#include "dcs.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "frequencies.h"
#include "functions.h"
#include "helper/battery.h"
#ifdef ENABLE_MDC1200
	#include "mdc1200.h"
#endif
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/menu.h"
#include "ui/ui.h"

vfo_info_t     *g_tx_vfo;
vfo_info_t     *g_rx_vfo;
vfo_info_t     *g_current_vfo;
dcs_code_type_t g_selected_code_type;
dcs_code_type_t g_current_code_type;
uint8_t         g_selected_code;
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

uint8_t RADIO_FindNextChannel(uint8_t Channel, scan_state_dir_t Direction, bool bCheckScanList, uint8_t VFO)
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

void RADIO_InitInfo(vfo_info_t *p_vfo, const uint8_t ChannelSave, const uint32_t Frequency)
{
	if (p_vfo == NULL)
		return;
	
	memset(p_vfo, 0, sizeof(*p_vfo));

	p_vfo->band                     = FREQUENCY_GetBand(Frequency);
	p_vfo->scanlist_1_participation = 1;
	p_vfo->scanlist_2_participation = 1;
	p_vfo->step_setting             = STEP_12_5kHz;
	p_vfo->step_freq                = STEP_FREQ_TABLE[p_vfo->step_setting];
	p_vfo->channel_save             = ChannelSave;
	p_vfo->frequency_reverse        = false;
	p_vfo->output_power             = OUTPUT_POWER_LOW;
	p_vfo->freq_config_rx.frequency = Frequency;
	p_vfo->freq_config_tx.frequency = Frequency;
	p_vfo->p_rx                     = &p_vfo->freq_config_rx;
	p_vfo->p_tx                     = &p_vfo->freq_config_tx;
	p_vfo->compand                  = 0;  // off
	p_vfo->squelch_level            = 0;  // use main squelch
	p_vfo->freq_in_channel          = 0xff;

	if (ChannelSave == (FREQ_CHANNEL_FIRST + BAND2_108MHz))
		p_vfo->am_mode = 1;

	RADIO_ConfigureSquelchAndOutputPower(p_vfo);
}

void RADIO_configure_channel(const unsigned int VFO, const unsigned int configure)
{
	uint8_t     Channel;
	uint8_t     Attributes;
	uint8_t     Band;
	uint16_t    Base;
	uint32_t    Frequency;
	vfo_info_t *p_vfo = &g_eeprom.vfo_info[VFO];

	if (!g_setting_350_enable)
	{
		if (g_eeprom.freq_channel[VFO] == (FREQ_CHANNEL_LAST - 2))
			g_eeprom.freq_channel[VFO] = FREQ_CHANNEL_LAST - 1;

		if (g_eeprom.screen_channel[VFO] == (FREQ_CHANNEL_LAST - 2))
			g_eeprom.screen_channel[VFO] = FREQ_CHANNEL_LAST - 1;
	}

	Channel = g_eeprom.screen_channel[VFO];

	p_vfo->freq_in_channel = 0xff;

	if (IS_VALID_CHANNEL(Channel))
	{
		#ifdef ENABLE_NOAA
			if (Channel >= NOAA_CHANNEL_FIRST)
			{
				RADIO_InitInfo(p_vfo, g_eeprom.screen_channel[VFO], NOAA_FREQUENCY_TABLE[Channel - NOAA_CHANNEL_FIRST]);
				if (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF)
					return;
				g_eeprom.cross_vfo_rx_tx = CROSS_BAND_OFF;
				g_update_status = true;
				return;
			}
		#endif

		if (Channel <= USER_CHANNEL_LAST)
		{
			Channel = RADIO_FindNextChannel(Channel, SCAN_STATE_DIR_FORWARD, false, VFO);
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

		RADIO_InitInfo(p_vfo, Channel, FREQ_BAND_TABLE[Index].lower);
		return;
	}

	Band = Attributes & USER_CH_BAND_MASK;
	if (Band > BAND7_470MHz)
		Band = BAND6_400MHz;

	if (Channel <= USER_CHANNEL_LAST)
	{	// USER channel
		p_vfo->band                     = Band;
		p_vfo->scanlist_2_participation = (Attributes & USER_CH_SCANLIST2) ? 1 : 0;
		p_vfo->scanlist_1_participation = (Attributes & USER_CH_SCANLIST1) ? 1 : 0;
	}
	else
	if (IS_FREQ_CHANNEL(Channel))
	{	// VFO channel
		Band                        = Channel - FREQ_CHANNEL_FIRST;
		g_eeprom.vfo_info[VFO].band = Band; // shouldn't this be  "Band / 2" ? .. two VFO's per band
		#if 0
			p_vfo->scanlist_2_participation = 1;
			p_vfo->scanlist_1_participation = 1;
		#else
			// allowing the vfo's to be included in the scanning
			p_vfo->scanlist_2_participation = (Attributes & USER_CH_SCANLIST2) ? 1 : 0;
			p_vfo->scanlist_1_participation = (Attributes & USER_CH_SCANLIST1) ? 1 : 0;
		#endif
	}

	p_vfo->channel_save = Channel;

	if (Channel <= USER_CHANNEL_LAST)
		Base = Channel * 16;
	else
		Base = 0x0C80 + ((Channel - FREQ_CHANNEL_FIRST) * 16 * 2) + (VFO * 16);  // VFO channel

	if (configure == VFO_CONFIGURE_RELOAD || IS_FREQ_CHANNEL(Channel))
	{
		t_channel m_channel;

		EEPROM_ReadBuffer(Base, &m_channel, sizeof(m_channel));

		p_vfo->freq_config_rx.frequency =  m_channel.frequency;
		p_vfo->tx_offset_freq           = (m_channel.offset <= 100000000) ? m_channel.offset : 1000000;
		p_vfo->tx_offset_freq_dir       = (m_channel.tx_offset_dir <= TX_OFFSET_FREQ_DIR_SUB) ? m_channel.tx_offset_dir : TX_OFFSET_FREQ_DIR_OFF;
		p_vfo->am_mode                  =  m_channel.am_mode;
		p_vfo->step_setting             = (m_channel.step_setting < ARRAY_SIZE(STEP_FREQ_TABLE)) ? m_channel.step_setting : STEP_12_5kHz;
		p_vfo->step_freq                = STEP_FREQ_TABLE[p_vfo->step_setting];
		p_vfo->scrambling_type          = (m_channel.scrambler < ARRAY_SIZE(g_sub_menu_scrambler)) ? m_channel.scrambler : 0;

		p_vfo->freq_config_rx.code_type = m_channel.rx_ctcss_cdcss_type;
		switch (m_channel.rx_ctcss_cdcss_type)
		{
			default:
			case CODE_TYPE_NONE:
				p_vfo->freq_config_rx.code_type = CODE_TYPE_NONE;
				p_vfo->freq_config_rx.code      = 0;
				break;
			case CODE_TYPE_CONTINUOUS_TONE:
				p_vfo->freq_config_rx.code      = (m_channel.rx_ctcss_cdcss_code < ARRAY_SIZE(CTCSS_OPTIONS)) ? m_channel.rx_ctcss_cdcss_code : 0;
				break;
			case CODE_TYPE_DIGITAL:
			case CODE_TYPE_REVERSE_DIGITAL:
				p_vfo->freq_config_rx.code      = (m_channel.rx_ctcss_cdcss_code < ARRAY_SIZE(DCS_OPTIONS)) ? m_channel.rx_ctcss_cdcss_code : 0;
				break;
		}

		p_vfo->freq_config_tx.code_type = m_channel.tx_ctcss_cdcss_type;
		switch (m_channel.tx_ctcss_cdcss_type)
		{
			default:
			case CODE_TYPE_NONE:
				p_vfo->freq_config_tx.code_type = CODE_TYPE_NONE;
				p_vfo->freq_config_tx.code      = 0;
				break;
			case CODE_TYPE_CONTINUOUS_TONE:
				p_vfo->freq_config_tx.code      = (m_channel.tx_ctcss_cdcss_code < ARRAY_SIZE(CTCSS_OPTIONS)) ? m_channel.tx_ctcss_cdcss_code : 0;
				break;
			case CODE_TYPE_DIGITAL:
			case CODE_TYPE_REVERSE_DIGITAL:
				p_vfo->freq_config_tx.code      = (m_channel.tx_ctcss_cdcss_code < ARRAY_SIZE(DCS_OPTIONS)) ? m_channel.tx_ctcss_cdcss_code : 0;
				break;
		}

		#ifdef ENABLE_MDC1200
			p_vfo->mdc1200_mode = m_channel.mdc1200_mode;
		#endif

		p_vfo->frequency_reverse = m_channel.frequency_reverse ? true : false;
		p_vfo->channel_bandwidth = m_channel.channel_bandwidth ? true : false;
		p_vfo->output_power      = m_channel.tx_power;
		p_vfo->busy_channel_lock = m_channel.busy_channel_lock ? true : false;
		p_vfo->compand           = m_channel.compand;

		p_vfo->dtmf_decoding_enable = m_channel.dtmf_decoding_enable ? true : false;
		p_vfo->dtmf_ptt_id_tx_mode  = m_channel.dtmf_ptt_id_tx_mode;

		p_vfo->squelch_level = (m_channel.squelch_level < 10) ? m_channel.squelch_level : 0;
	}

	Frequency = p_vfo->freq_config_rx.frequency;

	if (Frequency < FREQ_BAND_TABLE[Band].lower)
		Frequency = FREQ_BAND_TABLE[Band].lower;
	else
	if (Frequency >= FREQ_BAND_TABLE[Band].upper)
		Frequency = FREQUENCY_floor_to_step(Frequency, p_vfo->step_freq, FREQ_BAND_TABLE[Band].lower, FREQ_BAND_TABLE[Band].upper);
	else
	if (Channel >= FREQ_CHANNEL_FIRST)
		Frequency = FREQUENCY_floor_to_step(Frequency, p_vfo->step_freq, FREQ_BAND_TABLE[Band].lower, FREQ_BAND_TABLE[Band].upper);

	if (!g_setting_350_enable && Frequency >= 35000000 && Frequency < 40000000)
	{	// 350~400Mhz not allowed

		// hop onto the next band up
		Frequency                       = 43350000;
		p_vfo->freq_config_rx.frequency = Frequency;
		p_vfo->freq_config_tx.frequency = Frequency;
		Band                            = FREQUENCY_GetBand(Frequency);
		p_vfo->band                     = Band;
		p_vfo->frequency_reverse        = 0;
		p_vfo->tx_offset_freq_dir       = TX_OFFSET_FREQ_DIR_OFF;
		p_vfo->tx_offset_freq           = 0;
		
		
		// TODO: also update other settings such as step size
		
		
	}

	p_vfo->freq_config_rx.frequency = Frequency;

	if (Frequency >= AIR_BAND.lower && Frequency < AIR_BAND.upper)
	{	// air band
		p_vfo->tx_offset_freq_dir = TX_OFFSET_FREQ_DIR_OFF;
		p_vfo->tx_offset_freq     = 0;
	}
	else
	if (Channel > USER_CHANNEL_LAST)
	{
		p_vfo->tx_offset_freq = FREQUENCY_floor_to_step(p_vfo->tx_offset_freq, p_vfo->step_freq, 0, p_vfo->tx_offset_freq);
	}

	RADIO_ApplyOffset(p_vfo);

	// channel name
	memset(p_vfo->name, 0, sizeof(p_vfo->name));
	if (Channel <= USER_CHANNEL_LAST)
		EEPROM_ReadBuffer(0x0F50 + (Channel * 16), p_vfo->name, 10);	// only 10 bytes used

	if (!p_vfo->frequency_reverse)
	{
		p_vfo->p_rx = &p_vfo->freq_config_rx;
		p_vfo->p_tx = &p_vfo->freq_config_tx;
	}
	else
	{
		p_vfo->p_rx = &p_vfo->freq_config_tx;
		p_vfo->p_tx = &p_vfo->freq_config_rx;
	}

	if (p_vfo->am_mode)
	{	// freq/chan is in AM mode
		// disable stuff, even though it can all still be used with AM ???
		p_vfo->scrambling_type          = 0;
//		p_vfo->dtmf_decoding_enable     = false;
		p_vfo->freq_config_rx.code_type = CODE_TYPE_NONE;
		p_vfo->freq_config_tx.code_type = CODE_TYPE_NONE;
	}

	RADIO_ConfigureSquelchAndOutputPower(p_vfo);

	if (IS_FREQ_CHANNEL(Channel))
		p_vfo->freq_in_channel = BOARD_find_channel(Frequency); // remember if a channel has this frequency
}

void RADIO_ConfigureSquelchAndOutputPower(vfo_info_t *p_vfo)
{
	uint8_t          TX_power[3];
	uint16_t         Base;
	frequency_band_t Band;
	uint8_t          squelch_level;

	// *******************************
	// squelch

	Band = FREQUENCY_GetBand(p_vfo->p_rx->frequency);
	Base = (Band < BAND4_174MHz) ? 0x1E60 : 0x1E00;

	squelch_level = (p_vfo->squelch_level > 0) ? p_vfo->squelch_level : g_eeprom.squelch_level;
	
	// note that 'noise' and 'glitch' values are inverted compared to 'rssi' values

	if (squelch_level == 0)
	{	// squelch == 0 (off)
		p_vfo->squelch_open_rssi_thresh    = 0;     // 0 ~ 255
		p_vfo->squelch_close_rssi_thresh   = 0;     // 0 ~ 255

		p_vfo->squelch_open_noise_thresh   = 127;   // 127 ~ 0
		p_vfo->squelch_close_noise_thresh  = 127;   // 127 ~ 0

		p_vfo->squelch_close_glitch_thresh = 255;   // 255 ~ 0
		p_vfo->squelch_open_glitch_thresh  = 255;   // 255 ~ 0
	}
	else
	{	// squelch >= 1
		Base += squelch_level;                                                   // my eeprom squelch-1
		                                                                         // VHF   UHF
		EEPROM_ReadBuffer(Base + 0x00, &p_vfo->squelch_open_rssi_thresh,    1);  //  50    10
		EEPROM_ReadBuffer(Base + 0x10, &p_vfo->squelch_close_rssi_thresh,   1);  //  40     5

		EEPROM_ReadBuffer(Base + 0x20, &p_vfo->squelch_open_noise_thresh,   1);  //  65    90
		EEPROM_ReadBuffer(Base + 0x30, &p_vfo->squelch_close_noise_thresh,  1);  //  70   100

		EEPROM_ReadBuffer(Base + 0x40, &p_vfo->squelch_close_glitch_thresh, 1);  //  90    90
		EEPROM_ReadBuffer(Base + 0x50, &p_vfo->squelch_open_glitch_thresh,  1);  // 100   100

		// *********

		// used in AM mode
		int16_t rssi_open    = p_vfo->squelch_open_rssi_thresh;      // 0 ~ 255
		int16_t rssi_close   = p_vfo->squelch_close_rssi_thresh;     // 0 ~ 255

		// used in FM mode
		int16_t noise_open   = p_vfo->squelch_open_noise_thresh;     // 127 ~ 0
		int16_t noise_close  = p_vfo->squelch_close_noise_thresh;    // 127 ~ 0

		// used in both modes ?
		int16_t glitch_open  = p_vfo->squelch_open_glitch_thresh;    // 255 ~ 0
		int16_t glitch_close = p_vfo->squelch_close_glitch_thresh;   // 255 ~ 0

		// *********

		#if ENABLE_SQUELCH_MORE_SENSITIVE
			// make squelch a little more sensitive
			//
			// getting the best general settings here is experimental, bare with me

			#if 0
//				rssi_open   = (rssi_open   * 8) / 9;
				noise_open  = (noise_open  * 9) / 8;
				glitch_open = (glitch_open * 9) / 8;
			#else
				// even more sensitive .. use when RX bandwidths are fixed (no weak signal auto adjust)
//				rssi_open   = (rssi_open   * 1) / 2;
				noise_open  = (noise_open  * 2) / 1;
				glitch_open = (glitch_open * 2) / 1;
			#endif

		#else
			// more sensitive .. use when RX bandwidths are fixed (no weak signal auto adjust)
//			rssi_open   = (rssi_open   * 3) / 4;
			noise_open  = (noise_open  * 4) / 3;
			glitch_open = (glitch_open * 4) / 3;
		#endif

		// *********
		// ensure the 'close' threshold is lower than the 'open' threshold
		// ie, maintain a minimum level of hysteresis

//		rssi_close   = (rssi_open   * 4) / 6;
		noise_close  = (noise_open  * 6) / 4;
		glitch_close = (glitch_open * 6) / 4;

//		if (rssi_open  <  8)
//			rssi_open  =  8;
//		if (rssi_close > (rssi_open   - 8))
//			rssi_close =  rssi_open   - 8;

		if (noise_open  > (127 - 4))
			noise_open  =  127 - 4;
		if (noise_close < (noise_open  + 4))
			noise_close =  noise_open  + 4;

		if (glitch_open  > (255 - 8))
			glitch_open  =  255 - 8;
		if (glitch_close < (glitch_open + 8))
			glitch_close =  glitch_open + 8;

		// *********

		p_vfo->squelch_open_rssi_thresh    = (rssi_open    > 255) ? 255 : (rssi_open    < 0) ? 0 : rssi_open;
		p_vfo->squelch_close_rssi_thresh   = (rssi_close   > 255) ? 255 : (rssi_close   < 0) ? 0 : rssi_close;

		p_vfo->squelch_open_noise_thresh   = (noise_open   > 127) ? 127 : (noise_open   < 0) ? 0 : noise_open;
		p_vfo->squelch_close_noise_thresh  = (noise_close  > 127) ? 127 : (noise_close  < 0) ? 0 : noise_close;

		p_vfo->squelch_open_glitch_thresh  = (glitch_open  > 255) ? 255 : (glitch_open  < 0) ? 0 : glitch_open;
		p_vfo->squelch_close_glitch_thresh = (glitch_close > 255) ? 255 : (glitch_close < 0) ? 0 : glitch_close;
	}

	// *******************************
	// output power

	// my calibration data
	//
	// 1ED0    32 32 32   64 64 64   8C 8C 8C   FF FF FF FF FF FF FF ..  50 MHz
	// 1EE0    32 32 32   64 64 64   8C 8C 8C   FF FF FF FF FF FF FF .. 108 MHz
	// 1EF0    5F 5F 5F   69 69 69   91 91 8F   FF FF FF FF FF FF FF .. 137 MHz
	// 1F00    32 32 32   64 64 64   8C 8C 8C   FF FF FF FF FF FF FF .. 174 MHz
	// 1F10    5A 5A 5A   64 64 64   82 82 82   FF FF FF FF FF FF FF .. 350 MHz
	// 1F20    5A 5A 5A   64 64 64   8F 91 8A   FF FF FF FF FF FF FF .. 400 MHz
	// 1F30    32 32 32   64 64 64   8C 8C 8C   FF FF FF FF FF FF FF .. 470 MHz

	Band = FREQUENCY_GetBand(p_vfo->p_tx->frequency);

	EEPROM_ReadBuffer(0x1ED0 + (Band * 16) + (p_vfo->output_power * 3), TX_power, 3);

	#ifdef ENABLE_REDUCE_LOW_MID_TX_POWER
		// make low and mid even lower
		if (p_vfo->output_power == OUTPUT_POWER_LOW)
		{
			TX_power[0] /= 5;
			TX_power[1] /= 5;
			TX_power[2] /= 5;
		}
		else
		if (p_vfo->output_power == OUTPUT_POWER_MID)
		{
			TX_power[0] /= 3;
			TX_power[1] /= 3;
			TX_power[2] /= 3;
		}
	#endif

	p_vfo->txp_calculated_setting = FREQUENCY_CalculateOutputPower(
		TX_power[0],
		TX_power[1],
		TX_power[2],
		 FREQ_BAND_TABLE[Band].lower,
		(FREQ_BAND_TABLE[Band].lower + FREQ_BAND_TABLE[Band].upper) / 2,
		 FREQ_BAND_TABLE[Band].upper,
		p_vfo->p_tx->frequency);

	// *******************************
}

void RADIO_ApplyOffset(vfo_info_t *p_vfo)
{
	uint32_t Frequency = p_vfo->freq_config_rx.frequency;

	switch (p_vfo->tx_offset_freq_dir)
	{
		case TX_OFFSET_FREQ_DIR_OFF:
			break;
		case TX_OFFSET_FREQ_DIR_ADD:
			Frequency += p_vfo->tx_offset_freq;
			break;
		case TX_OFFSET_FREQ_DIR_SUB:
			Frequency -= p_vfo->tx_offset_freq;
			break;
	}

	if (Frequency < FREQ_BAND_TABLE[0].lower)
		Frequency = FREQ_BAND_TABLE[0].lower;
	else
	if (Frequency > FREQ_BAND_TABLE[ARRAY_SIZE(FREQ_BAND_TABLE) - 1].upper)
		Frequency = FREQ_BAND_TABLE[ARRAY_SIZE(FREQ_BAND_TABLE) - 1].upper;

	p_vfo->freq_config_tx.frequency = Frequency;
}

static void RADIO_SelectCurrentVfo(void)
{
 	g_current_vfo = (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF) ? g_rx_vfo : &g_eeprom.vfo_info[g_eeprom.tx_vfo];
}

void RADIO_select_vfos(void)
{
	g_eeprom.tx_vfo = get_TX_VFO();
	g_eeprom.rx_vfo = (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF) ? g_eeprom.tx_vfo : (g_eeprom.tx_vfo + 1) & 1u;

	g_tx_vfo = &g_eeprom.vfo_info[g_eeprom.tx_vfo];
	g_rx_vfo = &g_eeprom.vfo_info[g_eeprom.rx_vfo];

	RADIO_SelectCurrentVfo();
}

void RADIO_setup_registers(bool switch_to_function_foreground)
{
	BK4819_filter_bandwidth_t Bandwidth = g_rx_vfo->channel_bandwidth;
	uint16_t                  interrupt_mask;
	uint32_t                  Frequency;

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
	g_speaker_enabled = false;

	BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, false);

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

	BK4819_set_GPIO_pin(BK4819_GPIO5_PIN1_RED, false);         // LED off
	BK4819_SetupPowerAmplifier(0, 0);
	BK4819_set_GPIO_pin(BK4819_GPIO1_PIN29_PA_ENABLE, false);  // PA off

	while (1)
	{	// wait for the interrupt to clear ?
		const uint16_t status_bits = BK4819_ReadRegister(0x0C);
		if ((status_bits & (1u << 0)) == 0)
			break;
		BK4819_WriteRegister(0x02, 0);   // clear the interrupt bits
		SYSTEM_DelayMs(1);
	}

	BK4819_WriteRegister(0x3F, 0);       // disable interrupts

	// mic gain 0.5dB/step 0 to 31
	BK4819_WriteRegister(0x7D, 0xE940 | (g_eeprom.mic_sensitivity_tuning & 0x1f));

	#ifdef ENABLE_NOAA
		if (IS_NOAA_CHANNEL(g_rx_vfo->channel_save) && g_is_noaa_mode)
			Frequency = NOAA_FREQUENCY_TABLE[g_noaa_channel];
		else
	#endif
			Frequency = g_rx_vfo->p_rx->frequency;

	BK4819_set_rf_frequency(Frequency, false);
	BK4819_set_rf_filter_path(Frequency);

	BK4819_SetupSquelch(
		g_rx_vfo->squelch_open_rssi_thresh,    g_rx_vfo->squelch_close_rssi_thresh,
		g_rx_vfo->squelch_open_noise_thresh,   g_rx_vfo->squelch_close_noise_thresh,
		g_rx_vfo->squelch_close_glitch_thresh, g_rx_vfo->squelch_open_glitch_thresh);

	BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_RX_ENABLE, true);

	// AF RX Gain and DAC
	BK4819_WriteRegister(0x48, 0xB3A8);  // 1011 00 111010 1000

	interrupt_mask = BK4819_REG_3F_SQUELCH_FOUND | BK4819_REG_3F_SQUELCH_LOST;

	if (IS_NOT_NOAA_CHANNEL(g_rx_vfo->channel_save))
	{
		if (g_rx_vfo->am_mode == 0)
		{	// FM
			uint8_t code_type = g_selected_code_type;
			uint8_t Code      = g_selected_code;

			if (g_css_scan_mode == CSS_SCAN_MODE_OFF)
			{
				code_type = g_rx_vfo->p_rx->code_type;
				Code      = g_rx_vfo->p_rx->code;
			}

			switch (code_type)
			{
				default:
				case CODE_TYPE_NONE:
					BK4819_SetCTCSSFrequency(670);

					//#ifndef ENABLE_CTCSS_TAIL_PHASE_SHIFT
						BK4819_SetTailDetection(550);		// QS's 55Hz tone method
					//#else
					//	BK4819_SetTailDetection(670);       // 67Hz
					//#endif

					interrupt_mask = BK4819_REG_3F_CxCSS_TAIL | BK4819_REG_3F_SQUELCH_FOUND | BK4819_REG_3F_SQUELCH_LOST;
					break;

				case CODE_TYPE_CONTINUOUS_TONE:
					BK4819_SetCTCSSFrequency(CTCSS_OPTIONS[Code]);

					//#ifndef ENABLE_CTCSS_TAIL_PHASE_SHIFT
						BK4819_SetTailDetection(550);		// QS's 55Hz tone method
					//#else
					//	BK4819_SetTailDetection(CTCSS_OPTIONS[Code]);
					//#endif

					interrupt_mask =
						BK4819_REG_3F_CxCSS_TAIL    |
						BK4819_REG_3F_CTCSS_FOUND   |
						BK4819_REG_3F_CTCSS_LOST    |
						BK4819_REG_3F_SQUELCH_FOUND |
						BK4819_REG_3F_SQUELCH_LOST;

					break;

				case CODE_TYPE_DIGITAL:
				case CODE_TYPE_REVERSE_DIGITAL:
					BK4819_SetCDCSSCodeWord(DCS_GetGolayCodeWord(code_type, Code));
					interrupt_mask =
						BK4819_REG_3F_CxCSS_TAIL    |
						BK4819_REG_3F_CDCSS_FOUND   |
						BK4819_REG_3F_CDCSS_LOST    |
						BK4819_REG_3F_SQUELCH_FOUND |
						BK4819_REG_3F_SQUELCH_LOST;
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
			interrupt_mask =
				BK4819_REG_3F_CTCSS_FOUND   |
				BK4819_REG_3F_CTCSS_LOST    |
				BK4819_REG_3F_SQUELCH_FOUND |
				BK4819_REG_3F_SQUELCH_LOST;
		}
	#endif

	#ifdef ENABLE_VOX
		if (
			#ifdef ENABLE_FMRADIO
				!g_fm_radio_mode &&
			#endif
			g_eeprom.vox_switch &&
			IS_NOT_NOAA_CHANNEL(g_current_vfo->channel_save) &&
			g_current_vfo->am_mode == 0)
		{
			BK4819_EnableVox(g_eeprom.vox1_threshold, g_eeprom.vox0_threshold);
			interrupt_mask |= BK4819_REG_3F_VOX_FOUND | BK4819_REG_3F_VOX_LOST;
		}
		else
	#endif
			BK4819_DisableVox();

	// RX expander
	BK4819_SetCompander((g_rx_vfo->am_mode == 0 && g_rx_vfo->compand >= 2) ? g_rx_vfo->compand : 0);

	#if 0
		#ifdef ENABLE_KILL_REVIVE
			if (!g_rx_vfo->dtmf_decoding_enable && !g_setting_radio_disabled)
		#else
			if (!g_rx_vfo->dtmf_decoding_enable)
		#endif
		{
			BK4819_DisableDTMF();
		}
		else
		{
			BK4819_EnableDTMF();
			interrupt_mask |= BK4819_REG_3F_DTMF_5TONE_FOUND;
		}
	#else
		if (g_current_function != FUNCTION_TRANSMIT)
		{
			BK4819_DisableDTMF();
			BK4819_EnableDTMF();
			interrupt_mask |= BK4819_REG_3F_DTMF_5TONE_FOUND;
		}
		else
		{
			BK4819_DisableDTMF();
		}
	#endif

	// enable/disable BK4819 selected interrupts
	BK4819_WriteRegister(0x3F, interrupt_mask);

	FUNCTION_Init();

	if (switch_to_function_foreground)
	{
		if (g_monitor_enabled)
			APP_start_listening(FUNCTION_MONITOR, false);
		else
			FUNCTION_Select(FUNCTION_FOREGROUND);
	}
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

void RADIO_enableTX(const bool fsk_tx)
{
	BK4819_filter_bandwidth_t Bandwidth = g_current_vfo->channel_bandwidth;

	// disable the speaker
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
	g_speaker_enabled = false;

	BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_RX_ENABLE, false);

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

	// if DTMF is enabled when TX'ing, it changes the TX audio filtering ! .. 1of11
	// so MAKE SURE that DTMF is disabled - until needed
	BK4819_DisableDTMF();

	BK4819_SetCompander((!fsk_tx && g_rx_vfo->am_mode == 0 && (g_rx_vfo->compand == 1 || g_rx_vfo->compand >= 3)) ? g_rx_vfo->compand : 0);

	BK4819_set_rf_frequency(g_current_vfo->p_tx->frequency, false);
	BK4819_set_rf_filter_path(g_current_vfo->p_tx->frequency);

	BK4819_PrepareTransmit();
	BK4819_set_GPIO_pin(BK4819_GPIO1_PIN29_PA_ENABLE, true);                // PA on
	if (g_screen_to_display != DISPLAY_AIRCOPY)
		BK4819_SetupPowerAmplifier(g_current_vfo->txp_calculated_setting, g_current_vfo->p_tx->frequency);
	else
		BK4819_SetupPowerAmplifier(0, g_current_vfo->p_tx->frequency);      // very low power when in AIRCOPY mode

	BK4819_set_GPIO_pin(BK4819_GPIO5_PIN1_RED, true);                       // turn the RED LED on

	if (fsk_tx)
	{
		BK4819_ExitSubAu();
	}
	else
	{
		switch (g_current_vfo->p_tx->code_type)
		{
			default:
			case CODE_TYPE_NONE:
				BK4819_ExitSubAu();
				break;

			case CODE_TYPE_CONTINUOUS_TONE:
				BK4819_SetCTCSSFrequency(CTCSS_OPTIONS[g_current_vfo->p_tx->code]);
				break;

			case CODE_TYPE_DIGITAL:
			case CODE_TYPE_REVERSE_DIGITAL:
				BK4819_SetCDCSSCodeWord(DCS_GetGolayCodeWord(g_current_vfo->p_tx->code_type, g_current_vfo->p_tx->code));
				break;
		}
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
#if 0
		if (g_rx_vfo_is_active)
		{	// use the TX vfo
			g_eeprom.rx_vfo    = g_eeprom.tx_vfo;
			g_rx_vfo           = &g_eeprom.vfo_info[g_eeprom.tx_vfo];
			g_rx_vfo_is_active = false;
		}
		g_current_vfo = g_rx_vfo;
#else
		if (!g_rx_vfo_is_active)
		{	// use the current RX vfo
			g_eeprom.rx_vfo    = g_eeprom.tx_vfo;
			g_rx_vfo           = &g_eeprom.vfo_info[g_eeprom.tx_vfo];
			g_rx_vfo_is_active = true;
		}
		g_current_vfo = g_rx_vfo;
#endif

		g_update_status = true;
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
	if (FREQUENCY_tx_freq_check(g_current_vfo->p_tx->frequency) == 0)
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
			g_dtmf_is_tx                    = true;
			g_dtmf_call_state               = DTMF_CALL_STATE_NONE;
			g_dtmf_tx_stop_count_down_500ms = dtmf_txstop_countdown_500ms;
		}
		else
		{
			g_dtmf_call_state = DTMF_CALL_STATE_CALL_OUT;
			g_dtmf_is_tx      = false;
		}
	}

	FUNCTION_Select(FUNCTION_TRANSMIT);

	g_tx_timer_count_down_500ms = 0;    // no timeout

	#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
		if (g_alarm_state == ALARM_STATE_OFF)
	#endif
	{
		if (g_eeprom.tx_timeout_timer == 0)
			g_tx_timer_count_down_500ms = 60;   // 30 sec
		else
		if (g_eeprom.tx_timeout_timer < (ARRAY_SIZE(g_sub_menu_tx_timeout) - 1))
			g_tx_timer_count_down_500ms = 120 * g_eeprom.tx_timeout_timer;  // minutes
		else
			g_tx_timer_count_down_500ms = 120 * 15;  // 15 minutes
	}

	g_tx_timeout_reached = false;
	g_flag_end_tx        = false;
	g_rtte_count_down    = 0;
	g_dtmf_reply_state   = DTMF_REPLY_NONE;
}

void RADIO_EnableCxCSS(void)
{
	switch (g_current_vfo->p_tx->code_type)
	{
		default:
		case CODE_TYPE_NONE:
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
	RADIO_setup_registers(true);
}

void RADIO_tx_eot(void)
{
	#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
		if (g_alarm_state != ALARM_STATE_OFF)
		{	// don't send EOT if TX'ing tone/alarm
			BK4819_ExitDTMF_TX(true);
			return;
		}
	#endif

	if (g_dtmf_call_state == DTMF_CALL_STATE_NONE &&
	   (g_current_vfo->dtmf_ptt_id_tx_mode == PTT_ID_TX_DOWN || g_current_vfo->dtmf_ptt_id_tx_mode == PTT_ID_BOTH))
	{	// end-of-tx
		if (g_eeprom.dtmf_side_tone)
		{
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
			g_speaker_enabled = true;
			SYSTEM_DelayMs(60);
		}
		BK4819_EnterDTMF_TX(g_eeprom.dtmf_side_tone);
		BK4819_PlayDTMFString(
				g_eeprom.dtmf_key_down_code,
				0,
				g_eeprom.dtmf_first_code_persist_time,
				g_eeprom.dtmf_hash_code_persist_time,
				g_eeprom.dtmf_code_persist_time,
				g_eeprom.dtmf_code_interval_time);

		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
		g_speaker_enabled = false;
	}
	else
	if (g_eeprom.roger_mode == ROGER_MODE_ROGER)
	{
		BK4819_PlayRoger();
	}
	else
#ifdef ENABLE_MDC1200
//	if (g_eeprom.roger_mode == ROGER_MODE_MDC)
	if (g_current_vfo->mdc1200_mode == MDC1200_MODE_EOT || g_current_vfo->mdc1200_mode == MDC1200_MODE_BOTH)
	{
		BK4819_send_MDC1200(MDC1200_OP_CODE_POST_ID, 0x00, g_eeprom.mdc1200_id);
	}
	else
#endif
	if (g_current_vfo->dtmf_ptt_id_tx_mode == PTT_ID_APOLLO)
	{
		BK4819_PlayTone(APOLLO_TONE2_HZ, APOLLO_TONE_MS, 28);
	}
	
	BK4819_ExitDTMF_TX(true);
}
