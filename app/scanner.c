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
#include "app/generic.h"
#include "app/scanner.h"
#include "audio.h"
#include "driver/bk4819.h"
#include "driver/uart.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

dcs_code_type_t   g_scan_css_result_type;
uint8_t           g_scan_css_result_code;
bool              g_flag_start_scan;
bool              g_flag_stop_scan;
bool              g_scan_single_frequency;
scan_edit_state_t g_scanner_edit_state;
uint8_t           g_scan_channel;
uint32_t          g_scan_frequency;
bool              g_scan_pause_mode;
scan_css_state_t  g_scan_css_state;
volatile bool     g_schedule_scan_listen = true;
volatile uint16_t g_scan_pause_delay_in_10ms;
uint16_t          g_scan_freq_css_timer_10ms;
uint8_t           g_scan_hit_count;
bool              g_scan_use_css_result;
scan_state_dir_t  g_scan_state_dir;
bool              g_scan_keep_frequency;

static void SCANNER_Key_DIGITS(key_code_t Key, bool key_pressed, bool key_held)
{
	if (key_held || key_pressed)
		return;

	if (g_scanner_edit_state == SCAN_EDIT_STATE_SAVE)
	{
		uint16_t Channel;

		g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;

		INPUTBOX_Append(Key);

		g_request_display_screen = DISPLAY_SCANNER;

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
			g_show_chan_prefix = RADIO_CheckValidChannel(Channel, false, 0);
			g_scan_channel     = (uint8_t)Channel;
			return;
		}
	}

	g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}

static void SCANNER_Key_EXIT(bool key_pressed, bool key_held)
{
	if (key_held || key_pressed)
		return;

	g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;

	switch (g_scanner_edit_state)
	{
		case SCAN_EDIT_STATE_NONE:
			g_request_display_screen    = DISPLAY_MAIN;

			g_eeprom.cross_vfo_rx_tx = g_backup_cross_vfo_rx_tx;
			g_update_status            = true;
			g_flag_stop_scan            = true;
			g_vfo_configure_mode        = VFO_CONFIGURE_RELOAD;
			g_flag_reset_vfos           = true;
			#ifdef ENABLE_VOICE
				g_another_voice_id      = VOICE_ID_CANCEL;
			#endif
			break;

		case SCAN_EDIT_STATE_SAVE:
			if (g_input_box_index > 0)
			{
				g_input_box[--g_input_box_index] = 10;
				g_request_display_screen       = DISPLAY_SCANNER;
				break;
			}

			// Fallthrough

		case SCAN_EDIT_STATE_DONE:
			g_scanner_edit_state     = SCAN_EDIT_STATE_NONE;
			#ifdef ENABLE_VOICE
				g_another_voice_id   = VOICE_ID_CANCEL;
			#endif
			g_request_display_screen = DISPLAY_SCANNER;
			break;
	}
}

