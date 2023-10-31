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
#include "app/fm.h"
#include "app/generic.h"
#include "audio.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/bk1080.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
#include "frequencies.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

#define STATE_FREQ_MODE 0
#define STATE_USER_MODE 1
#define STATE_SAVE      2

uint16_t            g_fm_channels[20];
bool                g_fm_radio_mode;
fm_scan_state_dir_t g_fm_scan_state_dir;
bool                g_fm_auto_scan;
uint8_t             g_fm_channel_position;
bool                g_fm_found_frequency;
uint8_t             g_fm_resume_tick_500ms;
uint16_t            g_fm_restore_tick_10ms;
uint8_t             g_fm_radio_tick_500ms;
volatile uint16_t   g_fm_play_tick_10ms;
volatile bool       g_fm_schedule;

bool FM_check_valid_channel(const unsigned int Channel)
{
	return (Channel < ARRAY_SIZE(g_fm_channels) && (g_fm_channels[Channel] >= BK1080_freq_lower && g_fm_channels[Channel] < BK1080_freq_upper)) ? true : false;
}

unsigned int FM_find_next_channel(unsigned int Channel, const fm_scan_state_dir_t scan_state_dir)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(g_fm_channels); i++)
	{
		if (Channel > ARRAY_SIZE(g_fm_channels))
			Channel = ARRAY_SIZE(g_fm_channels) - 1;
		else
		if (Channel >= ARRAY_SIZE(g_fm_channels))
			Channel = 0;

		if (FM_check_valid_channel(Channel))
			return Channel;

		Channel += scan_state_dir;
	}

	return 0xFF;
}

int FM_configure_channel_state(void)
{
	g_eeprom.fm_frequency_playing = g_eeprom.fm_selected_frequency;

	if (g_eeprom.fm_channel_mode)
	{
		const uint8_t Channel = FM_find_next_channel(g_eeprom.fm_selected_channel, FM_CHANNEL_UP);
		if (Channel == 0xFF)
		{
			g_eeprom.fm_channel_mode = false;
			return -1;
		}

		g_eeprom.fm_selected_channel  = Channel;
		g_eeprom.fm_frequency_playing = g_fm_channels[Channel];
	}

	return 0;
}

void FM_erase_channels(void)
{
	unsigned int i;
	uint8_t      Template[8];

	memset(Template, 0xFF, sizeof(Template));
	for (i = 0; i < 5; i++)
		EEPROM_WriteBuffer8(0x0E40 + (i * 8), Template);

	memset(g_fm_channels, 0xFF, sizeof(g_fm_channels));
}

void FM_tune(uint16_t frequency, const fm_scan_state_dir_t scan_state_dir, const bool flag)
{
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

	g_fm_play_tick_10ms = (g_fm_scan_state_dir == FM_SCAN_STATE_DIR_OFF) ? fm_play_noscan_10ms : fm_play_scan_10ms;

	g_fm_schedule                 = false;
	g_fm_found_frequency          = false;
	g_ask_to_save                 = false;
	g_ask_to_delete               = false;
	g_eeprom.fm_frequency_playing = frequency;

	if (!flag)
	{	// wrap-a-around

		frequency += scan_state_dir;
		if (frequency < BK1080_freq_lower)
			frequency = BK1080_freq_upper - 1u;
		else
		if (frequency >= BK1080_freq_upper)
			frequency =  BK1080_freq_lower;

		g_eeprom.fm_frequency_playing = frequency;
	}

	g_fm_scan_state_dir = scan_state_dir;

	BK1080_SetFrequency(g_eeprom.fm_frequency_playing);

	if (g_fm_resume_tick_500ms < 10)
		g_fm_resume_tick_500ms = 10;  // update display for next 5 seconds
}

