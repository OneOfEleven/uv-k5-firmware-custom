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

#include "app/action.h"
#include "app/app.h"
#include "app/dtmf.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "app/search.h"
#include "audio.h"
#include "bsp/dp32g030/gpio.h"
#ifdef ENABLE_FMRADIO
	#include "driver/bk1080.h"
#endif
#include "driver/bk4819.h"
#include "driver/gpio.h"
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
#ifdef ENABLE_SCAN_IGNORE_LIST
	#include "freq_ignore.h"
#endif
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

static void ACTION_FlashLight(void)
{
	switch (g_flash_light_state)
	{
		case FLASHLIGHT_OFF:
			g_flash_light_state = FLASHLIGHT_ON;
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
			break;

		case FLASHLIGHT_ON:
			g_flash_light_state = FLASHLIGHT_BLINK;
			break;

		case FLASHLIGHT_BLINK:
			g_flash_light_blink_tick_10ms = 0;
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);

			g_flash_light_state = FLASHLIGHT_SOS;

			if (g_current_function == FUNCTION_POWER_SAVE)
				FUNCTION_Select(FUNCTION_RECEIVE);
			break;

		case FLASHLIGHT_SOS:
			#ifdef ENABLE_FLASH_LIGHT_SOS_TONE
				BK4819_StopTones(g_current_function == FUNCTION_TRANSMIT);
			#endif

		// Fallthrough

		default:
			g_flash_light_state = FLASHLIGHT_OFF;
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
			break;
	}
}

void ACTION_Power(void)
{
	if (++g_tx_vfo->channel.tx_power > OUTPUT_POWER_HIGH)
		g_tx_vfo->channel.tx_power = OUTPUT_POWER_LOW;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		UART_printf("act_pwr %u\r\n", g_tx_vfo->channel.tx_power);
	#endif

	g_request_save_channel = 1;

	#ifdef ENABLE_VOICE
		g_another_voice_id = VOICE_ID_POWER;
	#endif

	g_request_display_screen = g_current_display_screen;
}

void ACTION_Monitor(void)
{
	if (!g_monitor_enabled)  // (g_current_function != FUNCTION_MONITOR)
	{	// enable monitor mode

		g_monitor_enabled = true;
		g_beep_to_play    = BEEP_NONE;

		if (!g_squelch_open && GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER))
			BK4819_StopTones(g_current_function == FUNCTION_TRANSMIT);

		#ifdef ENABLE_NOAA
//			if (g_rx_vfo->channel_save >= NOAA_CHANNEL_FIRST && g_noaa_mode)
			if (IS_NOAA_CHANNEL(g_rx_vfo->channel_save) && g_noaa_mode)
				g_noaa_channel = g_rx_vfo->channel_save - NOAA_CHANNEL_FIRST;
		#endif

		APP_start_listening();
		return;
	}

	// disable monitor
	
	g_monitor_enabled = false;

	if (!g_squelch_open)
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
	
	if (g_scan_state_dir != SCAN_STATE_DIR_OFF)
		g_scan_pause_tick_10ms = g_eeprom.config.setting.scan_hold_time * 50;

	#ifdef g_power_save_expired
		if (g_eeprom.config.setting.dual_watch == DUAL_WATCH_OFF && g_noaa_mode)
		{
			g_noaa_tick_10ms = noaa_tick_10ms;
			g_schedule_noaa  = false;
		}
	#endif

	RADIO_setup_registers(true);

	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_mode)
		{
			FM_turn_on();
			g_request_display_screen = DISPLAY_FM;
		}
		else
	#endif
			g_request_display_screen = g_current_display_screen;
}

