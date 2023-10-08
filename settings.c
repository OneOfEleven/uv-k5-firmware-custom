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
#include "driver/uart.h"
#include "misc.h"
#include "settings.h"

eeprom_config_t g_eeprom;

#ifdef ENABLE_FMRADIO
	void SETTINGS_SaveFM(void)
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
		state.is_channel_selected = g_eeprom.fm_is_channel_mode;

		EEPROM_WriteBuffer(0x0E88, &state);

		for (i = 0; i < 5; i++)
			EEPROM_WriteBuffer(0x0E40 + (i * 8), &g_fm_channels[i * 4]);
	}
#endif

void SETTINGS_SaveVfoIndices(void)
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

	EEPROM_WriteBuffer(0x0E80, State);
}

void SETTINGS_SaveSettings(void)
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
	State[4] = g_eeprom.key_lock;
	#ifdef ENABLE_VOX
		State[5] = g_eeprom.vox_switch;
		State[6] = g_eeprom.vox_level;
	#else
		State[5] = false;
		State[6] = 0;
	#endif
	State[7] = g_eeprom.mic_sensitivity;
	EEPROM_WriteBuffer(0x0E70, State);

	State[0] = 0xFF;
	State[1] = g_eeprom.channel_display_mode;
	State[2] = g_eeprom.cross_vfo_rx_tx;
	State[3] = g_eeprom.battery_save;
	State[4] = g_eeprom.dual_watch;
	State[5] = g_eeprom.backlight;
	State[6] = g_eeprom.tail_note_elimination;
	State[7] = g_eeprom.vfo_open;
	EEPROM_WriteBuffer(0x0E78, State);

	State[0] = g_eeprom.beep_control;
	State[1] = g_eeprom.key1_short_press_action;
	State[2] = g_eeprom.key1_long_press_action;
	State[3] = g_eeprom.key2_short_press_action;
	State[4] = g_eeprom.key2_long_press_action;
	State[5] = g_eeprom.scan_resume_mode;
	State[6] = g_eeprom.auto_keypad_lock;
	State[7] = g_eeprom.pwr_on_display_mode;
	EEPROM_WriteBuffer(0x0E90, State);

	{
		struct {
			uint32_t password;
			uint32_t spare;
		} __attribute__((packed)) array;

		memset(&array, 0xff, sizeof(array));
		#ifdef ENABLE_PWRON_PASSWORD
			array.password = g_eeprom.power_on_password;
		#endif
		
		EEPROM_WriteBuffer(0x0E98, &array);
	}
	
	#ifdef ENABLE_VOICE
		memset(State, 0xFF, sizeof(State));
		State[0] = g_eeprom.voice_prompt;
		EEPROM_WriteBuffer(0x0EA0, State);
	#endif

	#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
		State[0] = g_eeprom.alarm_mode;
	#else
		State[0] = false;
	#endif
	State[1] = g_eeprom.roger_mode;
	State[2] = g_eeprom.repeater_tail_tone_elimination;
	State[3] = g_eeprom.tx_vfo;
	EEPROM_WriteBuffer(0x0EA8, State);

	State[0] = g_eeprom.dtmf_side_tone;
	State[1] = g_eeprom.dtmf_separate_code;
	State[2] = g_eeprom.dtmf_group_call_code;
	State[3] = g_eeprom.dtmf_decode_response;
	State[4] = g_eeprom.dtmf_auto_reset_time;
	State[5] = g_eeprom.dtmf_preload_time / 10U;
	State[6] = g_eeprom.dtmf_first_code_persist_time / 10U;
	State[7] = g_eeprom.dtmf_hash_code_persist_time / 10U;
	EEPROM_WriteBuffer(0x0ED0, State);

	memset(State, 0xFF, sizeof(State));
	State[0] = g_eeprom.dtmf_code_persist_time / 10U;
	State[1] = g_eeprom.dtmf_code_interval_time / 10U;
	State[2] = g_eeprom.permit_remote_kill;
	EEPROM_WriteBuffer(0x0ED8, State);

	State[0] = g_eeprom.scan_list_default;
	State[1] = g_eeprom.scan_list_enabled[0];
	State[2] = g_eeprom.scan_list_priority_ch1[0];
	State[3] = g_eeprom.scan_list_priority_ch2[0];
	State[4] = g_eeprom.scan_list_enabled[1];
	State[5] = g_eeprom.scan_list_priority_ch1[1];
	State[6] = g_eeprom.scan_list_priority_ch2[1];
	State[7] = 0xFF;
	EEPROM_WriteBuffer(0x0F18, State);

	memset(State, 0xFF, sizeof(State));
	State[0]  = g_setting_f_lock;
	State[1]  = g_setting_350_tx_enable;
	State[2]  = g_setting_killed;
	State[3]  = g_setting_200_tx_enable;
	State[4]  = g_setting_500_tx_enable;
	State[5]  = g_setting_350_enable;
	State[6]  = g_setting_scramble_enable;
	if (!g_Setting_tx_enable)             State[7] &= ~(1u << 0);
	if (!g_setting_live_dtmf_decoder) State[7] &= ~(1u << 1);
	State[7] = (State[7] & ~(3u << 2)) | ((g_setting_battery_text & 3u) << 2);
	#ifdef ENABLE_AUDIO_BAR
		if (!g_setting_mic_bar)           State[7] &= ~(1u << 4);
	#endif
	#ifdef ENABLE_AM_FIX
		if (!g_setting_am_fix)            State[7] &= ~(1u << 5);
	#endif
	State[7] = (State[7] & ~(3u << 6)) | ((g_setting_backlight_on_tx_rx & 3u) << 6);
	 
	EEPROM_WriteBuffer(0x0F40, State);
}

