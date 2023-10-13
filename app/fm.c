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
#include "app/fm.h"
#include "app/generic.h"
#include "audio.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/bk1080.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

#define STATE_FREQ_MODE 0
#define STATE_USER_MODE 1
#define STATE_SAVE      2

uint16_t          g_fm_channels[20];
bool              g_fm_radio_mode;
uint8_t           g_fm_radio_count_down_500ms;
volatile uint16_t g_fm_play_count_down_10ms;
volatile int8_t   g_fm_scan_state;
bool              g_fm_auto_scan;
uint8_t           g_fm_channel_position;
bool              g_fm_found_frequency;
bool              g_fm_auto_scan;
uint8_t           g_fm_resume_count_down_500ms;
uint16_t          g_fm_restore_count_down_10ms;

bool FM_CheckValidChannel(uint8_t Channel)
{
	return (Channel < ARRAY_SIZE(g_fm_channels) && (g_fm_channels[Channel] >= 760 && g_fm_channels[Channel] < 1080)) ? true : false;
}

uint8_t FM_FindNextChannel(uint8_t Channel, uint8_t Direction)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(g_fm_channels); i++)
	{
		if (Channel == 0xFF)
			Channel  = ARRAY_SIZE(g_fm_channels) - 1;
		else
		if (Channel >= ARRAY_SIZE(g_fm_channels))
			Channel = 0;

		if (FM_CheckValidChannel(Channel))
			return Channel;

		Channel += Direction;
	}

	return 0xFF;
}

int FM_ConfigureChannelState(void)
{
	g_eeprom.fm_frequency_playing = g_eeprom.fm_selected_frequency;

	if (g_eeprom.fm_is_channel_mode)
	{
		const uint8_t Channel = FM_FindNextChannel(g_eeprom.fm_selected_channel, FM_CHANNEL_UP);
		if (Channel == 0xFF)
		{
			g_eeprom.fm_is_channel_mode = false;
			return -1;
		}

		g_eeprom.fm_selected_channel  = Channel;
		g_eeprom.fm_frequency_playing = g_fm_channels[Channel];
	}

	return 0;
}

void FM_TurnOff(void)
{
	g_fm_radio_mode              = false;
	g_fm_scan_state              = FM_SCAN_OFF;
	g_fm_restore_count_down_10ms = 0;

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

	g_enable_speaker = false;

	BK1080_Init(0, false);

	g_update_status  = true;
}

void FM_EraseChannels(void)
{
	unsigned int i;
	uint8_t      Template[8];

	memset(Template, 0xFF, sizeof(Template));
	for (i = 0; i < 5; i++)
		EEPROM_WriteBuffer(0x0E40 + (i * 8), Template);

	memset(g_fm_channels, 0xFF, sizeof(g_fm_channels));
}

void FM_Tune(uint16_t Frequency, int8_t Step, bool flag)
{
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

	g_enable_speaker = false;

	g_fm_play_count_down_10ms = (g_fm_scan_state == FM_SCAN_OFF) ? fm_play_countdown_noscan_10ms : fm_play_countdown_scan_10ms;

	g_schedule_fm                 = false;
	g_fm_found_frequency          = false;
	g_ask_to_save                 = false;
	g_ask_to_delete               = false;
	g_eeprom.fm_frequency_playing = Frequency;

	if (!flag)
	{
		Frequency += Step;
		if (Frequency < FM_RADIO_BAND.lower)
			Frequency = FM_RADIO_BAND.upper;
		else
		if (Frequency > FM_RADIO_BAND.upper)
			Frequency = FM_RADIO_BAND.lower;

		g_eeprom.fm_frequency_playing = Frequency;
	}

	g_fm_scan_state = Step;

	BK1080_SetFrequency(g_eeprom.fm_frequency_playing);
}

void FM_PlayAndUpdate(void)
{
	g_fm_scan_state = FM_SCAN_OFF;

	if (g_fm_auto_scan)
	{
		g_eeprom.fm_is_channel_mode  = true;
		g_eeprom.fm_selected_channel = 0;
	}

	FM_ConfigureChannelState();
	BK1080_SetFrequency(g_eeprom.fm_frequency_playing);
	SETTINGS_SaveFM();

	g_fm_play_count_down_10ms = 0;
	g_schedule_fm             = false;
	g_ask_to_save             = false;

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

	g_enable_speaker = true;
}