void ACTION_Scan(bool bRestart)
{
	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_mode)
		{
			if (g_current_function != FUNCTION_RECEIVE &&
			    g_current_function != FUNCTION_TRANSMIT &&
			   !g_monitor_enabled)
			{
				GUI_SelectNextDisplay(DISPLAY_FM);

				if (g_fm_scan_state_dir != FM_SCAN_STATE_DIR_OFF)
				{	// already scanning - stop

					FM_stop_scan();

					#ifdef ENABLE_VOICE
						g_another_voice_id = VOICE_ID_SCANNING_STOP;
					#endif
				}
				else
				{	// start scanning
					uint16_t Frequency;

					if (bRestart)
					{	// scan with auto store
						FM_erase_channels();
						g_fm_auto_scan = true;
						Frequency      = BK1080_freq_lower;
					}
					else
					{	// scan without auto store
						g_fm_auto_scan = false;
						Frequency      = g_eeprom.config.setting.fm_radio.selected_frequency;
					}
					g_fm_channel_position = 0;

					BK1080_get_freq_offset(Frequency);

					FM_tune(Frequency, FM_SCAN_STATE_DIR_UP, bRestart);

					#ifdef ENABLE_VOICE
						g_another_voice_id = VOICE_ID_SCANNING_BEGIN;
					#endif
				}
			}

			return;
		}
	#endif

	if (g_current_display_screen != DISPLAY_SEARCH)
	{	// not in freq/ctcss/cdcss search mode

		g_monitor_enabled = false;
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

		DTMF_clear_RX();

		g_dtmf_rx_live_timeout = 0;
		memset(g_dtmf_rx_live, 0, sizeof(g_dtmf_rx_live));

		RADIO_select_vfos();

		if (IS_NOT_NOAA_CHANNEL(g_rx_vfo->channel_save))
		{
			GUI_SelectNextDisplay(DISPLAY_MAIN);

			if (g_scan_state_dir != SCAN_STATE_DIR_OFF)
			{	// currently scanning
		
				if (g_scan_next_channel <= USER_CHANNEL_LAST)
				{	// channel mode

					if (g_eeprom.config.setting.scan_list_default < 2)
					{	// keep scanning but toggle between scan lists

						//g_eeprom.config.setting.scan_list_default = (g_eeprom.config.setting.scan_list_default + 1) % 3;
						g_eeprom.config.setting.scan_list_default++;

						// jump to the next channel
						APP_channel_next(true, g_scan_state_dir);
						
						g_scan_pause_tick_10ms      = 0;
						g_scan_pause_time_mode = false;
	
						g_update_status = true;
						return;
					}

					g_eeprom.config.setting.scan_list_default = 0;	// back to scan list 1 - the next time we start scanning
				}

				// stop scanning
			
				APP_stop_scan();

				g_request_display_screen = DISPLAY_MAIN;
				return;
			}

			// start scanning
	
			#ifdef ENABLE_SCAN_IGNORE_LIST
//				FI_clear_freq_ignored();
			#endif

			g_monitor_enabled = false;
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

			RADIO_setup_registers(true);

			APP_channel_next(true, SCAN_STATE_DIR_FORWARD);

			g_scan_pause_tick_10ms      = 0;   // go NOW
			g_scan_pause_time_mode = false;
			
			#ifdef ENABLE_VOICE
				AUDIO_SetVoiceID(0, VOICE_ID_SCANNING_BEGIN);
				AUDIO_PlaySingleVoice(true);
			#endif
			
			// clear the other vfo's rssi level (to hide the antenna symbol)
			g_vfo_rssi_bar_level[(g_rx_vfo_num + 1) & 1u] = 0;
			
			g_update_status = true;
		}
		
		return;
	}

	// freq/ctcss/cdcss/search mode
	
	
	// TODO: fixme
	
	
//	if (!bRestart)
	if (!bRestart && g_scan_next_channel <= USER_CHANNEL_LAST)
	{	// channel mode, keep scanning but toggle between scan lists
		g_eeprom.config.setting.scan_list_default = (g_eeprom.config.setting.scan_list_default + 1) % 3;

		// jump to the next channel
		APP_channel_next(true, g_scan_state_dir);

		g_scan_pause_tick_10ms      = 0;
		g_scan_pause_time_mode = false;

		g_update_status = true;
	}
	else
	{	// stop scanning
		APP_stop_scan();
		g_request_display_screen = DISPLAY_MAIN;
	}
}

