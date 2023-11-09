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

#include "app/aircopy.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/aircopy.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

void UI_DisplayAircopy(void)
{
	const uint8_t errors = g_aircopy_rx_errors_fsk_crc + g_aircopy_rx_errors_magic + g_aircopy_rx_errors_crc;
	char str[17];

	if (g_current_display_screen != DISPLAY_AIRCOPY)
		return;

	// clear screen/display buffer
	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	// **********************************
	// upper text line

	strcpy(str, "AIR COPY");
	switch (g_aircopy_state)
	{
		case AIRCOPY_READY:       strcat(str, " READY"); break;
		case AIRCOPY_RX:          strcat(str, " RX");    break;
		case AIRCOPY_TX:          strcat(str, " TX");    break;
		case AIRCOPY_RX_COMPLETE: strcat(str, " DONE"); break;
		case AIRCOPY_TX_COMPLETE: strcat(str, " DONE"); break;
		default:                  strcat(str, " ERR");   break;
	}
	UI_PrintString(str, 0, LCD_WIDTH, 0, 8);

	// **********************************
	// center frequency text line

	if (g_input_box_index == 0)
	{	// show frequency

		const unsigned int x = 16;
		
		NUMBER_ToDigits(g_rx_vfo->freq_config_rx.frequency, str);
		UI_DisplayFrequencyBig(str, x, 2, 0, 0, 6);
		
		// show the remaining 2 small frequency digits
		#ifdef ENABLE_TRIM_TRAILING_ZEROS
		{
			unsigned int small_num = 2;
			if (str[7] == 0)
			{
				small_num--;
				if (str[6] == 0)
					small_num--;
			}
			UI_Displaysmall_digits(small_num, str + 6, x + 81, 3, true);
		}
		#else
			UI_Displaysmall_digits(2, str + 6, x + 81, 3, true);
		#endif
	}
	else
	{	// user is entering a new frequency
		UI_DisplayFrequencyBig(g_input_box, 16, 2, 1, 0, 6);
	}

	// **********************************
	// lower TX/RX status text line

	switch (g_aircopy_state)
	{
		case AIRCOPY_READY:
			UI_PrintString("EXIT rx    M tx", 0, LCD_WIDTH, 5, 7);
			break;

		case AIRCOPY_RX:
			sprintf(str, "RX %u.%u", g_aircopy_block_number, g_aircopy_block_max);
			if (errors > 0)
			{
				#if 1
					sprintf(str + strlen(str), " E %u", errors);
				#else
					sprintf(str + strlen(str), " E %u %u %u",
						g_aircopy_rx_errors_fsk_crc,
						g_aircopy_rx_errors_magic,
						g_aircopy_rx_errors_crc);
				#endif
			}
			UI_PrintString(str, 0, LCD_WIDTH, 5, 7);
			break;

		case AIRCOPY_TX:
			strcpy(str, (g_fsk_tx_timeout_10ms > 0) ? "*" : " ");
			sprintf(str + 1, " TX %u.%u", g_aircopy_block_number, g_aircopy_block_max);
			UI_PrintString(str, 0, LCD_WIDTH, 5, 7);
			break;

		case AIRCOPY_RX_COMPLETE:
			UI_PrintString("RX COMPLETE", 0, LCD_WIDTH, 5, 8);
			break;

		case AIRCOPY_TX_COMPLETE:
			UI_PrintString("TX COMPLETE", 0, LCD_WIDTH, 5, 8);
			break;

		default:
			strcpy(str, "ERROR");
			UI_PrintString(str, 0, LCD_WIDTH, 5, 7);
			break;
	}

	// **********************************

	ST7565_BlitFullScreen();
}
