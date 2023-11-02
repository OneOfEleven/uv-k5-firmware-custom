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

#include "app/dtmf.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
	#include "driver/bk1080.h"
#endif
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
#include "misc.h"
#include "settings.h"
#include "ui/menu.h"

// ******************************************


static const uint32_t DEFAULT_FREQUENCY_TABLE[] =
{
	14500000,    //
	14550000,    //
	43300000,    //
	43320000,    //
	43350000     //
};

t_eeprom         g_eeprom;
t_channel_attrib g_user_channel_attributes[FREQ_CHANNEL_LAST + 1];


void SETTINGS_read_eeprom(void)
{
	unsigned int index;

	// read the entire EEPROM contents into memory as a whole
	for (index = 0; index < sizeof(g_eeprom); index += 128)
		EEPROM_ReadBuffer(index, (uint8_t *)(&g_eeprom) + index, 128);

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		UART_printf("config size %04X %u\r\n"
		            "unused size %04X %u\r\n"
		            "calib  size %04X %u\r\n"
		            "eeprom size %04X %u\r\n",
		             sizeof(g_eeprom.config), sizeof(g_eeprom.config),
					 sizeof(g_eeprom.unused), sizeof(g_eeprom.unused),
					 sizeof(g_eeprom.calib),  sizeof(g_eeprom.calib),
					 sizeof(g_eeprom),        sizeof(g_eeprom));
	#endif

	// sanity checks ..

	// 0D60..0E27
	memcpy(&g_user_channel_attributes, &g_eeprom.config.channel_attributes, sizeof(g_user_channel_attributes));

	g_eeprom.config.setting.call1            = IS_USER_CHANNEL(g_eeprom.config.setting.call1)  ? g_eeprom.config.setting.call1 : USER_CHANNEL_FIRST;
	g_eeprom.config.setting.squelch_level    = (g_eeprom.config.setting.squelch_level < 10)    ? g_eeprom.config.setting.squelch_level : 1;
	g_eeprom.config.setting.tx_timeout       = (g_eeprom.config.setting.tx_timeout < 11)       ? g_eeprom.config.setting.tx_timeout : 1;
	g_eeprom.config.setting.noaa_auto_scan   = (g_eeprom.config.setting.noaa_auto_scan < 2)    ? g_eeprom.config.setting.noaa_auto_scan : 0;
#ifdef ENABLE_KEYLOCK
	g_eeprom.config.setting.key_lock         = (g_eeprom.config.setting.key_lock < 2)          ? g_eeprom.config.setting.key_lock : 0;
#endif
#ifdef ENABLE_VOX
	g_eeprom.config.setting.vox_switch       = (g_eeprom.config.setting.vox_switch < 2)        ? g_eeprom.config.setting.vox_switch : 0;
	g_eeprom.config.setting.vox_level        = (g_eeprom.config.setting.vox_level < 10)        ? g_eeprom.config.setting.vox_level : 1;
#endif
	g_eeprom.config.setting.mic_sensitivity  = (g_eeprom.config.setting.mic_sensitivity < 5)   ? g_eeprom.config.setting.mic_sensitivity : 4;

	#ifdef ENABLE_CONTRAST
		g_eeprom.config.setting.lcd_contrast = (g_eeprom.config.setting.lcd_contrast > 45)     ? 31 : (g_eeprom.config.setting.lcd_contrast < 26) ? 31 : g_eeprom.config.setting.lcd_contrast;
		g_setting_contrast                    =  g_eeprom.config.setting.lcd_contrast;
	#endif
	g_eeprom.config.setting.channel_display_mode  = (g_eeprom.config.setting.channel_display_mode < 4)  ? g_eeprom.config.setting.channel_display_mode : MDF_FREQUENCY;    // 4 instead of 3 - extra display mode
	g_eeprom.config.setting.cross_vfo             = (g_eeprom.config.setting.cross_vfo < 3)             ? g_eeprom.config.setting.cross_vfo : CROSS_BAND_OFF;
	g_eeprom.config.setting.battery_save_ratio    = (g_eeprom.config.setting.battery_save_ratio < 5)    ? g_eeprom.config.setting.battery_save_ratio : 4;
	g_eeprom.config.setting.dual_watch            = (g_eeprom.config.setting.dual_watch < 3)            ? g_eeprom.config.setting.dual_watch : DUAL_WATCH_CHAN_A;
	g_eeprom.config.setting.backlight_time        = (g_eeprom.config.setting.backlight_time < ARRAY_SIZE(g_sub_menu_backlight)) ? g_eeprom.config.setting.backlight_time : 3;
	g_eeprom.config.setting.tail_tone_elimination = (g_eeprom.config.setting.tail_tone_elimination < 2) ? g_eeprom.config.setting.tail_tone_elimination : 0;
	g_eeprom.config.setting.vfo_open              = (g_eeprom.config.setting.vfo_open < 2)              ? g_eeprom.config.setting.vfo_open : 1;

	if (g_eeprom.config.setting.vfo_open == 0)
	{
		for (index = 0; index < 2; index++)
			g_eeprom.config.setting.indices.vfo[index].screen = g_eeprom.config.setting.indices.vfo[index].user;
	}

	// 0E80
	for (index = 0; index < ARRAY_SIZE(g_eeprom.config.setting.indices.vfo); index++)
	{
		g_eeprom.config.setting.indices.vfo[index].screen    = IS_VALID_CHANNEL(g_eeprom.config.setting.indices.vfo[index].screen)   ? g_eeprom.config.setting.indices.vfo[index].screen    : (FREQ_CHANNEL_FIRST + BAND6_400MHz);
		g_eeprom.config.setting.indices.vfo[index].user      = IS_USER_CHANNEL(g_eeprom.config.setting.indices.vfo[index].user)      ? g_eeprom.config.setting.indices.vfo[index].user      :  USER_CHANNEL_FIRST;
		g_eeprom.config.setting.indices.vfo[index].frequency = IS_FREQ_CHANNEL(g_eeprom.config.setting.indices.vfo[index].frequency) ? g_eeprom.config.setting.indices.vfo[index].frequency : (FREQ_CHANNEL_FIRST + BAND6_400MHz);
	}
#ifdef ENABLE_NOAA
	for (index = 0; index < ARRAY_SIZE(g_eeprom.config.setting.indices.noaa_channel); index++)
		g_eeprom.config.setting.indices.noaa_channel[index] = IS_NOAA_CHANNEL(g_eeprom.config.setting.indices.noaa_channel[index]) ? g_eeprom.config.setting.indices.noaa_channel[index] : NOAA_CHANNEL_FIRST;
#endif

#ifdef ENABLE_FMRADIO
	// 0x0E88
	g_eeprom.config.setting.fm_radio.selected_frequency = (g_eeprom.config.setting.fm_radio.selected_frequency >= BK1080_freq_lower && g_eeprom.config.setting.fm_radio.selected_frequency < BK1080_freq_upper) ? g_eeprom.config.setting.fm_radio.selected_frequency : BK1080_freq_lower;
	g_eeprom.config.setting.fm_radio.selected_channel   = (g_eeprom.config.setting.fm_radio.selected_channel < ARRAY_SIZE(g_eeprom.config.setting.fm_channel)) ? g_eeprom.config.setting.fm_radio.selected_channel : 0;
	g_eeprom.config.setting.fm_radio.channel_mode       = (g_eeprom.config.setting.fm_radio.channel_mode < 2) ? !!g_eeprom.config.setting.fm_radio.channel_mode : 0;

	// 0E40..0E67
	FM_configure_channel_state();
#endif

	// 0E90..0E97
	g_eeprom.config.setting.beep_control = (g_eeprom.config.setting.beep_control < 2) ? g_eeprom.config.setting.beep_control : 0;
	g_eeprom.config.setting.key1_short   = (g_eeprom.config.setting.key1_short < ACTION_OPT_LEN) ? g_eeprom.config.setting.key1_short : ACTION_OPT_MONITOR;
	g_eeprom.config.setting.key1_long    = (g_eeprom.config.setting.key1_long  < ACTION_OPT_LEN) ? g_eeprom.config.setting.key1_long  : ACTION_OPT_FLASHLIGHT;
	g_eeprom.config.setting.key2_short   = (g_eeprom.config.setting.key2_short < ACTION_OPT_LEN) ? g_eeprom.config.setting.key2_short : ACTION_OPT_SCAN;
	g_eeprom.config.setting.key2_long    = (g_eeprom.config.setting.key2_long  < ACTION_OPT_LEN) ? g_eeprom.config.setting.key2_long  : ACTION_OPT_NONE;
	g_eeprom.config.setting.carrier_search_mode = (g_eeprom.config.setting.carrier_search_mode < 3) ? g_eeprom.config.setting.carrier_search_mode : SCAN_RESUME_CARRIER;
	g_eeprom.config.setting.auto_key_lock       = (g_eeprom.config.setting.auto_key_lock < 2)       ? g_eeprom.config.setting.auto_key_lock : 0;
	g_eeprom.config.setting.power_on_display_mode = (g_eeprom.config.setting.power_on_display_mode < 4) ? g_eeprom.config.setting.power_on_display_mode : PWR_ON_DISPLAY_MODE_VOLTAGE;

	// 0EA0..0EA7
	#ifdef ENABLE_VOICE
		g_eeprom.config.setting.voice_prompt = (g_eeprom.config.setting.voice_prompt < 3) ? g_eeprom.config.setting.voice_prompt : VOICE_PROMPT_ENGLISH;
	#endif

	// 0EA8..0EAF
	#ifdef ENABLE_ALARM
		g_eeprom.config.setting.alarm_mode = (g_eeprom.config.setting.alarm_mode < 2) ? g_eeprom.config.setting.alarm_mode : 1;
	#endif
	g_eeprom.config.setting.roger_mode = (g_eeprom.config.setting.roger_mode < 3) ? g_eeprom.config.setting.roger_mode : ROGER_MODE_OFF;
	g_eeprom.config.setting.repeater_tail_tone_elimination = (g_eeprom.config.setting.repeater_tail_tone_elimination < 11) ? g_eeprom.config.setting.repeater_tail_tone_elimination : 0;
	g_eeprom.config.setting.tx_vfo_num = (g_eeprom.config.setting.tx_vfo_num < 2) ? g_eeprom.config.setting.tx_vfo_num : 0;
	#if defined(ENABLE_AIRCOPY) && defined(ENABLE_AIRCOPY_REMEMBER_FREQ)
		if (g_eeprom.config.setting.air_copy_freq > 0 && g_eeprom.config.setting.air_copy_freq < 0xffffffff)
		{
			for (index = 0; index < ARRAY_SIZE(FREQ_BAND_TABLE); index++)
				if (g_eeprom.config.setting.air_copy_freq >= FREQ_BAND_TABLE[index].lower && g_eeprom.config.setting.air_copy_freq < FREQ_BAND_TABLE[index].upper)
					break;
			g_aircopy_freq = (index < ARRAY_SIZE(FREQ_BAND_TABLE)) ? g_eeprom.config.setting.air_copy_freq : 0xffffffff;
		}
	#endif

	// 0ED0..0ED7
	g_eeprom.config.setting.dtmf.side_tone               = (g_eeprom.config.setting.dtmf.side_tone < 2) ? g_eeprom.config.setting.dtmf.side_tone : 1;
	g_eeprom.config.setting.dtmf.separate_code           = DTMF_ValidateCodes((char *)(&g_eeprom.config.setting.dtmf.separate_code),   1) ? g_eeprom.config.setting.dtmf.separate_code   : '*';
	g_eeprom.config.setting.dtmf.group_call_code         = DTMF_ValidateCodes((char *)(&g_eeprom.config.setting.dtmf.group_call_code), 1) ? g_eeprom.config.setting.dtmf.group_call_code : '#';
	g_eeprom.config.setting.dtmf.decode_response         = (g_eeprom.config.setting.dtmf.decode_response < 4) ? g_eeprom.config.setting.dtmf.decode_response : DTMF_DEC_RESPONSE_RING;
	g_eeprom.config.setting.dtmf.auto_reset_time         = (g_eeprom.config.setting.dtmf.auto_reset_time <= DTMF_HOLD_MAX) ? g_eeprom.config.setting.dtmf.auto_reset_time : (g_eeprom.config.setting.dtmf.auto_reset_time >= DTMF_HOLD_MIN) ? g_eeprom.config.setting.dtmf.auto_reset_time : DTMF_HOLD_MAX;
	g_eeprom.config.setting.dtmf.preload_time            = (g_eeprom.config.setting.dtmf.preload_time < 10) ? g_eeprom.config.setting.dtmf.preload_time : 20;
	g_eeprom.config.setting.dtmf.first_code_persist_time = (g_eeprom.config.setting.dtmf.first_code_persist_time < 10) ? g_eeprom.config.setting.dtmf.first_code_persist_time : 7;
	g_eeprom.config.setting.dtmf.hash_code_persist_time  = (g_eeprom.config.setting.dtmf.hash_code_persist_time < 10) ? g_eeprom.config.setting.dtmf.hash_code_persist_time : 7;
	g_eeprom.config.setting.dtmf.code_persist_time       = (g_eeprom.config.setting.dtmf.code_persist_time < 10) ? g_eeprom.config.setting.dtmf.code_persist_time : 7;
	g_eeprom.config.setting.dtmf.code_interval_time      = (g_eeprom.config.setting.dtmf.code_interval_time < 10) ? g_eeprom.config.setting.dtmf.code_interval_time : 7;
	#ifdef ENABLE_KILL_REVIVE
		g_eeprom.config.setting.dtmf.permit_remote_kill  = (g_eeprom.config.setting.dtmf.permit_remote_kill <   2) ? g_eeprom.config.setting.dtmf.permit_remote_kill : 0;
	#else
		g_eeprom.config.setting.dtmf.permit_remote_kill  = 0;
	#endif

	// 0EE0..0EE7
	if (!DTMF_ValidateCodes(g_eeprom.config.setting.dtmf.ani_id, sizeof(g_eeprom.config.setting.dtmf.ani_id)))
	{
		memset(g_eeprom.config.setting.dtmf.ani_id, 0, sizeof(g_eeprom.config.setting.dtmf.ani_id));
		strcpy(g_eeprom.config.setting.dtmf.ani_id, "123");
	}

	#ifdef ENABLE_KILL_REVIVE
		// 0EE8..0EEF
		if (!DTMF_ValidateCodes(g_eeprom.config.setting.dtmf.kill_code, sizeof(g_eeprom.config.setting.dtmf.kill_code)))
		{
			memset(g_eeprom.config.setting.dtmf.kill_code, 0, sizeof(g_eeprom.config.setting.dtmf.kill_code));
			strcpy(g_eeprom.config.setting.dtmf.kill_code, "ABCD9");
		}

		// 0EF0..0EF7
		if (!DTMF_ValidateCodes(g_eeprom.config.setting.dtmf.revive_code, sizeof(g_eeprom.config.setting.dtmf.revive_code)))
		{
			memset(g_eeprom.config.setting.dtmf.revive_code, 0, sizeof(g_eeprom.config.setting.dtmf.revive_code));
			strcpy(g_eeprom.config.setting.dtmf.revive_code, "9DCBA");
		}
	#else
		memset(g_eeprom.config.setting.dtmf.kill_code,   0, sizeof(g_eeprom.config.setting.dtmf.kill_code));
		memset(g_eeprom.config.setting.dtmf.revive_code, 0, sizeof(g_eeprom.config.setting.dtmf.revive_code));
	#endif

	// 0EF8..0F07
	if (!DTMF_ValidateCodes(g_eeprom.config.setting.dtmf.key_up_code, sizeof(g_eeprom.config.setting.dtmf.key_up_code)))
	{
		memset(g_eeprom.config.setting.dtmf.key_up_code, 0, sizeof(g_eeprom.config.setting.dtmf.key_up_code));
		strcpy(g_eeprom.config.setting.dtmf.key_up_code, "12345");
	}

	// 0F08..0F17
	if (!DTMF_ValidateCodes(g_eeprom.config.setting.dtmf.key_down_code, sizeof(g_eeprom.config.setting.dtmf.key_down_code)))
	{
		memset(g_eeprom.config.setting.dtmf.key_down_code, 0, sizeof(g_eeprom.config.setting.dtmf.key_down_code));
		strcpy(g_eeprom.config.setting.dtmf.key_down_code, "54321");
	}

	// 0F18..0F1F
	g_eeprom.config.setting.scan_list_default = (g_eeprom.config.setting.scan_list_default < 3) ? g_eeprom.config.setting.scan_list_default : 0;  // we now have 'all' channel scan option
	for (index = 0; index < ARRAY_SIZE(g_eeprom.config.setting.priority_scan_list); index++)
	{
		unsigned int k;
		g_eeprom.config.setting.priority_scan_list[index].enabled = (g_eeprom.config.setting.priority_scan_list[index].enabled < 2) ? g_eeprom.config.setting.priority_scan_list[index].enabled : 0;
		for (k = 0; k < ARRAY_SIZE(g_eeprom.config.setting.priority_scan_list[index].channel); k++)
			if (!IS_USER_CHANNEL(g_eeprom.config.setting.priority_scan_list[index].channel[k]))
				g_eeprom.config.setting.priority_scan_list[index].channel[k] = 0xff;
	}
	g_eeprom.config.setting.unused10 = 0xff;

	// 0F30..0F3F .. AES key
	g_has_aes_key = false;
	#if ENABLE_RESET_AES_KEY
		// wipe that darned AES key
		memset(&g_eeprom.config.setting.aes_key, 0xff, sizeof(g_eeprom.config.setting.aes_key));
	#else
		for (index = 0; index < ARRAY_SIZE(g_eeprom.config.setting.aes_key) && !g_has_aes_key; index++)
			if (g_eeprom.config.setting.aes_key[index] != 0xffffffff)
				g_has_aes_key = true;
	#endif

	// 0F40..0F47
	g_eeprom.config.setting.freq_lock = (g_eeprom.config.setting.freq_lock < FREQ_LOCK_LAST) ? g_eeprom.config.setting.freq_lock : FREQ_LOCK_NORMAL;
//	g_eeprom.config.setting.enable_tx_350       = (g_eeprom.config.setting.enable_tx_350 < 2) ? g_eeprom.config.setting.enable_tx_350 : false;
	#ifdef ENABLE_KILL_REVIVE
//		g_eeprom.config.setting.radio_disabled  = (g_eeprom.config.setting.radio_disabled < 2) ? g_eeprom.config.setting.radio_disabled : 0;
	#else
		g_eeprom.config.setting.radio_disabled  = 0;
	#endif
//	g_eeprom.config.setting.enable_tx_200       = (g_eeprom.config.setting.enable_tx_200 < 2) ? g_eeprom.config.setting.enable_tx_200 : 0;
//	g_eeprom.config.setting.enable_tx_470       = (g_eeprom.config.setting.enable_tx_470 < 2) ? g_eeprom.config.setting.enable_tx_470 : 0;
//	g_eeprom.config.setting.enable_350          = (g_eeprom.config.setting.enable_350 < 2)    ? g_eeprom.config.setting.enable_350 : 1;
//	g_eeprom.config.setting.enable_scrambler    = (g_eeprom.config.setting.enable_scrambler & (1u << 0)) ? 1 : 0;
	#ifdef ENABLE_RX_SIGNAL_BAR
//		g_eeprom.config.setting.enable_rssi_bar = (Data[6] & (1u << 1)) ? true : false;
	#else
		g_eeprom.config.setting.enable_rssi_bar = 0;
	#endif
//	g_eeprom.config.setting.tx_enable          = (Data[7] & (1u << 0)) ? true : false;
//	g_eeprom.config.setting.dtmf_live_decoder  = (Data[7] & (1u << 1)) ? true : false;
	g_eeprom.config.setting.battery_text       = (g_eeprom.config.setting.battery_text < 3) ? g_eeprom.config.setting.battery_text : 2;
	#ifdef ENABLE_TX_AUDIO_BAR
//		g_eeprom.config.setting.mic_bar        = (Data[7] & (1u << 4)) ? true : false;
	#endif
	#ifdef ENABLE_AM_FIX
//		g_eeprom.config.setting.am_fix         = (Data[7] & (1u << 5)) ? true : false;
	#endif
//	g_eeprom.config.setting.backlight_on_tx_rx = (Data[7] >> 6) & 3u;

	// 0F48..0F4F
	g_eeprom.config.setting.scan_hold_time = (g_eeprom.config.setting.scan_hold_time > 40) ? 6 : (g_eeprom.config.setting.scan_hold_time < 2) ? 6 : g_eeprom.config.setting.scan_hold_time;

	memset(&g_eeprom.config.unused13, 0xff, sizeof(g_eeprom.config.unused13));

	memset(&g_eeprom.unused, 0xff, sizeof(g_eeprom.unused));

	// ****************************************

	memset(&g_eeprom.calib.unused3, 0xff, sizeof(g_eeprom.calib.unused3));

	memcpy(&g_eeprom_rssi_calib[0], &g_eeprom.calib.rssi_band_123, 8);
	memcpy(&g_eeprom_rssi_calib[1], &g_eeprom_rssi_calib[0], 8);
	memcpy(&g_eeprom_rssi_calib[2], &g_eeprom_rssi_calib[0], 8);
	memcpy(&g_eeprom_rssi_calib[3], &g_eeprom.calib.rssi_band_4567, 8);
	memcpy(&g_eeprom_rssi_calib[4], &g_eeprom_rssi_calib[3], 8);
	memcpy(&g_eeprom_rssi_calib[5], &g_eeprom_rssi_calib[3], 8);
	memcpy(&g_eeprom_rssi_calib[6], &g_eeprom_rssi_calib[3], 8);

	if (g_eeprom.calib.battery[0] >= 5000)
	{
		g_eeprom.calib.battery[0] = 1900;
		g_eeprom.calib.battery[1] = 2000;
	}
	g_eeprom.calib.battery[5] = 2300;

	#ifdef ENABLE_VOX
		g_vox_threshold[1] = g_eeprom.calib.vox[0].threshold[g_eeprom.config.setting.vox_level];
		g_vox_threshold[0] = g_eeprom.calib.vox[1].threshold[g_eeprom.config.setting.vox_level];
	#endif

	//EEPROM_ReadBuffer(0x1F80 + g_eeprom.config.setting.mic_sensitivity, &Mic, 1);
	//g_mic_sensitivity_tuning = (Mic < 32) ? Mic : 15;
	g_mic_sensitivity_tuning = g_mic_gain_dB_2[g_eeprom.config.setting.mic_sensitivity];

	g_eeprom.calib.bk4819_xtal_freq_low = (g_eeprom.calib.bk4819_xtal_freq_low >= -1000 && g_eeprom.calib.bk4819_xtal_freq_low <= 1000) ? g_eeprom.calib.bk4819_xtal_freq_low : 0;

	g_eeprom.calib.volume_gain = (g_eeprom.calib.volume_gain < 64) ? g_eeprom.calib.volume_gain : 58;
	g_eeprom.calib.dac_gain    = (g_eeprom.calib.dac_gain    < 16) ? g_eeprom.calib.dac_gain    : 8;

	BK4819_WriteRegister(0x3B, 22656 + g_eeprom.calib.bk4819_xtal_freq_low);
//	BK4819_WriteRegister(0x3C, g_eeprom.calib.BK4819_XTAL_FREQ_HIGH);

	// ****************************************
}

