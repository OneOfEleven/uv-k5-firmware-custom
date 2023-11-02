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

#include <stdlib.h>  // abs()

#include "app/dtmf.h"
#include "app/generic.h"
#include "app/search.h"
#include "audio.h"
#include "board.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/bk4819.h"
#include "driver/gpio.h"
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

search_css_state_t  g_search_css_state;
bool                g_search_use_css_result;
dcs_code_type_t     g_search_css_result_type;
uint8_t             g_search_css_result_code;

bool                g_search_flag_start_scan;
bool                g_search_flag_stop_scan;

uint8_t             g_search_show_chan_prefix;

bool                g_search_single_frequency;
uint8_t             g_search_hit_count;

uint16_t            g_search_freq_css_tick_10ms;
uint16_t            g_search_tick_10ms;

search_edit_state_t g_search_edit_state;

uint8_t             g_search_channel;
uint32_t            g_search_frequency;
step_setting_t      g_search_step_setting;

static void SEARCH_Key_DIGITS(key_code_t Key, bool key_pressed, bool key_held)
{
	if (key_pressed)
	{
		if (!key_held)
			g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;
		return;
	}

	if (g_search_edit_state == SEARCH_EDIT_STATE_SAVE_CHAN)
	{
		uint16_t Channel;

		g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;

		INPUTBOX_append(Key);

		g_request_display_screen = DISPLAY_SEARCH;

		if (g_input_box_index < 3)
		{
			#ifdef ENABLE_VOICE
				g_another_voice_id = (voice_id_t)Key;
			#endif
			return;
		}

		g_input_box_index = 0;

		Channel = ((g_input_box[0] * 100) + (g_input_box[1] * 10) + g_input_box[2]) - 1;
		if (Channel <= USER_CHANNEL_LAST)
		{
			#ifdef ENABLE_VOICE
				g_another_voice_id = (voice_id_t)Key;
			#endif
			g_search_show_chan_prefix = RADIO_CheckValidChannel(Channel, false, 0);
			g_search_channel    = (uint8_t)Channel;
			return;
		}
	}

//	g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}

static void SEARCH_Key_EXIT(bool key_pressed, bool key_held)
{
	if (key_pressed)
	{
		if (!key_held)
			g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;
		return;
	}

	switch (g_search_edit_state)
	{
		case SEARCH_EDIT_STATE_NONE:
			g_eeprom.config.setting.cross_vfo = g_backup_cross_vfo;
			g_update_status          = true;
			g_vfo_configure_mode     = VFO_CONFIGURE_RELOAD;
			g_flag_reset_vfos        = true;
			g_search_flag_stop_scan  = true;

			#ifdef ENABLE_VOICE
				g_another_voice_id   = VOICE_ID_CANCEL;
			#endif

			g_request_display_screen = DISPLAY_MAIN;
			g_update_display         = true;
			break;

		case SEARCH_EDIT_STATE_SAVE_CHAN:
			if (g_input_box_index > 0)
			{
				g_input_box[--g_input_box_index] = 10;

				g_request_display_screen = DISPLAY_SEARCH;
				g_update_display         = true;
				break;
			}

			// Fallthrough

		case SEARCH_EDIT_STATE_SAVE_CONFIRM:
			g_search_edit_state = SEARCH_EDIT_STATE_NONE;

			#ifdef ENABLE_VOICE
				g_another_voice_id = VOICE_ID_CANCEL;
			#endif

			g_request_display_screen = DISPLAY_MAIN;
			g_update_display         = true;
			break;
	}
}

static void SEARCH_Key_MENU(bool key_pressed, bool key_held)
{
	uint8_t Channel;

	if (key_pressed)
	{
		if (!key_held)
			g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;
		return;
	}

	// key released
	
	if (g_search_css_state == SEARCH_CSS_STATE_OFF && !g_search_single_frequency)
		return;

	if (g_search_css_state == SEARCH_CSS_STATE_SCANNING && g_search_single_frequency)
		return;
	
	if (g_search_css_state == SEARCH_CSS_STATE_FAILED && g_search_single_frequency)
		return;

	g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;

	switch (g_search_edit_state)
	{
		case SEARCH_EDIT_STATE_NONE:

			if (!g_search_single_frequency)
			{
				// determine what the current step size is for the detected frequency
				// use the 7 VFO channels/bands to determine it
				const unsigned int band = (unsigned int)FREQUENCY_GetBand(g_search_frequency);
				g_search_step_setting = SETTINGS_fetch_frequency_step_setting(band, g_eeprom.config.setting.tx_vfo_num);
				{	// round to nearest step size
					const uint16_t step_size = STEP_FREQ_TABLE[g_search_step_setting];
					g_search_frequency = ((g_search_frequency + (step_size / 2)) / step_size) * step_size;
				}
			}

			if (g_tx_vfo->channel_save <= USER_CHANNEL_LAST)
			{	// save to channel
				g_search_channel = g_tx_vfo->channel_save;
				g_search_show_chan_prefix = RADIO_CheckValidChannel(g_tx_vfo->channel_save, false, 0);
				g_search_edit_state = SEARCH_EDIT_STATE_SAVE_CHAN;
			}
			else
			{	// save to VFO
				g_search_edit_state = SEARCH_EDIT_STATE_SAVE_CONFIRM;
			}

			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				//UART_SendText("edit none\r\n");
			#endif

			g_search_css_state = SEARCH_CSS_STATE_FOUND;

			#ifdef ENABLE_VOICE
				g_another_voice_id = VOICE_ID_MEMORY_CHANNEL;
			#endif

			g_request_display_screen = DISPLAY_SEARCH;
			g_update_status          = true;
			break;

		case SEARCH_EDIT_STATE_SAVE_CHAN:
			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//				UART_SendText("edit save chan\r\n");
			#endif

			if (g_input_box_index > 0)
				break;

			if (g_input_box_index == 0)
			{
				g_search_edit_state      = SEARCH_EDIT_STATE_SAVE_CONFIRM;
				g_beep_to_play           = BEEP_1KHZ_60MS_OPTIONAL;
				g_request_display_screen = DISPLAY_SEARCH;
			}

			// Fallthrough

//			break;

		case SEARCH_EDIT_STATE_SAVE_CONFIRM:
			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//				UART_SendText("edit save confirm\r\n");
			#endif

			if (!g_search_single_frequency)
			{	// save to VFO
				RADIO_InitInfo(g_tx_vfo, g_tx_vfo->channel_save, g_search_frequency);

				if (g_search_use_css_result)
				{
					g_tx_vfo->freq_config_rx.code_type = g_search_css_result_type;
					g_tx_vfo->freq_config_rx.code      = g_search_css_result_code;
				}

				g_tx_vfo->freq_config_tx = g_tx_vfo->freq_config_rx;
				g_tx_vfo->step_setting   = g_search_step_setting;
			}
			else
			{
				RADIO_configure_channel(0, VFO_CONFIGURE_RELOAD);
				RADIO_configure_channel(1, VFO_CONFIGURE_RELOAD);

				g_tx_vfo->freq_config_rx.code_type = g_search_css_result_type;
				g_tx_vfo->freq_config_rx.code      = g_search_css_result_code;

				g_tx_vfo->freq_config_tx.code_type = g_search_css_result_type;
				g_tx_vfo->freq_config_tx.code      = g_search_css_result_code;
			}

			if (g_tx_vfo->channel_save <= USER_CHANNEL_LAST)
			{
				Channel = g_search_channel;
				g_eeprom.config.setting.indices.vfo[g_eeprom.config.setting.tx_vfo_num].user = Channel;
			}
			else
			{
				Channel = FREQ_CHANNEL_FIRST + g_tx_vfo->band;
				g_eeprom.config.setting.indices.vfo[g_eeprom.config.setting.tx_vfo_num].frequency = Channel;
			}

			g_tx_vfo->channel_save = Channel;
			g_eeprom.config.setting.indices.vfo[g_eeprom.config.setting.tx_vfo_num].screen = Channel;
			g_request_save_channel = 2;

			#ifdef ENABLE_VOICE
				g_another_voice_id = VOICE_ID_CONFIRM;
			#endif

			if (!g_search_single_frequency)
			{	// FREQ/CTCSS/CDCSS search mode .. do another search
				g_search_css_state       = SEARCH_CSS_STATE_REPEAT;
				g_search_edit_state      = SEARCH_EDIT_STATE_NONE;
				g_request_display_screen = DISPLAY_SEARCH;
			}
			else
			{	// CTCSS/CDCSS search mode .. only do a single search
				g_search_css_state       = SEARCH_CSS_STATE_OFF;
				g_search_edit_state      = SEARCH_EDIT_STATE_NONE;
				g_request_display_screen = DISPLAY_MAIN;
			}

			g_update_display = true;
			break;

		default:
			break;
	}
}

static void SEARCH_Key_STAR(bool key_pressed, bool key_held)
{
	if (key_pressed)
	{
		if (!key_held)
			g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;
		return;
	}

	g_search_flag_start_scan = true;
}

static void SEARCH_Key_UP_DOWN(bool key_pressed, bool pKeyHeld, int8_t Direction)
{
	if (pKeyHeld)
	{
		if (!key_pressed)
			return;
	}
	else
	{
		if (key_pressed)
			return;

		g_input_box_index = 0;
		g_beep_to_play    = BEEP_1KHZ_60MS_OPTIONAL;
	}

	if (g_search_edit_state == SEARCH_EDIT_STATE_SAVE_CHAN)
	{
		g_search_channel          = NUMBER_AddWithWraparound(g_search_channel, Direction, 0, USER_CHANNEL_LAST);
		g_search_show_chan_prefix = RADIO_CheckValidChannel(g_search_channel, false, 0);
		g_request_display_screen  = DISPLAY_SEARCH;
	}
	else
		g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}

void SEARCH_process_key(key_code_t Key, bool key_pressed, bool key_held)
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
			SEARCH_Key_DIGITS(Key, key_pressed, key_held);
			break;
		case KEY_MENU:
			SEARCH_Key_MENU(key_pressed, key_held);
			break;
		case KEY_UP:
			SEARCH_Key_UP_DOWN(key_pressed, key_held,  1);
			break;
		case KEY_DOWN:
			SEARCH_Key_UP_DOWN(key_pressed, key_held, -1);
			break;
		case KEY_EXIT:
			SEARCH_Key_EXIT(key_pressed, key_held);
			break;
		case KEY_STAR:
			SEARCH_Key_STAR(key_pressed, key_held);
			break;
		case KEY_PTT:
			GENERIC_Key_PTT(key_pressed);
			break;
		default:
//			if (!key_held && key_pressed)
//				g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			break;
	}
}

