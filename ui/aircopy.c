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
#include "ui/ui.h"

void UI_DisplayAircopy(void)
{
	const uint8_t errors = g_aircopy_rx_errors_fsk_crc + g_aircopy_rx_errors_magic + g_aircopy_rx_errors_crc;
	char str[17];

	if (g_screen_to_display != DISPLAY_AIRCOPY)
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
		case AIRCOPY_RX_COMPLETE: strcat(str, " RDONE");  break;
		case AIRCOPY_TX_COMPLETE: strcat(str, " TDONE");  break;
		default:                  strcat(str, " ERR");   break;
	}
	UI_PrintString(str, 0, LCD_WIDTH - 1, 0, 8);

	// **********************************
	// center frequency text line

	if (g_input_box_index == 0)
	{	// show frequency
		NUMBER_ToDigits(g_rx_vfo->freq_config_rx.frequency, str);
		UI_DisplayFrequency(str, 16, 2, 0, 0);
		UI_Displaysmall_digits(2, str + 6, 97, 3, true);
	}
	else
	{	// user is entering a new frequency
		UI_DisplayFrequency(g_input_box, 16, 2, 1, 0);
	}

	// **********************************
	// lower TX/RX status text line

	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

	switch (g_aircopy_state)
	{
		case AIRCOPY_TX_COMPLETE:
//			UI_PrintString("TX COMPLETE", 0, LCD_WIDTH - 1, 5, 8);
//			break;

		case AIRCOPY_READY:
			UI_PrintString("EXIT rx    M tx", 0, LCD_WIDTH - 1, 5, 7);
			break;

		case AIRCOPY_RX_COMPLETE:
			if (errors == 0)
			{
				UI_PrintString("RX COMPLETE", 0, LCD_WIDTH - 1, 5, 8);
				break;
			}

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
			UI_PrintString(str, 0, LCD_WIDTH - 1, 5, 7);
			break;

		case AIRCOPY_TX:
			strcpy(str, (g_fsk_tx_timeout_10ms > 0) ? "*" : " ");
			sprintf(str + 1, " TX %u.%u", g_aircopy_block_number, g_aircopy_block_max);
			UI_PrintString(str, 0, LCD_WIDTH - 1, 5, 7);
			break;

		default:
			strcpy(str, "ERROR");
			UI_PrintString(str, 0, LCD_WIDTH - 1, 5, 7);
			break;
	}

	#pragma GCC diagnostic pop

	// **********************************

	ST7565_BlitFullScreen();
}