void SETTINGS_write_eeprom_config(void)
{	// save the entire EEPROM config contents
	uint32_t index;
	for (index = 0; index < sizeof(g_eeprom.config); index += 8)
		EEPROM_WriteBuffer8(index, (uint8_t *)(&g_eeprom) + index);
}

#ifdef ENABLE_FMRADIO
	void SETTINGS_save_fm(void)
	{
		unsigned int i;

		uint16_t index = (uint16_t)((uint8_t *)&g_eeprom.config.setting.fm_radio - (uint8_t *)&g_eeprom.config);
		EEPROM_WriteBuffer8(index, &g_eeprom.config.setting.fm_radio);

		index = (uint16_t)((uint8_t *)&g_eeprom.config.setting.fm_channel - (uint8_t *)&g_eeprom.config);
		for (i = 0; i < sizeof(g_eeprom.config.setting.fm_channel); i += 8)
			EEPROM_WriteBuffer8(index + i, ((uint8_t *)&g_eeprom.config.setting.fm_channel) + i);
	}
#endif

void SETTINGS_save_vfo_indices(void)
{
	const uint16_t index = (uint16_t)((uint8_t *)&g_eeprom.config.setting.indices - (uint8_t *)&g_eeprom.config);
	EEPROM_WriteBuffer8(index, &g_eeprom.config.setting.indices);
}