void FM_stop_scan(void)
{
	if (g_fm_scan_state_dir == FM_SCAN_STATE_DIR_OFF)
		return;

	g_fm_scan_state_dir = FM_SCAN_STATE_DIR_OFF;

	if (g_fm_auto_scan || g_eeprom.fm_channel_mode)
	{	// switch to channel mode
		g_eeprom.fm_channel_mode     = true;
		g_eeprom.fm_selected_channel = 0;
		FM_configure_channel_state();
	}
	else
	{
		g_eeprom.fm_channel_mode = false;
	}

	BK1080_SetFrequency(g_eeprom.fm_frequency_playing);

	SETTINGS_save_fm();

	g_fm_play_tick_10ms = 0;
	g_fm_schedule       = false;
	g_ask_to_save       = false;

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

	if (g_fm_resume_tick_500ms < 10)
		g_fm_resume_tick_500ms = 10;  // update display for next 5 seconds

	g_update_display = true;
}

int FM_check_frequency_lock(const uint16_t frequency, const uint16_t lower_limit)
{
	int ret = -1;

	const uint16_t rssi_status = BK1080_ReadRegister(BK1080_REG_10);
	const uint16_t dev_snr     = BK1080_ReadRegister(BK1080_REG_07);

	const int16_t freq_offset  = (int16_t)dev_snr / 16;
	const uint8_t snr          = dev_snr & 0x000f;

//	const uint8_t stc          = (rssi_status >> 14) & 1u;
//	const uint8_t sf_bl        = (rssi_status >> 13) & 1u;
	const uint8_t afc_railed   = (rssi_status >> 12) & 1u;
//	const uint8_t ste          = (rssi_status >> 9) & 1u;
//	const uint8_t st           = (rssi_status >> 8) & 1u;
	const uint8_t rssi         =  rssi_status & 0x00ff;

	if (afc_railed || snr < 2 || rssi < 10 || abs(freq_offset) > 250)
		goto Bail;

	if (frequency >= lower_limit && abs(((int)BK1080_freq_base - frequency)) == 1)
		if (abs(BK1080_freq_offset) < 20)
			goto Bail;

	ret = 0;

Bail:
	BK1080_freq_offset = freq_offset;
	BK1080_freq_base   = frequency;

	return ret;
}

void FM_scan(void)
{
	if (!FM_check_frequency_lock(g_eeprom.fm_frequency_playing, BK1080_freq_lower))
	{
		if (!g_fm_auto_scan)
		{
			g_fm_play_tick_10ms  = 0;
			g_fm_found_frequency = true;

			if (!g_eeprom.fm_channel_mode)
				g_eeprom.fm_selected_frequency = g_eeprom.fm_frequency_playing;

			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

			GUI_SelectNextDisplay(DISPLAY_FM);
			return;
		}

		if (g_fm_channel_position < ARRAY_SIZE(g_fm_channels))
			g_fm_channels[g_fm_channel_position++] = g_eeprom.fm_frequency_playing;

		if (g_fm_channel_position >= ARRAY_SIZE(g_fm_channels))
		{
			FM_stop_scan();
			GUI_SelectNextDisplay(DISPLAY_FM);
			return;
		}
	}

	if (g_fm_auto_scan && g_eeprom.fm_frequency_playing >= (BK1080_freq_upper - 1u))
		FM_stop_scan();
	else
		FM_tune(g_eeprom.fm_frequency_playing, g_fm_scan_state_dir, false);

	GUI_SelectNextDisplay(DISPLAY_FM);
}

void FM_turn_on(void)
{
	// mute the audio from the other radio chip (the transceiver chip)
	BK4819_SetAF(BK4819_AF_MUTE);

	g_fm_radio_mode        = true;
	g_fm_scan_state_dir    = FM_SCAN_STATE_DIR_OFF;
	g_fm_restore_tick_10ms = 0;
	g_fm_resume_tick_500ms = fm_resume_500ms;  // update display again in 'n' seconds

	// enable the FM radio chip/audio
	BK1080_Init(g_eeprom.fm_frequency_playing, true);

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

	g_update_display = true;
	g_update_status  = true;
}

void FM_turn_off(void)
{
	if (g_fm_radio_mode)
	{
		if (!g_squelch_open && !g_monitor_enabled)
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

		// disable the FM chip
		BK1080_Init(0, false);

		g_update_display = true;
		g_update_status  = true;
	}

	g_fm_radio_mode        = false;
	g_fm_scan_state_dir    = FM_SCAN_STATE_DIR_OFF;
	g_fm_restore_tick_10ms = 0;
	g_fm_resume_tick_500ms = 0;
}

