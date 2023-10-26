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

#if !defined(ENABLE_OVERLAY)
	#include "ARMCM0.h"
#endif
#include "app/dtmf.h"
#include "app/generic.h"
#include "app/menu.h"
#include "app/search.h"
#include "audio.h"
#include "board.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "driver/uart.h"
#include "frequencies.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#if defined(ENABLE_OVERLAY)
	#include "sram-overlay.h"
#endif
#include "ui/inputbox.h"
#include "ui/menu.h"
#include "ui/menu.h"
#include "ui/ui.h"

#ifdef ENABLE_F_CAL_MENU
	void writeXtalFreqCal(const int32_t value, const bool update_eeprom)
	{
		BK4819_WriteRegister(0x3B, 22656 + value);

		if (update_eeprom)
		{
			struct
			{
				int16_t  BK4819_XtalFreqLow;
				uint16_t EEPROM_1F8A;
				uint16_t EEPROM_1F8C;
				uint8_t  volume_gain;
				uint8_t  dac_gain;
			} __attribute__((packed)) misc;

			g_eeprom.BK4819_xtal_freq_low = value;

			// radio 1 .. 04 00 46 00 50 00 2C 0E
			// radio 2 .. 05 00 46 00 50 00 2C 0E
			//
			EEPROM_ReadBuffer(0x1F88, &misc, 8);
			misc.BK4819_XtalFreqLow = value;
			EEPROM_WriteBuffer8(0x1F88, &misc);
		}
	}
#endif

void MENU_start_css_scan(int8_t Direction)
{
	g_css_scan_mode  = CSS_SCAN_MODE_SCANNING;
	g_update_status = true;

	g_menu_scroll_direction = Direction;

	RADIO_select_vfos();

	MENU_SelectNextCode();

	g_scan_pause_10ms = scan_pause_css_10ms;
}

void MENU_stop_css_scan(void)
{
	g_css_scan_mode = CSS_SCAN_MODE_OFF;
	g_update_status = true;

	RADIO_setup_registers(true);
}

int MENU_GetLimits(uint8_t Cursor, int32_t *pMin, int32_t *pMax)
{
	switch (Cursor)
	{
		case MENU_SQL:
		case MENU_CHAN_SQL:
			*pMin = 0;
			*pMax = 9;
			break;

		case MENU_STEP:
			*pMin = 0;
			*pMax = ARRAY_SIZE(STEP_FREQ_TABLE) - 1;
			break;

		case MENU_AUTO_BACKLITE:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_backlight) - 1;
			break;

		case MENU_FREQ_LOCK:
			*pMin = 0;
			*pMax = FREQ_LOCK_LAST - 1;
			break;

		case MENU_MEM_DISP:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_mem_disp) - 1;
			break;

		case MENU_TX_POWER:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_tx_power) - 1;
			break;

		case MENU_SHIFT_DIR:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_shift_dir) - 1;
			break;

		case MENU_DUAL_WATCH:
			*pMin = 0;
//			*pMax = ARRAY_SIZE(g_sub_menu_dual_watch) - 1;
			*pMax = ARRAY_SIZE(g_sub_menu_off_on) - 1;
			break;

		case MENU_SCAN_HOLD:
			*pMin = 2;    //  1 second
			*pMax = 40;   // 20 seconds
			break;
			
		case MENU_CROSS_VFO:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_cross_vfo) - 1;
			break;

		#ifdef ENABLE_VOICE
			case MENU_VOICE:
				*pMin = 0;
				*pMax = ARRAY_SIZE(g_sub_menu_voice) - 1;
				break;
		#endif

		case MENU_SCAN_CAR_RESUME:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_scan_car_resume) - 1;
			break;

		case MENU_ROGER_MODE:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_roger_mode) - 1;
			break;

		case MENU_PON_MSG:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_pwr_on_msg) - 1;
			break;

		case MENU_RX_CDCSS:
		case MENU_TX_CDCSS:
			*pMin = 0;
			*pMax = 208;
			//*pMax = (ARRAY_SIZE(DCS_OPTIONS) * 2);
			break;

		case MENU_RX_CTCSS:
		case MENU_TX_CTCSS:
			*pMin = 0;
			*pMax = ARRAY_SIZE(CTCSS_OPTIONS) - 1;
			break;

		case MENU_BANDWIDTH:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_bandwidth) - 1;
			break;

		#ifdef ENABLE_ALARM
			case MENU_ALARM_MODE:
				*pMin = 0;
				*pMax = ARRAY_SIZE(g_sub_menu_alarm_mode) - 1;
				break;
		#endif

		#ifdef ENABLE_SIDE_BUTT_MENU
			case MENU_SIDE1_SHORT:
			case MENU_SIDE1_LONG:
			case MENU_SIDE2_SHORT:
			case MENU_SIDE2_LONG:
				*pMin = 0;
				*pMax = ARRAY_SIZE(g_sub_menu_side_butt) - 1;
				break;
		#endif

		case MENU_RESET:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_reset) - 1;
			break;

		case MENU_COMPAND:
		case MENU_AUTO_BACKLITE_ON_TX_RX:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_rx_tx) - 1;
			break;

		#ifdef ENABLE_CONTRAST
			case MENU_CONTRAST:
				//*pMin = 0;
				//*pMax = 63;
				*pMin = 26;
				*pMax = 45;
				break;
		#endif
		
		#ifdef ENABLE_AM_FIX_TEST1
			case MENU_AM_FIX_TEST1:
				*pMin = 0;
				*pMax = ARRAY_SIZE(g_sub_menu_AM_FIX_test1) - 1;
				break;
		#endif

		#ifdef ENABLE_AM_FIX
//			case MENU_AM_FIX:
		#endif
		#ifdef ENABLE_TX_AUDIO_BAR
			case MENU_TX_BAR:
		#endif
		#ifdef ENABLE_RX_SIGNAL_BAR
			case MENU_RX_BAR:
		#endif
		case MENU_BUSY_CHAN_LOCK:
		case MENU_BEEP:
		#ifdef ENABLE_KEYLOCK
		case MENU_AUTO_KEY_LOCK:
		#endif
		case MENU_S_ADD1:
		case MENU_S_ADD2:
		case MENU_STE:
		case MENU_DTMF_ST:
		case MENU_DTMF_DCD:
		case MENU_DTMF_LIVE_DEC:
		case MENU_MOD_MODE:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_off_on) - 1;
			break;

		#ifdef ENABLE_NOAA
			case MENU_NOAA_SCAN:
		#endif
		case MENU_350_TX:
		case MENU_174_TX:
		case MENU_470_TX:
		case MENU_350_EN:
		case MENU_SCRAMBLER_EN:
		case MENU_TX_EN:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_dis_en) - 1;
			break;

		case MENU_SCRAMBLER:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_scrambler) - 1;
			break;

		case MENU_TX_TO:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_tx_timeout) - 1;
			break;

		#ifdef ENABLE_VOX
			case MENU_VOX:
		#endif
		case MENU_RP_STE:
			*pMin = 0;
			*pMax = 10;
			break;

		case MENU_MEM_SAVE:
		case MENU_1_CALL:
		case MENU_MEM_DEL:
		case MENU_MEM_NAME:
			*pMin = 0;
			*pMax = USER_CHANNEL_LAST;
			break;

		case MENU_SLIST1:
		case MENU_SLIST2:
			*pMin = -1;
			*pMax = USER_CHANNEL_LAST;
			break;

		case MENU_BAT_SAVE:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_bat_save) - 1;
			break;

		case MENU_MIC_GAIN:
			*pMin = 0;
			*pMax = 4;
			break;

		case MENU_S_LIST:
			*pMin = 0;