static void SCANNER_Key_MENU(bool key_pressed, bool key_held)
{
	uint8_t Channel;

	if (key_held || key_pressed)
		return;

	if (g_scan_css_state == SCAN_CSS_STATE_OFF && !g_scan_single_frequency)
	{
		g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	if (g_scan_css_state == SCAN_CSS_STATE_SCANNING)
	{
		if (g_scan_single_frequency)
		{
			g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			return;
		}
	}

	if (g_scan_css_state == SCAN_CSS_STATE_FAILED)
	{
		g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;

	switch (g_scanner_edit_state)
	{
		case SCAN_EDIT_STATE_NONE:
			if (!g_scan_single_frequency)
			{
				#if 0
					uint32_t Freq250 = FREQUENCY_FloorToStep(g_scan_frequency, 250, 0);
					uint32_t Freq625 = FREQUENCY_FloorToStep(g_scan_frequency, 625, 0);

					int16_t Delta250 = (int16_t)g_scan_frequency - (int16_t)Freq250;
					int16_t Delta625;

					if (125 < Delta250)
					{
						Delta250 = 250 - Delta250;
						Freq250 += 250;
					}

					Delta625 = (int16_t)g_scan_frequency - (int16_t)Freq625;

					if (312 < Delta625)
					{
						Delta625 = 625 - Delta625;
						Freq625 += 625;
					}

					if (Delta625 < Delta250)
					{
						g_step_setting = STEP_6_25kHz;
						g_scan_frequency = Freq625;
					}
					else
					{
						g_step_setting = STEP_2_5kHz;
						g_scan_frequency = Freq250;
					}
				#elif 0

					#ifdef ENABLE_1250HZ_STEP
						const step_setting_t small_step = STEP_1_25kHz;
						const step_setting_t big_step   = STEP_6_25kHz;
					#else
						const step_setting_t small_step = STEP_2_5kHz;
						const step_setting_t big_step   = STEP_6_25kHz;
					#endif

					const uint32_t small_step_freq = STEP_FREQ_TABLE[small_step];
					const uint32_t big_step_freq   = STEP_FREQ_TABLE[big_step];

					uint32_t freq_small_step = FREQUENCY_FloorToStep(g_scan_frequency, small_step_freq, 0);
					uint32_t freq_big_step   = FREQUENCY_FloorToStep(g_scan_frequency, big_step_freq,   0);

					int32_t delta_small_step = (int32_t)g_scan_frequency - freq_small_step;
					int32_t delta_big_step   = (int32_t)g_scan_frequency - freq_big_step;

					if (delta_small_step > 125)
					{
						delta_small_step = STEP_FREQ_TABLE[small_step] - delta_small_step;
						freq_big_step += small_step_freq;
					}

					delta_big_step = (int32_t)g_scan_frequency - freq_big_step;

					if (delta_big_step > 312)
					{
						delta_big_step = big_step_freq - delta_big_step;
						freq_big_step += big_step_freq;
					}

					if (delta_small_step >= delta_big_step)
					{
						g_step_setting   = small_step;
						g_scan_frequency = freq_small_step;
					}
					else
					{
						g_step_setting   = big_step;
						g_scan_frequency = freq_big_step;
					}

				#else
					#ifdef ENABLE_1250HZ_STEP
						g_step_setting = STEP_1_25kHz;
					#else
						g_step_setting = STEP_2_5kHz;
					#endif
					{	// round to the nearest step size
						const uint32_t step = STEP_FREQ_TABLE[g_step_setting];
						g_scan_frequency = ((g_scan_frequency + (step / 2)) / step) * step;
					}
				#endif
			}

			if (g_tx_vfo->channel_save <= USER_CHANNEL_LAST)
			{
				g_scan_channel       = g_tx_vfo->channel_save;
				g_show_chan_prefix   = RADIO_CheckValidChannel(g_tx_vfo->channel_save, false, 0);
				g_scanner_edit_state = SCAN_EDIT_STATE_SAVE;
			}
			else
			{
				#if 0
					// save the VFO
					g_scanner_edit_state = SCAN_EDIT_STATE_DONE;
				#else
					// save to a desired channel
					g_scan_channel       = RADIO_FindNextChannel(0, SCAN_FWD, false, g_eeprom.tx_vfo);
					g_show_chan_prefix   = RADIO_CheckValidChannel(g_scan_channel, false, 0);
					g_scanner_edit_state = SCAN_EDIT_STATE_SAVE;
				#endif
			}

			g_scan_css_state = SCAN_CSS_STATE_FOUND;

			#ifdef ENABLE_VOICE
				g_another_voice_id = VOICE_ID_MEMORY_CHANNEL;
			#endif

			g_request_display_screen = DISPLAY_SCANNER;
			g_update_status = true;
			break;

		case SCAN_EDIT_STATE_SAVE:
			if (g_input_box_index == 0)
			{
				g_beep_to_play           = BEEP_1KHZ_60MS_OPTIONAL;
				g_request_display_screen = DISPLAY_SCANNER;
				g_scanner_edit_state     = SCAN_EDIT_STATE_DONE;
			}
			break;

		case SCAN_EDIT_STATE_DONE:
			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				//UART_SendText("edit done\r\n");
			#endif

			if (!g_scan_single_frequency)
			{
				RADIO_InitInfo(g_tx_vfo, g_tx_vfo->channel_save, g_scan_frequency);

				if (g_scan_use_css_result)
				{
					g_tx_vfo->freq_config_rx.code_type = g_scan_css_result_type;
					g_tx_vfo->freq_config_rx.code      = g_scan_css_result_code;
				}

				g_tx_vfo->freq_config_tx = g_tx_vfo->freq_config_rx;
				g_tx_vfo->step_setting   = g_step_setting;
			}
			else
			{
				RADIO_ConfigureChannel(0, VFO_CONFIGURE_RELOAD);
				RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);

				g_tx_vfo->freq_config_rx.code_type = g_scan_css_result_type;
				g_tx_vfo->freq_config_rx.code      = g_scan_css_result_code;
				g_tx_vfo->freq_config_tx.code_type = g_scan_css_result_type;
				g_tx_vfo->freq_config_tx.code      = g_scan_css_result_code;
			}

			if (g_tx_vfo->channel_save <= USER_CHANNEL_LAST)
			{
				Channel = g_scan_channel;
				g_eeprom.user_channel[g_eeprom.tx_vfo] = Channel;
			}
			else
			{
				Channel = g_tx_vfo->band + FREQ_CHANNEL_FIRST;
				g_eeprom.freq_channel[g_eeprom.tx_vfo] = Channel;
			}

			g_tx_vfo->channel_save = Channel;
			g_eeprom.screen_channel[g_eeprom.tx_vfo] = Channel;
			g_request_save_channel = 2;

			#ifdef ENABLE_VOICE
				g_another_voice_id = VOICE_ID_CONFIRM;
			#endif

			g_scanner_edit_state = SCAN_EDIT_STATE_NONE;

			g_request_display_screen = DISPLAY_SCANNER;
			break;

		default:
			g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;
			break;
	}
}

static void SCANNER_Key_STAR(bool key_pressed, bool key_held)
{
	if (key_held || key_pressed)
		return;

	g_beep_to_play    = BEEP_1KHZ_60MS_OPTIONAL;
	g_flag_start_scan = true;
}

static void SCANNER_Key_UP_DOWN(bool key_pressed, bool pKeyHeld, int8_t Direction)
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

	if (g_scanner_edit_state == SCAN_EDIT_STATE_SAVE)
	{
		g_scan_channel           = NUMBER_AddWithWraparound(g_scan_channel, Direction, 0, USER_CHANNEL_LAST);
		g_show_chan_prefix       = RADIO_CheckValidChannel(g_scan_channel, false, 0);
		g_request_display_screen = DISPLAY_SCANNER;
	}
	else
		g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}

