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

#ifndef GUI_H
#define GUI_H

#include <stdbool.h>
#include <stdint.h>

enum gui_display_type_e
{
	DISPLAY_MAIN = 0,
	DISPLAY_FM,
	DISPLAY_MENU,
	DISPLAY_SEARCH,
	DISPLAY_AIRCOPY,
	DISPLAY_INVALID     // 0xff
};
typedef enum gui_display_type_e gui_display_type_t;

extern gui_display_type_t g_current_display_screen;
extern gui_display_type_t g_request_display_screen;
extern uint8_t            g_ask_for_confirmation;
extern bool               g_ask_to_save;
extern bool               g_ask_to_delete;

void GUI_DisplayScreen(void);
void GUI_SelectNextDisplay(gui_display_type_t Display);

#endif

