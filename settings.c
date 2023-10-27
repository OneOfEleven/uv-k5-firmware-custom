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

#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "driver/eeprom.h"
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
#include "misc.h"
#include "settings.h"

eeprom_config_t g_eeprom;

#ifdef ENABLE_FMRADIO
	void SETTINGS_save_fm(void)
	{
		unsigned int i;

		struct
		{
			uint16_t frequency;
			uint8_t  channel;
			bool     is_channel_selected;
			uint8_t  padding[4];
		} state;

		memset(&state, 0xFF, sizeof(state));
		state.channel             = g_eeprom.fm_selected_channel;
		state.frequency           = g_eeprom.fm_selected_frequency;
		state.is_channel_selected = g_eeprom.fm_channel_mode;

		EEPROM_WriteBuffer8(0x0E88, &state);

		for (i = 0; i < 5; i++)
			EEPROM_WriteBuffer8(0x0E40 + (i * 8), &g_fm_channels[i * 4]);
	}
#endif

void SETTINGS_save_vfo_indices(void)
{
	uint8_t State[8];

	#ifndef ENABLE_NOAA
		EEPROM_ReadBuffer(0x0E80, State, sizeof(State));
	#endif

	State[0] = g_eeprom.screen_channel[0];
	State[1] = g_eeprom.user_channel[0];
	State[2] = g_eeprom.freq_channel[0];
	State[3] = g_eeprom.screen_channel[1];
	State[4] = g_eeprom.user_channel[1];
	State[5] = g_eeprom.freq_channel[1];
	#ifdef ENABLE_NOAA
		State[6] = g_eeprom.noaa_channel[0];
		State[7] = g_eeprom.noaa_channel[1];
	#endif

	EEPROM_WriteBuffer8(0x0E80, State);
}

