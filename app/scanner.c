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
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

dcs_code_type_t    gScanCssResultType;
uint8_t           gScanCssResultCode;
bool              g_flag_start_scan;
bool              g_flag_stop_scan;
bool              g_scan_single_frequency;
SCAN_edit_state_t gScannerEditState;
uint8_t           gScanChannel;
uint32_t          gScanFrequency;
bool              gScanPauseMode;
SCAN_CssState_t   gScanCssState;
volatile bool     g_schedule_scan_listen = true;
volatile uint16_t g_scan_pause_delay_in_10ms;
uint8_t           gScanProgressIndicator;
uint8_t           gScanHitCount;
bool              gScanUseCssResult;
int8_t            g_scan_state_dir;
bool              bScanKeepFrequency;

static void SCANNER_Key_DIGITS(key_code_t Key, bool key_pressed, bool key_held)
{
	if (!key_held && key_pressed)
	{
		if (gScannerEditState == SCAN_EDIT_STATE_BUSY)
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
				gScanChannel  = (uint8_t)Channel;
				return;
			}
		}

		g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
	}
}

static void SCANNER_Key_EXIT(bool key_pressed, bool key_held)
{
	if (!key_held && key_pressed)
	{
		g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;

		switch (gScannerEditState)
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

			case SCAN_EDIT_STATE_BUSY:
				if (g_input_box_index > 0)
				{
					g_input_box[--g_input_box_index] = 10;
					g_request_display_screen       = DISPLAY_SCANNER;
					break;
				}

				// Fallthrough

			case SCAN_EDIT_STATE_DONE:
				gScannerEditState     = SCAN_EDIT_STATE_NONE;
				#ifdef ENABLE_VOICE
					g_another_voice_id   = VOICE_ID_CANCEL;
				#endif
				g_request_display_screen = DISPLAY_SCANNER;
				break;
		}
	}
}