//			*pMax = 1;
			*pMax = 2;
			break;

		case MENU_DTMF_RSP:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_dtmf_rsp) - 1;
			break;

		#ifdef ENABLE_MDC1200
			case MENU_MDC1200_MODE:
				*pMin = 0;
				*pMax = ARRAY_SIZE(g_sub_menu_mdc1200_mode) - 1;
				break;
		
			case MENU_MDC1200_ID:
				*pMin = 0;
				*pMax = 0xffff;
				break;
		#endif
		
		case MENU_PTT_ID:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_ptt_id) - 1;
			break;

		case MENU_BAT_TXT:
			*pMin = 0;
			*pMax = ARRAY_SIZE(g_sub_menu_bat_text) - 1;
			break;

		case MENU_DTMF_HOLD:
			*pMin = DTMF_HOLD_MIN;
			*pMax = DTMF_HOLD_MAX;
			break;

		case MENU_DTMF_PRE:
			*pMin = 3;
			*pMax = 99;
			break;

		case MENU_DTMF_LIST:
			*pMin = 1;
			*pMax = 16;
			break;

		#ifdef ENABLE_F_CAL_MENU
			case MENU_F_CALI:
				*pMin = -50;
				*pMax = +50;
				break;
		#endif

		case MENU_BAT_CAL:
			*pMin = 1600;  // 0
			*pMax = 2200;  // 2300
			break;

		default:
			return -1;
	}

	return 0;
}

