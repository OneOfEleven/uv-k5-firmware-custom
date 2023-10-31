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

#include "app/dtmf.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "app/search.h"
#include "driver/keyboard.h"
#include "misc.h"
#ifdef ENABLE_AIRCOPY
	#include "ui/aircopy.h"
#endif
#ifdef ENABLE_FMRADIO
	#include "ui/fmradio.h"
#endif
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/menu.h"
#include "ui/search.h"
#include "ui/ui.h"

gui_display_type_t g_current_display_screen;
gui_display_type_t g_request_display_screen = DISPLAY_INVALID;
uint8_t            g_ask_for_confirmation;
bool               g_ask_to_save;
bool               g_ask_to_delete;

void GUI_DisplayScreen(void)
{
	g_update_display = false;

	switch (g_current_display_screen)
	{
		case DISPLAY_MAIN:
			UI_DisplayMain();
			break;

		#ifdef ENABLE_FMRADIO
			case DISPLAY_FM:
				UI_DisplayFM();
				break;
		#endif
		
		case DISPLAY_MENU:
			UI_DisplayMenu();
			break;

		case DISPLAY_SEARCH:
			UI_DisplaySearch();
			break;

		#ifdef ENABLE_AIRCOPY
			case DISPLAY_AIRCOPY:
				UI_DisplayAircopy();
				break;
		#endif

		default:
			break;
	}
}

void GUI_SelectNextDisplay(gui_display_type_t Display)
{
	if (Display == DISPLAY_INVALID)
		return;

	if (g_current_display_screen != Display)
	{
		DTMF_clear_input_box();

		g_input_box_index      = 0;
		g_in_sub_menu          = false;
		g_css_scan_mode        = CSS_SCAN_MODE_OFF;
		g_scan_state_dir       = SCAN_STATE_DIR_OFF;
		#ifdef ENABLE_FMRADIO
			g_fm_scan_state_dir = FM_SCAN_STATE_DIR_OFF;
			g_update_display    = true;
		#endif
		g_ask_for_confirmation = 0;
		g_ask_to_save          = false;
		g_ask_to_delete        = false;
		g_fkey_pressed         = false;

		g_update_status        = true;
	}

	g_current_display_screen = Display;
	g_update_display    = true;
}
