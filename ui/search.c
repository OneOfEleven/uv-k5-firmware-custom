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

#include "app/search.h"
#include "board.h"
#include "dcs.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "ui/helper.h"
#include "ui/search.h"
#include "ui/ui.h"

void UI_DisplaySearch(void)
{
	char String[17];
	bool text_centered = false;

	if (g_current_display_screen != DISPLAY_SEARCH)
		return;
	
	// clear display buffer
	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	// ***********************************
	// frequency text line

	switch (g_search_css_state)
	{
		default:
		case SEARCH_CSS_STATE_OFF:
			if (!g_search_single_frequency)
			{
				strcpy(String, "FREQ scanning");
				break;
			}
			
			// Fallthrough

		case SEARCH_CSS_STATE_SCANNING:
		case SEARCH_CSS_STATE_FOUND:
		case SEARCH_CSS_STATE_FAILED:
		case SEARCH_CSS_STATE_REPEAT:
			{
				const uint32_t freq = g_search_frequency;
				sprintf(String, "FREQ %u.%05u", freq / 100000, freq % 100000);
			}
			break;

		case SEARCH_CSS_STATE_FREQ_FAILED:
			strcpy(String, "FREQ none found");
			break;
	}

	UI_PrintString(String, 2, 0, 1, 8);

	// ***********************************
	// CODE text line
	
	memset(String, 0, sizeof(String));

	switch (g_search_css_state)
	{
		default:
		case SEARCH_CSS_STATE_OFF:
			strcpy(String, "CODE");
			break;

		case SEARCH_CSS_STATE_SCANNING:
			strcpy(String, "CODE scanning");
			break;

		case SEARCH_CSS_STATE_FOUND:
		case SEARCH_CSS_STATE_REPEAT:
			strcpy(String, "CODE none found");
			if (g_search_use_css_result)
			{
				switch (g_search_css_result_type)
				{
					default:
					case CODE_TYPE_NONE:
						break;
					case CODE_TYPE_CONTINUOUS_TONE:
						sprintf(String, "CTCSS %u.%uHz", CTCSS_OPTIONS[g_search_css_result_code] / 10, CTCSS_OPTIONS[g_search_css_result_code] % 10);
						break;
					case CODE_TYPE_DIGITAL:
					case CODE_TYPE_REVERSE_DIGITAL:
						sprintf(String, "CDCSS D%03oN", DCS_OPTIONS[g_search_css_result_code]);
						break;
				}
			}				
			break;

		case SEARCH_CSS_STATE_FAILED:
			strcpy(String, "CODE none found");
			break;
	}

	UI_PrintString(String, 2, 0, 3, 8);

	// ***********************************
	// bottom text line

	memset(String, 0, sizeof(String));

	switch (g_search_edit_state)
	{
		default:
		case SEARCH_EDIT_STATE_NONE:

			switch (g_search_css_state)
			{
				default:
				case SEARCH_CSS_STATE_OFF:
				case SEARCH_CSS_STATE_SCANNING:	// rolling indicator
					memset(String, 0, sizeof(String));
					memset(String, '.', 15);
					String[(g_search_freq_css_tick_10ms / 32) % 15] = '#';
					break;

				case SEARCH_CSS_STATE_FOUND:
					strcpy(String, "* repeat M save");
					text_centered = true;
					break;

				case SEARCH_CSS_STATE_FAILED:
					if (!g_search_single_frequency)
					{
						strcpy(String, "* repeat M save");
						text_centered = true;
						break;
					}
					
				// Fallthrough

				case SEARCH_CSS_STATE_FREQ_FAILED:
				case SEARCH_CSS_STATE_REPEAT:
					strcpy(String, "* repeat");
					text_centered = true;
					break;
			}

			break;

		case SEARCH_EDIT_STATE_SAVE_CHAN:
			strcpy(String, "SAVE ");
			{
				char s[11];
				BOARD_fetchChannelName(s, g_search_channel);
				if (s[0] == 0)
					UI_GenerateChannelStringEx(s, g_search_show_chan_prefix ? "CH-" : "", g_search_channel);
				strcat(String, s);
			}
			break;

		case SEARCH_EDIT_STATE_SAVE_CONFIRM:
			strcpy(String, "* repeat Save ?");
			text_centered = true;
			break;
	}

	UI_PrintString(String, text_centered ? 0 : 2, text_centered ? 127 : 0, 5, 8);

	// ***********************************

	ST7565_BlitFullScreen();
}