void SETTINGS_save(void)
{
	uint32_t index;

	#ifndef ENABLE_KEYLOCK
		g_eeprom.config.setting.key_lock = 0;
	#endif

	#ifndef ENABLE_VOX
//		g_eeprom.config.setting.vox_switch = 0;
//		g_eeprom.config.setting.vox_level  = 0;
	#endif

	#ifndef ENABLE_CONTRAST
//		g_eeprom.config.setting.unused4 = 0xff;
	#endif

//	memset(&g_eeprom.config.setting.unused6, 0xff, sizeof(g_eeprom.config.setting.unused6));

	#ifndef ENABLE_PWRON_PASSWORD
		memset(&g_eeprom.config.setting.power_on_password, 0xff, sizeof(g_eeprom.config.setting.power_on_password));
	#endif

	#if !defined(ENABLE_ALARM) && !defined(ENABLE_TX1750)
		g_eeprom.config.setting.alarm_mode = 0;
	#endif

	#if defined(ENABLE_AIRCOPY) && defined(ENABLE_AIRCOPY_REMEMBER_FREQ)
		// remember the AIRCOPY frequency
		g_eeprom.config.setting.air_copy_freq = g_aircopy_freq;
	#else
		memset(&g_eeprom.config.setting.unused8, 0xff, sizeof(g_eeprom.config.setting.unused8));
	#endif

	#ifndef ENABLE_KILL_REVIVE
		g_eeprom.config.setting.radio_disabled = 0;
	#endif

	for (index = 0; index < sizeof(g_eeprom.config.setting); index += 8)
	{
		const uint16_t offset = (uint16_t)((uint8_t *)&g_eeprom.config.setting - (uint8_t *)&g_eeprom.config);
		EEPROM_WriteBuffer8(offset + index, (uint8_t *)(&g_eeprom.config.setting) + index);
	}
}

