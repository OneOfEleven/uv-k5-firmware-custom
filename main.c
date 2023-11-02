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
#ifdef ENABLE_FMRADIO
	#include "driver/bk1080.h"
#endif
#include "driver/bk4819.h"
#include "driver/crc.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "driver/systick.h"
#if defined(ENABLE_UART)
	#include "driver/uart.h"
#endif
#include "external/printf/printf.h"
#include "helper/battery.h"
#include "helper/boot.h"
#ifdef ENABLE_MDC1200
	#include "mdc1200.h"
#endif
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/lock.h"
#include "ui/menu.h"
#include "ui/status.h"
#include "version.h"

void MAIN_DisplayClear(void)
{
	memset(g_status_line,  0, sizeof(g_status_line));
	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	ST7565_BlitStatusLine();
	ST7565_BlitFullScreen();
}

void MAIN_DisplayReadingEEPROM(void)
{
	memset(g_status_line,  0, sizeof(g_status_line));
	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	UI_PrintString("READING",   0, LCD_WIDTH, 1, 10);
	UI_PrintString("EEPROM", 0, LCD_WIDTH, 3, 10);

	ST7565_BlitStatusLine();
	ST7565_BlitFullScreen();
}

void MAIN_DisplayReleaseKeys(void)
{
	memset(g_status_line,  0, sizeof(g_status_line));
	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	UI_PrintString("RELEASE",  0, LCD_WIDTH, 1, 10);
	UI_PrintString("ALL KEYS", 0, LCD_WIDTH, 3, 10);

	ST7565_BlitStatusLine();  // blank status line
	ST7565_BlitFullScreen();
}