void SEARCH_process(void)
{
	uint32_t                 result;
	int32_t                  delta;
	uint16_t                 ctcss_freq;
	BK4819_CSS_scan_result_t scan_result;

	switch (g_search_css_state)
	{
		case SEARCH_CSS_STATE_OFF:

			if (g_search_freq_css_tick_10ms >= search_freq_css_10ms)
			{	// FREQ/CTCSS/CDCSS search timeout

				if (!g_search_single_frequency)
				{	// FREQ search timeout

					#ifdef ENABLE_FREQ_SEARCH_TIMEOUT
						BK4819_DisableFrequencyScan();

						g_search_css_state = SEARCH_CSS_STATE_FREQ_FAILED;

						AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);

						g_update_status  = true;
						g_update_display = true;
						break;
					#endif
				}
				else
				{	// CTCSS/CDCSS search timeout

					#ifdef ENABLE_CODE_SEARCH_TIMEOUT
						BK4819_DisableFrequencyScan();

						g_search_css_state = SEARCH_CSS_STATE_FREQ_FAILED;

						AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);

						g_update_status  = true;
						g_update_display = true;
						break;
					#endif
				}
			}

			if (!BK4819_GetFrequencyScanResult(&result))
				break;   // still scanning

			// accept only within 1kHz
			delta = result - g_search_frequency;
			g_search_hit_count = (abs(delta) < 100) ? g_search_hit_count + 1 : 0;

			BK4819_DisableFrequencyScan();

			g_search_frequency = result;

			if (g_search_hit_count < 3)
			{	// keep scanning for an RF carrier
				BK4819_EnableFrequencyScan();
			}
			else
			{	// 3 matching consecutive results
				//
				// RF carrier found, move on to CTCSS/CDCSS search

				BK4819_set_scan_frequency(g_search_frequency);

				g_search_css_result_type    = CODE_TYPE_NONE;
				g_search_css_result_code    = 0xff;
				g_search_hit_count          = 0;
				g_search_use_css_result     = false;
				g_search_freq_css_tick_10ms = 0;
				g_search_css_state          = SEARCH_CSS_STATE_SCANNING;

				g_update_status  = true;
				g_update_display = true;
				GUI_SelectNextDisplay(DISPLAY_SEARCH);
			}

			g_search_tick_10ms = search_10ms;
			break;

		case SEARCH_CSS_STATE_SCANNING:

			if (g_search_freq_css_tick_10ms >= search_freq_css_10ms)
			{	// CTCSS/CDCSS search timeout

				#if defined(ENABLE_CODE_SEARCH_TIMEOUT)
					g_search_css_state       = SEARCH_CSS_STATE_FAILED;
//					g_search_hit_count       = 0;
//					g_search_css_result_type = CODE_TYPE_NONE;
//					g_search_css_result_code = 0xff;
//					g_search_use_css_result  = false;

					BK4819_Idle();

					AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);

					g_update_status  = true;
					g_update_display = true;
					break;

				#else
					if (!g_search_single_frequency)
					{
						g_search_css_state = SEARCH_CSS_STATE_FAILED;

						BK4819_Idle();

						AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);

						g_update_status  = true;
						g_update_display = true;
						break;
					}
				#endif
			}

			scan_result = BK4819_GetCxCSSScanResult(&result, &ctcss_freq);
			if (scan_result == BK4819_CSS_RESULT_NOT_FOUND)
				break;

			BK4819_Idle();

			if (scan_result == BK4819_CSS_RESULT_CDCSS)
			{	// found a CDCSS code

				const uint8_t code = DCS_GetCdcssCode(result);
				if (code != 0xFF)
				{
					g_search_hit_count       = 0;
					g_search_css_result_type = CODE_TYPE_DIGITAL;
					g_search_css_result_code = code;
					g_search_css_state       = SEARCH_CSS_STATE_FOUND;
					g_search_use_css_result  = true;

					AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);

					g_update_status  = true;
					g_update_display = true;
				}
				else
				{
					g_search_hit_count       = 0;
					g_search_css_result_type = CODE_TYPE_NONE;
					g_search_css_result_code = 0xff;
					g_search_use_css_result  = false;
				}
			}
			else
			if (scan_result == BK4819_CSS_RESULT_CTCSS)
			{	// found a CTCSS tone

				const uint8_t code = DCS_GetCtcssCode(ctcss_freq);
				if (code != 0xFF)
				{
					if (code == g_search_css_result_code && g_search_css_result_type == CODE_TYPE_CONTINUOUS_TONE)
					{	// same code

						if (++g_search_hit_count >= 3)
						{	// 3 matching consecutive results

							g_search_css_state      = SEARCH_CSS_STATE_FOUND;
							g_search_use_css_result = true;

							AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);

							g_update_status  = true;
							g_update_display = true;
						}
					}
					else
					{	// reset
						g_search_hit_count       = 1;
						g_search_css_result_type = CODE_TYPE_CONTINUOUS_TONE;
						g_search_css_result_code = code;
						g_search_use_css_result  = false;
					}
				}
				else
				{	// reset
					g_search_hit_count       = 0;
					g_search_css_result_type = CODE_TYPE_NONE;
					g_search_css_result_code = 0xff;
					g_search_use_css_result  = false;
				}
			}

			if (g_search_css_state == SEARCH_CSS_STATE_OFF || g_search_css_state == SEARCH_CSS_STATE_SCANNING)
			{	// re-start scan

				BK4819_set_scan_frequency(g_search_frequency);
				g_search_tick_10ms = search_10ms;
			}

			GUI_SelectNextDisplay(DISPLAY_SEARCH);
			break;

		//case SEARCH_CSS_STATE_FOUND:
		//case SEARCH_CSS_STATE_FAILED:
		//case SEARCH_CSS_STATE_REPEAT:
		default:
			break;
	}
}