void FM_toggle_chan_freq_mode(void)
{
	g_eeprom.fm_channel_mode = !g_eeprom.fm_channel_mode;

	FM_stop_scan();

	if (!FM_configure_channel_state())
	{
		BK1080_SetFrequency(g_eeprom.fm_frequency_playing);
		g_request_save_fm = true;
	}
}

// ***************************************

static void FM_Key_DIGITS(const key_code_t Key, const bool key_pressed, const bool key_held)
{

	// beeps cause bad audio clicks anf audio breaks
	// so don't use them

	g_key_input_count_down = key_input_timeout_500ms;

	if (key_held && !key_pressed)
		return;  // key just released after long press

	if (!key_held && key_pressed)
	{	// key just pressed
		return;
	}

	// long press key or short key release

	if (g_input_box_index == 0)
	{
		if (Key == KEY_0 || (Key >= KEY_2 && Key <= KEY_5))
		{	// can't start a frequency with those keys
			if (Key == KEY_3)
			{	// can't start a frequency with a '3', so just go straight to the function
				FM_toggle_chan_freq_mode();
			}
			return;
		}
	}

	if (!g_fkey_pressed && !key_held)
	{	// short key release
		uint8_t State;

		if (g_ask_to_delete)
			return;

		if (g_ask_to_save)
		{
			State = STATE_SAVE;
		}
		else
		{
			if (g_fm_scan_state_dir != FM_SCAN_STATE_DIR_OFF)
				return;

			State = g_eeprom.fm_channel_mode ? STATE_USER_MODE : STATE_FREQ_MODE;
		}

		INPUTBOX_append(Key);

		g_request_display_screen = DISPLAY_FM;

		if (State == STATE_FREQ_MODE)
		{	// frequency mode

			if (g_input_box_index == 1)
			{
				if (g_input_box[0] > 1)
				{
					g_input_box[1] = g_input_box[0];
					g_input_box[0] = 0;
					g_input_box_index = 2;
				}
			}
			else
			if (g_input_box_index > 3)
			{
				uint32_t Frequency;

				g_input_box_index = 0;

				NUMBER_Get(g_input_box, &Frequency);

				Frequency /= 10000;

				if (Frequency <  BK1080_freq_lower)
					Frequency =  BK1080_freq_lower;
				if (Frequency >= BK1080_freq_upper)
					Frequency =  BK1080_freq_upper - 1u;

				g_eeprom.fm_selected_frequency = (uint16_t)Frequency;

				#ifdef ENABLE_VOICE
					g_another_voice_id = (voice_id_t)Key;
				#endif

				g_eeprom.fm_frequency_playing = g_eeprom.fm_selected_frequency;
				BK1080_SetFrequency(g_eeprom.fm_frequency_playing);

				g_request_save_fm = true;
				return;
			}
		}
		else
		if (g_input_box_index == 2)
		{	// channel mode

			uint8_t Channel;

			g_input_box_index = 0;
			Channel = ((g_input_box[0] * 10) + g_input_box[1]) - 1;

			if (State == STATE_USER_MODE)
			{
				if (FM_check_valid_channel(Channel))
				{
					#ifdef ENABLE_VOICE
						g_another_voice_id = (voice_id_t)Key;
					#endif

					g_eeprom.fm_selected_channel  = Channel;
					g_eeprom.fm_frequency_playing = g_fm_channels[Channel];

					BK1080_SetFrequency(g_eeprom.fm_frequency_playing);

					g_request_save_fm = true;
					return;
				}
			}
			else
			if (Channel < ARRAY_SIZE(g_fm_channels))
			{
				#ifdef ENABLE_VOICE
					g_another_voice_id = (voice_id_t)Key;
				#endif

				g_request_display_screen = DISPLAY_FM;
				g_input_box_index        = 0;
				g_fm_channel_position    = Channel;
				return;
			}

			return;
		}

		#ifdef ENABLE_VOICE
			g_another_voice_id = (voice_id_t)Key;
		#endif

		return;
	}

	// with f-key, or long press

	g_fkey_pressed           = false;
	g_update_status          = true;
	g_request_display_screen = DISPLAY_FM;

	switch (Key)
	{
		case KEY_0:
			ACTION_FM();
			break;

		case KEY_3:
			FM_toggle_chan_freq_mode();
			break;

		default:
			break;
	}
}

