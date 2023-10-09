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
#include "dcs.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "ui/helper.h"
#include "ui/scanner.h"

void UI_DisplayScanner(void)
{
	char    String[17];
	bool    text_centered = false;

	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	memset(String, 0, sizeof(String));
	if (g_scan_single_frequency || (gScanCssState != SCAN_CSS_STATE_OFF && gScanCssState != SCAN_CSS_STATE_FAILED))
	{
		const uint32_t freq = gScanFrequency;
		sprintf(String, "FREQ %u.%05u", freq / 100000, freq % 100000);
	}
	else
	{
		strcpy(String, "FREQ scanning");
	}
	UI_PrintString(String, 2, 0, 1, 8);

	memset(String, 0, sizeof(String));
	if (gScanCssState < SCAN_CSS_STATE_FOUND || !gScanUseCssResult)
		strcpy(String, "CODE scanning");
	else
	if (gScanCssResultType == CODE_TYPE_CONTINUOUS_TONE)
		sprintf(String, " CTC %u.%uHz", CTCSS_OPTIONS[gScanCssResultCode] / 10, CTCSS_OPTIONS[gScanCssResultCode] % 10);
	else
		sprintf(String, " DCS D%03oN", DCS_OPTIONS[gScanCssResultCode]);
	UI_PrintString(String, 2, 0, 3, 8);

	memset(String, 0, sizeof(String));
	switch (gScannerEditState)
	{
		default:
		case SCAN_EDIT_STATE_NONE:
			if (gScanCssState < SCAN_CSS_STATE_FOUND)
			{	// rolling indicator
				memset(String, 0, sizeof(String));
				memset(String, '.', 15);
				String[gScanProgressIndicator % 15] = '#';
			}
			else
			if (gScanCssState == SCAN_CSS_STATE_FOUND)
			{
				strcpy(String, "* repeat  M save");
				text_centered = true;
			}
			else
			{
				strcpy(String, "SCAN FAIL");
			}
			break;

		case SCAN_EDIT_STATE_BUSY:
			strcpy(String, "SAVE ");
			UI_GenerateChannelStringEx(String + 5, g_show_chan_prefix, gScanChannel);
			break;

		case SCAN_EDIT_STATE_DONE:
			text_centered = true;
			strcpy(String, "SAVE ?");
			break;
	}
	UI_PrintString(String, text_centered ? 0 : 2, text_centered ? 127 : 0, 5, 8);
	
	ST7565_BlitFullScreen();
}