void SETTINGS_save(void)
{
	uint8_t State[8];

	State[0] = g_eeprom.chan_1_call;
	State[1] = g_eeprom.squelch_level;
	State[2] = g_eeprom.tx_timeout_timer;
	#ifdef ENABLE_NOAA
		State[3] = g_eeprom.noaa_auto_scan;
	#else
		State[3] = false;
	#endif
	
	#ifdef ENABLE_KEYLOCK
		State[4] = g_eeprom.key_lock;
	#else
		State[4] = false;
	#endif
	#ifdef ENABLE_VOX
		State[5] = g_eeprom.vox_switch;
		State[6] = g_eeprom.vox_level;
	#else
		State[5] = false;
		State[6] = 0;
	#endif
	State[7] = g_eeprom.mic_sensitivity;
	EEPROM_WriteBuffer8(0x0E70, State);

	#ifdef ENABLE_CONTRAST
		State[0] = g_setting_contrast;
	#else
		State[0] = 0xFF;
	#endif
	State[1] = g_eeprom.channel_display_mode;
	State[2] = g_eeprom.cross_vfo_rx_tx;
	State[3] = g_eeprom.battery_save;
	State[4] = g_eeprom.dual_watch;
	State[5] = g_eeprom.backlight;
	State[6] = g_eeprom.tail_note_elimination;
	State[7] = g_eeprom.vfo_open;
	EEPROM_WriteBuffer8(0x0E78, State);

	State[0] = g_eeprom.beep_control;
	State[1] = g_eeprom.key1_short_press_action;
	State[2] = g_eeprom.key1_long_press_action;
	State[3] = g_eeprom.key2_short_press_action;
	State[4] = g_eeprom.key2_long_press_action;
	State[5] = g_eeprom.scan_resume_mode;
	State[6] = g_eeprom.auto_keypad_lock;
	State[7] = g_eeprom.pwr_on_display_mode;
	EEPROM_WriteBuffer8(0x0E90, State);

	{
		struct {
			uint32_t     password;
			#ifdef ENABLE_MDC1200
				uint16_t mdc1200_id;     // 1of11
				uint8_t  spare[2];
			#else
				uint8_t  spare[4];
			#endif
		} __attribute__((packed)) array;

		memset(&array, 0xff, sizeof(array));
		#ifdef ENABLE_PWRON_PASSWORD
			array.password = g_eeprom.power_on_password;
		#endif
		#ifdef ENABLE_MDC1200
			array.mdc1200_id = g_eeprom.mdc1200_id;
		#endif

		EEPROM_WriteBuffer8(0x0E98, &array);
	}

	#ifdef ENABLE_VOICE
		memset(State, 0xFF, sizeof(State));
		State[0] = g_eeprom.voice_prompt;
		EEPROM_WriteBuffer8(0x0EA0, State);
	#endif

	// *****************************

	{
		struct {
			uint8_t  alarm_mode;
			uint8_t  roger_mode;
			uint8_t  repeater_tail_tone_elimination;
			uint8_t  tx_vfo;
			uint32_t air_copy_freq;
		} __attribute__((packed)) array;

		memset(&array, 0xff, sizeof(array));

		#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
			array.alarm_mode                 = g_eeprom.alarm_mode;
		#else
			array.alarm_mode                 = false;
		#endif
		array.roger_mode                     = g_eeprom.roger_mode;
		array.repeater_tail_tone_elimination = g_eeprom.repeater_tail_tone_elimination;
		array.tx_vfo                         = g_eeprom.tx_vfo;
		#ifdef ENABLE_AIRCOPY_REMEMBER_FREQ
			// remember the AIRCOPY frequency
			array.air_copy_freq              = g_aircopy_freq;
		#endif

		EEPROM_WriteBuffer8(0x0EA8, &array);
	}

	State[0] = g_eeprom.dtmf_side_tone;
	State[1] = g_eeprom.dtmf_separate_code;
	State[2] = g_eeprom.dtmf_group_call_code;
	State[3] = g_eeprom.dtmf_decode_response;
	State[4] = g_eeprom.dtmf_auto_reset_time;
	State[5] = g_eeprom.dtmf_preload_time / 10U;
	State[6] = g_eeprom.dtmf_first_code_persist_time / 10U;
	State[7] = g_eeprom.dtmf_hash_code_persist_time / 10U;
	EEPROM_WriteBuffer8(0x0ED0, State);

	memset(State, 0xFF, sizeof(State));
	State[0] = g_eeprom.dtmf_code_persist_time / 10U;
	State[1] = g_eeprom.dtmf_code_interval_time / 10U;
	State[2] = g_eeprom.permit_remote_kill;
	EEPROM_WriteBuffer8(0x0ED8, State);

	State[0] = g_eeprom.scan_list_default;
	State[1] = g_eeprom.scan_list_enabled[0];
	State[2] = g_eeprom.scan_list_priority_ch1[0];
	State[3] = g_eeprom.scan_list_priority_ch2[0];
	State[4] = g_eeprom.scan_list_enabled[1];
	State[5] = g_eeprom.scan_list_priority_ch1[1];
	State[6] = g_eeprom.scan_list_priority_ch2[1];
	State[7] = 0xFF;
	EEPROM_WriteBuffer8(0x0F18, State);

	memset(State, 0xFF, sizeof(State));
	State[0]  = g_setting_freq_lock;
	State[1]  = g_setting_350_tx_enable;
	#ifdef ENABLE_KILL_REVIVE
		State[2] = g_setting_radio_disabled;
	#else
		State[2] = false;
	#endif
	State[3]  = g_setting_174_tx_enable;
	State[4]  = g_setting_470_tx_enable;
	State[5]  = g_setting_350_enable;
	if (!g_setting_scramble_enable)       State[6] &= ~(1u << 0);
	#ifdef ENABLE_RX_SIGNAL_BAR
		if (!g_setting_rssi_bar)          State[6] &= ~(1u << 1);
	#endif
	if (!g_setting_tx_enable)             State[7] &= ~(1u << 0);
	if (!g_setting_live_dtmf_decoder)     State[7] &= ~(1u << 1);
	State[7] = (State[7] & ~(3u << 2)) | ((g_setting_battery_text & 3u) << 2);
	#ifdef ENABLE_TX_AUDIO_BAR
		if (!g_setting_mic_bar)           State[7] &= ~(1u << 4);
	#endif
	#ifdef ENABLE_AM_FIX
//		if (!g_setting_am_fix)            State[7] &= ~(1u << 5);
	#endif
	State[7] = (State[7] & ~(3u << 6)) | ((g_setting_backlight_on_tx_rx & 3u) << 6);
	EEPROM_WriteBuffer8(0x0F40, State);

	memset(State, 0xFF, sizeof(State));
	State[0] = g_eeprom.scan_hold_time_500ms;
	EEPROM_WriteBuffer8(0x0F48, State);
}