#ifdef ENABLE_VOX
	void ACTION_Vox(void)
	{
		// toggle VOX on/off
		g_eeprom.config.setting.vox_enabled = (g_eeprom.config.setting.vox_enabled + 1) & 1u;
		g_request_save_settings = true;
		g_flag_reconfigure_vfos = true;
		#ifdef ENABLE_VOICE
			g_another_voice_id = VOICE_ID_VOX;
		#endif
		g_update_status = true;
	}
#endif

#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
	static void ACTION_AlarmOr1750(const bool b1750)
	{
		g_input_box_index = 0;

		(void)b1750;  // stop unused compile warning
		
		#if defined(ENABLE_ALARM) && defined(ENABLE_TX1750)
			g_alarm_state = b1750 ? ALARM_STATE_TX1750 : ALARM_STATE_TXALARM;
			g_alarm_running_counter_10ms = 0;
		#elif defined(ENABLE_ALARM)
			g_alarm_state          = ALARM_STATE_TXALARM;
			g_alarm_running_counter_10ms = 0;
		#else
			g_alarm_state = ALARM_STATE_TX1750;
		#endif

		g_flag_prepare_tx = true;

		if (g_current_display_screen != DISPLAY_MENU)     // 1of11 .. don't close the menu
			g_request_display_screen = DISPLAY_MAIN;
	}
#endif


#ifdef ENABLE_FMRADIO
	void ACTION_FM(void)
	{
		if (g_current_function != FUNCTION_TRANSMIT)
		{
			if (g_fm_radio_mode)
			{	// return normal service
		
				FM_turn_off();

				g_input_box_index = 0;
				#ifdef ENABLE_VOX
					g_vox_resume_tick_10ms = 80;
				#endif
				g_flag_reconfigure_vfos  = true;

				g_request_display_screen = DISPLAY_MAIN;
				return;
			}

			// switch to FM radio mode

			g_monitor_enabled = false;

			RADIO_select_vfos();
			RADIO_setup_registers(true);

			FM_turn_on();

			g_input_box_index = 0;

			g_request_display_screen = DISPLAY_FM;
		}
	}
#endif

void ACTION_process(const key_code_t Key, const bool key_pressed, const bool key_held)
{
	uint8_t Short = ACTION_OPT_NONE;
	uint8_t Long  = ACTION_OPT_NONE;

	if (Key == KEY_SIDE1)
	{
		Short = g_eeprom.config.setting.key1_short;
		Long  = g_eeprom.config.setting.key1_long;
	}
	else
	if (Key == KEY_SIDE2)
	{
		Short = g_eeprom.config.setting.key2_short;
		Long  = g_eeprom.config.setting.key2_long;
	}

	if (!key_held && key_pressed)
		return;

	if (key_held || key_pressed)
	{
		if (!key_held)
			return;

		Short = Long;

		if (!key_pressed)
			return;
	}

	switch (Short)
	{
		default:
		case ACTION_OPT_NONE:
			break;
		case ACTION_OPT_FLASHLIGHT:
			ACTION_FlashLight();
			break;
		case ACTION_OPT_POWER:
			ACTION_Power();
			break;
		case ACTION_OPT_MONITOR:
			ACTION_Monitor();
			break;
		case ACTION_OPT_SCAN:
			ACTION_Scan(true);
			break;
		case ACTION_OPT_VOX:
			#ifdef ENABLE_VOX
				ACTION_Vox();
			#endif
			break;
		case ACTION_OPT_ALARM:
			#ifdef ENABLE_ALARM
				ACTION_AlarmOr1750(false);
			#endif
			break;
		#ifdef ENABLE_FMRADIO
			case ACTION_OPT_FM:
				ACTION_FM();
				break;
		#endif
		case ACTION_OPT_1750:
			#ifdef ENABLE_TX1750
				ACTION_AlarmOr1750(true);
			#endif
			break;
	}
}