void SEARCH_Start(void)
{
	uint8_t  BackupStep;
	uint16_t BackupStepFreq;

	BK4819_StopScan();

	RADIO_select_vfos();

	#ifdef ENABLE_NOAA
		if (IS_NOAA_CHANNEL(g_rx_vfo->channel_save))
			g_rx_vfo->channel_save = FREQ_CHANNEL_FIRST + BAND6_400MHz;
	#endif

	BackupStep     = g_rx_vfo->step_setting;
	BackupStepFreq = g_rx_vfo->step_freq;

	RADIO_InitInfo(g_rx_vfo, g_rx_vfo->channel_save, g_rx_vfo->p_rx->frequency);

	g_rx_vfo->step_setting = BackupStep;
	g_rx_vfo->step_freq    = BackupStepFreq;

	g_monitor_enabled = false;
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

	RADIO_setup_registers(true);

	#ifdef ENABLE_NOAA
		g_noaa_mode = false;
	#endif

	if (g_search_single_frequency)
	{
		g_search_css_state    = SEARCH_CSS_STATE_SCANNING;
		g_search_frequency    = g_rx_vfo->p_rx->frequency;
		g_search_step_setting = g_rx_vfo->step_setting;

		BK4819_set_rf_filter_path(g_search_frequency);

		BK4819_set_scan_frequency(g_search_frequency);
	}
	else
	{
		g_search_css_state = SEARCH_CSS_STATE_OFF;
		g_search_frequency = 0xFFFFFFFF;

#if 1
		// this is why it needs such a strong signal
		BK4819_set_rf_filter_path(0xFFFFFFFF);  // disable the LNA filter paths - why it needs a strong signal
#else
		BK4819_set_rf_filter_path(g_rx_vfo->p_rx->frequency);  // lets have a play !
#endif

		BK4819_EnableFrequencyScan();
	}

	DTMF_clear_RX();

	#ifdef ENABLE_VOX
		g_vox_lost              = false;
	#endif

	g_cxcss_tail_found          = false;
	g_cdcss_lost                = false;
	g_cdcss_code_type           = 0;
	g_ctcss_lost                = false;

	g_squelch_open              = false;
	g_search_css_result_type    = CODE_TYPE_NONE;
	g_search_css_result_code    = 0xff;
	g_search_hit_count          = 0;
	g_search_use_css_result     = false;
	g_search_edit_state         = SEARCH_EDIT_STATE_NONE;
	g_search_freq_css_tick_10ms = 0;
	g_search_tick_10ms          = search_10ms;
//	g_search_flag_start_scan    = false;

	g_request_display_screen = DISPLAY_SEARCH;
	g_update_status          = true;
}
