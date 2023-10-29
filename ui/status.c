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

#include "app/search.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "bitmaps.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "app/dtmf.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#include "ui/battery.h"
#include "ui/helper.h"
#include "ui/ui.h"
#include "ui/status.h"

void UI_DisplayStatus(const bool test_display)
{
	uint8_t     *line = g_status_line;
	unsigned int x    = 0;
	unsigned int x1   = 0;
	
	g_update_status = false;
	
	memset(g_status_line, 0, sizeof(g_status_line));

	// **************

	// POWER-SAVE indicator
	if (g_current_function == FUNCTION_TRANSMIT)
	{
		memcpy(line + x, BITMAP_TX, sizeof(BITMAP_TX));
		x1 = x + sizeof(BITMAP_TX);
	}
	else
	if (g_current_function == FUNCTION_RECEIVE ||
	    g_current_function == FUNCTION_NEW_RECEIVE ||
		g_monitor_enabled)
	{
		memcpy(line + x, BITMAP_RX, sizeof(BITMAP_RX));
		x1 = x + sizeof(BITMAP_RX);
	}
	else
	if (g_current_function == FUNCTION_POWER_SAVE || test_display)
	{
		memcpy(line + x, BITMAP_POWERSAVE, sizeof(BITMAP_POWERSAVE));
		x1 = x + sizeof(BITMAP_POWERSAVE);
	}
	x += sizeof(BITMAP_POWERSAVE);

	#ifdef ENABLE_NOAA
		// NOASS SCAN indicator
		if (g_is_noaa_mode || test_display)
		{
			memcpy(line + x, BITMAP_NOAA, sizeof(BITMAP_NOAA));
			x1 = x + sizeof(BITMAP_NOAA);
		}
		x += sizeof(BITMAP_NOAA);
	#else
		// hmmm, what to put in it's place
	#endif
	
	#ifdef ENABLE_KILL_REVIVE
		if (g_setting_radio_disabled)
		{
			memset(line + x, 0xFF, 10);
			x1 = x + 10;
		}
		else
	#endif
	{
	#ifdef ENABLE_FMRADIO
		// FM indicator
		if (g_fm_radio_mode || test_display)
		{
			memcpy(line + x, BITMAP_FM, sizeof(BITMAP_FM));
			x1 = x + sizeof(BITMAP_FM);
		}
		else
	#endif
		// SCAN indicator
		if (g_scan_state_dir != SCAN_STATE_DIR_OFF || test_display)
		{
			// don't display this if in search mode
			if (g_current_display_screen != DISPLAY_SEARCH)
			{
				if (g_scan_next_channel <= USER_CHANNEL_LAST)
				{	// channel mode
					if (g_eeprom.scan_list_default == 0)
						UI_PrintStringSmallBuffer("1", line + x);
					else
					if (g_eeprom.scan_list_default == 1)
						UI_PrintStringSmallBuffer("2", line + x);
					else
					if (g_eeprom.scan_list_default == 2)
						UI_PrintStringSmallBuffer("*", line + x);
				}
				else
				{	// frequency mode
					UI_PrintStringSmallBuffer("S", line + x);
				}
				x1 = x + 7;
			}
		}
	}
	x += 7;  // font character width

	#ifdef ENABLE_VOICE
		// VOICE indicator
		if (g_eeprom.voice_prompt != VOICE_PROMPT_OFF || test_display)
		{
			memcpy(line + x, BITMAP_VOICE_PROMPT, sizeof(BITMAP_VOICE_PROMPT));
			x1 = x + sizeof(BITMAP_VOICE_PROMPT);
		}
		x += sizeof(BITMAP_VOICE_PROMPT);
	#else
		// hmmm, what to put in it's place
	#endif

	// DUAL-WATCH indicator
	if (g_eeprom.dual_watch != DUAL_WATCH_OFF || test_display)
	{
		if (g_dual_watch_tick_10ms > dual_watch_delay_toggle_10ms ||
	        g_dtmf_call_state != DTMF_CALL_STATE_NONE ||
		    g_scan_state_dir != SCAN_STATE_DIR_OFF  ||
			g_css_scan_mode != CSS_SCAN_MODE_OFF    ||
			(g_current_function != FUNCTION_FOREGROUND && g_current_function != FUNCTION_POWER_SAVE) ||
			g_current_display_screen == DISPLAY_SEARCH)
		{
			memcpy(line + x, BITMAP_TDR_HOLDING, sizeof(BITMAP_TDR_HOLDING));
		}
		else
		{
			memcpy(line + x, BITMAP_TDR_RUNNING, sizeof(BITMAP_TDR_RUNNING));
		}
		x1 = x + sizeof(BITMAP_TDR_RUNNING);
	}
	x += sizeof(BITMAP_TDR_RUNNING);

	// monitor
	if (g_monitor_enabled)
	{
		memcpy(line + x, BITMAP_MONITOR, sizeof(BITMAP_MONITOR));
		x1 = x + sizeof(BITMAP_MONITOR);
	}
	x += sizeof(BITMAP_MONITOR);

	// CROSS-VFO indicator
	if (g_eeprom.cross_vfo_rx_tx != CROSS_BAND_OFF || test_display)
	{
		memcpy(line + x, BITMAP_XB, sizeof(BITMAP_XB));
		x1 = x + sizeof(BITMAP_XB);
	}
	x += sizeof(BITMAP_XB);
	
	#ifdef ENABLE_VOX
		// VOX indicator
		if (g_eeprom.vox_switch || test_display)
		{
			memcpy(line + x, BITMAP_VOX, sizeof(BITMAP_VOX));
			x1 = x + sizeof(BITMAP_VOX);
		}
		x += sizeof(BITMAP_VOX);
	#endif

	#ifdef ENABLE_KEYLOCK
	// KEY-LOCK indicator
	if (g_eeprom.key_lock || test_display)
	{
		memcpy(line + x, BITMAP_KEYLOCK, sizeof(BITMAP_KEYLOCK));
		x += sizeof(BITMAP_KEYLOCK);
		x1 = x;
	}
	else
	#endif
	if (g_fkey_pressed)
	{
		memcpy(line + x, BITMAP_F_KEY, sizeof(BITMAP_F_KEY));
		x += sizeof(BITMAP_F_KEY);
		x1 = x;
	}

	{	// battery voltage or percentage text
		char         s[8];
		unsigned int space_needed;
		
		unsigned int x2 = LCD_WIDTH - sizeof(BITMAP_BATTERY_LEVEL) - 3;

		if (g_charging_with_type_c)
			x2 -= sizeof(BITMAP_USB_C);  // the radio is on USB charge

		switch (g_setting_battery_text)
		{
			default:
			case 0:
				break;
	
			case 1:		// voltage
			{
				const uint16_t voltage = (g_battery_voltage_average <= 999) ? g_battery_voltage_average : 999; // limit to 9.99V
				sprintf(s, "%u.%02uV", voltage / 100, voltage % 100);
				space_needed = (7 * strlen(s));
				if (x2 >= (x1 + space_needed))
					UI_PrintStringSmallBuffer(s, line + x2 - space_needed);
				break;
			}
			
			case 2:		// percentage
			{
				sprintf(s, "%u%%", BATTERY_VoltsToPercent(g_battery_voltage_average));
				space_needed = (7 * strlen(s));
				if (x2 >= (x1 + space_needed))
					UI_PrintStringSmallBuffer(s, line + x2 - space_needed);
				break;
			}
		}
	}
		
	// move to right side of the screen
	x = LCD_WIDTH - sizeof(BITMAP_BATTERY_LEVEL) - sizeof(BITMAP_USB_C);
	
	// USB-C charge indicator
	if (g_charging_with_type_c || test_display)
		memcpy(line + x, BITMAP_USB_C, sizeof(BITMAP_USB_C));
	x += sizeof(BITMAP_USB_C);

	// BATTERY LEVEL indicator
	UI_DrawBattery(line + x, g_battery_display_level, g_low_battery_blink);
	
	// **************

	ST7565_BlitStatusLine();
}
