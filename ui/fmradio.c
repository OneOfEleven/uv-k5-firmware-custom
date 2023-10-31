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

		if (g_fm_scan_state_dir == FM_SCAN_STATE_DIR_OFF)
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
		if (FM_check_valid_channel(chan))
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

	if (!g_ask_to_delete &&
	    !g_ask_to_save &&
	    (g_fm_scan_state_dir != FM_SCAN_STATE_DIR_OFF || g_fm_resume_tick_500ms > 0))
	{
		const uint16_t rssi_status = BK1080_ReadRegister(BK1080_REG_10);
		const uint16_t dev_snr     = BK1080_ReadRegister(BK1080_REG_07);

		const int16_t freq_offset  = (int16_t)dev_snr / 16;
		const uint8_t snr          = dev_snr & 0x000f;

//		const uint8_t stc          = (rssi_status >> 14) & 1u;
//		const uint8_t sf_bl        = (rssi_status >> 13) & 1u;
		const uint8_t afc_railed   = (rssi_status >> 12) & 1u;
		const uint8_t ste          = (rssi_status >> 9) & 1u;
		const uint8_t st           = (rssi_status >> 8) & 1u;
		const uint8_t rssi         =  rssi_status & 0x00ff;

		sprintf(str, "%s %s %c %2udBuV %2u",
			ste        ? "STE" : "ste",
			st         ? "ST"  : "st",
			afc_railed ? 'R'   : 'r',
			rssi,
			snr);
		UI_PrintStringSmall(str, 0, 0, 6);

		sprintf(str, "%c%d", (freq_offset > 0) ? '+' : (freq_offset < 0) ? '-' : ' ', abs(freq_offset));
		UI_PrintStringSmall(str, 0, 0, 5);
	}

	// *************************************

	ST7565_BlitFullScreen();
}
