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
#include "bsp/dp32g030/gpio.h"
#include "dcs.h"
#include "driver/backlight.h"
#ifdef ENABLE_FMRADIO
	#include "driver/bk1080.h"
#endif
#include "driver/bk4819.h"
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
#include "ui/status.h"
#include "ui/ui.h"

function_type_t g_current_function;

void FUNCTION_Init(void)
{
	if (IS_NOT_NOAA_CHANNEL(g_rx_vfo->channel_save))
	{
		g_current_code_type = g_selected_code_type;
		if (g_css_scan_mode == CSS_SCAN_MODE_OFF)
			g_current_code_type = (g_rx_vfo->am_mode > 0) ? CODE_TYPE_NONE : g_rx_vfo->p_rx->code_type;
	}
	else
		g_current_code_type = CODE_TYPE_CONTINUOUS_TONE;

	DTMF_clear_RX();

	g_cxcss_tail_found = false;
	g_cdcss_lost       = false;
	g_ctcss_lost       = false;

	#ifdef ENABLE_VOX
		g_vox_lost = false;
	#endif

	g_squelch_open = false;

	g_flag_tail_tone_elimination_complete = false;
	g_tail_tone_elimination_tick_10ms     = 0;
	g_found_ctcss                         = false;
	g_found_cdcss                         = false;
	g_found_ctcss_tick_10ms               = 0;
	g_found_cdcss_tick_10ms               = 0;
	g_end_of_rx_detected_maybe            = false;

	#ifdef ENABLE_NOAA
		g_noaa_tick_10ms = 0;
	#endif

	g_update_status = true;
}