void SETTINGS_save_channel(const unsigned int channel, const unsigned int vfo, const vfo_info_t *p_vfo, const unsigned int mode)
{
	const unsigned int chan  = CHANNEL_NUM(channel, vfo);
	t_channel         *p_channel = &g_eeprom.config.channel[chan];
	unsigned int       eeprom_addr = chan * 16;

	if (IS_NOAA_CHANNEL(channel))
		return;

	if (mode < 2 && channel <= USER_CHANNEL_LAST)
		return;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		UART_printf("sav_chan %04X  %3u %3u %u %u\r\n", eeprom_addr, chan, channel, vfo, mode);
	#endif

	// ****************

	if (p_vfo != NULL)
	{
		memset(p_channel, 0, sizeof(t_channel));
		p_channel->frequency            = p_vfo->freq_config_rx.frequency;
		p_channel->tx_offset            = p_vfo->tx_offset_freq;
		p_channel->rx_ctcss_cdcss_code  = p_vfo->freq_config_rx.code;
		p_channel->tx_ctcss_cdcss_code  = p_vfo->freq_config_tx.code;
		p_channel->rx_ctcss_cdcss_type  = p_vfo->freq_config_rx.code_type;
//		p_channel->unused1:2
		p_channel->tx_ctcss_cdcss_type  = p_vfo->freq_config_tx.code_type;
		#ifdef ENABLE_MDC1200
			p_channel->mdc1200_mode     = p_vfo->mdc1200_mode;
		#endif
		p_channel->tx_offset_dir        = p_vfo->tx_offset_freq_dir;
//		p_channel->unused3:2
		p_channel->am_mode              = p_vfo->am_mode;
//		p_channel->unused4:3
		p_channel->frequency_reverse    = p_vfo->frequency_reverse;
		p_channel->channel_bandwidth    = p_vfo->channel_bandwidth;
		p_channel->tx_power             = p_vfo->output_power;
		p_channel->busy_channel_lock    = p_vfo->busy_channel_lock;
//		p_channel->unused5:1
		p_channel->compand              = p_vfo->compand;
		p_channel->dtmf_decoding_enable = p_vfo->dtmf_decoding_enable;
		p_channel->dtmf_ptt_id_tx_mode  = p_vfo->dtmf_ptt_id_tx_mode;
//		p_channel->unused6:4
		p_channel->step_setting         = p_vfo->step_setting;
		p_channel->scrambler            = p_vfo->scrambling_type;
		p_channel->squelch_level        = p_vfo->squelch_level;
	}
	else
	if (channel <= USER_CHANNEL_LAST)
	{	// user channel
		memset(p_channel, 0xff, sizeof(t_channel));
	}

	EEPROM_WriteBuffer8(eeprom_addr + 0, (uint8_t *)(p_channel) + 0);
	EEPROM_WriteBuffer8(eeprom_addr + 8, (uint8_t *)(p_channel) + 8);

	// ****************

	SETTINGS_save_chan_attribs_name(channel, p_vfo);

	if (channel <= USER_CHANNEL_LAST)
	{	// user channel, it has a channel name
		const unsigned int eeprom_addr = 0x0F50 + (channel * 16);

		memset(&g_eeprom.config.channel_name[channel], (p_vfo != NULL) ? 0x00 : 0xff, sizeof(g_eeprom.config.channel_name[channel]));

		#ifndef ENABLE_KEEP_MEM_NAME

			// clear/reset the channel name
			EEPROM_WriteBuffer8(eeprom_addr + 0, g_eeprom.config.channel_name[channel] + 0);
			EEPROM_WriteBuffer8(eeprom_addr + 8, g_eeprom.config.channel_name[channel] + 8);

		#else

			if (p_vfo != NULL)
				memcpy(&g_eeprom.config.channel_name[channel], p_vfo->name, 10);

			if (mode >= 3 || p_vfo == NULL)
			{	// save the channel name

				EEPROM_WriteBuffer8(eeprom_addr + 0, &g_eeprom.config.channel_name[channel] + 0);
				EEPROM_WriteBuffer8(eeprom_addr + 8, &g_eeprom.config.channel_name[channel] + 8);
			}

		#endif
	}
}

