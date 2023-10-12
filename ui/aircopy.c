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

#include "app/aircopy.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "misc.h"
#include "radio.h"
#include "ui/aircopy.h"
#include "ui/helper.h"
#include "ui/inputbox.h"

void UI_DisplayAircopy(void)
{
	char String[16];

	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	// **********************************

	strcpy(String, "AIR COPY");

	switch (g_aircopy_state)
	{
		case AIRCOPY_READY:       strcat(String, " READY"); break;
		case AIRCOPY_RX:          strcat(String, " RX");    break;
		case AIRCOPY_TX:          strcat(String, " TX");    break;
		case AIRCOPY_RX_COMPLETE: strcat(String, " DONE");  break;
		case AIRCOPY_TX_COMPLETE: strcat(String, " DONE");  break;
		default:                  strcat(String, " ???");   break;
	}
	UI_PrintString(String, 0, LCD_WIDTH - 1, 0, 8);

	// **********************************

	if (g_input_box_index == 0)
	{
		NUMBER_ToDigits(g_rx_vfo->freq_config_rx.frequency, String);
		UI_DisplayFrequency(String, 16, 2, 0, 0);
		UI_Displaysmall_digits(2, String + 6, 97, 3, true);
	}
	else
		UI_DisplayFrequency(g_input_box, 16, 2, 1, 0);

	// **********************************
	
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

	switch (g_aircopy_state)
	{
		case AIRCOPY_READY:
			UI_PrintString("EXIT rx    M tx", 0, LCD_WIDTH - 1, 5, 7);
			break;

		case AIRCOPY_RX_COMPLETE:
			if (g_errors_during_air_copy == 0)
			{
				UI_PrintString("RX COMPLETE", 0, LCD_WIDTH - 1, 5, 8);
				break;
			}

		case AIRCOPY_RX:
			sprintf(String, "RX %u.%u", g_air_copy_block_number, g_air_copy_block_max);
			if (g_errors_during_air_copy > 0)
				sprintf(String + strlen(String), " E %u", g_errors_during_air_copy);
			UI_PrintString(String, 0, LCD_WIDTH - 1, 5, 7);
			break;

		case AIRCOPY_TX_COMPLETE:
			UI_PrintString("TX COMPLETE", 0, LCD_WIDTH - 1, 5, 8);
			break;

		case AIRCOPY_TX:
			sprintf(String, "TX %u.%u", g_air_copy_block_number, g_air_copy_block_max);
			UI_PrintString(String, 0, LCD_WIDTH - 1, 5, 7);
			break;

		default:
			strcpy(String, " ???");
			UI_PrintString(String, 0, LCD_WIDTH - 1, 5, 7);
			break;
	}

	#pragma GCC diagnostic pop

	// **********************************

	ST7565_BlitFullScreen();
}