void FUNCTION_Select(function_type_t Function)
{
	const function_type_t prev_func = g_current_function;
	const bool was_power_save = (prev_func == FUNCTION_POWER_SAVE);

	g_current_function = Function;

	if (was_power_save && Function != FUNCTION_POWER_SAVE)
	{	// wake up
		BK4819_Conditional_RX_TurnOn();
		g_rx_idle_mode = false;
		UI_DisplayStatus(false);
	}

	g_update_status = true;

	switch (Function)
	{
		case FUNCTION_FOREGROUND:
			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_SendText("func forground\r\n");
			#endif

			if (g_dtmf_reply_state != DTMF_REPLY_NONE)
				RADIO_PrepareCssTX();

			if (prev_func == FUNCTION_TRANSMIT)
			{
				g_vfo_rssi_bar_level[0] = 0;
				g_vfo_rssi_bar_level[1] = 0;
			}
			else
			if (prev_func != FUNCTION_RECEIVE)
				break;

			#ifdef ENABLE_FMRADIO
				if (g_fm_radio_mode)
					g_fm_restore_tick_10ms = fm_restore_10ms;
			#endif

			if (g_dtmf_call_state == DTMF_CALL_STATE_CALL_OUT ||
			    g_dtmf_call_state == DTMF_CALL_STATE_RECEIVED ||
				g_dtmf_call_state == DTMF_CALL_STATE_RECEIVED_STAY)
			{
				g_dtmf_auto_reset_time_500ms = g_eeprom.dtmf_auto_reset_time * 2;
			}

			return;

		case FUNCTION_NEW_RECEIVE:
			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_SendText("func new receive\r\n");
			#endif
			break;
			
		case FUNCTION_RECEIVE:
			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_SendText("func receive\r\n");
			#endif
			break;

		case FUNCTION_POWER_SAVE:
			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_SendText("func power save\r\n");
			#endif

			if (g_flash_light_state != FLASHLIGHT_SOS)
			{
				g_speaker_enabled = false;
				g_monitor_enabled = false;
				GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
			}
			
			g_power_save_tick_10ms = g_eeprom.battery_save * 10;
			g_power_save_expired   = false;

			g_rx_idle_mode = true;

			BK4819_DisableVox();			
			BK4819_Sleep();

			BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_RX_ENABLE, false);

			if (g_current_display_screen != DISPLAY_MENU)
				GUI_SelectNextDisplay(DISPLAY_MAIN);

			return;

		case FUNCTION_TRANSMIT:
			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_SendText("func transmit\r\n");
			#endif

			if (g_setting_backlight_on_tx_rx == 1 || g_setting_backlight_on_tx_rx == 3)
				backlight_turn_on(backlight_tx_rx_time_500ms);

			if (g_eeprom.dual_watch != DUAL_WATCH_OFF)
			{	// dual-RX is enabled
				g_dual_watch_tick_10ms = dual_watch_delay_after_tx_10ms;
				if (g_dual_watch_tick_10ms < (g_eeprom.scan_hold_time_500ms * 50))
					g_dual_watch_tick_10ms = g_eeprom.scan_hold_time_500ms * 50;
			}

			#ifdef ENABLE_MDC1200
				BK4819_enable_mdc1200_rx(false);
			#endif

			// if DTMF is enabled when TX'ing, it changes the TX audio filtering ! .. 1of11
			// so MAKE SURE that DTMF is disabled - until needed
			BK4819_DisableDTMF();

			// clear the DTMF RX buffer
			DTMF_clear_RX();

			// clear the DTMF RX live decoder buffer
			g_dtmf_rx_live_timeout = 0;
			memset(g_dtmf_rx_live, 0, sizeof(g_dtmf_rx_live));

			#ifdef ENABLE_FMRADIO
				// disable the FM radio
				if (g_fm_radio_mode)
					BK1080_Init(0, false);
			#endif

			g_update_status = true;

			GUI_DisplayScreen();

			#ifdef ENABLE_ALARM
				if (g_alarm_state == ALARM_STATE_TXALARM && g_eeprom.alarm_mode != ALARM_MODE_TONE)
				{	// enable the alarm tone but not the TX
			
					g_alarm_state = ALARM_STATE_ALARM;

					GUI_DisplayScreen();

					GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

					SYSTEM_DelayMs(2);
					BK4819_StartTone1(500, 28, true);
					SYSTEM_DelayMs(2);

					g_speaker_enabled = true;
					GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

					SYSTEM_DelayMs(60);
					BK4819_ExitTxMute();

					g_alarm_tone_counter_10ms = 0;
					break;
				}
			#endif

			if (g_current_vfo->scrambling_type == 0 || !g_setting_scramble_enable)
				BK4819_DisableScramble();

			RADIO_enableTX(false);

			#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
				if (g_alarm_state != ALARM_STATE_OFF)
				{
					#ifdef ENABLE_TX1750
						if (g_alarm_state == ALARM_STATE_TX1750)
							BK4819_TransmitTone(true, 1750);
					#endif
					#ifdef ENABLE_ALARM
						if (g_alarm_state == ALARM_STATE_TXALARM)
							BK4819_TransmitTone(true, 500);
					#endif

					SYSTEM_DelayMs(2);

					g_speaker_enabled = true;
					GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

					#ifdef ENABLE_ALARM
						g_alarm_tone_counter_10ms = 0;
					#endif
					break;
				}
				else
			#endif

			if (!DTMF_Reply())
			{
			#ifdef ENABLE_MDC1200
				if (g_current_vfo->mdc1200_mode == MDC1200_MODE_BOT || g_current_vfo->mdc1200_mode == MDC1200_MODE_BOTH)
				{
					BK4819_WriteRegister(0x30,
						(1u  << 15) |    // enable  VCO calibration
						(1u  << 14) |    // enable something or other
						(0u  << 10) |    // diable  RX link
						(1u  <<  9) |    // enable  AF DAC
						(1u  <<  8) |    // enable  DISC mode, what's DISC mode ?
						(15u <<  4) |    // enable  PLL/VCO
						(1u  <<  3) |    // enable  PA gain
						(0u  <<  2) |    // disable MIC ADC
						(1u  <<  1) |    // enable  TX DSP
						(0u  <<  0));    // disable RX DSP
					SYSTEM_DelayMs(120);
					BK4819_send_MDC1200(MDC1200_OP_CODE_PTT_ID, 0x80, g_eeprom.mdc1200_id);
				}
				else
			#endif
				if (g_current_vfo->dtmf_ptt_id_tx_mode == PTT_ID_APOLLO)
				{
					BK4819_PlayTone(APOLLO_TONE1_HZ, APOLLO_TONE_MS, 0);
				}
			}
/*			
			BK4819_WriteRegister(0x30,
				(1u  << 15) |    // enable  VCO calibration
				(1u  << 14) |    // enable  something or other
				(0u  << 10) |    // diable  RX link
				(1u  <<  9) |    // enable  AF DAC
				(1u  <<  8) |    // enable  DISC mode, what's DISC mode ?
				(15u <<  4) |    // enable  PLL/VCO
				(1u  <<  3) |    // enable  PA gain
				(1u  <<  2) |    // enable  MIC ADC
				(1u  <<  1) |    // enable  TX DSP
				(0u  <<  0));    // disable RX DSP
*/
			if (g_current_vfo->scrambling_type > 0 && g_setting_scramble_enable)
			{
				BK4819_EnableScramble(g_current_vfo->scrambling_type - 1);
			}
			
			break;

		case FUNCTION_PANADAPTER:
			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_SendText("func panadpter\r\n");
			#endif

			break;
	}

	g_schedule_power_save_tick_10ms = battery_save_count_10ms;
	g_schedule_power_save          = false;

	#ifdef ENABLE_FMRADIO
		g_fm_restore_tick_10ms = 0;
	#endif

	g_update_status = true;
}
