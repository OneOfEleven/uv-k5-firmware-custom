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
#include "bsp/dp32g030/pwmplus.h"
#include "bsp/dp32g030/portcon.h"
#include "driver/gpio.h"
#include "settings.h"

uint16_t g_backlight_tick_10ms;

void BACKLIGHT_init(void)
{
	// 48MHz / 94 / 1024 ~ 500Hz
	const uint32_t PWM_FREQUENCY_HZ =  1000;
	PWM_PLUS0_CLKSRC |= (CPU_CLOCK_HZ / 1024 / PWM_FREQUENCY_HZ) << 16;
	PWM_PLUS0_PERIOD = 1023;

	PORTCON_PORTB_SEL0 &= ~(PORTCON_PORTB_SEL0_B6_MASK);
	PORTCON_PORTB_SEL0 |= PORTCON_PORTB_SEL0_B6_BITS_PWMP0_CH0;

	PWM_PLUS0_GEN = PWMPLUS_GEN_CH0_OE_BITS_ENABLE  | PWMPLUS_GEN_CH0_OUTINV_BITS_ENABLE;
	PWM_PLUS0_CFG = PWMPLUS_CFG_CNT_REP_BITS_ENABLE | PWMPLUS_CFG_COUNTER_EN_BITS_ENABLE;
}

uint16_t BACKLIGHT_ticks(void)
{
	uint16_t ticks = 0;
	switch (g_eeprom.config.setting.backlight_time)
	{
		case 1:	ticks =   5; break;  // 5 sec
		case 2:	ticks =  10; break;  // 10 sec
		case 3: ticks =  20; break;  // 20 sec
		case 4: ticks =  60; break;  // 1 min
		case 5: ticks = 120; break;  // 2 min
		case 6: ticks = 240; break;  // 4 min
	}
	return ticks * 100;
}

bool BACKLIGHT_is_on(void)
{
	return (PWM_PLUS0_CH0_COMP > 0) ? true : false;
}

void BACKLIGHT_set_brightness(unsigned int brightness)
{
	if (brightness > BACKLIGHT_MAX_BRIGHTNESS)
		brightness = BACKLIGHT_MAX_BRIGHTNESS;

	PWM_PLUS0_CH0_COMP = (1023ul * brightness * brightness * brightness) / (BACKLIGHT_MAX_BRIGHTNESS * BACKLIGHT_MAX_BRIGHTNESS * BACKLIGHT_MAX_BRIGHTNESS);
	//PWM_PLUS0_SWLOAD = 1;
}

void BACKLIGHT_turn_on(const unsigned int min_secs)
{
	const uint16_t min_ticks = 100u * min_secs;
	if (min_ticks > 0)
	{
		if (g_backlight_tick_10ms < min_ticks)
			g_backlight_tick_10ms = min_ticks;
		BACKLIGHT_set_brightness(BACKLIGHT_MAX_BRIGHTNESS);
	}
	else
	if (g_eeprom.config.setting.backlight_time > 0)
	{
		g_backlight_tick_10ms = BACKLIGHT_ticks();
		BACKLIGHT_set_brightness(BACKLIGHT_MAX_BRIGHTNESS);
	}
}

void BACKLIGHT_turn_off(void)
{
	if (g_backlight_tick_10ms > BACKLIGHT_MAX_BRIGHTNESS)
		g_backlight_tick_10ms = BACKLIGHT_MAX_BRIGHTNESS;

//	g_backlight_tick_10ms = 0;

	BACKLIGHT_set_brightness(g_backlight_tick_10ms);
}
