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

#include "app/app.h"
#include "app/dtmf.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#ifdef ENABLE_AM_FIX
	#include "am_fix.h"
#endif
#include "audio.h"
#include "board.h"
#include "bsp/dp32g030/gpio.h"
#include "dcs.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/system.h"
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
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

vfo_info_t      g_vfo_info[2];
vfo_info_t     *g_tx_vfo;
vfo_info_t     *g_rx_vfo;
vfo_info_t     *g_current_vfo;
dcs_code_type_t g_selected_code_type;
dcs_code_type_t g_current_code_type;
uint8_t         g_selected_code;
vfo_state_t     g_vfo_state[2];

bool RADIO_CheckValidChannel(uint16_t Channel, bool bCheckScanList, uint8_t VFO)
{	// return true if the channel appears valid

	unsigned int   i;
	uint8_t        priority_channel[2];

	if (Channel > USER_CHANNEL_LAST)
		return false;

	if (g_eeprom.config.channel_attributes[Channel].band > BAND7_470MHz)
		return false;

	if (bCheckScanList)
	{
		switch (VFO)
		{
			case 0:
				if (g_eeprom.config.channel_attributes[Channel].scanlist1 == 0)
					return false;

				for (i = 0; i < 2; i++)
					priority_channel[i] = g_eeprom.config.setting.priority_scan_list[VFO].channel[i];
				break;

			case 1:
				if (g_eeprom.config.channel_attributes[Channel].scanlist2 == 0)
					return false;

				for (i = 0; i < 2; i++)
					priority_channel[i] = g_eeprom.config.setting.priority_scan_list[VFO].channel[i];
				break;

			default:
				return true;
		}

		if (priority_channel[0] == Channel)
			return false;

		if (priority_channel[1] == Channel)
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

	p_vfo->channel_attributes.band      = FREQUENCY_GetBand(Frequency);
	p_vfo->channel_attributes.scanlist1 = 1;
	p_vfo->channel_attributes.scanlist2 = 1;
	p_vfo->channel.step_setting         = STEP_12_5kHz;
	p_vfo->step_freq                    = STEP_FREQ_TABLE[p_vfo->channel.step_setting];
	p_vfo->channel_save                 = ChannelSave;
	p_vfo->channel.frequency_reverse    = false;
	p_vfo->channel.tx_power             = OUTPUT_POWER_LOW;
	p_vfo->freq_config_rx.frequency     = Frequency;
	p_vfo->freq_config_tx.frequency     = Frequency;
	p_vfo->p_rx                         = &p_vfo->freq_config_rx;
	p_vfo->p_tx                         = &p_vfo->freq_config_tx;
	p_vfo->channel.compand              = 0;  // off
	p_vfo->channel.squelch_level        = 0;  // use main squelch
	p_vfo->freq_in_channel              = 0xff;

	if (ChannelSave == (FREQ_CHANNEL_FIRST + BAND2_108MHz))
		p_vfo->channel.am_mode = 1;    // AM

	RADIO_ConfigureSquelchAndOutputPower(p_vfo);
}

void RADIO_configure_channel(const unsigned int VFO, const unsigned int configure)
{
	unsigned int     channel;
	unsigned int     chan;
	t_channel_attrib attributes;
//	uint16_t         base;
	uint32_t         frequency;
	vfo_info_t      *p_vfo = &g_vfo_info[VFO];

	if (!g_eeprom.config.setting.enable_350)
	{
		if (g_eeprom.config.setting.indices.vfo[VFO].frequency == (FREQ_CHANNEL_LAST - 2))
			g_eeprom.config.setting.indices.vfo[VFO].frequency = FREQ_CHANNEL_LAST - 1;

		if (g_eeprom.config.setting.indices.vfo[VFO].screen == (FREQ_CHANNEL_LAST - 2))
			g_eeprom.config.setting.indices.vfo[VFO].screen = FREQ_CHANNEL_LAST - 1;
	}

	channel = g_eeprom.config.setting.indices.vfo[VFO].screen;

	p_vfo->freq_in_channel = 0xff;

	if (IS_VALID_CHANNEL(channel))
	{
		#ifdef ENABLE_NOAA
			if (channel >= NOAA_CHANNEL_FIRST)
			{
				RADIO_InitInfo(p_vfo, g_eeprom.config.setting.indices.vfo[VFO].screen, NOAA_FREQUENCY_TABLE[channel - NOAA_CHANNEL_FIRST]);
				if (g_eeprom.config.setting.cross_vfo == CROSS_BAND_OFF)
					return;
				g_eeprom.config.setting.cross_vfo = CROSS_BAND_OFF;
				g_update_status = true;
				return;
			}
		#endif

		if (channel <= USER_CHANNEL_LAST)
		{
			channel = RADIO_FindNextChannel(channel, SCAN_STATE_DIR_FORWARD, false, VFO);
			if (!IS_VALID_CHANNEL(channel))
			{
				channel = g_eeprom.config.setting.indices.vfo[VFO].frequency;
				g_eeprom.config.setting.indices.vfo[VFO].screen = channel;
			}
			else
			{
				g_eeprom.config.setting.indices.vfo[VFO].screen = channel;
				g_eeprom.config.setting.indices.vfo[VFO].user   = channel;
			}
		}
	}
	else
	{
		channel = FREQ_CHANNEL_LAST - 1;
	}

	chan = CHANNEL_NUM(channel, VFO);

	attributes = g_eeprom.config.channel_attributes[channel];

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		UART_printf("config chan 1   %u %u   %u %u %u\r\n", channel, chan, attributes.band, attributes.scanlist1, attributes.scanlist2);
	#endif

	if (attributes.attributes == 0xff)
	{	// invalid/unused channel

		unsigned int index;

		if (channel <= USER_CHANNEL_LAST)
		{
			channel = g_eeprom.config.setting.indices.vfo[VFO].frequency;
			g_eeprom.config.setting.indices.vfo[VFO].screen = channel;
		}

		index = channel - FREQ_CHANNEL_FIRST;

		RADIO_InitInfo(p_vfo, channel, FREQ_BAND_TABLE[index].lower);
		return;
	}

	if (attributes.band > BAND7_470MHz)
		attributes.band = BAND6_400MHz;

	if (channel <= USER_CHANNEL_LAST)
	{	// USER channel
		p_vfo->channel_attributes = attributes;
	}
	else
	if (IS_FREQ_CHANNEL(channel))
	{	// VFO channel
		attributes.band = channel - FREQ_CHANNEL_FIRST;
		p_vfo->channel_attributes = attributes;
		#if 0
			// don't allow the VFO's to change their scanlist bits
			p_vfo->channel_attributes.scanlist2 = 1;
			p_vfo->channel_attributes.scanlist1 = 1;
		#endif
	}

	p_vfo->channel_save = channel;

//	if (channel <= USER_CHANNEL_LAST)
//		base = channel * 16;
//	else
//		base = 0x0C80 + ((channel - FREQ_CHANNEL_FIRST) * 16 * 2) + (VFO * 16);  // VFO channel

	if (configure == VFO_CONFIGURE_RELOAD || IS_FREQ_CHANNEL(channel))
	{
//		EEPROM_ReadBuffer(Base, &m_channel, sizeof(t_channel));

//		EEPROM_ReadBuffer(Base, p_vfo->channel, sizeof(t_channel));
		memcpy(&p_vfo->channel, &g_eeprom.config.channel[chan], sizeof(t_channel));

		p_vfo->step_freq = STEP_FREQ_TABLE[p_vfo->channel.step_setting];

		p_vfo->channel_attributes = g_eeprom.config.channel_attributes[channel];

		p_vfo->freq_config_rx.frequency = p_vfo->channel.frequency;

		p_vfo->freq_config_rx.code_type = p_vfo->channel.rx_ctcss_cdcss_type;
		p_vfo->freq_config_rx.code      = p_vfo->channel.rx_ctcss_cdcss_code;

		p_vfo->freq_config_tx.code_type = p_vfo->channel.tx_ctcss_cdcss_type;
		p_vfo->freq_config_tx.code      = p_vfo->channel.tx_ctcss_cdcss_code;

		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//			UART_printf("config chan 2   %u %u   %u %u %u  %uHz\r\n", channel, chan, p_vfo->channel_attributes.band, p_vfo->channel_attributes.scanlist1, p_vfo->channel_attributes.scanlist2, p_vfo->channel.frequency * 10);
		#endif
	}
	else
	{
		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//			UART_printf("config chan 3   %u   %u %u %u  %uHz\r\n", channel, p_vfo->channel_attributes.band, p_vfo->channel_attributes.scanlist1, p_vfo->channel_attributes.scanlist2, p_vfo->channel.frequency * 10);
		#endif
	}

	frequency = p_vfo->freq_config_rx.frequency;

	if (frequency < FREQ_BAND_TABLE[attributes.band].lower)
		frequency = FREQ_BAND_TABLE[attributes.band].lower;
	else
	if (frequency >= FREQ_BAND_TABLE[attributes.band].upper)
		frequency = FREQUENCY_floor_to_step(frequency, p_vfo->step_freq, FREQ_BAND_TABLE[attributes.band].lower, FREQ_BAND_TABLE[attributes.band].upper);
	else
	if (channel >= FREQ_CHANNEL_FIRST)
		frequency = FREQUENCY_floor_to_step(frequency, p_vfo->step_freq, FREQ_BAND_TABLE[attributes.band].lower, FREQ_BAND_TABLE[attributes.band].upper);

	if (!g_eeprom.config.setting.enable_350 && frequency >= 35000000 && frequency < 40000000)
	{	// 350~400Mhz not allowed

		// hop onto the next band up
		frequency                        = 43350000;
		p_vfo->freq_config_rx.frequency  = frequency;
		p_vfo->freq_config_tx.frequency  = frequency;
		attributes.band                  = FREQUENCY_GetBand(frequency);
		p_vfo->channel_attributes.band   = attributes.band;
		p_vfo->channel.frequency_reverse = 0;
		p_vfo->channel.tx_offset_dir     = TX_OFFSET_FREQ_DIR_OFF;
		p_vfo->channel.tx_offset         = 0;


		// TODO: also update other settings such as step size


	}

	p_vfo->freq_config_rx.frequency = frequency;

	if (frequency >= AIR_BAND.lower && frequency < AIR_BAND.upper)
	{	// air band
		p_vfo->channel.tx_offset_dir = TX_OFFSET_FREQ_DIR_OFF;
		p_vfo->channel.tx_offset     = 0;
	}
	else
	if (channel > USER_CHANNEL_LAST)
	{
		p_vfo->channel.tx_offset = FREQUENCY_floor_to_step(p_vfo->channel.tx_offset + (p_vfo->step_freq / 2), p_vfo->step_freq, 0, p_vfo->channel.tx_offset + p_vfo->step_freq);
	}

	RADIO_ApplyOffset(p_vfo, true);

	// channel name
	memset(&p_vfo->channel_name, 0, sizeof(p_vfo->channel_name));
	if (channel <= USER_CHANNEL_LAST)
//		EEPROM_ReadBuffer(0x0F50 + (channel * 16), p_vfo->channel_name, 10);	// only 10 bytes used
		memcpy(p_vfo->channel_name.name, &g_eeprom.config.channel_name[channel].name, sizeof(p_vfo->channel_name.name));

	if (p_vfo->channel.am_mode > 0)
	{	// freq/chan is in AM mode
		// disable stuff, even though it can all still be used with AM ???
		p_vfo->channel.scrambler            = 0;
//		p_vfo->channel.dtmf_decoding_enable = 0;
		p_vfo->freq_config_rx.code_type     = CODE_TYPE_NONE;
		p_vfo->freq_config_tx.code_type     = CODE_TYPE_NONE;
	}

	RADIO_ConfigureSquelchAndOutputPower(p_vfo);

	#ifdef ENABLE_AM_FIX
		if (p_vfo->channel.am_mode > 0 && g_eeprom.config.setting.am_fix)
		{
			AM_fix_reset(VFO);
			AM_fix_10ms(VFO);
		}
		else
		{  // don't do agc in FM mode
			BK4819_DisableAGC();
			BK4819_WriteRegister(0x13, (orig_lnas << 8) | (orig_lna << 5) | (orig_mixer << 3) | (orig_pga << 0));
		}
	#else
		if (p_vfo->am_mode > 0)
		{
			BK4819_EnableAGC();
		}
		else
		{  // don't do agc in FM mode
			BK4819_DisableAGC();
			BK4819_WriteRegister(0x13, (orig_lnas << 8) | (orig_lna << 5) | (orig_mixer << 3) | (orig_pga << 0));
		}
	#endif

//	if (configure == VFO_CONFIGURE_RELOAD || IS_FREQ_CHANNEL(channel))
	if (IS_FREQ_CHANNEL(channel))
		p_vfo->freq_in_channel = SETTINGS_find_channel(frequency); // find channel that has this frequency
}

#ifdef ENABLE_VOX
	void RADIO_enable_vox(unsigned int level)
	{
		uint16_t threshold_enable;
		uint16_t threshold_disable;
	
		if (level > (ARRAY_SIZE(g_eeprom.calib.vox[0].threshold) - 1))
			level = ARRAY_SIZE(g_eeprom.calib.vox[0].threshold) - 1;
	
		// my eeprom values ..
		//
		// vox threshold enable   30 50 70 90 110 130 150 170 200 230 FFFF FFFF
		// vox threshold disable  20 40 60 80 100 120 140 160 190 220 FFFF FFFF
		//
		#ifdef ENABLE_VOX_MORE_SENSITIVE
			// more sensitive
			threshold_enable  = g_eeprom.calib.vox[0].threshold[level] / 3;
			threshold_disable = (threshold_enable > 13) ? threshold_enable - 10 : 3;
		#else
			threshold_enable  = g_eeprom.calib.vox[0].threshold[level];
			threshold_disable = g_eeprom.calib.vox[1].threshold[level];
		#endif
	
		BK4819_EnableVox(threshold_enable, threshold_disable);
	
		BK4819_WriteRegister(0x3F, BK4819_ReadRegister(0x3F) | BK4819_REG_3F_VOX_FOUND | BK4819_REG_3F_VOX_LOST);
	}
#endif

void RADIO_ConfigureSquelchAndOutputPower(vfo_info_t *p_vfo)
{
//	uint8_t          tx_power[3];
//	uint16_t         base;
//	frequency_band_t band;
	uint8_t          squelch_level;

	// *******************************
	// squelch

//	band = FREQUENCY_GetBand(p_vfo->p_rx->frequency);
//	base = (Band < BAND4_174MHz) ? 0x1E60 : 0x1E00;

	squelch_level = (p_vfo->channel.squelch_level > 0) ? p_vfo->channel.squelch_level : g_eeprom.config.setting.squelch_level;

	// note that 'noise' and 'glitch' values are inverted compared to 'rssi' values

	if (squelch_level == 0)
	{	// squelch == 0 (off)
		p_vfo->squelch_open_rssi_thresh    = 0;     // 0 ~ 255
		p_vfo->squelch_close_rssi_thresh   = 0;     // 0 ~ 255

		p_vfo->squelch_open_noise_thresh   = 127;   // 127 ~ 0
		p_vfo->squelch_close_noise_thresh  = 127;   // 127 ~ 0

		p_vfo->squelch_open_glitch_thresh  = 255;   // 255 ~ 0
		p_vfo->squelch_close_glitch_thresh = 255;   // 255 ~ 0
	}
	else
	{	// squelch >= 1
#if 0
		Base += squelch_level;                                                   // my eeprom squelch-1
		                                                                         // VHF   UHF
		EEPROM_ReadBuffer(Base + 0x00, &p_vfo->squelch_open_rssi_thresh,    1);  //  50    10
		EEPROM_ReadBuffer(Base + 0x10, &p_vfo->squelch_close_rssi_thresh,   1);  //  40     5

		EEPROM_ReadBuffer(Base + 0x20, &p_vfo->squelch_open_noise_thresh,   1);  //  65    90
		EEPROM_ReadBuffer(Base + 0x30, &p_vfo->squelch_close_noise_thresh,  1);  //  70   100

		EEPROM_ReadBuffer(Base + 0x40, &p_vfo->squelch_close_glitch_thresh, 1);  //  90    90      BUG  ??? .. these 2 swapped ?
		EEPROM_ReadBuffer(Base + 0x50, &p_vfo->squelch_open_glitch_thresh,  1);  // 100   100       "            "
#else
		unsigned int band = (unsigned int)FREQUENCY_GetBand(p_vfo->p_rx->frequency);
		band = (band < BAND4_174MHz) ? 1 : 0;

		p_vfo->squelch_open_rssi_thresh    = g_eeprom.calib.squelch_band[band].open_rssi_thresh[squelch_level];
		p_vfo->squelch_close_rssi_thresh   = g_eeprom.calib.squelch_band[band].close_rssi_thresh[squelch_level];

		p_vfo->squelch_open_noise_thresh   = g_eeprom.calib.squelch_band[band].open_noise_thresh[squelch_level];
		p_vfo->squelch_close_noise_thresh  = g_eeprom.calib.squelch_band[band].close_noise_thresh[squelch_level];

		p_vfo->squelch_open_glitch_thresh  = g_eeprom.calib.squelch_band[band].open_glitch_thresh[squelch_level];
		p_vfo->squelch_close_glitch_thresh = g_eeprom.calib.squelch_band[band].close_glitch_thresh[squelch_level];
#endif
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

	{
		// my calibration data
		//
		// 1ED0    32 32 32   64 64 64   8C 8C 8C   FF FF FF FF FF FF FF ..  50 MHz
		// 1EE0    32 32 32   64 64 64   8C 8C 8C   FF FF FF FF FF FF FF .. 108 MHz
		// 1EF0    5F 5F 5F   69 69 69   91 91 8F   FF FF FF FF FF FF FF .. 137 MHz
		// 1F00    32 32 32   64 64 64   8C 8C 8C   FF FF FF FF FF FF FF .. 174 MHz
		// 1F10    5A 5A 5A   64 64 64   82 82 82   FF FF FF FF FF FF FF .. 350 MHz
		// 1F20    5A 5A 5A   64 64 64   8F 91 8A   FF FF FF FF FF FF FF .. 400 MHz
		// 1F30    32 32 32   64 64 64   8C 8C 8C   FF FF FF FF FF FF FF .. 470 MHz
	
		uint8_t tx_power[3];
		const unsigned int band = (unsigned int)FREQUENCY_GetBand(p_vfo->p_tx->frequency);
	
//		EEPROM_ReadBuffer(0x1ED0 + (band * 16) + (p_vfo->output_power * 3), tx_power, 3);
		memcpy(&tx_power, &g_eeprom.calib.tx_band_power[band].level[p_vfo->channel.tx_power], 3);

		#ifdef ENABLE_REDUCE_LOW_MID_TX_POWER
			// make low and mid even lower
			if (p_vfo->channel.tx_power == OUTPUT_POWER_LOW)
			{
				tx_power[0] /= 5;    //tx_power[0] /= 8;
				tx_power[1] /= 5;    //tx_power[1] /= 8;
				tx_power[2] /= 5;    //tx_power[2] /= 8; get more low power
			}
			else
			if (p_vfo->channel.tx_power == OUTPUT_POWER_MID)
			{
				tx_power[0] /= 3;    //tx_power[0] /= 5;
				tx_power[1] /= 3;    //tx_power[1] /= 5;
				tx_power[2] /= 3;    //tx_power[2] /= 5;   get more low power
			}
		#endif
	
		p_vfo->txp_calculated_setting = FREQUENCY_CalculateOutputPower(
			tx_power[0],
			tx_power[1],
			tx_power[2],
			FREQ_BAND_TABLE[band].lower,
			(FREQ_BAND_TABLE[band].lower + FREQ_BAND_TABLE[band].upper) / 2,
			FREQ_BAND_TABLE[band].upper,
			p_vfo->p_tx->frequency);
	}

	// *******************************
}

void RADIO_ApplyOffset(vfo_info_t *p_vfo, const bool set_pees)
{
	uint32_t Frequency = p_vfo->freq_config_rx.frequency;

	switch (p_vfo->channel.tx_offset_dir)
	{
		case TX_OFFSET_FREQ_DIR_OFF:
			break;
		case TX_OFFSET_FREQ_DIR_ADD:
			Frequency += p_vfo->channel.tx_offset;
			break;
		case TX_OFFSET_FREQ_DIR_SUB:
			Frequency -= p_vfo->channel.tx_offset;
			break;
	}

	if (Frequency < FREQ_BAND_TABLE[0].lower)
		Frequency = FREQ_BAND_TABLE[0].lower;
	else
	if (Frequency > FREQ_BAND_TABLE[ARRAY_SIZE(FREQ_BAND_TABLE) - 1].upper)
		Frequency = FREQ_BAND_TABLE[ARRAY_SIZE(FREQ_BAND_TABLE) - 1].upper;

	p_vfo->freq_config_tx.frequency = Frequency;

	if (set_pees)
	{
		if (!p_vfo->channel.frequency_reverse)
		{
			p_vfo->p_rx = &p_vfo->freq_config_rx;
			p_vfo->p_tx = &p_vfo->freq_config_tx;
		}
		else
		{
			p_vfo->p_rx = &p_vfo->freq_config_tx;
			p_vfo->p_tx = &p_vfo->freq_config_rx;
		}
	}
}

static void RADIO_SelectCurrentVfo(void)
{
 	g_current_vfo = (g_eeprom.config.setting.cross_vfo == CROSS_BAND_OFF) ? g_rx_vfo : &g_vfo_info[g_eeprom.config.setting.tx_vfo_num];
}

void RADIO_select_vfos(void)
{
	g_eeprom.config.setting.tx_vfo_num = get_TX_VFO();
	g_rx_vfo_num = (g_eeprom.config.setting.cross_vfo == CROSS_BAND_OFF) ? g_eeprom.config.setting.tx_vfo_num : (g_eeprom.config.setting.tx_vfo_num + 1) & 1u;

	g_tx_vfo = &g_vfo_info[g_eeprom.config.setting.tx_vfo_num];
	g_rx_vfo = &g_vfo_info[g_rx_vfo_num];

	RADIO_SelectCurrentVfo();
}

void RADIO_setup_registers(bool switch_to_function_foreground)
{
	BK4819_filter_bandwidth_t Bandwidth = g_rx_vfo->channel.channel_bandwidth;
	uint16_t                  interrupt_mask;
	uint32_t                  Frequency;

	if (!g_monitor_enabled)
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

	// turn green LED off
	BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, false);

	switch (Bandwidth)
	{
		default:
			Bandwidth = BK4819_FILTER_BW_WIDE;

			// Fallthrough

		case BK4819_FILTER_BW_WIDE:
		case BK4819_FILTER_BW_NARROW:
			#ifdef ENABLE_AM_FIX
				#if 0
//					BK4819_SetFilterBandwidth(Bandwidth, g_rx_vfo->channel.am_mode > 0 && g_eeprom.config.setting.am_fix);
					BK4819_SetFilterBandwidth(Bandwidth, true);
				#else
					if (g_rx_vfo->channel.am_mode > 1)
						BK4819_SetFilterBandwidth(BK4819_FILTER_BW_NARROWER, false);
					else
						BK4819_SetFilterBandwidth(Bandwidth, true);
				#endif
			#else
				BK4819_SetFilterBandwidth(Bandwidth, false);
			#endif
			break;
	}

	BK4819_WriteRegister(0x30, 0);
	BK4819_WriteRegister(0x30, 
		BK4819_REG_30_ENABLE_VCO_CALIB |
//		BK4819_REG_30_ENABLE_UNKNOWN   |
		BK4819_REG_30_ENABLE_RX_LINK   |
		BK4819_REG_30_ENABLE_AF_DAC    |
		BK4819_REG_30_ENABLE_DISC_MODE |
		BK4819_REG_30_ENABLE_PLL_VCO   |
//		BK4819_REG_30_ENABLE_PA_GAIN   |
//		BK4819_REG_30_ENABLE_MIC_ADC   |
//		BK4819_REG_30_ENABLE_TX_DSP    |
		BK4819_REG_30_ENABLE_RX_DSP    |
	0);

	BK4819_set_GPIO_pin(BK4819_GPIO5_PIN1_RED, false);         // LED off
	BK4819_SetupPowerAmplifier(0, 0);
	BK4819_set_GPIO_pin(BK4819_GPIO1_PIN29_PA_ENABLE, false);  // PA off

	while (1)
	{	// wait for interrupts to clear
		const uint16_t int_bits = BK4819_ReadRegister(0x0C);
		if ((int_bits & (1u << 0)) == 0)
			break;
		BK4819_WriteRegister(0x02, 0);   // clear the interrupt bits
		SYSTEM_DelayMs(1);
	}
	BK4819_WriteRegister(0x3F, 0);       // disable interrupts

	#ifdef ENABLE_NOAA
		if (IS_NOAA_CHANNEL(g_rx_vfo->channel_save) && g_noaa_mode)
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

	// enable the RX front end
	BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_RX_ENABLE, true);

	// AF RX Gain and DAC
//	if (g_rx_vfo->channel.am_mode > 0)
//	{
//		BK4819_WriteRegister(0x48, 0xB3A8);   // 1011 0011 1010 1000
//	}
//	else
	{
		BK4819_WriteRegister(0x48,
			(11u << 12)                        |     // ??? .. 0 ~ 15, doesn't seem to make any difference
			( 0u << 10)                        |     // AF Rx Gain-1
			(g_eeprom.calib.volume_gain << 4) |     // AF Rx Gain-2
			(g_eeprom.calib.dac_gain    << 0));     // AF DAC Gain (after Gain-1 and Gain-2)
	}

	#ifdef ENABLE_VOICE
		#ifdef MUTE_AUDIO_FOR_VOICE
			if (g_voice_write_index == 0)
				AUDIO_set_mod_mode(g_rx_vfo->channel.am_mode);
		#else
			AUDIO_set_mod_mode(g_rx_vfo->channel.am_mode);
		#endif
	#else
		AUDIO_set_mod_mode(g_rx_vfo->channel.am_mode);
	#endif

	interrupt_mask = BK4819_REG_3F_SQUELCH_FOUND | BK4819_REG_3F_SQUELCH_LOST;

	if (IS_NOT_NOAA_CHANNEL(g_rx_vfo->channel_save))
	{
		if (g_rx_vfo->channel.am_mode == 0)
		{	// FM
			uint8_t code_type = g_selected_code_type;
			uint8_t code      = g_selected_code;

			if (g_css_scan_mode == CSS_SCAN_MODE_OFF)
			{
				code_type = g_rx_vfo->p_rx->code_type;
				code      = g_rx_vfo->p_rx->code;
			}

			switch (code_type)
			{
				default:
				case CODE_TYPE_NONE:
					BK4819_set_CTCSS_freq(670);
					BK4819_set_tail_detection(550);		// QS's 55Hz tone method

					interrupt_mask |= BK4819_REG_3F_CxCSS_TAIL;
					break;

				case CODE_TYPE_CONTINUOUS_TONE:
					BK4819_set_CTCSS_freq(CTCSS_TONE_LIST[code]);

//					#ifdef ENABLE_CTCSS_TAIL_PHASE_SHIFT
//						BK4819_set_tail_detection(CTCSS_TONE_LIST[code]); // doesn't work in RX mode
//					#else
//						BK4819_set_tail_detection(550);		// QS's 55Hz tone method
//					#endif

					interrupt_mask |=
						BK4819_REG_3F_CxCSS_TAIL |
						BK4819_REG_3F_CTCSS_FOUND |
						BK4819_REG_3F_CTCSS_LOST;

					break;

				case CODE_TYPE_DIGITAL:
				case CODE_TYPE_REVERSE_DIGITAL:
					BK4819_set_CDCSS_code(DCS_GetGolayCodeWord(code_type, code));

					interrupt_mask |=
						BK4819_REG_3F_CxCSS_TAIL |
						BK4819_REG_3F_CDCSS_FOUND |
						BK4819_REG_3F_CDCSS_LOST;

					break;
			}

			if (g_rx_vfo->channel.scrambler > 0 && g_eeprom.config.setting.enable_scrambler)
				BK4819_EnableScramble(g_rx_vfo->channel.scrambler - 1);
			else
				BK4819_DisableScramble();
		}
	}
	#ifdef ENABLE_NOAA
		else
		{
			BK4819_set_CTCSS_freq(0);      // NOAA 1050Hz stuff

			interrupt_mask |= BK4819_REG_3F_CTCSS_FOUND | BK4819_REG_3F_CTCSS_LOST;
		}
	#endif

	#ifdef ENABLE_VOX
		if (
			#ifdef ENABLE_FMRADIO
				!g_fm_radio_mode &&
			#endif
			g_eeprom.config.setting.vox_enabled &&
			IS_NOT_NOAA_CHANNEL(g_current_vfo->channel_save) &&
			g_current_vfo->channel.am_mode == 0)
		{
			RADIO_enable_vox(g_eeprom.config.setting.vox_level);
			interrupt_mask |= BK4819_REG_3F_VOX_FOUND | BK4819_REG_3F_VOX_LOST;
		}
		else
	#endif
			BK4819_DisableVox();

	// RX expander
	BK4819_SetCompander((g_rx_vfo->channel.am_mode == 0 && g_rx_vfo->channel.compand >= 2) ? g_rx_vfo->channel.compand : 0);

	BK4819_EnableDTMF();
	interrupt_mask |= BK4819_REG_3F_DTMF_5TONE_FOUND;

	#ifdef ENABLE_MDC1200
		BK4819_enable_mdc1200_rx(true);
		interrupt_mask |= BK4819_REG_3F_FSK_RX_SYNC | BK4819_REG_3F_FSK_RX_FINISHED | BK4819_REG_3F_FSK_FIFO_ALMOST_FULL;
	#endif

	// enable BK4819 interrupts
	BK4819_WriteRegister(0x3F, interrupt_mask);

	FUNCTION_Init();

	if (switch_to_function_foreground)
		FUNCTION_Select(FUNCTION_FOREGROUND);

//	if (g_monitor_enabled)
//		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
}

