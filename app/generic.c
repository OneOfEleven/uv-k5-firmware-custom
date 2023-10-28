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

#include "app/app.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "app/generic.h"
#include "app/menu.h"
#include "app/search.h"
#include "audio.h"
#include "driver/keyboard.h"
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
#include "dtmf.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

void GENERIC_Key_F(bool key_pressed, bool key_held)
{
	g_key_input_count_down = key_input_timeout_500ms;

	if (g_input_box_index > 0)
	{
		#ifdef ENABLE_FMRADIO
			if (!g_fm_radio_mode)
		#endif
				if (!key_held && key_pressed)
					g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	if (key_held)
	{	// f-key held

		#ifdef ENABLE_KEYLOCK
		if (key_pressed && g_current_display_screen != DISPLAY_MENU && g_current_function != FUNCTION_TRANSMIT)
		{	// toggle the keyboad lock

			#ifdef ENABLE_VOICE
				g_another_voice_id = g_eeprom.key_lock ? VOICE_ID_UNLOCK : VOICE_ID_LOCK;
			#endif

			g_eeprom.key_lock       = !g_eeprom.key_lock;
			g_request_save_settings = true;
			g_update_status         = true;

			// keypad is locked, tell the user
			g_keypad_locked  = 4;      // 2 second pop-up
			g_update_display = true;
		}
		#endif
		
		return;
	}

	if (key_pressed)
	{
		#ifdef ENABLE_FMRADIO
			if (!g_fm_radio_mode)
		#endif
				g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;
		return;
	}

	if (g_current_function == FUNCTION_TRANSMIT)
		return;
	
	// toggle the f-key flag
	g_fkey_pressed = !g_fkey_pressed;

	#ifdef ENABLE_VOICE
		if (!g_fkey_pressed)
			g_another_voice_id = VOICE_ID_CANCEL;
	#endif

	g_update_status = true;
}

void GENERIC_Key_PTT(bool key_pressed)
{
	g_input_box_index = 0;

	if (!key_pressed || g_serial_config_tick_500ms > 0)
	{	// PTT released

		if (g_current_function == FUNCTION_TRANSMIT)
		{	// we are transmitting .. stop

			if (g_flag_end_tx)
			{
				FUNCTION_Select(FUNCTION_FOREGROUND);
			}
			else
			{
				APP_end_tx();

				if (g_eeprom.repeater_tail_tone_elimination == 0)
					FUNCTION_Select(FUNCTION_FOREGROUND);
				else
					g_rtte_count_down = g_eeprom.repeater_tail_tone_elimination * 10;
			}

			g_flag_end_tx = false;

			#ifdef ENABLE_VOX
				g_vox_noise_detected = false;
			#endif

			RADIO_Setg_vfo_state(VFO_STATE_NORMAL);

			if (g_current_display_screen != DISPLAY_MENU)     // 1of11 .. don't close the menu
				g_request_display_screen = DISPLAY_MAIN;
		}

		return;
	}

	// PTT pressed

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		UART_printf("gene key 1 %u\r\n", key_pressed);
	#endif

	if (g_scan_state_dir != SCAN_STATE_DIR_OFF ||   // freq/chan scanning
	    g_current_display_screen == DISPLAY_SEARCH  ||   // CTCSS/CDCSS scanning
	    g_css_scan_mode != CSS_SCAN_MODE_OFF)       //   "     "
	{	// we're scanning .. stop

		if (g_current_display_screen == DISPLAY_SEARCH)
		{	// CTCSS/CDCSS scanning .. stop
			g_eeprom.cross_vfo_rx_tx = g_backup_cross_vfo_rx_tx;
			g_search_flag_stop_scan  = true;
			g_vfo_configure_mode     = VFO_CONFIGURE_RELOAD;
			g_flag_reset_vfos        = true;
		}
		else
		if (g_scan_state_dir != SCAN_STATE_DIR_OFF)
		{	// freq/chan scanning . .stop
			APP_stop_scan();
			g_request_display_screen = DISPLAY_MAIN;
		}
		else
		if (g_css_scan_mode != CSS_SCAN_MODE_OFF)
		{	// CTCSS/CDCSS scanning .. stop
			MENU_stop_css_scan();

			#ifdef ENABLE_VOICE
				g_another_voice_id = VOICE_ID_SCANNING_STOP;
			#endif
		}

		goto cancel_tx;
	}

	#ifdef ENABLE_FMRADIO
		if (g_fm_scan_state != FM_SCAN_OFF)
		{	// FM radio is scanning .. stop
			FM_PlayAndUpdate();
			#ifdef ENABLE_VOICE
				g_another_voice_id = VOICE_ID_SCANNING_STOP;
			#endif
			g_request_display_screen = DISPLAY_FM;
			goto cancel_tx;
		}

		if (g_current_display_screen == DISPLAY_FM)
			goto start_tx;	// listening to the FM radio .. start TX'ing
	#endif

	if (g_current_function == FUNCTION_TRANSMIT && g_rtte_count_down == 0)
	{	// already transmitting
		g_input_box_index = 0;
		return;
	}

	if (g_current_display_screen != DISPLAY_MENU)     // 1of11 .. don't close the menu
		g_request_display_screen = DISPLAY_MAIN;

	if (!g_dtmf_input_mode && g_dtmf_input_box_index == 0)
		goto start_tx;	// wasn't entering a DTMF code .. start TX'ing (maybe)

	// was entering a DTMF string

	if (g_dtmf_input_box_index > 0 || g_dtmf_prev_index > 0)
	{	// going to transmit a DTMF string

		if (g_dtmf_input_box_index == 0 && g_dtmf_prev_index > 0)
			g_dtmf_input_box_index = g_dtmf_prev_index;           // use the previous DTMF string

		if (g_dtmf_input_box_index < sizeof(g_dtmf_input_box))
			g_dtmf_input_box[g_dtmf_input_box_index] = 0;             // NULL term the string

		#if 0
			// append our DTMF ID to the inputted DTMF code -
			//  IF the user inputted code is exactly 3 digits long
			if (g_dtmf_input_box_index == 3)
				g_dtmf_call_mode = DTMF_CheckGroupCall(g_dtmf_input_box, 3);
			else
				g_dtmf_call_mode = DTMF_CALL_MODE_DTMF;
		#else
			// append our DTMF ID to the inputted DTMF code -
			//  IF the user inputted code is exactly 3 digits long and D-DCD is enabled
			if (g_dtmf_input_box_index == 3 && g_tx_vfo->dtmf_decoding_enable > 0)
				g_dtmf_call_mode = DTMF_CheckGroupCall(g_dtmf_input_box, 3);
			else
				g_dtmf_call_mode = DTMF_CALL_MODE_DTMF;
		#endif

		// remember the DTMF string
		g_dtmf_prev_index = g_dtmf_input_box_index;
		strcpy(g_dtmf_string, g_dtmf_input_box);

		g_dtmf_reply_state = DTMF_REPLY_ANI;
		g_dtmf_state       = DTMF_STATE_0;
	}

	DTMF_clear_input_box();

start_tx:
	// request start TX
	g_flag_prepare_tx = true;
	goto done;

cancel_tx:
	g_ptt_was_pressed = true;

done:
	g_ptt_debounce = 0;
	if (g_current_display_screen != DISPLAY_MENU && g_request_display_screen != DISPLAY_FM)     // 1of11 .. don't close the menu
		g_request_display_screen = DISPLAY_MAIN;
	g_update_status  = true;
	g_update_display = true;
}