void MENU_AcceptSetting(void)
{
	int32_t        Min;
	int32_t        Max;
	uint8_t        Code;
	freq_config_t *pConfig = &g_tx_vfo->freq_config_rx;

	if (!MENU_GetLimits(g_menu_cursor, &Min, &Max))
	{
		if (g_sub_menu_selection < Min) g_sub_menu_selection = Min;
		else
		if (g_sub_menu_selection > Max) g_sub_menu_selection = Max;
	}

	switch (g_menu_cursor)
	{
		default:
			return;

		case MENU_SQL:
			g_eeprom.squelch_level = g_sub_menu_selection;
			g_vfo_configure_mode   = VFO_CONFIGURE;
			break;

		case MENU_CHAN_SQL:
			g_tx_vfo->squelch_level = g_sub_menu_selection;
			g_request_save_channel  = 1;
			g_vfo_configure_mode    = VFO_CONFIGURE;
			return;

		case MENU_STEP:
			g_tx_vfo->step_setting = step_freq_table_sorted[g_sub_menu_selection];
			g_request_save_channel = 1;
			g_vfo_configure_mode   = VFO_CONFIGURE_RELOAD;
			return;

		case MENU_TX_POWER:
			g_tx_vfo->output_power = g_sub_menu_selection;
			g_request_save_channel = 1;
			g_vfo_configure_mode   = VFO_CONFIGURE_RELOAD;
			return;

		case MENU_TX_CDCSS:
			pConfig = &g_tx_vfo->freq_config_tx;

			// Fallthrough

		case MENU_RX_CDCSS:
			if (g_sub_menu_selection == 0)
			{
				if (pConfig->code_type != CODE_TYPE_DIGITAL && pConfig->code_type != CODE_TYPE_REVERSE_DIGITAL)
				{
					g_request_save_channel = 1;
					return;
				}
				Code               = 0;
				pConfig->code_type = CODE_TYPE_NONE;
			}
			else
			if (g_sub_menu_selection < 105)
			{
				pConfig->code_type = CODE_TYPE_DIGITAL;
				Code               = g_sub_menu_selection - 1;
			}
			else
			{
				pConfig->code_type = CODE_TYPE_REVERSE_DIGITAL;
				Code               = g_sub_menu_selection - 105;
			}

			pConfig->code       = Code;
			g_request_save_channel = 1;
			return;

		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

		case MENU_TX_CTCSS:
			pConfig = &g_tx_vfo->freq_config_tx;
		case MENU_RX_CTCSS:
			if (g_sub_menu_selection == 0)
			{
				if (pConfig->code_type != CODE_TYPE_CONTINUOUS_TONE)
				{
					g_request_save_channel = 1;
					return;
				}
				Code               = 0;
				pConfig->code      = Code;
				pConfig->code_type = CODE_TYPE_NONE;

				BK4819_ExitSubAu();
			}
			else
			{
				pConfig->code_type = CODE_TYPE_CONTINUOUS_TONE;
				Code               = g_sub_menu_selection - 1;
				pConfig->code      = Code;

				BK4819_SetCTCSSFrequency(CTCSS_OPTIONS[Code]);
			}

			g_request_save_channel = 1;
			return;

		#pragma GCC diagnostic pop

		case MENU_SHIFT_DIR:
			g_tx_vfo->tx_offset_freq_dir = g_sub_menu_selection;
			g_request_save_channel       = 1;
			return;

		case MENU_OFFSET:
			g_tx_vfo->tx_offset_freq = g_sub_menu_selection;
			g_request_save_channel   = 1;
			return;

		case MENU_BANDWIDTH:
			g_tx_vfo->channel_bandwidth = g_sub_menu_selection;
			g_request_save_channel      = 1;
			return;

		case MENU_SCRAMBLER:
			g_tx_vfo->scrambling_type = g_sub_menu_selection;
			#if 0
				if (g_sub_menu_selection > 0 && g_setting_scramble_enable)
					BK4819_EnableScramble(g_sub_menu_selection - 1);
				else
					BK4819_DisableScramble();
			#endif
			g_request_save_channel = IS_FREQ_CHANNEL(g_tx_vfo->channel_save) ? 2 : 1;
			return;

		case MENU_BUSY_CHAN_LOCK:
			g_tx_vfo->busy_channel_lock = g_sub_menu_selection;
			g_request_save_channel      = 1;
			return;

		case MENU_MEM_SAVE:
			g_tx_vfo->channel_save = g_sub_menu_selection;
			#if 0
				g_eeprom.user_channel[0] = g_sub_menu_selection;
			#else
				g_eeprom.user_channel[g_eeprom.tx_vfo] = g_sub_menu_selection;
			#endif
			g_request_save_channel = 2;
			g_vfo_configure_mode   = VFO_CONFIGURE_RELOAD;
			g_flag_reset_vfos       = true;
			return;

		case MENU_MEM_NAME:
			{	// trailing trim
				for (int i = 9; i >= 0; i--)
				{
					if (g_edit[i] != ' ' && g_edit[i] != '_' && g_edit[i] != 0x00 && g_edit[i] != 0xff)
						break;
					g_edit[i] = ' ';
				}
			}

			// save the channel name
			memset(g_tx_vfo->name, 0, sizeof(g_tx_vfo->name));
			memcpy(g_tx_vfo->name, g_edit, 10);
			SETTINGS_save_channel(g_sub_menu_selection, g_eeprom.tx_vfo, g_tx_vfo, 3);
			g_flag_reconfigure_vfos = true;
			return;

		case MENU_BAT_SAVE:
			g_eeprom.battery_save = g_sub_menu_selection;
			break;

		#ifdef ENABLE_VOX
			case MENU_VOX:
				g_eeprom.vox_switch = g_sub_menu_selection != 0;
				if (g_eeprom.vox_switch)
					g_eeprom.vox_level = g_sub_menu_selection - 1;
				BOARD_EEPROM_LoadCalibration();
				g_flag_reconfigure_vfos = true;
				g_update_status         = true;
				break;
		#endif

		case MENU_AUTO_BACKLITE:
			g_eeprom.backlight = g_sub_menu_selection;
			break;

		case MENU_AUTO_BACKLITE_ON_TX_RX:
			g_setting_backlight_on_tx_rx = g_sub_menu_selection;
			break;

		#ifdef ENABLE_CONTRAST
			case MENU_CONTRAST:
				g_setting_contrast = g_sub_menu_selection;
				ST7565_SetContrast(g_setting_contrast);
				break;
		#endif
		
		case MENU_DUAL_WATCH:
//			g_eeprom.dual_watch = g_sub_menu_selection;
			g_eeprom.dual_watch = (g_sub_menu_selection > 0) ? 1 + g_eeprom.tx_vfo : DUAL_WATCH_OFF;

			g_flag_reconfigure_vfos = true;
			g_update_status         = true;
			break;

		case MENU_SCAN_HOLD:
			g_eeprom.scan_hold_time_500ms = g_sub_menu_selection;
			break;

		case MENU_CROSS_VFO:
			if (IS_NOAA_CHANNEL(g_eeprom.screen_channel[0]))
				return;
			if (IS_NOAA_CHANNEL(g_eeprom.screen_channel[1]))
				return;

			g_eeprom.cross_vfo_rx_tx = g_sub_menu_selection;
			g_flag_reconfigure_vfos  = true;
			g_update_status          = true;
			break;

		case MENU_BEEP:
			g_eeprom.beep_control = g_sub_menu_selection;
			break;

		case MENU_TX_TO:
			g_eeprom.tx_timeout_timer = g_sub_menu_selection;
			break;

		#ifdef ENABLE_VOICE
			case MENU_VOICE:
				g_eeprom.voice_prompt = g_sub_menu_selection;
				g_update_status       = true;
				break;
		#endif

		case MENU_SCAN_CAR_RESUME:
			g_eeprom.scan_resume_mode = g_sub_menu_selection;
			break;

		case MENU_MEM_DISP:
			g_eeprom.channel_display_mode = g_sub_menu_selection;
			break;

		#ifdef ENABLE_KEYLOCK
		case MENU_AUTO_KEY_LOCK:
			g_eeprom.auto_keypad_lock   = g_sub_menu_selection;
			g_key_lock_count_down_500ms = key_lock_timeout_500ms;
			break;
		#endif

		case MENU_S_ADD1:
			g_tx_vfo->scanlist_1_participation = g_sub_menu_selection;
			SETTINGS_save_chan_attribs_name(g_tx_vfo->channel_save, g_tx_vfo);
			g_vfo_configure_mode = VFO_CONFIGURE;
			g_flag_reset_vfos    = true;
			return;

		case MENU_S_ADD2:
			g_tx_vfo->scanlist_2_participation = g_sub_menu_selection;
			SETTINGS_save_chan_attribs_name(g_tx_vfo->channel_save, g_tx_vfo);
			g_vfo_configure_mode = VFO_CONFIGURE;	
			g_flag_reset_vfos    = true;
			return;

		case MENU_STE:
			g_eeprom.tail_note_elimination = g_sub_menu_selection;
			break;

		case MENU_RP_STE:
			g_eeprom.repeater_tail_tone_elimination = g_sub_menu_selection;
			break;

		case MENU_MIC_GAIN:
			g_eeprom.mic_sensitivity = g_sub_menu_selection;
			BOARD_EEPROM_LoadCalibration();
			g_flag_reconfigure_vfos = true;
			break;

		#ifdef ENABLE_TX_AUDIO_BAR
			case MENU_TX_BAR:
				g_setting_mic_bar = g_sub_menu_selection;
				break;
		#endif

		#ifdef ENABLE_RX_SIGNAL_BAR
			case MENU_RX_BAR:
				g_setting_rssi_bar = g_sub_menu_selection;
				break;
		#endif

		case MENU_COMPAND:
			g_tx_vfo->compand = g_sub_menu_selection;
			#if 1
				g_request_save_channel = 1;
			#else
				SETTINGS_save_channel(g_sub_menu_selection, g_eeprom.tx_vfo, g_tx_vfo, 3);
				g_flag_reconfigure_vfos = true;
			#endif
			return;

		case MENU_1_CALL:
			g_eeprom.chan_1_call = g_sub_menu_selection;
			break;

		case MENU_S_LIST:
			g_eeprom.scan_list_default = g_sub_menu_selection;
			break;

		#ifdef ENABLE_ALARM
			case MENU_ALARM_MODE:
				g_eeprom.alarm_mode = g_sub_menu_selection;
				break;
		#endif

		case MENU_DTMF_ST:
			g_eeprom.dtmf_side_tone = g_sub_menu_selection;
			break;

		case MENU_DTMF_RSP:
			g_eeprom.dtmf_decode_response = g_sub_menu_selection;
			break;

		case MENU_DTMF_HOLD:
			g_eeprom.dtmf_auto_reset_time = g_sub_menu_selection;
			break;

		case MENU_DTMF_PRE:
			g_eeprom.dtmf_preload_time = g_sub_menu_selection * 10;
			break;

		#ifdef ENABLE_MDC1200
			case MENU_MDC1200_MODE:
				g_tx_vfo->mdc1200_mode = g_sub_menu_selection;
				g_request_save_channel = 1;
				break;

			case MENU_MDC1200_ID:
				g_eeprom.mdc1200_id = g_sub_menu_selection;
				break;
		#endif

		case MENU_PTT_ID:
			g_tx_vfo->dtmf_ptt_id_tx_mode = g_sub_menu_selection;
			if (g_tx_vfo->dtmf_ptt_id_tx_mode == PTT_ID_TX_DOWN ||
			    g_tx_vfo->dtmf_ptt_id_tx_mode == PTT_ID_BOTH    ||
			    g_tx_vfo->dtmf_ptt_id_tx_mode == PTT_ID_APOLLO)
			{
				g_eeprom.roger_mode = ROGER_MODE_OFF;
				break;
			}
			g_request_save_channel = 1;
			return;

		case MENU_BAT_TXT:
			g_setting_battery_text = g_sub_menu_selection;
			break;

		case MENU_DTMF_DCD:
			g_tx_vfo->dtmf_decoding_enable = g_sub_menu_selection;
			DTMF_clear_RX();
			g_request_save_channel = 1;
			return;

		case MENU_DTMF_LIVE_DEC:
			g_setting_live_dtmf_decoder = g_sub_menu_selection;
			g_dtmf_rx_live_timeout      = 0;
			memset(g_dtmf_rx_live, 0, sizeof(g_dtmf_rx_live));
			if (!g_setting_live_dtmf_decoder)
				BK4819_DisableDTMF();
			g_flag_reconfigure_vfos = true;
			g_update_status         = true;
			break;

		case MENU_DTMF_LIST:
			g_dtmf_chosen_contact = g_sub_menu_selection - 1;
			if (g_dtmf_is_contact_valid)
			{
				GUI_SelectNextDisplay(DISPLAY_MAIN);
				g_dtmf_input_mode        = true;
				g_dtmf_input_box_index   = 3;
				memcpy(g_dtmf_input_box, g_dtmf_id, 4);
				g_request_display_screen = DISPLAY_INVALID;
			}
			return;

		case MENU_PON_MSG:
			g_eeprom.pwr_on_display_mode = g_sub_menu_selection;
			break;

		case MENU_ROGER_MODE:
			g_eeprom.roger_mode = g_sub_menu_selection;
			if (g_eeprom.roger_mode != ROGER_MODE_OFF)
			{
				if (g_tx_vfo->dtmf_ptt_id_tx_mode == PTT_ID_TX_DOWN ||
				    g_tx_vfo->dtmf_ptt_id_tx_mode == PTT_ID_BOTH    ||
				    g_tx_vfo->dtmf_ptt_id_tx_mode == PTT_ID_APOLLO)
				{
					g_tx_vfo->dtmf_ptt_id_tx_mode = PTT_ID_OFF;  // // disable PTT ID tail
					g_request_save_channel = 1;
				}
			}
			break;

		case MENU_MOD_MODE:
			g_tx_vfo->am_mode      = g_sub_menu_selection;
			g_request_save_channel = 1;
		return;
/*
		#ifdef ENABLE_AM_FIX
			case MENU_AM_FIX:
				g_setting_am_fix     = g_sub_menu_selection;
				g_vfo_configure_mode = VFO_CONFIGURE_RELOAD;
				g_flag_reset_vfos    = true;
				break;
		#endif
*/
		#ifdef ENABLE_AM_FIX_TEST1
			case MENU_AM_FIX_TEST1:
				g_setting_am_fix_test1 = g_sub_menu_selection;
				g_vfo_configure_mode   = VFO_CONFIGURE_RELOAD;
				g_flag_reset_vfos      = true;
				break;
		#endif

		#ifdef ENABLE_NOAA
			case MENU_NOAA_SCAN:
				g_eeprom.noaa_auto_scan = g_sub_menu_selection;
				g_flag_reconfigure_vfos = true;
				break;
		#endif

		case MENU_MEM_DEL:
			SETTINGS_save_chan_attribs_name(g_sub_menu_selection, NULL);
			g_vfo_configure_mode = VFO_CONFIGURE_RELOAD;
			g_flag_reset_vfos    = true;
			return;

		#ifdef ENABLE_SIDE_BUTT_MENU
		case MENU_SIDE1_SHORT:
			g_eeprom.key1_short_press_action = g_sub_menu_selection;
			break;

		case MENU_SIDE1_LONG:
			g_eeprom.key1_long_press_action = g_sub_menu_selection;
			break;

		case MENU_SIDE2_SHORT:
			g_eeprom.key2_short_press_action = g_sub_menu_selection;
			break;

		case MENU_SIDE2_LONG:
			g_eeprom.key2_long_press_action = g_sub_menu_selection;
			break;
		#endif

		case MENU_RESET:
			BOARD_FactoryReset(g_sub_menu_selection);
			return;

		case MENU_350_TX:
			g_setting_350_tx_enable = g_sub_menu_selection;
			break;

		case MENU_FREQ_LOCK:
			g_setting_freq_lock = g_sub_menu_selection;
			break;

		case MENU_174_TX:
			g_setting_174_tx_enable = g_sub_menu_selection;
			break;

		case MENU_470_TX:
			g_setting_470_tx_enable = g_sub_menu_selection;
			break;

		case MENU_350_EN:
			g_setting_350_enable = g_sub_menu_selection;
			g_vfo_configure_mode = VFO_CONFIGURE_RELOAD;
			g_flag_reset_vfos    = true;
			break;

		case MENU_SCRAMBLER_EN:
			g_setting_scramble_enable = g_sub_menu_selection;
			g_flag_reconfigure_vfos    = true;
			break;

		case MENU_TX_EN:
			g_setting_tx_enable = g_sub_menu_selection;
			break;

#ifdef ENABLE_F_CAL_MENU
			case MENU_F_CALI:
				writeXtalFreqCal(g_sub_menu_selection, true);
				return;
#endif

		case MENU_BAT_CAL:
		{
			uint16_t buf[4];

			g_battery_calibration[0] = (520ul * g_sub_menu_selection) / 760;  // 5.20V empty, blinking above this value, reduced functionality below
			g_battery_calibration[1] = (700ul * g_sub_menu_selection) / 760;  // 7.00V,  ~5%, 1 bars above this value
			g_battery_calibration[2] = (745ul * g_sub_menu_selection) / 760;  // 7.45V, ~17%, 2 bars above this value
			g_battery_calibration[3] =          g_sub_menu_selection;         // 7.6V,  ~29%, 3 bars above this value
			g_battery_calibration[4] = (788ul * g_sub_menu_selection) / 760;  // 7.88V, ~65%, 4 bars above this value
			g_battery_calibration[5] = 2300;
			EEPROM_WriteBuffer8(0x1F40, g_battery_calibration);

			EEPROM_ReadBuffer( 0x1F48, buf, sizeof(buf));
			buf[0] = g_battery_calibration[4];
			buf[1] = g_battery_calibration[5];
			EEPROM_WriteBuffer8(0x1F48, buf);

			break;
		}
	}

	g_request_save_settings = true;
}