void SETTINGS_save_chan_attribs_name(const unsigned int channel, const vfo_info_t *p_vfo)
{
	const unsigned int index = channel & ~7ul;     // eeprom writes are always 8 bytes in length

	if (channel >= ARRAY_SIZE(g_user_channel_attributes))
		return;

	if (IS_NOAA_CHANNEL(channel))
		return;

	if (p_vfo != NULL)
	{	// channel attributes

		t_channel_attrib attribs;

 		attribs.band      = p_vfo->band & 7u;
		attribs.unused    = 3u;
		attribs.scanlist2 = p_vfo->scanlist_2_participation;
		attribs.scanlist1 = p_vfo->scanlist_1_participation;

		g_user_channel_attributes[channel]           = attribs;            // remember new attributes
		g_eeprom.config.channel_attributes[channel] = attribs;

		EEPROM_WriteBuffer8(0x0D60 + index, g_user_channel_attributes + index);
	}
	else
	if (channel <= USER_CHANNEL_LAST)
	{	// user channel
		g_user_channel_attributes[channel].attributes          = 0xff;
		g_eeprom.config.channel_attributes[channel].attributes = 0xff;

		EEPROM_WriteBuffer8(0x0D60 + index, g_user_channel_attributes + index);
	}

	if (channel <= USER_CHANNEL_LAST)
	{	// user memory channel
		const unsigned int index = channel * 16;

		if (p_vfo != NULL)
		{
			memset(&g_eeprom.config.channel_name[channel], 0, sizeof(g_eeprom.config.channel_name[channel]));
			memcpy(&g_eeprom.config.channel_name[channel], p_vfo->name, 10);
		}
		else
		{
			memset(&g_eeprom.config.channel_name[channel], 0xff, sizeof(g_eeprom.config.channel_name[channel]));
		}

		EEPROM_WriteBuffer8(0x0F50 + 0 + index, &g_eeprom.config.channel_name[channel] + 0);
		EEPROM_WriteBuffer8(0x0F50 + 8 + index, &g_eeprom.config.channel_name[channel] + 8);
	}
}

