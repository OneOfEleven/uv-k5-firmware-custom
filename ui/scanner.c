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

#include <stdbool.h>
#include <string.h>

#include "app/scanner.h"
#include "board.h"
#include "dcs.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "ui/helper.h"
#include "ui/scanner.h"
#include "ui/ui.h"

void UI_DisplayScanner(void)
{
	char String[17];
	bool text_centered = false;

	if (g_screen_to_display != DISPLAY_SCANNER)
		return;
	
	// clear display buffer
	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	// ***********************************
	// frequency text line

	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

	switch (g_scan_css_state)
	{
		default:
		case SCAN_CSS_STATE_OFF:
			if (!g_scan_single_frequency)
			{
				strcpy(String, "FREQ scanning");
				break;
			}
			
		case SCAN_CSS_STATE_SCANNING:
		case SCAN_CSS_STATE_FOUND:
		case SCAN_CSS_STATE_FAILED:
			{
				const uint32_t freq = g_scan_frequency;
				sprintf(String, "FREQ %u.%05u", freq / 100000, freq % 100000);
			}
			break;

		case SCAN_CSS_STATE_FREQ_FAILED:
			strcpy(String, "FREQ not found");
			break;
	}

	#pragma GCC diagnostic pop

	UI_PrintString(String, 2, 0, 1, 8);

	// ***********************************
	// CODE text line
	
	memset(String, 0, sizeof(String));

	switch (g_scan_css_state)
	{
		default:
		case SCAN_CSS_STATE_OFF:
		case SCAN_CSS_STATE_FREQ_FAILED:
			strcpy(String, "CODE");
			break;

		case SCAN_CSS_STATE_SCANNING:
			strcpy(String, "CODE scanning");
			break;

		case SCAN_CSS_STATE_FOUND:

			switch (g_scan_css_result_type)
			{
				default:
				case CODE_TYPE_OFF:
					strcpy(String, "CODE none");
					break;
				case CODE_TYPE_CONTINUOUS_TONE:
					sprintf(String, "CTCSS %u.%uHz", CTCSS_OPTIONS[g_scan_css_result_code] / 10, CTCSS_OPTIONS[g_scan_css_result_code] % 10);
					break;
				case CODE_TYPE_DIGITAL:
				case CODE_TYPE_REVERSE_DIGITAL:
					sprintf(String, "CDCSS D%03oN", DCS_OPTIONS[g_scan_css_result_code]);
					break;
			}			
			break;

		case SCAN_CSS_STATE_FAILED:
			strcpy(String, "CODE none");
			break;
	}

	UI_PrintString(String, 2, 0, 3, 8);

	// ***********************************
	// bottom text line

	memset(String, 0, sizeof(String));

	switch (g_scanner_edit_state)
	{
		default:
		case SCAN_EDIT_STATE_NONE:

			#pragma GCC diagnostic push
			#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

			switch (g_scan_css_state)
			{
				default:
				case SCAN_CSS_STATE_OFF:
				case SCAN_CSS_STATE_SCANNING:	// rolling indicator
					memset(String, 0, sizeof(String));
					memset(String, '.', 15);
					String[(g_scan_freq_css_timer_10ms / 32) % 15] = '#';
					break;

				case SCAN_CSS_STATE_FOUND:
					strcpy(String, "* repeat  M save");
					text_centered = true;
					break;

				case SCAN_CSS_STATE_FAILED:
					if (!g_scan_single_frequency)
					{
						strcpy(String, "* repeat  M save");
						text_centered = true;
						break;
					}
					
				case SCAN_CSS_STATE_FREQ_FAILED:
					strcpy(String, "* repeat");
					text_centered = true;
					break;
			}

			#pragma GCC diagnostic pop

			break;

		case SCAN_EDIT_STATE_SAVE:
			strcpy(String, "SAVE ");
			{
				char s[11];
				BOARD_fetchChannelName(s, g_scan_channel);
				if (s[0] == 0)
					UI_GenerateChannelStringEx(s, g_show_chan_prefix ? "CH-" : "", g_scan_channel);
				strcat(String, s);
			}
			break;

		case SCAN_EDIT_STATE_DONE:
//			strcpy(String, "* repeat  M save");
			strcpy(String, "* repeat");
			text_centered = true;
			break;
	}

	UI_PrintString(String, text_centered ? 0 : 2, text_centered ? 127 : 0, 5, 8);

	// ***********************************

	ST7565_BlitFullScreen();
}