#ifdef ENABLE_NOAA
	void RADIO_ConfigureNOAA(void)
	{
		uint8_t ChanAB;

		g_update_status = true;

		if (g_eeprom.config.setting.noaa_auto_scan)
		{
			if (g_eeprom.config.setting.dual_watch != DUAL_WATCH_OFF)
			{
				if (IS_NOT_NOAA_CHANNEL(g_eeprom.config.setting.indices.vfo[0].screen))
				{
					if (IS_NOT_NOAA_CHANNEL(g_eeprom.config.setting.indices.vfo[1].screen))
					{
						g_noaa_mode = false;
						return;
					}
					ChanAB = 1;
				}
				else
					ChanAB = 0;

				if (!g_noaa_mode)
					g_noaa_channel = g_vfo_info[ChanAB].channel_save - NOAA_CHANNEL_FIRST;

				g_noaa_mode = true;
				return;
			}

			if (g_rx_vfo->channel_save >= NOAA_CHANNEL_FIRST)
			{
				g_noaa_mode          = true;
				g_noaa_channel         = g_rx_vfo->channel_save - NOAA_CHANNEL_FIRST;
				g_noaa_tick_10ms = noaa_tick_2_10ms;
				g_schedule_noaa        = false;
			}
			else
				g_noaa_mode = false;
		}
		else
			g_noaa_mode = false;
	}
