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

#ifdef ENABLE_PWRON_PASSWORD

#include "ARMCM0.h"
#include "app/uart.h"
#include "audio.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "misc.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/lock.h"

static void Render(void)
{
	unsigned int i;
	char         String[7];

	memset(g_status_line,  0, sizeof(g_status_line));
	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	strcpy(String, "LOCK");
	UI_PrintString(String, 0, 127, 1, 10);
	for (i = 0; i < 6; i++)
		String[i] = (g_input_box[i] == 10) ? '-' : '*';
	String[6] = 0;
	UI_PrintString(String, 0, 127, 3, 12);

	ST7565_BlitStatusLine();
	ST7565_BlitFullScreen();
}

void UI_DisplayLock(void)
{
	unsigned int g_debounce_counter = 0;
//	bool         g_key_being_held   = false;
	key_code_t   g_key_reading_0    = KEY_INVALID;
//	key_code_t   g_key_reading_1;
	key_code_t   Key;
	beep_type_t  Beep;

	g_update_display = true;

	memset(g_input_box, 10, sizeof(g_input_box));

	while (1)
	{
		while (!g_next_time_slice) {}

		// TODO: Original code doesn't do the below, but is needed for proper key debounce

		g_next_time_slice = false;

		Key = KEYBOARD_Poll();

		if (g_key_reading_0 == Key)
		{
			if (++g_debounce_counter == key_debounce_10ms)
			{
				if (Key == KEY_INVALID)
				{
//					g_key_reading_1 = KEY_INVALID;
				}
				else
				{
//					g_key_reading_1 = Key;

					switch (Key)
					{
						case KEY_0:
						case KEY_1:
						case KEY_2:
						case KEY_3:
						case KEY_4:
						case KEY_5:
						case KEY_6:
						case KEY_7:
						case KEY_8:
						case KEY_9:
							INPUTBOX_append(Key - KEY_0);

							if (g_input_box_index < 6)   // 6 frequency digits
							{
								Beep = BEEP_1KHZ_60MS_OPTIONAL;
							}
							else
							{
								uint32_t Password;

								g_input_box_index = 0;

								NUMBER_Get(g_input_box, &Password);

								if ((g_eeprom.config.setting.power_on_password * 100) == Password)
								{
									AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
									return;
								}

								memset(g_input_box, 10, sizeof(g_input_box));

								Beep = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
							}

							AUDIO_PlayBeep(Beep);

							g_update_display = true;
							break;

						case KEY_EXIT:
							if (g_input_box_index > 0)
							{
								g_input_box[--g_input_box_index] = 10;
								g_update_display = true;
							}

							AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);

						default:
							break;
					}
				}

//				g_key_being_held = false;
			}
		}
		else
		{
			g_debounce_counter = 0;
			g_key_reading_0    = Key;
		}

		if (UART_IsCommandAvailable())
		{
			__disable_irq();
			UART_HandleCommand();
			__enable_irq();
		}

		if (g_update_display)
		{
			Render();
			g_update_display = false;
		}
	}
}

#endif