void MENU_SelectNextCode(void)
{
	int32_t UpperLimit;

	if (g_menu_cursor == MENU_RX_CDCSS)
		UpperLimit = 208;
		//UpperLimit = ARRAY_SIZE(DCS_OPTIONS);
	else
	if (g_menu_cursor == MENU_RX_CTCSS)
		UpperLimit = ARRAY_SIZE(CTCSS_OPTIONS) - 1;
	else
		return;

	g_sub_menu_selection = NUMBER_AddWithWraparound(g_sub_menu_selection, g_menu_scroll_direction, 1, UpperLimit);

	if (g_menu_cursor == MENU_RX_CDCSS)
	{
		if (g_sub_menu_selection > 104)
		{
			g_selected_code_type = CODE_TYPE_REVERSE_DIGITAL;
			g_selected_code      = g_sub_menu_selection - 105;
		}
		else
		{
			g_selected_code_type = CODE_TYPE_DIGITAL;
			g_selected_code      = g_sub_menu_selection - 1;
		}

	}
	else
	{
		g_selected_code_type = CODE_TYPE_CONTINUOUS_TONE;
		g_selected_code      = g_sub_menu_selection - 1;
	}

	RADIO_setup_registers(true);

	g_scan_pause_10ms = (g_selected_code_type == CODE_TYPE_CONTINUOUS_TONE) ? scan_pause_ctcss_10ms : scan_pause_cdcss_10ms;

	g_update_display = true;
}

static void MENU_ClampSelection(int8_t Direction)
{
	int32_t Min;
	int32_t Max;

	if (!MENU_GetLimits(g_menu_cursor, &Min, &Max))
	{
		int32_t Selection = g_sub_menu_selection;
		if (Selection < Min) Selection = Min;
		else
		if (Selection > Max) Selection = Max;
		g_sub_menu_selection = NUMBER_AddWithWraparound(Selection, Direction, Min, Max);
	}
}