#endif

void RADIO_enableTX(const bool fsk_tx)
{
	BK4819_filter_bandwidth_t Bandwidth = g_current_vfo->channel.channel_bandwidth;

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

	BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_RX_ENABLE, false);

	switch (Bandwidth)
	{
		default:
			Bandwidth = BK4819_FILTER_BW_WIDE;

			// Fallthrough

		case BK4819_FILTER_BW_WIDE:
		case BK4819_FILTER_BW_NARROW:
			#ifdef ENABLE_AM_FIX
				#if 0
//					BK4819_SetFilterBandwidth(Bandwidth, g_current_vfo->channel.am_mode > 0 && g_eeprom.config.setting.am_fix);
					BK4819_SetFilterBandwidth(Bandwidth, true);
				#else
					if (g_current_vfo->channel.am_mode > 1)
						BK4819_SetFilterBandwidth(BK4819_FILTER_BW_NARROWER, false);
					else
						BK4819_SetFilterBandwidth(Bandwidth, true);
				#endif
			#else
				BK4819_SetFilterBandwidth(Bandwidth, false);
			#endif
			break;
	}

	// if DTMF is enabled when TX'ing, it changes the TX audio filtering ! .. 1of11
	// so MAKE SURE that DTMF is disabled - until needed
	BK4819_DisableDTMF();

	BK4819_SetCompander((!fsk_tx && g_rx_vfo->channel.am_mode == 0 && (g_rx_vfo->channel.compand == 1 || g_rx_vfo->channel.compand >= 3)) ? g_rx_vfo->channel.compand : 0);

	BK4819_set_rf_frequency(g_current_vfo->p_tx->frequency, false);
	BK4819_set_rf_filter_path(g_current_vfo->p_tx->frequency);

	BK4819_PrepareTransmit();
	BK4819_set_GPIO_pin(BK4819_GPIO1_PIN29_PA_ENABLE, true);                // PA on
	if (g_current_display_screen != DISPLAY_AIRCOPY)
		BK4819_SetupPowerAmplifier(g_current_vfo->txp_calculated_setting, g_current_vfo->p_tx->frequency);
	else
		BK4819_SetupPowerAmplifier(0, g_current_vfo->p_tx->frequency);      // very low power when in AIRCOPY mode

	BK4819_set_GPIO_pin(BK4819_GPIO5_PIN1_RED, true);                       // turn the RED LED on

	if (fsk_tx)
	{
		BK4819_disable_sub_audible();
	}
	else
	{
		switch (g_current_vfo->p_tx->code_type)
		{
			default:
			case CODE_TYPE_NONE:
				BK4819_disable_sub_audible();
				break;

			case CODE_TYPE_CONTINUOUS_TONE:
				BK4819_gen_tail(4);
				BK4819_set_CTCSS_freq(CTCSS_TONE_LIST[g_current_vfo->p_tx->code]);
				break;

			case CODE_TYPE_DIGITAL:
			case CODE_TYPE_REVERSE_DIGITAL:
				BK4819_gen_tail(4);
				BK4819_set_CDCSS_code(DCS_GetGolayCodeWord(g_current_vfo->p_tx->code_type, g_current_vfo->p_tx->code));
				break;
		}
	}
}