int FM_CheckFrequencyLock(uint16_t Frequency, uint16_t LowerLimit)
{
	int ret = -1;

	const uint16_t Test2 = BK1080_ReadRegister(BK1080_REG_07);

	// This is supposed to be a signed value, but above function is unsigned
	const uint16_t Deviation = BK1080_REG_07_GET_FREQD(Test2);

	if (BK1080_REG_07_GET_SNR(Test2) >= 2)
	{
		const uint16_t Status = BK1080_ReadRegister(BK1080_REG_10);

		if ((Status & BK1080_REG_10_MASK_AFCRL) == BK1080_REG_10_AFCRL_NOT_RAILED &&
		    BK1080_REG_10_GET_RSSI(Status) >= 10)
		{
			//if (Deviation > -281 && Deviation < 280)
			if (Deviation < 280 || Deviation > 3815)
			{
				if (Frequency > LowerLimit && (Frequency - BK1080_BaseFrequency) == 1)
				{
					if (BK1080_FrequencyDeviation & 0x800)
						goto Bail;

					if (BK1080_FrequencyDeviation < 20)
						goto Bail;
				}

				if (Frequency >= LowerLimit && (BK1080_BaseFrequency - Frequency) == 1)
				{
					if ((BK1080_FrequencyDeviation & 0x800) == 0)
						goto Bail;

					// if (BK1080_FrequencyDeviation > -21)
					if (BK1080_FrequencyDeviation > 4075)
						goto Bail;
				}

				ret = 0;
			}
		}
	}

Bail:
	BK1080_FrequencyDeviation = Deviation;
	BK1080_BaseFrequency      = Frequency;

	return ret;
}

static void FM_Key_DIGITS(key_code_t Key, bool key_pressed, bool key_held)
{


	// beeps cause bad audio clicks anf audio breaks :(
	// so don't use them


	g_key_input_count_down = key_input_timeout_500ms;

	if (key_held && !key_pressed)
		return;  // key just released after long press

	if (!key_held && key_pressed)
	{	// key just pressed
		return;
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
			if (g_fm_scan_state != FM_SCAN_OFF)
				return;

			State = g_eeprom.fm_is_channel_mode ? STATE_USER_MODE : STATE_FREQ_MODE;
		}

		INPUTBOX_Append(Key);

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

				if (Frequency < FM_RADIO_BAND.lower || Frequency > FM_RADIO_BAND.upper)
				{
					g_request_display_screen = DISPLAY_FM;
					return;
				}

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
				if (FM_CheckValidChannel(Channel))
				{
					#ifdef ENABLE_VOICE
						g_another_voice_id = (voice_id_t)Key;
					#endif
					g_eeprom.fm_selected_channel = Channel;
					g_eeprom.fm_frequency_playing = g_fm_channels[Channel];
					BK1080_SetFrequency(g_eeprom.fm_frequency_playing);
					g_request_save_fm = true;
					return;
				}
			}
			else
			if (Channel < 20)
			{
				#ifdef ENABLE_VOICE
					g_another_voice_id = (voice_id_t)Key;
				#endif
				g_request_display_screen = DISPLAY_FM;
				g_input_box_index = 0;
				g_fm_channel_position = Channel;
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
			g_eeprom.fm_is_channel_mode = !g_eeprom.fm_is_channel_mode;

			if (!FM_ConfigureChannelState())
			{
				BK1080_SetFrequency(g_eeprom.fm_frequency_playing);
				g_request_save_fm = true;
			}
			break;

		default:
			break;
	}
}

static void FM_Key_STAR(bool key_pressed, bool key_held)
{
	g_key_input_count_down = key_input_timeout_500ms;

	if (key_held && !key_pressed)
		return;

	if (!key_held && !key_pressed)
		ACTION_Scan(false);   // short key press just released .. scan without store
	else
	if (key_held && key_pressed)
		ACTION_Scan(true);   // long key press still pressed .. scan and store
}

static void FM_Key_EXIT(bool key_pressed, bool key_held)
{
	g_key_input_count_down = key_input_timeout_500ms;

	if (key_held || key_pressed)
		return;

	if (g_fm_scan_state == FM_SCAN_OFF)
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
		FM_PlayAndUpdate();

		#ifdef ENABLE_VOICE
			g_another_voice_id = VOICE_ID_SCANNING_STOP;
		#endif
	}

	g_request_display_screen = DISPLAY_FM;
}