void SCANNER_ProcessKeys(key_code_t Key, bool key_pressed, bool key_held)
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
			SCANNER_Key_DIGITS(Key, key_pressed, key_held);
			break;
		case KEY_MENU:
			SCANNER_Key_MENU(key_pressed, key_held);
			break;
		case KEY_UP:
			SCANNER_Key_UP_DOWN(key_pressed, key_held,  1);
			break;
		case KEY_DOWN:
			SCANNER_Key_UP_DOWN(key_pressed, key_held, -1);
			break;
		case KEY_EXIT:
			SCANNER_Key_EXIT(key_pressed, key_held);
			break;
		case KEY_STAR:
			SCANNER_Key_STAR(key_pressed, key_held);
			break;
		case KEY_PTT:
			GENERIC_Key_PTT(key_pressed);
			break;
		default:
			if (!key_held && key_pressed)
				g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			break;
	}
}

void SCANNER_Start(void)
{
	uint8_t  BackupStep;
	uint16_t BackupStepFreq;

	BK4819_StopScan();

	RADIO_SelectVfos();

	#ifdef ENABLE_NOAA
		if (IS_NOAA_CHANNEL(g_rx_vfo->channel_save))
			g_rx_vfo->channel_save = FREQ_CHANNEL_FIRST + BAND6_400MHz;
	#endif

	BackupStep     = g_rx_vfo->step_setting;
	BackupStepFreq = g_rx_vfo->step_freq;

	RADIO_InitInfo(g_rx_vfo, g_rx_vfo->channel_save, g_rx_vfo->pRX->frequency);

	g_rx_vfo->step_setting = BackupStep;
	g_rx_vfo->step_freq    = BackupStepFreq;

	RADIO_SetupRegisters(true);

	#ifdef ENABLE_NOAA
		g_is_noaa_mode = false;
	#endif

	if (g_scan_single_frequency)
	{
		g_scan_css_state = SCAN_CSS_STATE_SCANNING;
		g_scan_frequency = g_rx_vfo->pRX->frequency;
		g_step_setting   = g_rx_vfo->step_setting;

		BK4819_PickRXFilterPathBasedOnFrequency(g_scan_frequency);
		BK4819_SetScanFrequency(g_scan_frequency);
	}
	else
	{
		g_scan_css_state = SCAN_CSS_STATE_OFF;
		g_scan_frequency = 0xFFFFFFFF;

		BK4819_PickRXFilterPathBasedOnFrequency(0xFFFFFFFF);
		BK4819_EnableFrequencyScan();
	}

	DTMF_clear_RX();

	g_scan_delay_10ms          = scan_freq_css_delay_10ms;
	g_scan_css_result_code     = 0xFF;
	g_scan_css_result_type     = 0xFF;
	g_scan_hit_count           = 0;
	g_scan_use_css_result      = false;
	g_CxCSS_tail_found         = false;
	g_CDCSS_lost               = false;
	g_CDCSS_code_type          = 0;
	g_CTCSS_lost               = false;
	#ifdef ENABLE_VOX
		g_vox_lost             = false;
	#endif
	g_squelch_lost             = false;
	g_scanner_edit_state       = SCAN_EDIT_STATE_NONE;
	g_scan_freq_css_timer_10ms = 0;
//	g_flag_start_scan          = false;

	g_update_status = true;
}