void RADIO_set_vfo_state(vfo_state_t State)
{
	if (State == VFO_STATE_NORMAL)
	{
		g_vfo_state[0] = VFO_STATE_NORMAL;
		g_vfo_state[1] = VFO_STATE_NORMAL;
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
			const unsigned int vfo = (g_eeprom.config.setting.cross_vfo == CROSS_BAND_OFF) ? g_rx_vfo_num : g_eeprom.config.setting.tx_vfo_num;
			g_vfo_state[vfo] = State;
		}

		// cause a display update to remove the message
		g_update_screen_tick_500ms = 8;    // 4 seconds
	}

	g_update_display = true;
}

void RADIO_PrepareTX(void)
{
	vfo_state_t State = VFO_STATE_NORMAL;  // default to OK for TX

	if (g_eeprom.config.setting.dual_watch != DUAL_WATCH_OFF)
	{	// dual-RX is enabled
#if 0
		if (g_rx_vfo_is_active)
		{	// use the TX vfo
			g_rx_vfo_num       = g_eeprom.config.setting.tx_vfo_num;
			g_rx_vfo           = &g_vfo_info[g_eeprom.config.setting.tx_vfo_num];
			g_rx_vfo_is_active = false;
		}
		g_current_vfo = g_rx_vfo;
#else
		if (!g_rx_vfo_is_active)
		{	// use the current RX vfo
			g_rx_vfo_num       = g_eeprom.config.setting.tx_vfo_num;
			g_rx_vfo           = &g_vfo_info[g_eeprom.config.setting.tx_vfo_num];
			g_rx_vfo_is_active = true;
		}
		g_current_vfo = g_rx_vfo;
#endif

		g_update_status = true;
	}

	RADIO_SelectCurrentVfo();

	#ifndef ENABLE_TX_WHEN_AM
		if (g_current_vfo->channel.am_mode > 0)
		{	// not allowed to TX if not in FM mode
			State = VFO_STATE_TX_DISABLE;
		}
		else
	#endif
	if (!g_eeprom.config.setting.tx_enable || g_serial_config_tick_500ms > 0)
	{	// TX is disabled or config upload/download in progress
		State = VFO_STATE_TX_DISABLE;
	}
	else
	if (FREQUENCY_tx_freq_check(g_current_vfo->p_tx->frequency) == 0)
	{	// TX frequency is allowed
		if (g_current_vfo->channel.busy_channel_lock && g_current_function == FUNCTION_RECEIVE)
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

		RADIO_set_vfo_state(State);

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
			g_dtmf_tx_stop_tick_500ms = dtmf_txstop_500ms;
		}
		else
		{
			g_dtmf_call_state = DTMF_CALL_STATE_CALL_OUT;
			g_dtmf_is_tx      = false;
		}
	}

	FUNCTION_Select(FUNCTION_TRANSMIT);

	g_tx_timer_tick_500ms = 0;    // no timeout

	#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
		if (g_alarm_state == ALARM_STATE_OFF)
	#endif
	{
		if (g_eeprom.config.setting.tx_timeout == 0)
			g_tx_timer_tick_500ms = 60;   // 30 sec
		else
		if (g_eeprom.config.setting.tx_timeout < (ARRAY_SIZE(g_sub_menu_tx_timeout) - 1))
			g_tx_timer_tick_500ms = 120 * g_eeprom.config.setting.tx_timeout;  // minutes
		else
			g_tx_timer_tick_500ms = 120 * 15;  // 15 minutes
	}

	g_tx_timeout_reached = false;
	g_flag_end_tx        = false;
	g_rtte_count_down    = 0;
	g_dtmf_reply_state   = DTMF_REPLY_NONE;
}