void SETTINGS_SaveChannel(uint8_t Channel, uint8_t VFO, const vfo_info_t *pVFO, uint8_t Mode)
{
	#ifdef ENABLE_NOAA
		if (IS_NOT_NOAA_CHANNEL(Channel))
	#endif
	{
		const uint16_t OffsetMR  = Channel * 16;
		      uint16_t OffsetVFO = OffsetMR;

		if (Channel > USER_CHANNEL_LAST)
		{	// it's a VFO, not a channel
			OffsetVFO  = (VFO == 0) ? 0x0C80 : 0x0C90;
			OffsetVFO += (Channel - FREQ_CHANNEL_FIRST) * 32;
		}

		if (Mode >= 2 || Channel > USER_CHANNEL_LAST)
		{	// copy VFO to a channel

			uint8_t State[8];

			((uint32_t *)State)[0] = pVFO->freq_config_rx.frequency;
			((uint32_t *)State)[1] = pVFO->tx_offset_freq;
			EEPROM_WriteBuffer(OffsetVFO + 0, State);

			State[0] =  pVFO->freq_config_rx.code;
			State[1] =  pVFO->freq_config_tx.code;
			State[2] = (pVFO->freq_config_tx.code_type << 4) | pVFO->freq_config_rx.code_type;
			State[3] = ((pVFO->am_mode & 1u)          << 4) | pVFO->tx_offset_freq_dir;
			State[4] = 0
				| (pVFO->busy_channel_lock << 4)
				| (pVFO->output_power      << 2)
				| (pVFO->channel_bandwidth << 1)
				| (pVFO->frequency_reverse  << 0);
			State[5] = ((pVFO->dtmf_ptt_id_tx_mode & 7u) << 1) | ((pVFO->dtmf_decoding_enable & 1u) << 0);
			State[6] =  pVFO->step_setting;
			State[7] =  pVFO->scrambling_type;
			EEPROM_WriteBuffer(OffsetVFO + 8, State);

			SETTINGS_UpdateChannel(Channel, pVFO, true);

			if (Channel <= USER_CHANNEL_LAST)
			{	// it's a memory channel
		
				#ifndef ENABLE_KEEP_MEM_NAME
					// clear/reset the channel name
					//memset(&State, 0xFF, sizeof(State));
					memset(&State, 0x00, sizeof(State));     // follow the QS way
					EEPROM_WriteBuffer(0x0F50 + OffsetMR, State);
					EEPROM_WriteBuffer(0x0F58 + OffsetMR, State);
				#else
					if (Mode >= 3)
					{	// save the channel name
						memmove(State, pVFO->name + 0, 8);
						EEPROM_WriteBuffer(0x0F50 + OffsetMR, State);
						//memset(State, 0xFF, sizeof(State));
						memset(State, 0x00, sizeof(State));  // follow the QS way
						memmove(State, pVFO->name + 8, 2);
						EEPROM_WriteBuffer(0x0F58 + OffsetMR, State);
					}
				#endif
			}
		}
	}
}

void SETTINGS_UpdateChannel(uint8_t Channel, const vfo_info_t *pVFO, bool keep)
{
	#ifdef ENABLE_NOAA
		if (IS_NOT_NOAA_CHANNEL(Channel))
	#endif
	{
		uint8_t  State[8];
		uint8_t  Attributes = 0xFF;        // default attributes
		uint16_t Offset = 0x0D60 + (Channel & ~7u);
		
		Attributes &= (uint8_t)(~USER_CH_COMPAND);  // default to '0' = compander disabled

		EEPROM_ReadBuffer(Offset, State, sizeof(State));

		if (keep)
		{
			Attributes = (pVFO->scanlist_1_participation << 7) | (pVFO->scanlist_2_participation << 6) | (pVFO->compander << 4) | (pVFO->band << 0);
			if (State[Channel & 7u] == Attributes)
				return; // no change in the attributes
		}

		State[Channel & 7u] = Attributes;

		EEPROM_WriteBuffer(Offset, State);

		g_user_channel_attributes[Channel] = Attributes;

//		#ifndef ENABLE_KEEP_MEM_NAME
			if (Channel <= USER_CHANNEL_LAST)
			{	// it's a memory channel
		
				const uint16_t OffsetMR = Channel * 16;
				if (!keep)
				{	// clear/reset the channel name
					//memset(&State, 0xFF, sizeof(State));
					memset(&State, 0x00, sizeof(State));   // follow the QS way
					EEPROM_WriteBuffer(0x0F50 + OffsetMR, State);
					EEPROM_WriteBuffer(0x0F58 + OffsetMR, State);
				}
//				else
//				{	// update the channel name
//					memmove(State, pVFO->name + 0, 8);
//					EEPROM_WriteBuffer(0x0F50 + OffsetMR, State);
//					//memset(State, 0xFF, sizeof(State));
//					memset(State, 0x00, sizeof(State));  // follow the QS way
//					memmove(State, pVFO->name + 8, 2);
//					EEPROM_WriteBuffer(0x0F58 + OffsetMR, State);
//				}
			}
//		#endif
	}
}
