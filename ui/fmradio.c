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
	char         String[16];

	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	#ifdef ENABLE_KEYLOCK
	if (g_eeprom.key_lock && g_keypad_locked > 0)
	{	// tell user how to unlock the keyboard
		backlight_turn_on(0);
		UI_PrintString("Long press #", 0, LCD_WIDTH, 1, 8);
		UI_PrintString("to unlock",    0, LCD_WIDTH, 3, 8);
		ST7565_BlitFullScreen();
		return;
	}
	#endif

	// *************************************
	// upper text line
	
	UI_PrintString("FM", 0, 127, 0, 12);

	// *************************************
	// middle text line
	
	if (g_ask_to_save)
	{
		const unsigned int freq = g_eeprom.fm_frequency_playing;
		sprintf(String, "SAVE %u.%u ?", freq / 10, freq % 10);
	}
	else
	if (g_ask_to_delete)
	{
		strcpy(String, "DELETE ?");
	}
	else
	{
		memset(String, 0, sizeof(String));

		if (g_fm_scan_state == FM_SCAN_OFF)
		{
			if (!g_eeprom.fm_channel_mode)
			{
				for (i = 0; i < ARRAY_SIZE(g_fm_channels); i++)
				{
					if (g_eeprom.fm_frequency_playing == g_fm_channels[i])
					{
						sprintf(String, "VFO (CH %u)", 1 + i);
						break;
					}
				}

				if (i >= ARRAY_SIZE(g_fm_channels))
					strcpy(String, "VFO");
			}
			else
				sprintf(String, "CH %u", 1 + g_eeprom.fm_selected_channel);
		}
		else
		if (!g_fm_auto_scan)
			strcpy(String, "FREQ SCAN");
		else
			sprintf(String, "A-SCAN %2u", 1 + g_fm_channel_position);
	}

	UI_PrintString(String, 0, 127, 2, 10);

	// *************************************
	// lower text line
	
	memset(String, 0, sizeof(String));

	if (g_ask_to_save)
	{	// channel mode
		const unsigned int chan = g_fm_channel_position;
		const uint32_t     freq = g_fm_channels[chan];
		UI_GenerateChannelString(String, chan, ' ');
		if (FM_CheckValidChannel(chan))
			sprintf(String + strlen(String), " (%u.%u)", freq / 10, freq % 10);
	}
	else
	if (g_eeprom.fm_channel_mode && g_input_box_index > 0)
	{	// user is entering a channel number
		UI_GenerateChannelString(String, g_fm_channel_position, ' ');
	}
	else
	if (!g_ask_to_delete)
	{
		if (g_input_box_index == 0)
		{	// frequency mode
			const uint32_t freq = g_eeprom.fm_frequency_playing;
			NUMBER_ToDigits(freq * 10000, String);
			UI_DisplayFrequency(String, 23, 4, false, true);
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
		sprintf(String, "CH %u (%u.%u)", 1 + chan, freq / 10, freq % 10);
	}

	UI_PrintString(String, 0, 127, 4, (strlen(String) >= 8) ? 8 : 10);
	
	// *************************************

	ST7565_BlitFullScreen();
}