void RADIO_enable_CxCSS_tail(void)
{
	switch (g_current_vfo->p_tx->code_type)
	{
		default:
		case CODE_TYPE_NONE:
			break;

		case CODE_TYPE_CONTINUOUS_TONE:
			BK4819_enable_CTCSS_tail();
			SYSTEM_DelayMs(200);
			break;

		case CODE_TYPE_DIGITAL:
		case CODE_TYPE_REVERSE_DIGITAL:
			BK4819_enable_CDCSS_tail();
			SYSTEM_DelayMs(200);
			break;
	}
}

void RADIO_PrepareCssTX(void)
{
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

	RADIO_PrepareTX();

	SYSTEM_DelayMs(200);

	RADIO_enable_CxCSS_tail();

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
	   (g_current_vfo->channel.dtmf_ptt_id_tx_mode == PTT_ID_TX_DOWN || g_current_vfo->channel.dtmf_ptt_id_tx_mode == PTT_ID_BOTH))
	{	// end-of-tx
		if (g_eeprom.config.setting.dtmf.side_tone)
		{
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

			SYSTEM_DelayMs(60);
		}

		BK4819_EnterDTMF_TX(g_eeprom.config.setting.dtmf.side_tone);
		BK4819_PlayDTMFString(
				g_eeprom.config.setting.dtmf.key_down_code,
				0,
				g_eeprom.config.setting.dtmf.first_code_persist_time * 10,
				g_eeprom.config.setting.dtmf.hash_code_persist_time * 10,
				g_eeprom.config.setting.dtmf.code_persist_time * 10,
				g_eeprom.config.setting.dtmf.code_interval_time * 10);

		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
	}
	else
	if (g_eeprom.config.setting.roger_mode == ROGER_MODE_ROGER)
	{
		BK4819_PlayRoger();
	}
	else
#ifdef ENABLE_MDC1200
//	if (g_eeprom.config.setting.roger_mode == ROGER_MODE_MDC)
	if (g_current_vfo->channel.mdc1200_mode == MDC1200_MODE_EOT || g_current_vfo->channel.mdc1200_mode == MDC1200_MODE_BOTH)
	{
		BK4819_send_MDC1200(MDC1200_OP_CODE_POST_ID, 0x00, g_eeprom.config.setting.mdc1200_id);
	}
	else
#endif
	if (g_current_vfo->channel.dtmf_ptt_id_tx_mode == PTT_ID_APOLLO)
	{
		BK4819_PlayTone(APOLLO_TONE2_HZ, APOLLO_TONE_MS, 28);
	}

	BK4819_ExitDTMF_TX(true);
}
