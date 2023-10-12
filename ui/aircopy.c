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

	strcpy(String, "AIR COPY");
	switch (g_aircopy_state)
	{
		case AIRCOPY_READY:    strcat(String, " READY"); break;
		case AIRCOPY_RX:       strcat(String, " RX");    break;
		case AIRCOPY_TX:       strcat(String, " TX");    break;
		case AIRCOPY_COMPLETE: strcat(String, " DONE");  break;
		default:               strcat(String, " ???");   break;
	}
	UI_PrintString(String, 2, 127, 0, 8);

	if (g_input_box_index == 0)
	{
		NUMBER_ToDigits(g_rx_vfo->freq_config_rx.frequency, String);
		UI_DisplayFrequency(String, 16, 2, 0, 0);
		UI_Displaysmall_digits(2, String + 6, 97, 3, true);
	}
	else
		UI_DisplayFrequency(g_input_box, 16, 2, 1, 0);

	memset(String, 0, sizeof(String));
	if (g_aircopy_state == AIRCOPY_RX)
		sprintf(String, "RCV %u  E %u", g_air_copy_block_number, g_errors_during_air_copy);
	else
	if (g_aircopy_state == AIRCOPY_TX)
		sprintf(String, "SND %u", g_air_copy_block_number);
	UI_PrintString(String, 2, 127, 4, 8);

	switch (g_aircopy_state)
	{
		case AIRCOPY_READY:    strcpy(String, "EXIT rx    M tx"); break;
		case AIRCOPY_RX:       strcpy(String, "receive mode");    break;
		case AIRCOPY_TX:       strcpy(String, "transmit mode");   break;
		case AIRCOPY_COMPLETE: strcpy(String, "finished");        break;
		default:               strcpy(String, "???");             break;
	}
	UI_PrintStringSmall(String, 0, 127, 6);

	ST7565_BlitFullScreen();
}
