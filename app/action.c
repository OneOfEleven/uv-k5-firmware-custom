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
#include "driver/uart.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

static void ACTION_FlashLight(void)
{
	switch (g_flash_light_state)
	{
		case 0:
			g_flash_light_state++;
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
			break;
		case 1:
			g_flash_light_state++;
			break;
		default:
			g_flash_light_state = 0;
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
	}
}

void ACTION_Power(void)
{
	if (++g_tx_vfo->output_power > OUTPUT_POWER_HIGH)
		g_tx_vfo->output_power = OUTPUT_POWER_LOW;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		UART_printf("act_pwr %u\r\n", g_tx_vfo->output_power);
	#endif

	g_request_save_channel = 1;

	#ifdef ENABLE_VOICE
		g_another_voice_id = VOICE_ID_POWER;
	#endif

	g_request_display_screen = g_screen_to_display;
}

void ACTION_Monitor(void)
{
	if (g_current_function != FUNCTION_MONITOR)
	{	// enable the monitor
		RADIO_select_vfos();
		#ifdef ENABLE_NOAA
			if (g_rx_vfo->channel_save >= NOAA_CHANNEL_FIRST && g_is_noaa_mode)
				g_noaa_channel = g_rx_vfo->channel_save - NOAA_CHANNEL_FIRST;
		#endif
		g_monitor_enabled = true;
		RADIO_setup_registers(true);
		APP_start_listening(FUNCTION_MONITOR, false);
		return;
	}

	g_monitor_enabled = false;
	
	if (g_scan_state_dir != SCAN_STATE_DIR_OFF)
		g_scan_pause_10ms = g_eeprom.scan_hold_time_500ms * 50;

	#ifdef g_power_save_expired
		if (g_eeprom.dual_watch == DUAL_WATCH_OFF && g_is_noaa_mode)
		{
			g_noaa_count_down_10ms = noaa_count_down_10ms;
			g_schedule_noaa        = false;
		}
	#endif

	RADIO_setup_registers(true);

	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_mode)
		{
			FM_Start();
			g_request_display_screen = DISPLAY_FM;
		}
		else
	#endif
			g_request_display_screen = g_screen_to_display;
}

void ACTION_Scan(bool bRestart)
{
	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_mode)
		{
			if (g_current_function != FUNCTION_RECEIVE &&
			    g_current_function != FUNCTION_MONITOR &&
			    g_current_function != FUNCTION_TRANSMIT)
			{
				GUI_SelectNextDisplay(DISPLAY_FM);

				g_monitor_enabled = false;

				if (g_fm_scan_state != FM_SCAN_OFF)
				{	// already scanning

					FM_PlayAndUpdate();

					#ifdef ENABLE_VOICE
						g_another_voice_id = VOICE_ID_SCANNING_STOP;
					#endif
				}
				else
				{	// start scanning
					uint16_t Frequency;

					if (bRestart)
					{	// scan with auto store
						FM_EraseChannels();
						g_fm_auto_scan        = true;
						g_fm_channel_position = 0;
						Frequency             = FM_RADIO_BAND.lower;
					}
					else
					{	// scan without auto store
						g_fm_auto_scan        = false;
						g_fm_channel_position = 0;
						Frequency             = g_eeprom.fm_frequency_playing;
					}

					BK1080_GetFrequencyDeviation(Frequency);

					FM_Tune(Frequency, 1, bRestart);

					#ifdef ENABLE_VOICE
						g_another_voice_id = VOICE_ID_SCANNING_BEGIN;
					#endif
				}
			}

			return;
		}
	#endif

	if (g_screen_to_display != DISPLAY_SEARCH)
	{	// not in freq/ctcss/cdcss search mode

		g_monitor_enabled = false;

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

					if (g_eeprom.scan_list_default < 2)
					{	// keep scanning but toggle between scan lists

						//g_eeprom.scan_list_default = (g_eeprom.scan_list_default + 1) % 3;
						g_eeprom.scan_list_default++;

						// jump to the next channel
						APP_channel_next(true, g_scan_state_dir);
						
						g_scan_pause_10ms      = 0;
						g_scan_pause_time_mode = false;
	
						g_update_status = true;
						return;
					}

					g_eeprom.scan_list_default = 0;	// back to scan list 1 - the next time we start scanning
				}

				// stop scanning
			
				APP_stop_scan();

				g_request_display_screen = DISPLAY_MAIN;
				return;
			}

			// start scanning
	
			// disable monitor mode
			g_monitor_enabled = false;
			RADIO_setup_registers(true);

			APP_channel_next(true, SCAN_STATE_DIR_FORWARD);

			g_scan_pause_10ms      = 0;   // go NOW
			g_scan_pause_time_mode = false;
			
			#ifdef ENABLE_VOICE
				AUDIO_SetVoiceID(0, VOICE_ID_SCANNING_BEGIN);
				AUDIO_PlaySingleVoice(true);
			#endif
			
			// clear the other vfo's rssi level (to hide the antenna symbol)
			g_vfo_rssi_bar_level[(g_eeprom.rx_vfo + 1) & 1u] = 0;
			
			g_update_status = true;
		}
		
		return;
	}

	// freq/ctcss/cdcss/search mode
	
	
	// TODO: fixme
	
	
//	if (!bRestart)
	if (!bRestart && g_scan_next_channel <= USER_CHANNEL_LAST)
	{	// channel mode, keep scanning but toggle between scan lists
		g_eeprom.scan_list_default = (g_eeprom.scan_list_default + 1) % 3;

		// jump to the next channel
		APP_channel_next(true, g_scan_state_dir);

		g_scan_pause_10ms      = 0;
		g_scan_pause_time_mode = false;

		g_update_status = true;
	}
	else
	{	// stop scanning
		g_monitor_enabled = false;
		APP_stop_scan();
		g_request_display_screen = DISPLAY_MAIN;
	}
}

#ifdef ENABLE_VOX
	void ACTION_Vox(void)
	{
		g_eeprom.vox_switch   = !g_eeprom.vox_switch;
		g_request_save_settings = true;
		g_flag_reconfigure_vfos = true;
		#ifdef ENABLE_VOICE
			g_another_voice_id  = VOICE_ID_VOX;
		#endif
		g_update_status        = true;
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

		if (g_screen_to_display != DISPLAY_MENU)     // 1of11 .. don't close the menu
			g_request_display_screen = DISPLAY_MAIN;
	}
#endif


#ifdef ENABLE_FMRADIO
	void ACTION_FM(void)
	{
		if (g_current_function != FUNCTION_TRANSMIT && g_current_function != FUNCTION_MONITOR)
		{
			if (g_fm_radio_mode)
			{
				FM_TurnOff();

				g_input_box_index = 0;
				#ifdef ENABLE_VOX
					g_vox_resume_count_down = 80;
				#endif
				g_flag_reconfigure_vfos  = true;

				g_request_display_screen = DISPLAY_MAIN;
				return;
			}

			g_monitor_enabled = false;

			RADIO_select_vfos();
			RADIO_setup_registers(true);

			FM_Start();

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
		Short = g_eeprom.key1_short_press_action;
		Long  = g_eeprom.key1_long_press_action;
	}
	else
	if (Key == KEY_SIDE2)
	{
		Short = g_eeprom.key2_short_press_action;
		Long  = g_eeprom.key2_long_press_action;
	}

	if (!key_held && key_pressed)
	{
		g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;
		return;
	}

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
