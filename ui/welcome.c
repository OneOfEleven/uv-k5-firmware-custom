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

#include "driver/eeprom.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "helper/battery.h"
#include "settings.h"
#include "misc.h"
#include "ui/helper.h"
#include "ui/welcome.h"
#include "ui/status.h"
#include "version.h"

void UI_DisplayReleaseKeys(void)
{
	memset(g_status_line,  0, sizeof(g_status_line));
	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	UI_PrintString("RELEASE",  0, LCD_WIDTH, 1, 10);
	UI_PrintString("ALL KEYS", 0, LCD_WIDTH, 3, 10);

	ST7565_BlitStatusLine();  // blank status line
	ST7565_BlitFullScreen();
}

void UI_DisplayWelcome(void)
{
	char str0[17];
	char str1[17];
	char str2[17];
	
	memset(g_status_line,  0, sizeof(g_status_line));
	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	if (g_eeprom.pwr_on_display_mode == PWR_ON_DISPLAY_MODE_NONE)
	{
		ST7565_FillScreen(0xFF);
	}
	else
	if (g_eeprom.pwr_on_display_mode == PWR_ON_DISPLAY_MODE_FULL_SCREEN)
	{
		ST7565_FillScreen(0xFF);
	}
	else
	{
		unsigned int slen = strlen(Version_str);
		if (slen > (sizeof(str2) - 1))
			slen =  sizeof(str2) - 1;

		memset(str0, 0, sizeof(str0));
		memset(str1, 0, sizeof(str1));
		memset(str2, 0, sizeof(str2));

		if (g_eeprom.pwr_on_display_mode == PWR_ON_DISPLAY_MODE_VOLTAGE)
		{
			strcpy(str0, "VOLTAGE");
			sprintf(str1, "%u.%02uV %u%%",
				g_battery_voltage_average / 100,
				g_battery_voltage_average % 100,
				BATTERY_VoltsToPercent(g_battery_voltage_average));
		}
		else
		{
			EEPROM_ReadBuffer(0x0EB0, str0, 16);
			EEPROM_ReadBuffer(0x0EC0, str1, 16);
		}

		memmove(str2, Version_str, slen);
		
		UI_PrintString(str0, 0, LCD_WIDTH, 0, 10);
		UI_PrintString(str1, 0, LCD_WIDTH, 2, 10);
		UI_PrintStringSmall(str2, 0, LCD_WIDTH, 4);
		UI_PrintStringSmall(__DATE__, 0, LCD_WIDTH, 5);
		UI_PrintStringSmall(__TIME__, 0, LCD_WIDTH, 6);

		#if 1
			ST7565_BlitStatusLine();  // blank status line
		#else
			UI_DisplayStatus(true);   // show all status line symbols (test)
		#endif
		
		ST7565_BlitFullScreen();
	}
}

