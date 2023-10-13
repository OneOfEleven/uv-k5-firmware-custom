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
#include <stdio.h>     // NULL

#ifdef ENABLE_AM_FIX
	#include "am_fix.h"
#endif
#include "app/app.h"
#include "app/dtmf.h"
#include "audio.h"
#include "bsp/dp32g030/gpio.h"
#include "bsp/dp32g030/syscon.h"
#include "board.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "driver/uart.h"
#include "helper/battery.h"
#include "helper/boot.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/lock.h"
#include "ui/welcome.h"
#include "ui/menu.h"
#include "version.h"

void _putchar(char c)
{
	UART_Send((uint8_t *)&c, 1);
}

void Main(void)
{
	unsigned int i;
	boot_mode_t  BootMode;

	// Enable clock gating of blocks we need
	SYSCON_DEV_CLK_GATE = 0
		| SYSCON_DEV_CLK_GATE_GPIOA_BITS_ENABLE
		| SYSCON_DEV_CLK_GATE_GPIOB_BITS_ENABLE
		| SYSCON_DEV_CLK_GATE_GPIOC_BITS_ENABLE
		| SYSCON_DEV_CLK_GATE_UART1_BITS_ENABLE
		| SYSCON_DEV_CLK_GATE_SPI0_BITS_ENABLE
		| SYSCON_DEV_CLK_GATE_SARADC_BITS_ENABLE
		| SYSCON_DEV_CLK_GATE_CRC_BITS_ENABLE
		| SYSCON_DEV_CLK_GATE_AES_BITS_ENABLE;

	SYSTICK_Init();
	BOARD_Init();
	UART_Init();

	g_boot_counter_10ms = 250;   // 2.5 sec

	#if defined(ENABLE_UART)
		UART_SendText(UART_Version_str);
		UART_SendText("\r\n");
	#endif

	// Not implementing authentic device checks

	memset(&g_eeprom, 0, sizeof(g_eeprom));

	memset(g_dtmf_string, '-', sizeof(g_dtmf_string));
	g_dtmf_string[sizeof(g_dtmf_string) - 1] = 0;

#if 0
	SETTINGS_restore_calibration();
#endif

	BK4819_Init();

	BOARD_ADC_GetBatteryInfo(&g_usb_current_voltage, &g_usb_current);

	BOARD_EEPROM_load();

	BOARD_EEPROM_LoadMoreSettings();

	RADIO_ConfigureChannel(0, VFO_CONFIGURE_RELOAD);
	RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);

	RADIO_SelectVfos();

	RADIO_SetupRegisters(true);

	for (i = 0; i < ARRAY_SIZE(g_battery_voltages); i++)
		BOARD_ADC_GetBatteryInfo(&g_battery_voltages[i], &g_usb_current);

	BATTERY_GetReadings(false);

	ST7565_SetContrast(g_setting_contrast);

	#ifdef ENABLE_AM_FIX
		AM_fix_init();
	#endif

	BootMode = BOOT_GetMode();

	g_unhide_hidden = (BootMode == BOOT_MODE_UNHIDE_HIDDEN); // flag to say include the hidden menu items

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		if (g_unhide_hidden)
			UART_SendText("boot_unhide_hidden\r\n");
	#endif

	// sort the menu list
	UI_SortMenu(!g_unhide_hidden);

	// wait for user to release all butts before moving on
	if (!GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT) ||
	     KEYBOARD_Poll() != KEY_INVALID ||
		 BootMode != BOOT_MODE_NORMAL)
	{
		backlight_turn_on();
		UI_DisplayReleaseKeys();
		i = 0;
		while (i < (500 / 10))  // 500ms
		{
			i = (GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT) && KEYBOARD_Poll() == KEY_INVALID) ? i + 1 : 0;
			SYSTEM_DelayMs(10);
		}
	}

	if (!g_charging_with_type_c && g_battery_display_level == 0)
	{
		FUNCTION_Select(FUNCTION_POWER_SAVE);

		if (g_eeprom.backlight < (ARRAY_SIZE(g_sub_menu_backlight) - 1))
			GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);	// turn the backlight OFF
		else
			GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);  	// turn the backlight ON

		g_reduced_service = true;
	}
	else
	{
		UI_DisplayWelcome();

		backlight_turn_on();

		#ifdef ENABLE_VOICE
//			AUDIO_SetVoiceID(0, VOICE_ID_WELCOME);
//			AUDIO_PlaySingleVoice(0);
		#endif

		if (g_eeprom.pwr_on_display_mode != PWR_ON_DISPLAY_MODE_NONE)
		{	// 2.55 second boot-up screen
			while (g_boot_counter_10ms > 0)
			{
				if (KEYBOARD_Poll() != KEY_INVALID)
				{	// halt boot beeps and cancel boot screen
					g_boot_counter_10ms = 0;
					break;
				}
				#ifdef ENABLE_BOOT_BEEPS
					if ((g_boot_counter_10ms % 25) == 0)
						AUDIO_PlayBeep(BEEP_880HZ_40MS_OPTIONAL);
				#endif
			}
		}

		#ifdef ENABLE_PWRON_PASSWORD
			if (g_eeprom.power_on_password < 1000000)
			{
				g_password_locked = true;
				UI_DisplayLock();
				g_password_locked = false;
			}
		#endif

		BOOT_ProcessMode(BootMode);

		GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_VOICE_0);

		g_update_status = true;

		#ifdef ENABLE_VOICE
		if (g_eeprom.voice_prompt != VOICE_PROMPT_OFF)
		{
			const uint8_t Channel = g_eeprom.screen_channel[g_eeprom.tx_vfo];

			AUDIO_SetVoiceID(0, VOICE_ID_WELCOME);
			AUDIO_PlaySingleVoice(0);

			if (IS_USER_CHANNEL(Channel))
			{
				AUDIO_SetVoiceID(1, VOICE_ID_CHANNEL_MODE);
				AUDIO_SetDigitVoice(2, Channel + 1);
			}
			else
			if (IS_FREQ_CHANNEL(Channel))
				AUDIO_SetVoiceID(1, VOICE_ID_FREQUENCY_MODE);
		}
		#endif

		#ifdef ENABLE_NOAA
			RADIO_ConfigureNOAA();
		#endif

	}

	while (1)
	{
		APP_Update();

		if (g_next_time_slice)
		{
			APP_TimeSlice10ms();
			g_next_time_slice = false;
		}

		if (g_next_time_slice_500ms)
		{
			APP_TimeSlice500ms();
			g_next_time_slice_500ms = false;
		}
	}
}
