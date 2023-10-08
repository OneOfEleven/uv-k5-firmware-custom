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

#ifdef ENABLE_FMRADIO

#include <string.h>

#include "app/fm.h"
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

	memset(String, 0, sizeof(String));
	strcpy(String, "FM");
	UI_PrintString(String, 0, 127, 0, 12);

	memset(String, 0, sizeof(String));
	if (g_ask_to_save)
	{
		strcpy(String, "SAVE?");
	}
	else
	if (g_ask_to_delete)
	{
		strcpy(String, "DEL?");
	}
	else
	{
		if (g_fm_scan_state == FM_SCAN_OFF)
		{
			if (!g_eeprom.fm_is_channel_mode)
			{
				for (i = 0; i < 20; i++)
				{
					if (g_eeprom.fm_frequency_playing == g_fm_channels[i])
					{
						sprintf(String, "VFO(CH%02u)", i + 1);
						break;
					}
				}

				if (i == 20)
					strcpy(String, "VFO");
			}
			else
				sprintf(String, "MR(CH%02u)", g_eeprom.fm_selected_channel + 1);
		}
		else
		{
			if (!g_fm_auto_scan)
				strcpy(String, "M-SCAN");
			else
				sprintf(String, "A-SCAN(%u)", g_fm_channel_position + 1);
		}
	}
	UI_PrintString(String, 0, 127, 2, 10);

	memset(String, 0, sizeof(String));
	if (g_ask_to_save || (g_eeprom.fm_is_channel_mode && g_input_box_index > 0))
	{
		UI_GenerateChannelString(String, g_fm_channel_position);
	}
	else
	if (!g_ask_to_delete)
	{
		if (g_input_box_index == 0)
		{
			NUMBER_ToDigits(g_eeprom.fm_frequency_playing * 10000, String);
			UI_DisplayFrequency(String, 23, 4, false, true);
		}
		else
			UI_DisplayFrequency(g_input_box, 23, 4, true, false);

		ST7565_BlitFullScreen();
		return;
	}
	else
	{
		sprintf(String, "CH-%02u", g_eeprom.fm_selected_channel + 1);
	}
	UI_PrintString(String, 0, 127, 4, 10);

	ST7565_BlitFullScreen();
}

#endif