static void FM_Key_STAR(const bool key_pressed, const bool key_held)
{
	g_key_input_count_down = key_input_timeout_500ms;

	if (!key_held && !key_pressed)
	{
		ACTION_Scan(false);   // short key press just released .. frequency scan without store
	}
	else
	if (key_held && key_pressed)
	{
		ACTION_Scan(true);    // long key press still pressed .. frequency scan and store
	}

	g_fkey_pressed  = false;
	g_update_status = true;
}

static void FM_Key_EXIT(const bool key_pressed, const bool key_held)
{
	(void)key_held;

	g_key_input_count_down = key_input_timeout_500ms;

//	if (key_held || key_pressed)
	if (key_pressed)
		return;

	if (g_fm_scan_state_dir == FM_SCAN_STATE_DIR_OFF)
	{
		if (g_input_box_index == 0)
		{
			if (!g_ask_to_save && !g_ask_to_delete)
			{
				ACTION_FM();
				return;
			}

			g_ask_to_save   = false;
			g_ask_to_delete = false;
		}
		else
		{
			g_input_box[--g_input_box_index] = 10;

			if (g_input_box_index)
			{
				if (g_input_box_index != 1)
				{
					g_request_display_screen = DISPLAY_FM;
					return;
				}

				if (g_input_box[0] != 0)
				{
					g_request_display_screen = DISPLAY_FM;
					return;
				}
			}

			g_input_box_index = 0;
		}

		#ifdef ENABLE_VOICE
			g_another_voice_id = VOICE_ID_CANCEL;
		#endif
	}
	else
	{
		FM_stop_scan();

		#ifdef ENABLE_VOICE
			g_another_voice_id = VOICE_ID_SCANNING_STOP;
		#endif
	}

	g_request_display_screen = DISPLAY_FM;
}

static void FM_Key_MENU(const bool key_pressed, const bool key_held)
{
	(void)key_held;

	unsigned int i;
	int channel = -1;

	g_key_input_count_down = key_input_timeout_500ms;

//	if (key_held || key_pressed)
	if (key_pressed)
		return;   // key still pressed

	// see if the frequency is already stored in a channel
	for (i = 0; i < ARRAY_SIZE(g_fm_channels) && channel < 0; i++)
		if (g_fm_channels[i] == g_eeprom.fm_frequency_playing)
			channel = i;  // found it in the channel list

	g_request_display_screen = DISPLAY_FM;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		//UART_SendText("fm menu 1\r\n");
	#endif

	if (g_fm_scan_state_dir == FM_SCAN_STATE_DIR_OFF)
	{	// not scanning

		if (!g_eeprom.fm_channel_mode)
		{	// frequency mode

			if (g_ask_to_save)
			{
				if (channel < 0)
				{
					g_fm_channels[g_fm_channel_position] = g_eeprom.fm_frequency_playing;
					g_ask_to_save                        = false;
					g_request_save_fm                    = true;
				}
			}
			else
			if (channel < 0)
				g_ask_to_save = true;
		}
		else
		{	// channel mode
			if (g_ask_to_delete)
			{
				g_fm_channels[g_eeprom.fm_selected_channel] = 0xFFFF;

				FM_configure_channel_state();
				BK1080_SetFrequency(g_eeprom.fm_frequency_playing);

				g_request_save_fm = true;
				g_ask_to_delete   = false;
			}
			else
				g_ask_to_delete = true;
		}

		return;
	}

	// scanning

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		//UART_SendText("fm menu 2\r\n");
	#endif

	if (g_fm_auto_scan || !g_fm_found_frequency)
	{
		g_input_box_index = 0;
		return;
	}

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		//UART_SendText("fm menu 3\r\n");
	#endif

	if (g_ask_to_save)
	{
		g_fm_channels[g_fm_channel_position] = g_eeprom.fm_frequency_playing;
		g_ask_to_save     = false;
		g_request_save_fm = true;
		return;
	}

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		//UART_SendText("fm menu 4\r\n");
	#endif

	if (channel < 0)
		g_ask_to_save = true;
}