void MENU_ShowCurrentSetting(void)
{
	switch (g_menu_cursor)
	{
		case MENU_SQL:
			g_sub_menu_selection = g_eeprom.squelch_level;
			break;

		case MENU_CHAN_SQL:
			g_sub_menu_selection = g_tx_vfo->squelch_level;
			break;

		case MENU_STEP:
			g_sub_menu_selection = FREQUENCY_get_step_index(STEP_FREQ_TABLE[g_tx_vfo->step_setting]);
			break;
		
		case MENU_TX_POWER:
			g_sub_menu_selection = g_tx_vfo->output_power;
			break;

		case MENU_RX_CDCSS:
			switch (g_tx_vfo->freq_config_rx.code_type)
			{
				case CODE_TYPE_DIGITAL:
					g_sub_menu_selection = g_tx_vfo->freq_config_rx.code + 1;
					break;
				case CODE_TYPE_REVERSE_DIGITAL:
					g_sub_menu_selection = g_tx_vfo->freq_config_rx.code + 105;
					break;
				default:
					g_sub_menu_selection = 0;
					break;
			}
			break;

		case MENU_RESET:
			g_sub_menu_selection = 0;
			break;

		case MENU_RX_CTCSS:
			g_sub_menu_selection = (g_tx_vfo->freq_config_rx.code_type == CODE_TYPE_CONTINUOUS_TONE) ? g_tx_vfo->freq_config_rx.code + 1 : 0;
			break;

		case MENU_TX_CDCSS:
			switch (g_tx_vfo->freq_config_tx.code_type)
			{
				case CODE_TYPE_DIGITAL:
					g_sub_menu_selection = g_tx_vfo->freq_config_tx.code + 1;
					break;
				case CODE_TYPE_REVERSE_DIGITAL:
					g_sub_menu_selection = g_tx_vfo->freq_config_tx.code + 105;
					break;
				default:
					g_sub_menu_selection = 0;
					break;
			}
			break;

		case MENU_TX_CTCSS:
			g_sub_menu_selection = (g_tx_vfo->freq_config_tx.code_type == CODE_TYPE_CONTINUOUS_TONE) ? g_tx_vfo->freq_config_tx.code + 1 : 0;
			break;

		case MENU_SHIFT_DIR:
			g_sub_menu_selection = g_tx_vfo->tx_offset_freq_dir;
			break;

		case MENU_OFFSET:
			g_sub_menu_selection = g_tx_vfo->tx_offset_freq;
			break;

		case MENU_BANDWIDTH:
			g_sub_menu_selection = g_tx_vfo->channel_bandwidth;
			break;

		case MENU_SCRAMBLER:
			g_sub_menu_selection = g_tx_vfo->scrambling_type;
			break;

		case MENU_BUSY_CHAN_LOCK:
			g_sub_menu_selection = g_tx_vfo->busy_channel_lock;
			break;

		case MENU_MEM_SAVE:
#if 0
				g_sub_menu_selection = g_eeprom.user_channel[0];
#else
				g_sub_menu_selection = g_eeprom.user_channel[g_eeprom.tx_vfo];
#endif
			break;

		case MENU_MEM_NAME:
			g_sub_menu_selection = g_eeprom.user_channel[g_eeprom.tx_vfo];
			break;

		case MENU_BAT_SAVE:
			g_sub_menu_selection = g_eeprom.battery_save;
			break;

#ifdef ENABLE_VOX
			case MENU_VOX:
				g_sub_menu_selection = g_eeprom.vox_switch ? g_eeprom.vox_level + 1 : 0;
				break;
#endif

		case MENU_AUTO_BACKLITE:
			g_sub_menu_selection = g_eeprom.backlight;

			g_backlight_count_down = 0;
			GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);  	// turn the backlight ON while in backlight menu
			break;

		case MENU_AUTO_BACKLITE_ON_TX_RX:
			g_sub_menu_selection = g_setting_backlight_on_tx_rx;
			break;

#ifdef ENABLE_CONTRAST
			case MENU_CONTRAST:
				g_sub_menu_selection = g_setting_contrast;
				break;
#endif
		
		case MENU_DUAL_WATCH:
//			g_sub_menu_selection = g_eeprom.dual_watch;
			g_sub_menu_selection = (g_eeprom.dual_watch == DUAL_WATCH_OFF) ? 0 : 1;
			break;

		case MENU_SCAN_HOLD:
			g_sub_menu_selection = g_eeprom.scan_hold_time_500ms;
			break;

		case MENU_CROSS_VFO:
			g_sub_menu_selection = g_eeprom.cross_vfo_rx_tx;
			break;

		case MENU_BEEP:
			g_sub_menu_selection = g_eeprom.beep_control;
			break;

		case MENU_TX_TO:
			g_sub_menu_selection = g_eeprom.tx_timeout_timer;
			break;

#ifdef ENABLE_VOICE
			case MENU_VOICE:
				g_sub_menu_selection = g_eeprom.voice_prompt;
				break;
#endif

		case MENU_SCAN_CAR_RESUME:
			g_sub_menu_selection = g_eeprom.scan_resume_mode;
			break;

		case MENU_MEM_DISP:
			g_sub_menu_selection = g_eeprom.channel_display_mode;
			break;

		#ifdef ENABLE_KEYLOCK
		case MENU_AUTO_KEY_LOCK:
			g_sub_menu_selection = g_eeprom.auto_keypad_lock;
			break;
		#endif

		case MENU_S_ADD1:
			g_sub_menu_selection = g_tx_vfo->scanlist_1_participation;
			break;

		case MENU_S_ADD2:
			g_sub_menu_selection = g_tx_vfo->scanlist_2_participation;
			break;

		case MENU_STE:
			g_sub_menu_selection = g_eeprom.tail_note_elimination;
			break;

		case MENU_RP_STE:
			g_sub_menu_selection = g_eeprom.repeater_tail_tone_elimination;
			break;

		case MENU_MIC_GAIN:
			g_sub_menu_selection = g_eeprom.mic_sensitivity;
			break;

#ifdef ENABLE_TX_AUDIO_BAR
			case MENU_TX_BAR:
				g_sub_menu_selection = g_setting_mic_bar;
				break;
#endif

#ifdef ENABLE_RX_SIGNAL_BAR
			case MENU_RX_BAR:
				g_sub_menu_selection = g_setting_rssi_bar;
				break;
#endif

		case MENU_COMPAND:
			g_sub_menu_selection = g_tx_vfo->compand;
			return;

		case MENU_1_CALL:
			g_sub_menu_selection = g_eeprom.chan_1_call;
			break;

		case MENU_S_LIST:
			g_sub_menu_selection = g_eeprom.scan_list_default;
			break;

		case MENU_SLIST1:
			g_sub_menu_selection = RADIO_FindNextChannel(0, 1, true, 0);
			break;

		case MENU_SLIST2:
			g_sub_menu_selection = RADIO_FindNextChannel(0, 1, true, 1);
			break;

#ifdef ENABLE_ALARM
			case MENU_ALARM_MODE:
				g_sub_menu_selection = g_eeprom.alarm_mode;
				break;
#endif

		case MENU_DTMF_ST:
			g_sub_menu_selection = g_eeprom.dtmf_side_tone;
			break;

		case MENU_DTMF_RSP:
			g_sub_menu_selection = g_eeprom.dtmf_decode_response;
			break;

		case MENU_DTMF_HOLD:
			g_sub_menu_selection = g_eeprom.dtmf_auto_reset_time;

			if (g_sub_menu_selection <= DTMF_HOLD_MIN)
				g_sub_menu_selection = DTMF_HOLD_MIN;
			else
			if (g_sub_menu_selection <= 10)
				g_sub_menu_selection = 10;
			else
			if (g_sub_menu_selection <= 20)
				g_sub_menu_selection = 20;
			else
			if (g_sub_menu_selection <= 30)
				g_sub_menu_selection = 30;
			else
			if (g_sub_menu_selection <= 40)
				g_sub_menu_selection = 40;
			else
			if (g_sub_menu_selection <= 50)
				g_sub_menu_selection = 50;
			else
			if (g_sub_menu_selection < DTMF_HOLD_MAX)
				g_sub_menu_selection = 50;
			else
				g_sub_menu_selection = DTMF_HOLD_MAX;

			break;

		case MENU_DTMF_PRE:
			g_sub_menu_selection = g_eeprom.dtmf_preload_time / 10;
			break;

		#ifdef ENABLE_MDC1200
			case MENU_MDC1200_MODE:
				g_sub_menu_selection = g_tx_vfo->mdc1200_mode;
				break;

			case MENU_MDC1200_ID:
				g_sub_menu_selection = g_eeprom.mdc1200_id;
				break;
		#endif

		case MENU_PTT_ID:
			g_sub_menu_selection = g_tx_vfo->dtmf_ptt_id_tx_mode;
			break;

		case MENU_BAT_TXT:
			g_sub_menu_selection = g_setting_battery_text;
			return;

		case MENU_DTMF_DCD:
			g_sub_menu_selection = g_tx_vfo->dtmf_decoding_enable;
			break;

		case MENU_DTMF_LIST:
			g_sub_menu_selection = g_dtmf_chosen_contact + 1;
			break;

		case MENU_DTMF_LIVE_DEC:
			g_sub_menu_selection = g_setting_live_dtmf_decoder;
			break;

		case MENU_PON_MSG:
			g_sub_menu_selection = g_eeprom.pwr_on_display_mode;
			break;

		case MENU_ROGER_MODE:
			g_sub_menu_selection = g_eeprom.roger_mode;
			break;

		case MENU_MOD_MODE:
			g_sub_menu_selection = g_tx_vfo->am_mode;
			break;
/*
#ifdef ENABLE_AM_FIX
			case MENU_AM_FIX:
				g_sub_menu_selection = g_setting_am_fix;
				break;
#endif
*/
#ifdef ENABLE_AM_FIX_TEST1
			case MENU_AM_FIX_TEST1:
				g_sub_menu_selection = g_setting_am_fix_test1;
				break;
#endif

#ifdef ENABLE_NOAA
			case MENU_NOAA_SCAN:
				g_sub_menu_selection = g_eeprom.noaa_auto_scan;
				break;
#endif

		case MENU_MEM_DEL:
#if 0
				g_sub_menu_selection = RADIO_FindNextChannel(g_eeprom.user_channel[0], 1, false, 1);
#else
				g_sub_menu_selection = RADIO_FindNextChannel(g_eeprom.user_channel[g_eeprom.tx_vfo], 1, false, 1);
#endif
			break;

#ifdef ENABLE_SIDE_BUTT_MENU
		case MENU_SIDE1_SHORT:
			g_sub_menu_selection = g_eeprom.key1_short_press_action;
			break;

		case MENU_SIDE1_LONG:
			g_sub_menu_selection = g_eeprom.key1_long_press_action;
			break;

		case MENU_SIDE2_SHORT:
			g_sub_menu_selection = g_eeprom.key2_short_press_action;
			break;

		case MENU_SIDE2_LONG:
			g_sub_menu_selection = g_eeprom.key2_long_press_action;
			break;
#endif

		case MENU_350_TX:
			g_sub_menu_selection = g_setting_350_tx_enable;
			break;

		case MENU_FREQ_LOCK:
			g_sub_menu_selection = g_setting_freq_lock;
			break;

		case MENU_174_TX:
			g_sub_menu_selection = g_setting_174_tx_enable;
			break;

		case MENU_470_TX:
			g_sub_menu_selection = g_setting_470_tx_enable;
			break;

		case MENU_350_EN:
			g_sub_menu_selection = g_setting_350_enable;
			break;

		case MENU_SCRAMBLER_EN:
			g_sub_menu_selection = g_setting_scramble_enable;
			break;

		case MENU_TX_EN:
			g_sub_menu_selection = g_setting_tx_enable;
			break;

#ifdef ENABLE_F_CAL_MENU
			case MENU_F_CALI:
				g_sub_menu_selection = g_eeprom.BK4819_xtal_freq_low;
				break;
#endif

		case MENU_BAT_CAL:
			g_sub_menu_selection = g_battery_calibration[3];
			break;

		default:
			return;
	}
}

