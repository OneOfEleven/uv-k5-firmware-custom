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

#include "backlight.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/gpio.h"
#include "settings.h"

// this is decremented once every 500ms
uint8_t gBacklightCountdown = 0;

void BACKLIGHT_TurnOn(void)
{
	if (gEeprom.BACKLIGHT == 0)
		return;

	// turn the backlight ON
	GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);
		
	switch (gEeprom.BACKLIGHT)
	{
		default:
		case 1:	// 5 sec
			gBacklightCountdown = 2 * 5;
			break;
		case 2:	// 10 sec
			gBacklightCountdown = 2 * 10;
			break;
		case 3:	// 20 sec
			gBacklightCountdown = 2 * 20;
			break;
		case 4:	// 40 sec
			gBacklightCountdown = 2 * 40;
			break;
		case 5:	// 80 sec
			gBacklightCountdown = 2 * 80;
			break;
		case 6:	// always on
			gBacklightCountdown = 0;
			break;
	}
}