unsigned int SETTINGS_find_channel(const uint32_t frequency)
{
	unsigned int chan;
	
	if (frequency == 0 || frequency == 0xffffffff)
		return 0xffffffff;
	
	for (chan = 0; chan <= USER_CHANNEL_LAST; chan++)
	{
		const uint32_t freq = g_eeprom.config.channel[chan].frequency;

		if (g_user_channel_attributes[chan].band > BAND7_470MHz || freq == 0 || freq == 0xffffffff)
			continue;
		
		if (freq == frequency)
			return chan;          // found it
	}
	
	return 0xffffffff;
}

uint32_t SETTINGS_fetch_channel_frequency(const int channel)
{
	uint32_t freq;

	if (channel < 0 || channel > (int)USER_CHANNEL_LAST)
		return 0;

	freq = g_eeprom.config.channel[channel].frequency;

	if (g_user_channel_attributes[channel].band > BAND7_470MHz || freq == 0 || freq == 0xffffffff)
		return 0;

	return freq;
}

unsigned int SETTINGS_fetch_channel_step_setting(const int channel)
{
	unsigned int step_setting;

	if (channel < 0)
		return 0;

	if (channel <= USER_CHANNEL_LAST)
		step_setting = g_eeprom.config.channel[channel].step_setting;
	else
	if (channel <= FREQ_CHANNEL_LAST)
		step_setting = g_eeprom.config.vfo_channel[(channel - FREQ_CHANNEL_FIRST) * 2].step_setting;

//	step_size = STEP_FREQ_TABLE[step_setting];

	return (step_setting >= ARRAY_SIZE(STEP_FREQ_TABLE)) ? STEP_12_5kHz : step_setting;
}

