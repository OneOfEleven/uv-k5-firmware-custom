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

#include "bsp/dp32g030/gpio.h"
#include "driver/backlight.h"
#include "driver/gpio.h"
#include "settings.h"

// this is decremented once every 500ms
uint16_t g_backlight_count_down = 0;

void backlight_turn_on(const uint16_t min_ticks)
{
	if (min_ticks > 0)
	{
		if (g_backlight_count_down < min_ticks)
			g_backlight_count_down = min_ticks;

		// turn the backlight ON
		GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);
	}
	else
	if (g_eeprom.backlight > 0)
	{
		// turn the backlight ON
		GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);
		
		switch (g_eeprom.backlight)
		{
			default:
			case 1:	// 5 sec
				g_backlight_count_down = 5;
				break;
			case 2:	// 10 sec
				g_backlight_count_down = 10;
				break;
			case 3:	// 20 sec
				g_backlight_count_down = 20;
				break;
			case 4:	// 1 min
				g_backlight_count_down = 60;
				break;
			case 5:	// 2 min
				g_backlight_count_down = 60 * 2;
				break;
			case 6:	// 4 min
				g_backlight_count_down = 60 * 4;
				break;
			case 7:	// always on
				g_backlight_count_down = 0;
				break;
		}
	
		g_backlight_count_down *= 2;
	}
}
