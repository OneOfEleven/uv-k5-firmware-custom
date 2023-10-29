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

#include "app/fm.h"
#include "driver/backlight.h"
#include "driver/bk1080.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "misc.h"
#include "settings.h"
#include "ui/fmradio.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

void UI_DisplayFM(void)
{
	unsigned int i;
	char         str[22];

	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	#ifdef ENABLE_KEYLOCK
	if (g_eeprom.key_lock && g_keypad_locked > 0)
	{	// tell user how to unlock the keyboard
		backlight_turn_on(0);
		UI_PrintString("Long press #", 0, LCD_WIDTH - 1, 1, 8);
		UI_PrintString("to unlock",    0, LCD_WIDTH - 1, 3, 8);
		ST7565_BlitFullScreen();
		return;
	}
	#endif

	// *************************************
	// upper text line
	
	UI_PrintString("FM", 0, LCD_WIDTH - 1, 0, 12);

	// *************************************
	// middle text line
	
	if (g_ask_to_save)
	{
		const unsigned int freq = g_eeprom.fm_frequency_playing;
		sprintf(str, "SAVE %u.%u ?", freq / 10, freq % 10);
	}
	else
	if (g_ask_to_delete)
	{
		strcpy(str, "DELETE ?");
	}
	else
	{
		memset(str, 0, sizeof(str));

		if (g_fm_scan_state == FM_SCAN_OFF)
		{
			if (!g_eeprom.fm_channel_mode)
			{
				for (i = 0; i < ARRAY_SIZE(g_fm_channels); i++)
				{
					if (g_eeprom.fm_frequency_playing == g_fm_channels[i])
					{
						sprintf(str, "VFO (CH %u)", 1 + i);
						break;
					}
				}

				if (i >= ARRAY_SIZE(g_fm_channels))
					strcpy(str, "VFO");
			}
			else
				sprintf(str, "CH %u", 1 + g_eeprom.fm_selected_channel);
		}
		else
		if (!g_fm_auto_scan)
			strcpy(str, "FREQ SCAN");
		else
			sprintf(str, "A-SCAN %2u", 1 + g_fm_channel_position);
	}

	UI_PrintString(str, 0, LCD_WIDTH - 1, 2, 10);

	// *************************************
	// lower text line
	
	memset(str, 0, sizeof(str));

	if (g_ask_to_save)
	{	// channel mode
		const unsigned int chan = g_fm_channel_position;
		const uint32_t     freq = g_fm_channels[chan];
		UI_GenerateChannelString(str, chan, ' ');
		if (FM_CheckValidChannel(chan))
			sprintf(str + strlen(str), " (%u.%u)", freq / 10, freq % 10);
	}
	else
	if (g_eeprom.fm_channel_mode && g_input_box_index > 0)
	{	// user is entering a channel number
		UI_GenerateChannelString(str, g_fm_channel_position, ' ');
	}
	else
	if (!g_ask_to_delete)
	{
		if (g_input_box_index == 0)
		{	// frequency mode
			const uint32_t freq = g_eeprom.fm_frequency_playing;
			NUMBER_ToDigits(freq * 10000, str);
			#ifdef ENABLE_TRIM_TRAILING_ZEROS
				UI_DisplayFrequency(str, 30, 4, false, true);
			#else
				UI_DisplayFrequency(str, 23, 4, false, true);
			#endif
		}
		else
		{	// user is entering a frequency
			UI_DisplayFrequency(g_input_box, 23, 4, true, false);
		}
	}
	else
	{	// delete channel
		const uint32_t chan = g_eeprom.fm_selected_channel;
		const uint32_t freq = g_fm_channels[chan];
		sprintf(str, "CH %u (%u.%u)", 1 + chan, freq / 10, freq % 10);
	}

	UI_PrintString(str, 0, LCD_WIDTH - 1, 4, (strlen(str) >= 8) ? 8 : 10);
	
	// *************************************

	// can't do this during FM radio - audio clicks else
	if (g_fm_scan_state != FM_SCAN_OFF)
	{
		const uint16_t val_07 = BK1080_ReadRegister(0x07);
		const uint16_t val_0A = BK1080_ReadRegister(0x0A);
		sprintf(str, "%s %s %2udBuV %2u",
			((val_0A >> 9) & 1u) ? "STE" : "ste",
			((val_0A >> 8) & 1u) ? "ST"  : "st",
			 (val_0A >> 0) & 0x00ff,
			 (val_07 >> 0) & 0x000f);
		UI_PrintStringSmall(str, 0, LCD_WIDTH, 6);
	}

	// *************************************

	ST7565_BlitFullScreen();
}
