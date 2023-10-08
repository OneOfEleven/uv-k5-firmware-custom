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

BOOT_Mode_t BOOT_GetMode(void)
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
		g_key_reading_0 = Keys[0];
		g_key_reading_1 = Keys[0];

		g_debounce_counter = 2;

		if (Keys[0] == KEY_SIDE1)
			return BOOT_MODE_F_LOCK;

		#ifdef ENABLE_AIRCOPY
			if (Keys[0] == KEY_SIDE2)
				return BOOT_MODE_AIRCOPY;
		#endif
	}

	return BOOT_MODE_NORMAL;
}

void BOOT_ProcessMode(BOOT_Mode_t Mode)
{
	if (Mode == BOOT_MODE_F_LOCK)
	{
		GUI_SelectNextDisplay(DISPLAY_MENU);
	}
	#ifdef ENABLE_AIRCOPY
		else
		if (Mode == BOOT_MODE_AIRCOPY)
		{
			g_eeprom.dual_watch               = DUAL_WATCH_OFF;
			g_eeprom.battery_save             = 0;
			#ifdef ENABLE_VOX
				g_eeprom.vox_switch           = false;
			#endif
			g_eeprom.cross_vfo_rx_tx          = CROSS_BAND_OFF;
			g_eeprom.auto_keypad_lock         = false;
			g_eeprom.key1_short_press_action  = ACTION_OPT_NONE;
			g_eeprom.key1_long_press_action   = ACTION_OPT_NONE;
			g_eeprom.key2_short_press_action  = ACTION_OPT_NONE;
			g_eeprom.key2_long_press_action   = ACTION_OPT_NONE;

			RADIO_InitInfo(gRxVfo, FREQ_CHANNEL_LAST - 1, 41002500);

			gRxVfo->channel_bandwidth        = BANDWIDTH_WIDE;
			gRxVfo->output_power             = OUTPUT_POWER_LOW;

			RADIO_ConfigureSquelchAndOutputPower(gRxVfo);

			gCurrentVfo = gRxVfo;

			RADIO_SetupRegisters(true);
			BK4819_SetupAircopy();
			BK4819_ResetFSK();

			gAircopyState = AIRCOPY_READY;

			GUI_SelectNextDisplay(DISPLAY_AIRCOPY);
		}
	#endif
	else
	{
		GUI_SelectNextDisplay(DISPLAY_MAIN);
	}
}
