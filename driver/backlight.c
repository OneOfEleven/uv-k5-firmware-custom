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
uint16_t g_backlight_tick_500ms = 0;

uint16_t backlight_ticks(void)
{
	uint16_t ticks = 0;
	switch (g_eeprom.config.setting.backlight_time)
	{
		case 1:	ticks =  5;     break;  // 5 sec
		case 2:	ticks = 10;     break;  // 10 sec
		case 3: ticks = 20;     break;  // 20 sec
		case 4: ticks = 60;     break;  // 1 min
		case 5: ticks = 60 * 2; break;  // 2 min
		case 6: ticks = 60 * 4; break;  // 4 min
	}
	return ticks * 2;
}

void backlight_turn_on(const uint16_t min_ticks)
{
	if (min_ticks > 0)
	{
		if (g_backlight_tick_500ms < min_ticks)
			g_backlight_tick_500ms = min_ticks;
		GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);
	}
	else
	if (g_eeprom.config.setting.backlight_time > 0)
	{
		GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);
		g_backlight_tick_500ms = backlight_ticks();
	}
}