static void MENU_Key_0_to_9(key_code_t Key, bool key_pressed, bool key_held)
{
	unsigned int index;
	int32_t      min;
	int32_t      max;
	uint32_t     value = 0;
	
	if (key_held || !key_pressed)
		return;

	g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;

	if (g_menu_cursor == MENU_MEM_NAME && g_edit_index >= 0)
	{	// currently editing the channel name

		if (g_edit_index < 10)
		{
			#pragma GCC diagnostic push
			#pragma GCC diagnostic ignored "-Wtype-limits"

			if (Key >= KEY_0 && Key <= KEY_9)
			{
				g_edit[g_edit_index] = '0' + Key - KEY_0;

				if (++g_edit_index >= 10)
				{	// exit edit
					g_flag_accept_setting  = false;
					g_ask_for_confirmation = 1;
				}

				g_request_display_screen = DISPLAY_MENU;
			}

			#pragma GCC diagnostic pop
		}

		return;
	}

	INPUTBOX_append(Key);

	g_request_display_screen = DISPLAY_MENU;

	if (!g_in_sub_menu)
	{
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

		switch (g_input_box_index)
		{
			case 2:
				g_input_box_index = 0;

				value = (g_input_box[0] * 10) + g_input_box[1];

				if (value > 0 && value <= g_menu_list_count)
				{
					g_menu_cursor       = value - 1;
					g_flag_refresh_menu = true;
					return;
				}

				if (value <= g_menu_list_count)
					break;

				g_input_box[0]    = g_input_box[1];
				g_input_box_index = 1;

			case 1:
				value = g_input_box[0];
				if (value > 0 && value <= g_menu_list_count)
				{
					g_menu_cursor       = value - 1;
					g_flag_refresh_menu = true;
					return;
				}
				break;
		}

		#pragma GCC diagnostic pop

		g_input_box_index = 0;

		g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	if (g_menu_cursor == MENU_OFFSET)
	{
		uint32_t Frequency;

		if (g_input_box_index < 8)
		{	// not yet enough characters
			#ifdef ENABLE_VOICE
				g_another_voice_id = (voice_id_t)Key;
			#endif
			return;
		}

		#ifdef ENABLE_VOICE
			g_another_voice_id = (voice_id_t)Key;
		#endif

		NUMBER_Get(g_input_box, &Frequency);
		g_input_box_index = 0;

		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
			UART_printf("offset 3 %u\r\n", Frequency);
		#endif

		#if 0
		{	// round
			const uint32_t step_size = g_tx_vfo->step_freq;
			Frequency = ((Frequency + (step_size / 2)) / step_size) * step_size;
		}
		#endif

		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
			UART_printf("offset 4 %u\r\n", Frequency);
		#endif
		
		g_sub_menu_selection = Frequency;

		return;
	}

	if (g_menu_cursor == MENU_BAT_CAL)
	{
		g_sub_menu_selection = INPUTBOX_value();   // get the current value from the input box

		if (g_input_box_index < 4)
		{	// not yet enough characters
			#ifdef ENABLE_VOICE
				g_another_voice_id = (voice_id_t)Key;
			#endif
			return;
		}

		#ifdef ENABLE_VOICE
			g_another_voice_id = (voice_id_t)Key;
		#endif

		g_input_box_index = 0;
		return;
	}

	if (g_menu_cursor == MENU_MEM_SAVE ||
	    g_menu_cursor == MENU_MEM_DEL ||
	    g_menu_cursor == MENU_1_CALL ||
	    g_menu_cursor == MENU_MEM_NAME)
	{	// enter 3-digit channel number

		if (g_input_box_index < 3)
		{
			#ifdef ENABLE_VOICE
				g_another_voice_id   = (voice_id_t)Key;
			#endif
			g_request_display_screen = DISPLAY_MENU;
			return;
		}

		g_input_box_index = 0;

		value = ((g_input_box[0] * 100) + (g_input_box[1] * 10) + g_input_box[2]) - 1;

		if (value <= USER_CHANNEL_LAST)
		{	// user channel
			#ifdef ENABLE_VOICE
				g_another_voice_id = (voice_id_t)Key;
			#endif
			g_sub_menu_selection = value;
			return;
		}

		g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	if (MENU_GetLimits(g_menu_cursor, &min, &max))
	{
		g_input_box_index = 0;
		g_beep_to_play    = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	index = (max >= 100000) ? 6 : (max >= 10000) ? 5 : (max >= 1000) ? 4 : (max >= 100) ? 3 : (max >= 10) ? 2 : 1;
//	NUMBER_Get(g_input_box, &value);
	switch (g_input_box_index)
	{
		case 1:
			value = g_input_box[0];
			break;
		case 2:
			value = (g_input_box[0] * 10) + g_input_box[1];
			break;
		case 3:
			value = (g_input_box[0] * 100) + (g_input_box[1] * 10) + g_input_box[2];
			break;
		case 4:
			value = (g_input_box[0] * 1000) + (g_input_box[1] * 100) + (g_input_box[2] * 10) + g_input_box[3];
			break;
	}

	if (index == g_input_box_index)
		g_input_box_index = 0;

	if ((int32_t)value <= max)
	{
		g_sub_menu_selection = value;
		return;
	}

	g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}

static void MENU_Key_EXIT(bool key_pressed, bool key_held)
{
	if (key_held || !key_pressed)
		return;

	g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;

	if (g_css_scan_mode == CSS_SCAN_MODE_OFF)
	{
		if (g_in_sub_menu)
		{
			if (g_input_box_index == 0 || g_menu_cursor != MENU_OFFSET)
			{
				g_ask_for_confirmation = 0;
				g_in_sub_menu       = false;
				g_input_box_index      = 0;
				g_flag_refresh_menu    = true;

#ifdef ENABLE_VOICE
					g_another_voice_id = VOICE_ID_CANCEL;
#endif
			}
			else
				g_input_box[--g_input_box_index] = 10;

			// ***********************

			g_request_display_screen = DISPLAY_MENU;
			return;
		}

#ifdef ENABLE_VOICE
			g_another_voice_id = VOICE_ID_CANCEL;
#endif

		g_request_display_screen = DISPLAY_MAIN;

		if (g_eeprom.backlight == 0)
		{
			g_backlight_count_down = 0;
			GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);	// turn the backlight OFF
		}
	}
	else
	{
		MENU_stop_css_scan();

#ifdef ENABLE_VOICE
			g_another_voice_id   = VOICE_ID_SCANNING_STOP;
#endif

		g_request_display_screen = DISPLAY_MENU;
	}

	g_ptt_was_released = true;
}

static void MENU_Key_MENU(const bool key_pressed, const bool key_held)
{
	if (key_held || !key_pressed)
		return;

	g_beep_to_play           = BEEP_1KHZ_60MS_OPTIONAL;
	g_request_display_screen = DISPLAY_MENU;

	if (!g_in_sub_menu)
	{
		#ifdef ENABLE_VOICE
			if (g_menu_cursor != MENU_SCRAMBLER)
				g_another_voice_id = g_menu_list[g_menu_list_sorted[g_menu_cursor]].voice_id;
		#endif

		#if 1
			if (g_menu_cursor == MENU_MEM_DEL || g_menu_cursor == MENU_MEM_NAME)
				if (!RADIO_CheckValidChannel(g_sub_menu_selection, false, 0))
					return;  // invalid channel
		#endif

		g_ask_for_confirmation = 0;
		g_in_sub_menu       = true;

//		if (g_menu_cursor != MENU_DTMF_LIST)
		{
			g_input_box_index = 0;
			g_edit_index        = -1;
		}

		return;
	}

	if (g_menu_cursor == MENU_MEM_NAME)
	{
		if (g_edit_index < 0)
		{	// enter channel name edit mode
			if (!RADIO_CheckValidChannel(g_sub_menu_selection, false, 0))
				return;

			BOARD_fetchChannelName(g_edit, g_sub_menu_selection);

			// pad the channel name out with '_'
			g_edit_index = strlen(g_edit);
			while (g_edit_index < 10)
				g_edit[g_edit_index++] = '_';
			g_edit[g_edit_index] = 0;
			g_edit_index = 0;  // 'g_edit_index' is going to be used as the cursor position

			// make a copy so we can test for change when exiting the menu item
			memcpy(g_edit_original, g_edit, sizeof(g_edit_original));

			return;
		}
		else
		if (g_edit_index >= 0 && g_edit_index < 10)
		{	// editing the channel name characters

			if (++g_edit_index < 10)
				return;	// next char

			// exit
			if (memcmp(g_edit_original, g_edit, sizeof(g_edit_original)) == 0)
			{	// no change - drop it
				g_flag_accept_setting  = false;
				g_in_sub_menu        = false;
				g_ask_for_confirmation = 0;
			}
			else
			{
				g_flag_accept_setting  = false;
				g_ask_for_confirmation = 0;
			}
		}
	}

	// exiting the sub menu

	if (g_in_sub_menu)
	{
		if (g_menu_cursor == MENU_RESET  ||
			g_menu_cursor == MENU_MEM_SAVE ||
			g_menu_cursor == MENU_MEM_DEL ||
			g_menu_cursor == MENU_MEM_NAME)
		{
			switch (g_ask_for_confirmation)
			{
				case 0:
					g_ask_for_confirmation = 1;
					break;

				case 1:
					g_ask_for_confirmation = 2;

					UI_DisplayMenu();

					if (g_menu_cursor == MENU_RESET)
					{
						#ifdef ENABLE_VOICE
							AUDIO_SetVoiceID(0, VOICE_ID_CONFIRM);
							AUDIO_PlaySingleVoice(true);
						#endif

						MENU_AcceptSetting();

						#if defined(ENABLE_OVERLAY)
							overlay_FLASH_RebootToBootloader();
						#else
							NVIC_SystemReset();
						#endif
					}

					g_flag_accept_setting  = true;
					g_in_sub_menu          = false;
					g_ask_for_confirmation = 0;
			}
		}
		else
		{
			g_flag_accept_setting = true;
			g_in_sub_menu         = false;
		}
	}

	if (g_css_scan_mode != CSS_SCAN_MODE_OFF)
	{
		g_css_scan_mode = CSS_SCAN_MODE_OFF;
		g_update_status = true;
	}

	#ifdef ENABLE_VOICE
		if (g_menu_cursor == MENU_SCRAMBLER)
			g_another_voice_id = (g_sub_menu_selection == 0) ? VOICE_ID_SCRAMBLER_OFF : VOICE_ID_SCRAMBLER_ON;
		else
			g_another_voice_id = VOICE_ID_CONFIRM;
	#endif

	g_input_box_index = 0;
}

static void MENU_Key_STAR(const bool key_pressed, const bool key_held)
{
	if (key_held || !key_pressed)
		return;

	g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;

	if (g_menu_cursor == MENU_MEM_NAME && g_edit_index >= 0)
	{	// currently editing the channel name

		if (g_edit_index < 10)
		{
			g_edit[g_edit_index] = '-';

			if (++g_edit_index >= 10)
			{	// exit edit
				g_flag_accept_setting  = false;
				g_ask_for_confirmation = 1;
			}

			g_request_display_screen = DISPLAY_MENU;
		}

		return;
	}

	RADIO_select_vfos();

	if (IS_NOT_NOAA_CHANNEL(g_rx_vfo->channel_save) && g_rx_vfo->am_mode == 0)
	{
		if (g_menu_cursor == MENU_RX_CTCSS || g_menu_cursor == MENU_RX_CDCSS)
		{	// scan CTCSS or DCS to find the tone/code of the incoming signal

			if (g_css_scan_mode == CSS_SCAN_MODE_OFF)
			{
				MENU_start_css_scan(1);
				g_request_display_screen = DISPLAY_MENU;
#ifdef ENABLE_VOICE
					AUDIO_SetVoiceID(0, VOICE_ID_SCANNING_BEGIN);
					AUDIO_PlaySingleVoice(1);
#endif
			}
			else
			{
				MENU_stop_css_scan();
				g_request_display_screen = DISPLAY_MENU;
#ifdef ENABLE_VOICE
					g_another_voice_id       = VOICE_ID_SCANNING_STOP;
#endif
			}
		}

		g_ptt_was_released = true;

		return;
	}

	g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}

static void MENU_Key_UP_DOWN(bool key_pressed, bool key_held, int8_t Direction)
{
	uint8_t VFO;
	uint8_t Channel;
	bool    bCheckScanList;

	if (g_menu_cursor == MENU_MEM_NAME && g_in_sub_menu && g_edit_index >= 0)
	{	// change the character
		if (key_pressed && g_edit_index < 10 && Direction != 0)
		{
			const char   unwanted[] = "$%&!\"':;?^`|{}";
			char         c          = g_edit[g_edit_index] + Direction;
			unsigned int i          = 0;
			while (i < sizeof(unwanted) && c >= 32 && c <= 126)
			{
				if (c == unwanted[i++])
				{	// choose next character
					c += Direction;
					i = 0;
				}
			}
			g_edit[g_edit_index] = (c < 32) ? 126 : (c > 126) ? 32 : c;

			g_request_display_screen = DISPLAY_MENU;
		}
		return;
	}

	if (!key_held)
	{
		if (!key_pressed)
			return;

		g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;

		g_input_box_index = 0;
	}
	else
	if (!key_pressed)
		return;

	if (g_css_scan_mode != CSS_SCAN_MODE_OFF)
	{
		MENU_start_css_scan(Direction);

		g_ptt_was_released       = true;
		g_request_display_screen = DISPLAY_MENU;
		return;
	}

	if (!g_in_sub_menu)
	{
		g_menu_cursor = NUMBER_AddWithWraparound(g_menu_cursor, -Direction, 0, g_menu_list_count - 1);

		g_flag_refresh_menu = true;

		g_request_display_screen = DISPLAY_MENU;

		if (g_menu_cursor != MENU_AUTO_BACKLITE && g_eeprom.backlight == 0)
		{
			g_backlight_count_down = 0;
			GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);	// turn the backlight OFF
		}

		return;
	}

	VFO = 0;

	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

	switch (g_menu_cursor)
	{
		case MENU_OFFSET:
		{
			const int32_t max_freq = 100000000;
			const int32_t step_size = g_tx_vfo->step_freq;
			int32_t offset = (int32_t)g_sub_menu_selection + (Direction * step_size);
			
			// wrap
			if (offset >= max_freq)
				offset = 0;                    
			else
			if (offset < 0)
				offset = max_freq - step_size;

			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_printf("offset 1 %u %u\r\n", offset, step_size);
			#endif

			offset = ((offset + (step_size / 2)) / step_size) * step_size;  // round

			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_printf("offset 2 %u %u\r\n", offset, step_size);
			#endif
			
			g_sub_menu_selection = offset;
			g_request_display_screen = DISPLAY_MENU;
			return;
		}

		case MENU_MEM_DEL:
		case MENU_1_CALL:
		case MENU_MEM_NAME:
			bCheckScanList = false;
			break;

		case MENU_SLIST2:
			VFO = 1;
		case MENU_SLIST1:
			bCheckScanList = true;
			break;

		default:
			MENU_ClampSelection(Direction);
			g_request_display_screen = DISPLAY_MENU;
			return;
	}

	#pragma GCC diagnostic pop

	Channel = RADIO_FindNextChannel(g_sub_menu_selection + Direction, Direction, bCheckScanList, VFO);
	if (Channel != 0xFF)
		g_sub_menu_selection = Channel;

	g_request_display_screen = DISPLAY_MENU;
}

void MENU_process_key(key_code_t Key, bool key_pressed, bool key_held)
{
	switch (Key)
	{
		case KEY_0:
		case KEY_1:
		case KEY_2:
		case KEY_3:
		case KEY_4:
		case KEY_5:
		case KEY_6:
		case KEY_7:
		case KEY_8:
		case KEY_9:
			MENU_Key_0_to_9(Key, key_pressed, key_held);
			break;
		case KEY_MENU:
			MENU_Key_MENU(key_pressed, key_held);
			break;
		case KEY_UP:
			MENU_Key_UP_DOWN(key_pressed, key_held,  1);
			break;
		case KEY_DOWN:
			MENU_Key_UP_DOWN(key_pressed, key_held, -1);
			break;
		case KEY_EXIT:
			MENU_Key_EXIT(key_pressed, key_held);
			break;
		case KEY_STAR:
			MENU_Key_STAR(key_pressed, key_held);
			break;
		case KEY_F:
			if (g_menu_cursor == MENU_MEM_NAME && g_edit_index >= 0)
			{	// currently editing the channel name
				if (!key_held && key_pressed)
				{
					g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;
					if (g_edit_index < 10)
					{
						g_edit[g_edit_index] = ' ';
						if (++g_edit_index >= 10)
						{	// exit edit
							g_flag_accept_setting  = false;
							g_ask_for_confirmation = 1;
						}
						g_request_display_screen = DISPLAY_MENU;
					}
				}
				break;
			}

			GENERIC_Key_F(key_pressed, key_held);
			break;
		case KEY_PTT:
			GENERIC_Key_PTT(key_pressed);
			break;
		default:
			if (!key_held && key_pressed)
				g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			break;
	}

	if (g_screen_to_display == DISPLAY_MENU)
	{
		if (g_menu_cursor == MENU_VOLTAGE ||
#ifdef ENABLE_F_CAL_MENU
			g_menu_cursor == MENU_F_CALI ||
#endif
			g_menu_cursor == MENU_BAT_CAL)
		{
			g_menu_count_down = menu_timeout_long_500ms;
		}
		else
		{
			g_menu_count_down = menu_timeout_500ms;
		}
	}
}