void MAIN_DisplayWelcome(void)
{
	char str0[17];
	char str1[17];
	char str2[17];
	
	memset(g_status_line,  0, sizeof(g_status_line));
	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	if (g_eeprom.config.setting.power_on_display_mode == PWR_ON_DISPLAY_MODE_NONE)
	{
		ST7565_FillScreen(0xFF);
	}
	else
	if (g_eeprom.config.setting.power_on_display_mode == PWR_ON_DISPLAY_MODE_FULL_SCREEN)
	{
		ST7565_FillScreen(0xFF);
	}
	else
	{
		unsigned int slen = strlen(Version_str);
		if (slen > (sizeof(str2) - 1))
			slen =  sizeof(str2) - 1;

		memset(str0, 0, sizeof(str0));
		memset(str1, 0, sizeof(str1));
		memset(str2, 0, sizeof(str2));

		if (g_eeprom.config.setting.power_on_display_mode == PWR_ON_DISPLAY_MODE_VOLTAGE)
		{
			strcpy(str0, "VOLTAGE");
			sprintf(str1, "%u.%02uV %u%%",
				g_battery_voltage_average / 100,
				g_battery_voltage_average % 100,
				BATTERY_VoltsToPercent(g_battery_voltage_average));
		}
		else
		{
			//EEPROM_ReadBuffer(0x0EB0, str0, 16);
			//sEEPROM_ReadBuffer(0x0EC0, str1, 16);
			memcpy(str0, g_eeprom.config.setting.welcome_line[0], 16);
			memcpy(str1, g_eeprom.config.setting.welcome_line[1], 16);
		}

		memcpy(str2, Version_str, slen);
		
		UI_PrintString(str0, 0, LCD_WIDTH, 0, 10);
		UI_PrintString(str1, 0, LCD_WIDTH, 2, 10);
		UI_PrintStringSmall(str2, 0, LCD_WIDTH, 4);
		UI_PrintStringSmall(__DATE__, 0, LCD_WIDTH, 5);
		UI_PrintStringSmall(__TIME__, 0, LCD_WIDTH, 6);

		#if 1
			ST7565_BlitStatusLine();  // blank status line
		#else
			UI_DisplayStatus(true);   // show all status line symbols (test)
		#endif
		
		ST7565_BlitFullScreen();
	}
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

	BOARD_PORTCON_Init();
	BOARD_GPIO_Init();
	CRC_Init();
	#ifdef ENABLE_UART
		UART_Init();
	#endif
	BOARD_ADC_Init();
	ST7565_Init(true);
	#ifdef ENABLE_FMRADIO
		BK1080_Init(0, false);
	#endif

	// ***************************

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

	#if defined(ENABLE_UART)
		UART_SendText(UART_Version_str);
		UART_SendText("\r\n");
	#endif

	BootMode = BOOT_GetMode();
	g_unhide_hidden = (BootMode == BOOT_MODE_UNHIDE_HIDDEN); // flag to say include the hidden menu items

	// load the entire EEPROM contents into memory
	GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);  // backlight on
	MAIN_DisplayReadingEEPROM();
	SETTINGS_read_eeprom();
	MAIN_DisplayClear();

	FREQUENCY_init();

	BK4819_Init();

	BOARD_ADC_GetBatteryInfo(&g_usb_current_voltage, &g_usb_current);

	#ifdef ENABLE_CONTRAST
		ST7565_SetContrast(g_setting_contrast);
	#endif

	#if defined(ENABLE_UART)
		UART_printf("BK4819  id %04X  rev %04X\r\n", BK4819_ReadRegister(0x00), BK4819_ReadRegister(0x01));
		#ifdef ENABLE_FMRADIO
			UART_printf("BK1080  id %04X  rev %04X\r\n", BK1080_ReadRegister(0x01), BK1080_ReadRegister(0x00));
		#endif
	#endif

	memset(g_dtmf_string, '-', sizeof(g_dtmf_string));
	g_dtmf_string[sizeof(g_dtmf_string) - 1] = 0;

	#ifdef ENABLE_MDC1200
		MDC1200_init();
	#endif

	#ifdef ENABLE_AM_FIX
		AM_fix_init();
	#endif

	BK4819_set_mic_gain(g_mic_sensitivity_tuning);

	RADIO_configure_channel(0, VFO_CONFIGURE_RELOAD);
	RADIO_configure_channel(1, VFO_CONFIGURE_RELOAD);

	RADIO_select_vfos();

	RADIO_setup_registers(true);

	for (i = 0; i < ARRAY_SIZE(g_battery_voltages); i++)
		BOARD_ADC_GetBatteryInfo(&g_battery_voltages[i], &g_usb_current);

	BATTERY_GetReadings(false);

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		if (g_unhide_hidden)
			UART_SendText("boot_unhide_hidden\r\n");
	#endif

	// sort the menu list
	UI_SortMenu(!g_unhide_hidden);

	// wait for user to release all buttons before moving on
	if (!GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT) ||
	     KEYBOARD_Poll() != KEY_INVALID ||
		 BootMode != BOOT_MODE_NORMAL)
	{
		GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);  // backlight on
		MAIN_DisplayReleaseKeys();
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

		if (g_eeprom.config.setting.backlight_time < (ARRAY_SIZE(g_sub_menu_backlight) - 1))
			GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);	// turn the backlight OFF
		else
			GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);  	// turn the backlight ON

		g_reduced_service = true;
	}
	else
	{
		MAIN_DisplayWelcome();

		backlight_turn_on(0);

		#ifdef ENABLE_VOICE
//			AUDIO_SetVoiceID(0, VOICE_ID_WELCOME);
//			AUDIO_PlaySingleVoice(0);
		#endif

		if (g_eeprom.config.setting.power_on_display_mode != PWR_ON_DISPLAY_MODE_NONE)
		{	// 3 second boot-up screen
			while (g_boot_tick_10ms > 0)
			{
				if (KEYBOARD_Poll() != KEY_INVALID)
				{	// halt boot beeps and cancel boot screen
					g_boot_tick_10ms = 0;
					break;
				}
				#ifdef ENABLE_BOOT_BEEPS
					if ((g_boot_tick_10ms % 25) == 0)
						AUDIO_PlayBeep(BEEP_880HZ_40MS_OPTIONAL);
				#endif
			}
		}

		#ifdef ENABLE_PWRON_PASSWORD
			if (g_eeprom.config.setting.power_on_password < 1000000)
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
		if (g_eeprom.config.setting.voice_prompt != VOICE_PROMPT_OFF)
		{
			const uint8_t Channel = g_eeprom.config.setting.indices.vfo[g_eeprom.config.setting.tx_vfo_num].screen;

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

	// Everything is initialised, set SLEEP* bits
	SYSCON_REGISTER |= SYSCON_REGISTER_SLEEPONEXIT_BITS_ENABLE;
	SYSCON_REGISTER |= SYSCON_REGISTER_SLEEPDEEP_BITS_ENABLE;

	while (1)
	{
		#if 1
			// Mask interrupts
			__asm volatile ("cpsid i");
			if (!g_next_time_slice)
				// Idle condition, hint the MCU to sleep
				// CMSIS suggests GCC reorders memory and is undesirable
				__asm volatile ("wfi":::"memory");
			// Unmask interrupts
			__asm volatile ("cpsie i");
		#endif

		if (g_next_time_slice)
		{
			APP_time_slice_10ms();
			g_next_time_slice = false;
		}

		if (g_next_time_slice_500ms)
		{
			APP_time_slice_500ms();
			g_next_time_slice_500ms = false;
		}
	}
}