void SCANNER_Stop(void)
{
	const uint8_t Previous = g_restore_channel;

	if (g_scan_state_dir == SCAN_OFF)
		return;   // but, but, we weren't doing anything !

	g_scan_state_dir = SCAN_OFF;

	if (!g_scan_keep_frequency)
	{
		if (g_next_channel <= USER_CHANNEL_LAST)
		{
			g_eeprom.user_channel[g_eeprom.rx_vfo]   = g_restore_channel;
			g_eeprom.screen_channel[g_eeprom.rx_vfo] = Previous;

			RADIO_ConfigureChannel(g_eeprom.rx_vfo, VFO_CONFIGURE_RELOAD);
		}
		else
		{
			g_rx_vfo->freq_config_rx.frequency = g_restore_frequency;

			RADIO_ApplyOffset(g_rx_vfo);
			RADIO_ConfigureSquelchAndOutputPower(g_rx_vfo);
		}

		RADIO_SetupRegisters(true);

		g_update_display = true;
		return;
	}

	if (g_rx_vfo->channel_save > USER_CHANNEL_LAST)
	{
		RADIO_ApplyOffset(g_rx_vfo);
		RADIO_ConfigureSquelchAndOutputPower(g_rx_vfo);
		SETTINGS_SaveChannel(g_rx_vfo->channel_save, g_eeprom.rx_vfo, g_rx_vfo, 1);
		return;
	}

	SETTINGS_SaveVfoIndices();

	g_update_status = true;
}