static void FM_Key_MENU(bool key_pressed, bool key_held)
{
	unsigned int i;
	int channel = -1;

	g_key_input_count_down = key_input_timeout_500ms;

	if (key_held || key_pressed)
		return;   // key still pressed

	// see if the frequency is already stored in a channel
	for (i = 0; i < ARRAY_SIZE(g_fm_channels) && channel < 0; i++)
		if (g_fm_channels[i] == g_eeprom.fm_frequency_playing)
			channel = i;  // found it in the channel list

	g_request_display_screen = DISPLAY_FM;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		//UART_SendText("fm menu 1\r\n");
	#endif

	if (g_fm_scan_state == FM_SCAN_OFF)
	{	// not scanning

		if (!g_eeprom.fm_is_channel_mode)
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

				FM_ConfigureChannelState();
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

static void FM_Key_UP_DOWN(bool key_pressed, bool key_held, int8_t Step)
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
		g_fm_channel_position    = NUMBER_AddWithWraparound(g_fm_channel_position, Step, 0, 19);
		return;
	}

	if (g_fm_scan_state != FM_SCAN_OFF)
	{
		if (g_fm_auto_scan)
			return;

		FM_Tune(g_eeprom.fm_frequency_playing, Step, false);
		g_request_display_screen = DISPLAY_FM;
		return;
	}

	if (g_eeprom.fm_is_channel_mode)
	{	// we're in channel mode
		const uint8_t Channel = FM_FindNextChannel(g_eeprom.fm_selected_channel + Step, Step);
		if (Channel == 0xFF || g_eeprom.fm_selected_channel == Channel)
			goto Bail;

		g_eeprom.fm_selected_channel  = Channel;
		g_eeprom.fm_frequency_playing = g_fm_channels[Channel];
	}
	else
	{	// no, frequency mode
		uint16_t Frequency = g_eeprom.fm_selected_frequency + Step;
		if (Frequency < FM_RADIO_BAND.lower)
			Frequency = FM_RADIO_BAND.upper;
		else
		if (Frequency > FM_RADIO_BAND.upper)
			Frequency = FM_RADIO_BAND.lower;

		g_eeprom.fm_frequency_playing  = Frequency;
		g_eeprom.fm_selected_frequency = g_eeprom.fm_frequency_playing;
	}

	g_request_save_fm = true;

Bail:
	BK1080_SetFrequency(g_eeprom.fm_frequency_playing);

	g_request_display_screen = DISPLAY_FM;
}

void FM_ProcessKeys(key_code_t Key, bool key_pressed, bool key_held)
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
			FM_Key_UP_DOWN(key_pressed, key_held, 1);
			break;
		case KEY_DOWN:
			FM_Key_UP_DOWN(key_pressed, key_held, -1);
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

void FM_Play(void)
{
	if (!FM_CheckFrequencyLock(g_eeprom.fm_frequency_playing, FM_RADIO_BAND.lower))
	{
		if (!g_fm_auto_scan)
		{
			g_fm_play_count_down_10ms = 0;
			g_fm_found_frequency      = true;

			if (!g_eeprom.fm_is_channel_mode)
				g_eeprom.fm_selected_frequency = g_eeprom.fm_frequency_playing;

			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
			g_enable_speaker = true;

			GUI_SelectNextDisplay(DISPLAY_FM);
			return;
		}

		if (g_fm_channel_position < ARRAY_SIZE(g_fm_channels))
			g_fm_channels[g_fm_channel_position++] = g_eeprom.fm_frequency_playing;

		if (g_fm_channel_position >= ARRAY_SIZE(g_fm_channels))
		{
			FM_PlayAndUpdate();
			GUI_SelectNextDisplay(DISPLAY_FM);
			return;
		}
	}

	if (g_fm_auto_scan && g_eeprom.fm_frequency_playing >= FM_RADIO_BAND.upper)
		FM_PlayAndUpdate();
	else
		FM_Tune(g_eeprom.fm_frequency_playing, g_fm_scan_state, false);

	GUI_SelectNextDisplay(DISPLAY_FM);
}

void FM_Start(void)
{
	g_fm_radio_mode              = true;
	g_fm_scan_state              = FM_SCAN_OFF;
	g_fm_restore_count_down_10ms = 0;

	BK1080_Init(g_eeprom.fm_frequency_playing, true);

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

	g_enable_speaker = true;
	g_update_status  = true;
}
