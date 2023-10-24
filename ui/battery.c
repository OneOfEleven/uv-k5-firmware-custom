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

#include <stddef.h>
#include <string.h>

#include "bitmaps.h"
#include "driver/st7565.h"
#include "functions.h"
#include "ui/battery.h"

void UI_DrawBattery(uint8_t *bitmap, const unsigned int level, const unsigned int blink)
{
	if (blink == 1)
	{
		memset(bitmap, 0, sizeof(BITMAP_BATTERY_LEVEL));
	}
	else 
	{
		memcpy(bitmap, BITMAP_BATTERY_LEVEL, sizeof(BITMAP_BATTERY_LEVEL));
		if (level > 1)
		{
			unsigned int i;
			unsigned int bars = level - 1;
			if (bars > 4)
				bars = 4;
			for (i = 0; i < bars; i++)
			{
				#ifdef ENABLE_REVERSE_BAT_SYMBOL
					bitmap[3 + (i * 3) + 0] = __extension__ 0b01011101;
					bitmap[3 + (i * 3) + 1] = __extension__ 0b01011101;
				#else
					bitmap[sizeof(BITMAP_BATTERY_LEVEL) - 3 - (i * 3) - 0] = __extension__ 0b01011101;
					bitmap[sizeof(BITMAP_BATTERY_LEVEL) - 3 - (i * 3) - 1] = __extension__ 0b01011101;
				#endif
			}
		}
	}
}

void UI_DisplayBattery(const unsigned int level, const unsigned int blink)
{
	uint8_t bitmap[sizeof(BITMAP_BATTERY_LEVEL)];
	UI_DrawBattery(bitmap, level, blink);
	ST7565_DrawLine(LCD_WIDTH - sizeof(bitmap), 0, sizeof(bitmap), bitmap);
}