void SETTINGS_save_channel(const unsigned int channel, const unsigned int vfo, const vfo_info_t *p_vfo, const unsigned int mode)
{
	unsigned int eeprom_addr = channel * 16;
	t_channel    m_channel;

	if (IS_NOAA_CHANNEL(channel))
		return;

	if (mode < 2 && channel <= USER_CHANNEL_LAST)
		return;

	if (IS_FREQ_CHANNEL(channel))
		eeprom_addr  = 0x0C80 + (16 * vfo) + ((channel - FREQ_CHANNEL_FIRST) * 16 * 2);  // a VFO

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		UART_printf("sav_chan %04X  %3u %u %u\r\n", eeprom_addr, channel, vfo, mode);
	#endif

	// ****************

	if (p_vfo != NULL)
	{
		memset(&m_channel, 0, sizeof(m_channel));
		m_channel.frequency            = p_vfo->freq_config_rx.frequency;
		m_channel.tx_offset            = p_vfo->tx_offset_freq;
		m_channel.rx_ctcss_cdcss_code  = p_vfo->freq_config_rx.code;
		m_channel.tx_ctcss_cdcss_code  = p_vfo->freq_config_tx.code;
		m_channel.rx_ctcss_cdcss_type  = p_vfo->freq_config_rx.code_type;
//		m_channel.unused1:2
		m_channel.tx_ctcss_cdcss_type  = p_vfo->freq_config_tx.code_type;
		#ifdef ENABLE_MDC1200
			m_channel.mdc1200_mode     = p_vfo->mdc1200_mode;
		#endif
		m_channel.tx_offset_dir        = p_vfo->tx_offset_freq_dir;
//		m_channel.unused3:2
		m_channel.am_mode              = p_vfo->am_mode & 1u;
//		m_channel.unused4:3
		m_channel.frequency_reverse    = p_vfo->frequency_reverse;
		m_channel.channel_bandwidth    = p_vfo->channel_bandwidth;
		m_channel.tx_power             = p_vfo->output_power;
		m_channel.busy_channel_lock    = p_vfo->busy_channel_lock;
//		m_channel.unused5:1
		m_channel.compand              = p_vfo->compand;
		m_channel.dtmf_decoding_enable = p_vfo->dtmf_decoding_enable;
		m_channel.dtmf_ptt_id_tx_mode  = p_vfo->dtmf_ptt_id_tx_mode;
//		m_channel.unused6:4
		m_channel.step_setting         = p_vfo->step_setting;
		m_channel.scrambler            = p_vfo->scrambling_type;
		m_channel.squelch_level        = p_vfo->squelch_level;
	}
	else
	if (channel <= USER_CHANNEL_LAST)
	{	// user channel
		memset(&m_channel, 0xff, sizeof(m_channel));
	}

	EEPROM_WriteBuffer8(eeprom_addr + 0, (uint8_t *)(&m_channel) + 0);
	EEPROM_WriteBuffer8(eeprom_addr + 8, (uint8_t *)(&m_channel) + 8);

	// ****************

	SETTINGS_save_chan_attribs_name(channel, p_vfo);

	if (channel <= USER_CHANNEL_LAST)
	{	// user channel, it has a channel name
		const unsigned int eeprom_addr = 0x0F50 + (channel * 16);
		uint8_t            name[16];

		memset(name, (p_vfo != NULL) ? 0x00 : 0xff, sizeof(name));

		#ifndef ENABLE_KEEP_MEM_NAME
			// clear/reset the channel name
			EEPROM_WriteBuffer8(eeprom_addr + 0, name + 0);
			EEPROM_WriteBuffer8(eeprom_addr + 8, name + 8);
		#else
			if (p_vfo != NULL)
				memcpy(name, p_vfo->name, 10);
			if (mode >= 3 || p_vfo == NULL)
			{	// save the channel name
				EEPROM_WriteBuffer8(eeprom_addr + 0, name + 0);
				EEPROM_WriteBuffer8(eeprom_addr + 8, name + 8);
			}
		#endif
	}
}

void SETTINGS_save_chan_attribs_name(const unsigned int channel, const vfo_info_t *p_vfo)
{
	if (channel >= ARRAY_SIZE(g_user_channel_attributes))
		return;

	if (IS_NOAA_CHANNEL(channel))
		return;

	if (p_vfo != NULL)
	{	// channel attributes

		const uint8_t attribs =
			((p_vfo->scanlist_1_participation & 1u) << 7) |
			((p_vfo->scanlist_2_participation & 1u) << 6) |
			((3u)                                   << 4) |
			((p_vfo->band & 7u)                     << 0);

		const unsigned int index = channel & ~7ul;      // eeprom writes are always 8 bytes in length
		g_user_channel_attributes[channel] = attribs;   // remember new attributes
		EEPROM_WriteBuffer8(0x0D60 + index, g_user_channel_attributes + index);
	}
	else
	if (channel <= USER_CHANNEL_LAST)
	{	// user channel
		const unsigned int index = channel & ~7ul;      // eeprom writes are always 8 bytes in length
		g_user_channel_attributes[channel] = 0xff;
		EEPROM_WriteBuffer8(0x0D60 + index, g_user_channel_attributes + index);
	}

	if (channel <= USER_CHANNEL_LAST)
	{	// user memory channel
		const unsigned int index = channel * 16;
		uint8_t            name[16];

		if (p_vfo != NULL)
		{
			memset(name, 0, sizeof(name));
			memcpy(name, p_vfo->name, 10);
		}
		else
		{
			memset(name, 0xff, sizeof(name));
		}

		EEPROM_WriteBuffer8(0x0F50 + 0 + index, name + 0);
		EEPROM_WriteBuffer8(0x0F50 + 8 + index, name + 8);
	}
}