unsigned int SETTINGS_fetch_frequency_step_setting(const int channel, const int vfo)
{
	unsigned int step_setting;

	if (channel < 0 || channel > (FREQ_CHANNEL_LAST - FREQ_CHANNEL_FIRST) || vfo < 0 || vfo >= 2)
		return 0;

	step_setting = g_eeprom.config.vfo_channel[(channel * 2) + vfo].step_setting;

//	step_size = STEP_FREQ_TABLE[step_setting];

	return (step_setting >= ARRAY_SIZE(STEP_FREQ_TABLE)) ? STEP_12_5kHz : step_setting;
}

void SETTINGS_fetch_channel_name(char *s, const int channel)
{
	int i;

	if (s == NULL)
		return;

	memset(s, 0, 11);  // 's' had better be large enough !

	if (channel < 0 || channel > (int)USER_CHANNEL_LAST)
		return;

	if (g_user_channel_attributes[channel].band > BAND7_470MHz)
		return;

	memcpy(s, &g_eeprom.config.channel_name[channel], 10);

	for (i = 0; i < 10; i++)
		if (s[i] < 32 || s[i] > 127)
			break;                // invalid char

	s[i--] = 0;                   // null term

	while (i >= 0 && s[i] == 32)  // trim trailing spaces
		s[i--] = 0;               // null term
}

