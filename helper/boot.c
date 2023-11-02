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

#ifdef ENABLE_AIRCOPY
	#include "app/aircopy.h"
#endif
#include "bsp/dp32g030/gpio.h"
#include "driver/bk4819.h"
#include "driver/keyboard.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "helper/boot.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/menu.h"
#include "ui/ui.h"

boot_mode_t BOOT_GetMode(void)
{
	unsigned int i;
	key_code_t   Keys[2];

	for (i = 0; i < 2; i++)
	{
		if (GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT))
			return BOOT_MODE_NORMAL;   // PTT not pressed
		Keys[i] = KEYBOARD_Poll();
		SYSTEM_DelayMs(20);
	}

	if (Keys[0] == Keys[1])
	{
		if (Keys[0] == KEY_SIDE1)
			return BOOT_MODE_UNHIDE_HIDDEN;

		#ifdef ENABLE_AIRCOPY
			if (Keys[0] == KEY_SIDE2)
				return BOOT_MODE_AIRCOPY;
		#endif
	}

	return BOOT_MODE_NORMAL;
}

void BOOT_ProcessMode(boot_mode_t Mode)
{
	if (Mode == BOOT_MODE_UNHIDE_HIDDEN)
	{
		GUI_SelectNextDisplay(DISPLAY_MENU);
	}
	#ifdef ENABLE_AIRCOPY
		else
		if (Mode == BOOT_MODE_AIRCOPY)
		{
			g_eeprom.config.setting.dual_watch         = DUAL_WATCH_OFF;
			g_eeprom.config.setting.battery_save_ratio = 0;
			#ifdef ENABLE_VOX
				g_eeprom.config.setting.vox_switch   = false;
			#endif
			g_eeprom.config.setting.cross_vfo  = CROSS_BAND_OFF;
			g_eeprom.config.setting.auto_key_lock = 0;
			g_eeprom.config.setting.key1_short  = ACTION_OPT_NONE;
			g_eeprom.config.setting.key1_long   = ACTION_OPT_NONE;
			g_eeprom.config.setting.key2_short  = ACTION_OPT_NONE;
			g_eeprom.config.setting.key2_long   = ACTION_OPT_NONE;

			RADIO_InitInfo(g_rx_vfo, FREQ_CHANNEL_LAST - 1, g_aircopy_freq);

			g_rx_vfo->channel_bandwidth        = BANDWIDTH_WIDE;
			g_rx_vfo->output_power             = OUTPUT_POWER_LOW;

			RADIO_ConfigureSquelchAndOutputPower(g_rx_vfo);

			g_current_vfo = g_rx_vfo;

			AIRCOPY_init();
		}
	#endif
	else
	{
		GUI_SelectNextDisplay(DISPLAY_MAIN);
	}
}