static void SCANNER_Key_MENU(bool key_pressed, bool key_held)
{
	uint8_t Channel;

	if (key_held)
		return;

	if (!key_pressed)
		return;

	if (gScanCssState == SCAN_CSS_STATE_OFF && !g_scan_single_frequency)
	{
		g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	if (gScanCssState == SCAN_CSS_STATE_SCANNING)
	{
		if (g_scan_single_frequency)
		{
			g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			return;
		}
	}

	if (gScanCssState == SCAN_CSS_STATE_FAILED)
	{
		g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;

	switch (gScannerEditState)
	{
		case SCAN_EDIT_STATE_NONE:
			if (!g_scan_single_frequency)
			{

				#if 0
					uint32_t Freq250 = FREQUENCY_FloorToStep(gScanFrequency, 250, 0);
					uint32_t Freq625 = FREQUENCY_FloorToStep(gScanFrequency, 625, 0);

					int16_t Delta250 = (int16_t)gScanFrequency - (int16_t)Freq250;
					int16_t Delta625;

					if (125 < Delta250)
					{
						Delta250 = 250 - Delta250;
						Freq250 += 250;
					}

					Delta625 = (int16_t)gScanFrequency - (int16_t)Freq625;

					if (312 < Delta625)
					{
						Delta625 = 625 - Delta625;
						Freq625 += 625;
					}

					if (Delta625 < Delta250)
					{
						g_step_setting = STEP_6_25kHz;
						gScanFrequency = Freq625;
					}
					else
					{
						g_step_setting = STEP_2_5kHz;
						gScanFrequency = Freq250;
					}
				#else

					#ifdef ENABLE_1250HZ_STEP
						const step_setting_t small_step = STEP_1_25kHz;
						const step_setting_t big_step   = STEP_6_25kHz;
					#else
						const step_setting_t small_step = STEP_2_5kHz;
						const step_setting_t big_step   = STEP_6_25kHz;
					#endif

					const uint32_t small_step_freq = STEP_FREQ_TABLE[small_step];
					const uint32_t big_step_freq   = STEP_FREQ_TABLE[big_step];

					uint32_t freq_small_step = FREQUENCY_FloorToStep(gScanFrequency, small_step_freq, 0);
					uint32_t freq_big_step   = FREQUENCY_FloorToStep(gScanFrequency, big_step_freq,   0);

					int32_t delta_small_step = (int32_t)gScanFrequency - freq_small_step;
					int32_t delta_big_step   = (int32_t)gScanFrequency - freq_big_step;

					if (delta_small_step > 125)
					{
						delta_small_step = STEP_FREQ_TABLE[small_step] - delta_small_step;
						freq_big_step += small_step_freq;
					}

					delta_big_step = (int32_t)gScanFrequency - freq_big_step;

					if (delta_big_step > 312)
					{
						delta_big_step = big_step_freq - delta_big_step;
						freq_big_step += big_step_freq;
					}

					if (delta_small_step >= delta_big_step)
					{
						g_step_setting   = small_step;
						gScanFrequency = freq_small_step;
					}
					else
					{
						g_step_setting   = big_step;
						gScanFrequency = freq_big_step;
					}
				#endif
			}

			if (g_tx_vfo->channel_save <= USER_CHANNEL_LAST)
			{
				gScannerEditState = SCAN_EDIT_STATE_BUSY;
				gScanChannel      = g_tx_vfo->channel_save;
				g_show_chan_prefix     = RADIO_CheckValidChannel(g_tx_vfo->channel_save, false, 0);
			}
			else
			{
				gScannerEditState = SCAN_EDIT_STATE_DONE;
			}

			gScanCssState = SCAN_CSS_STATE_FOUND;

			#ifdef ENABLE_VOICE
				g_another_voice_id = VOICE_ID_MEMORY_CHANNEL;
			#endif

			g_request_display_screen = DISPLAY_SCANNER;
			g_update_status = true;
			break;

		case SCAN_EDIT_STATE_BUSY:
			if (g_input_box_index == 0)
			{
				g_beep_to_play           = BEEP_1KHZ_60MS_OPTIONAL;
				g_request_display_screen = DISPLAY_SCANNER;
				gScannerEditState     = SCAN_EDIT_STATE_DONE;
			}
			break;

		case SCAN_EDIT_STATE_DONE:
			if (!g_scan_single_frequency)
			{
				RADIO_InitInfo(g_tx_vfo, g_tx_vfo->channel_save, gScanFrequency);

				if (gScanUseCssResult)
				{
					g_tx_vfo->freq_config_rx.code_type = gScanCssResultType;
					g_tx_vfo->freq_config_rx.code     = gScanCssResultCode;
				}

				g_tx_vfo->freq_config_tx = g_tx_vfo->freq_config_rx;
				g_tx_vfo->step_setting   = g_step_setting;
			}
			else
			{
				RADIO_ConfigureChannel(0, VFO_CONFIGURE_RELOAD);
				RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);

				g_tx_vfo->freq_config_rx.code_type = gScanCssResultType;
				g_tx_vfo->freq_config_rx.code     = gScanCssResultCode;
				g_tx_vfo->freq_config_tx.code_type = gScanCssResultType;
				g_tx_vfo->freq_config_tx.code     = gScanCssResultCode;
			}

			if (g_tx_vfo->channel_save <= USER_CHANNEL_LAST)
			{
				Channel = gScanChannel;
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

			gScannerEditState = SCAN_EDIT_STATE_NONE;

			g_request_display_screen = DISPLAY_SCANNER;
			break;

		default:
			g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;
			break;
	}
}

static void SCANNER_Key_STAR(bool key_pressed, bool key_held)
{
	if (!key_held && key_pressed)
	{
		g_beep_to_play    = BEEP_1KHZ_60MS_OPTIONAL;
		g_flag_start_scan = true;
	}
	return;
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
		if (!key_pressed)
			return;

		g_input_box_index = 0;
		g_beep_to_play    = BEEP_1KHZ_60MS_OPTIONAL;
	}

	if (gScannerEditState == SCAN_EDIT_STATE_BUSY)
	{
		gScanChannel          = NUMBER_AddWithWraparound(gScanChannel, Direction, 0, USER_CHANNEL_LAST);
		g_show_chan_prefix         = RADIO_CheckValidChannel(gScanChannel, false, 0);
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

	g_rx_vfo->step_setting  = BackupStep;
	g_rx_vfo->step_freq = BackupStepFreq;

	RADIO_SetupRegisters(true);

	#ifdef ENABLE_NOAA
		g_is_noaa_mode = false;
	#endif

	if (g_scan_single_frequency)
	{
		gScanCssState  = SCAN_CSS_STATE_SCANNING;
		gScanFrequency = g_rx_vfo->pRX->frequency;
		g_step_setting   = g_rx_vfo->step_setting;

		BK4819_PickRXFilterPathBasedOnFrequency(gScanFrequency);
		BK4819_SetScanFrequency(gScanFrequency);
	}
	else
	{
		gScanCssState  = SCAN_CSS_STATE_OFF;
		gScanFrequency = 0xFFFFFFFF;

		BK4819_PickRXFilterPathBasedOnFrequency(0xFFFFFFFF);
		BK4819_EnableFrequencyScan();
	}

	DTMF_clear_RX();

	g_scan_delay_10ms        = scan_freq_css_delay_10ms;
	gScanCssResultCode     = 0xFF;
	gScanCssResultType     = 0xFF;
	gScanHitCount          = 0;
	gScanUseCssResult      = false;
	g_CxCSS_tail_found     = false;
	g_CDCSS_lost           = false;
	g_CDCSS_code_type         = 0;
	g_CTCSS_lost           = false;
	#ifdef ENABLE_VOX
		g_vox_lost         = false;
	#endif
	g_squelch_lost          = false;
	gScannerEditState      = SCAN_EDIT_STATE_NONE;
	gScanProgressIndicator = 0;
//	g_flag_start_scan         = false;

	g_update_status = true;
}

void SCANNER_Stop(void)
{
	const uint8_t Previous = g_restore_channel;

	if (g_scan_state_dir == SCAN_OFF)
		return;   // but, but, we weren't !
	
	g_scan_state_dir = SCAN_OFF;

	if (!bScanKeepFrequency)
	{
		if (g_next_channel <= USER_CHANNEL_LAST)
		{
			g_eeprom.user_channel[g_eeprom.rx_vfo]     = g_restore_channel;
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