void SETTINGS_factory_reset(bool bIsAll)
{
	uint16_t i;
	uint8_t  Template[8];

	memset(Template, 0xFF, sizeof(Template));

	for (i = 0x0C80; i < 0x1E00; i += 8)
	{
		if (
			!(i >= 0x0EE0 && i < 0x0F18) &&         // ANI ID + DTMF codes
			!(i >= 0x0F30 && i < 0x0F50) &&         // AES KEY + F LOCK + Scramble Enable
			!(i >= 0x1C00 && i < 0x1E00) &&         // DTMF contacts
			!(i >= 0x0EB0 && i < 0x0ED0) &&         // Welcome strings
			!(i >= 0x0EA0 && i < 0x0EA8) &&         // Voice Prompt
			(bIsAll ||
			(
				!(i >= 0x0D60 && i < 0x0E28) &&     // MR Channel Attributes
				!(i >= 0x0F18 && i < 0x0F30) &&     // Scan List
				!(i >= 0x0F50 && i < 0x1C00) &&     // MR Channel Names
				!(i >= 0x0E40 && i < 0x0E70) &&     // FM Channels
				!(i >= 0x0E88 && i < 0x0E90)        // FM settings
				))
			)
		{
			EEPROM_WriteBuffer8(i, Template);
		}
	}

	if (bIsAll)
	{
		RADIO_InitInfo(g_rx_vfo, FREQ_CHANNEL_FIRST + BAND6_400MHz, 43350000);

		// set the first few memory channels
		for (i = 0; i < ARRAY_SIZE(DEFAULT_FREQUENCY_TABLE); i++)
		{
			const uint32_t Frequency           = DEFAULT_FREQUENCY_TABLE[i];
			g_rx_vfo->freq_config_rx.frequency = Frequency;
			g_rx_vfo->freq_config_tx.frequency = Frequency;
			g_rx_vfo->band                     = FREQUENCY_GetBand(Frequency);
			SETTINGS_save_channel(USER_CHANNEL_FIRST + i, 0, g_rx_vfo, 2);
		}
	}
}