static void FM_Key_UP_DOWN(const bool key_pressed, const bool key_held, const fm_scan_state_dir_t scan_state_dir)
{
	g_key_input_count_down = key_input_timeout_500ms;

	if (key_held || !key_pressed)
	{
		if (g_input_box_index > 0)
			return;

		if (!key_pressed)
			return;
	}
	else
	if (g_input_box_index > 0)
		return;

	if (g_ask_to_save)
	{
		g_request_display_screen = DISPLAY_FM;
		g_fm_channel_position    = NUMBER_AddWithWraparound(g_fm_channel_position, scan_state_dir, 0, 19);
		return;
	}

	if (g_fm_scan_state_dir != FM_SCAN_STATE_DIR_OFF)
	{	// scanning

		if (g_fm_auto_scan)
			return;

		FM_tune(g_eeprom.fm_frequency_playing, scan_state_dir, false);

		g_request_display_screen = DISPLAY_FM;
		return;
	}

	if (g_eeprom.fm_channel_mode)
	{	// we're in channel mode
		const uint8_t Channel = FM_find_next_channel(g_eeprom.fm_selected_channel + scan_state_dir, scan_state_dir);
		if (Channel == 0xFF || g_eeprom.fm_selected_channel == Channel)
			goto Bail;

		g_eeprom.fm_selected_channel  = Channel;
		g_eeprom.fm_frequency_playing = g_fm_channels[Channel];
	}
	else
	{	// no, frequency mode
		uint16_t Frequency = g_eeprom.fm_selected_frequency + scan_state_dir;
		if (Frequency < BK1080_freq_lower)
			Frequency = BK1080_freq_upper - 1u;
		else
		if (Frequency >= BK1080_freq_upper)
			Frequency =  BK1080_freq_lower;

		g_eeprom.fm_frequency_playing  = Frequency;
		g_eeprom.fm_selected_frequency = g_eeprom.fm_frequency_playing;
	}

	if (g_current_display_screen == DISPLAY_FM && g_fm_scan_state_dir == FM_SCAN_STATE_DIR_OFF)
	{
//		if (g_fm_resume_tick_500ms < fm_resume_500ms)
//			g_fm_resume_tick_500ms = fm_resume_500ms;  // update display for next 'n' seconds
		if (g_fm_resume_tick_500ms < 10)
			g_fm_resume_tick_500ms = 10;  // update display for next 5 seconds
		g_update_display = true;
	}

	g_request_save_fm = true;

Bail:
	BK1080_SetFrequency(g_eeprom.fm_frequency_playing);

	g_request_display_screen = DISPLAY_FM;
}

void FM_process_key(key_code_t Key, bool key_pressed, bool key_held)
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
			FM_Key_DIGITS(Key, key_pressed, key_held);
			break;
		case KEY_MENU:
			FM_Key_MENU(key_pressed, key_held);
			return;
		case KEY_UP:
			FM_Key_UP_DOWN(key_pressed, key_held, FM_SCAN_STATE_DIR_UP);
			break;
		case KEY_DOWN:
			FM_Key_UP_DOWN(key_pressed, key_held, FM_SCAN_STATE_DIR_DOWN);
			break;;
		case KEY_STAR:
			FM_Key_STAR(key_pressed, key_held);
			break;
		case KEY_EXIT:
			FM_Key_EXIT(key_pressed, key_held);
			break;
		case KEY_F:
			GENERIC_Key_F(key_pressed, key_held);
			break;
		case KEY_PTT:
			GENERIC_Key_PTT(key_pressed);
			break;
		default:
			break;
	}
}
