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

	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="
	#pragma GCC diagnostic pop

#include <string.h>
#include <stdlib.h>  // abs()

#include "app/dtmf.h"
#ifdef ENABLE_AM_FIX_SHOW_DATA
	#include "am_fix.h"
#endif
#include "bitmaps.h"
#include "board.h"
#include "driver/bk4819.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/menu.h"
#include "ui/ui.h"

center_line_t center_line = CENTER_LINE_NONE;

// ***************************************************************************

#ifdef ENABLE_SHOW_TX_TIMEOUT
	bool UI_DisplayTXCountdown(const bool now)
	{
		unsigned int timeout_secs = 0;

		if (g_current_function != FUNCTION_TRANSMIT || g_screen_to_display != DISPLAY_MAIN)
			return false;

		if (center_line != CENTER_LINE_NONE && center_line != CENTER_LINE_TX_TIMEOUT)
			return false;

		if (g_eeprom.tx_timeout_timer == 0)
			timeout_secs = 30;   // 30 sec
		else
		if (g_eeprom.tx_timeout_timer < (ARRAY_SIZE(g_sub_menu_TOT) - 1))
			timeout_secs = 60 * g_eeprom.tx_timeout_timer;  // minutes
		else
			timeout_secs = 60 * 15;  // 15 minutes

		if (timeout_secs == 0 || g_tx_timer_count_down_500ms == 0)
			return false;
		
		{
			const unsigned int line      = 3;
			const unsigned int txt_width = 7 * 6;                 // 6 text chars
			const unsigned int bar_x     = 2 + txt_width + 4;     // X coord of bar graph
			const unsigned int bar_width = LCD_WIDTH - 1 - bar_x;
			const unsigned int secs      = g_tx_timer_count_down_500ms / 2;
			const unsigned int level     = ((secs * bar_width) + (timeout_secs / 2)) / timeout_secs;   // with rounding
//			const unsigned int level     = (((timeout_secs - secs) * bar_width) + (timeout_secs / 2)) / timeout_secs;   // with rounding
			const unsigned int len       = (level <= bar_width) ? level : bar_width;
			uint8_t           *p_line    = g_frame_buffer[line];
			unsigned int       i;
			char               s[16];
			
			if (now)
				memset(p_line, 0, LCD_WIDTH);

			sprintf(s, "TX %u", secs);
			UI_PrintStringSmall(s, 2, 0, line);

			#if 1
				// solid bar
				for (i = 0; i < bar_width; i++)
					p_line[bar_x + i] = (i > len) ? ((i & 1) == 0) ? 0x41 : 0x00 : ((i & 1) == 0) ? 0x7f : 0x3e;
			#else
				// knuled bar
				for (i = 0; i < bar_width; i += 2)
					p_line[bar_x + i] = (i <= len) ? 0x7f : 0x41;
			#endif

			if (now)
				ST7565_BlitFullScreen();
		}
		
		return true;
	}
#endif

void UI_drawBars(uint8_t *p, const unsigned int level)
{
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

	switch (level)
	{
		default:
		case 7: memmove(p + 20, BITMAP_ANTENNA_LEVEL6, sizeof(BITMAP_ANTENNA_LEVEL6));
		case 6: memmove(p + 17, BITMAP_ANTENNA_LEVEL5, sizeof(BITMAP_ANTENNA_LEVEL5));
		case 5: memmove(p + 14, BITMAP_ANTENNA_LEVEL4, sizeof(BITMAP_ANTENNA_LEVEL4));
		case 4: memmove(p + 11, BITMAP_ANTENNA_LEVEL3, sizeof(BITMAP_ANTENNA_LEVEL3));
		case 3: memmove(p +  8, BITMAP_ANTENNA_LEVEL2, sizeof(BITMAP_ANTENNA_LEVEL2));
		case 2: memmove(p +  5, BITMAP_ANTENNA_LEVEL1, sizeof(BITMAP_ANTENNA_LEVEL1));
		case 1: memmove(p +  0, BITMAP_ANTENNA,       sizeof(BITMAP_ANTENNA));
		case 0: break;
	}

	#pragma GCC diagnostic pop
}

#ifdef ENABLE_AUDIO_BAR

	unsigned int sqrt16(unsigned int value)
	{	// return square root of 'value'
		unsigned int shift = 16;         // number of bits supplied in 'value' .. 2 ~ 32
		unsigned int bit   = 1u << --shift;
		unsigned int sqrti = 0;
		while (bit)
		{
			const unsigned int temp = ((sqrti << 1) | bit) << shift--;
			if (value >= temp)
			{
				value -= temp;
				sqrti |= bit;
			}
			bit >>= 1;
		}
		return sqrti;
	}

	void UI_DisplayAudioBar(void)
	{
		if (g_setting_mic_bar)
		{
			const unsigned int line      = 3;
			const unsigned int bar_x     = 2;
			const unsigned int bar_width = LCD_WIDTH - 2 - bar_x;
			unsigned int       i;

			if (g_current_function != FUNCTION_TRANSMIT ||
				g_screen_to_display != DISPLAY_MAIN      ||
				g_dtmf_call_state != DTMF_CALL_STATE_NONE)
			{
				return;  // screen is in use
			}

			#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
				if (g_alarm_state != ALARM_STATE_OFF)
					return;
			#endif

			{
				#if 1
					// TX audio level

					const unsigned int voice_amp  = BK4819_GetVoiceAmplitudeOut();  // 15:0

//					const unsigned int max        = 65535;
//					const unsigned int level      = ((voice_amp * bar_width) + (max / 2)) / max;            // with rounding
//					const unsigned int len        = (level <= bar_width) ? level : bar_width;

					// make non-linear to make more sensitive at low values
					const unsigned int level      = voice_amp * 8;
					const unsigned int sqrt_level = sqrt16((level < 65535) ? level : 65535);
					const unsigned int len        = (sqrt_level <= bar_width) ? sqrt_level : bar_width;

				#else
					// TX/RX AF input level (dB)

					const uint8_t      af_tx_rx   = BK4819_GetAfTxRx();             //  6:0
					const unsigned int max        = 63;
					const unsigned int level      = (((uint16_t)af_tx_rx * bar_width) + (max / 2)) / max;   // with rounding
					const unsigned int len        = (level <= bar_width) ? level : bar_width;

				#endif

				uint8_t *p_line = g_frame_buffer[line];

				memset(p_line, 0, LCD_WIDTH);

				#if 1
					// solid bar
					for (i = 0; i < bar_width; i++)
						p_line[bar_x + i] = (i > len) ? ((i & 1) == 0) ? 0x41 : 0x00 : ((i & 1) == 0) ? 0x7f : 0x3e;
				#else
					// knuled bar
					for (i = 0; i < bar_width; i += 2)
						p_line[bar_x + i] = (i <= len) ? 0x7f : 0x41;
				#endif

				if (g_current_function == FUNCTION_TRANSMIT)
					ST7565_BlitFullScreen();
			}
		}
	}
#endif

#ifdef ENABLE_RSSI_BAR
	void UI_DisplayRSSIBar(const int16_t rssi, const bool now)
	{
//		const int16_t      s0_dBm       = -127;                  // S0 .. base level
		const int16_t      s0_dBm       = -147;                  // S0 .. base level

		const int16_t      s9_dBm       = s0_dBm + (6 * 9);      // S9 .. 6dB/S-Point
		const int16_t      bar_max_dBm  = s9_dBm + 30;           // S9+30dB
//		const int16_t      bar_min_dBm  = s0_dBm + (6 * 0);      // S0
		const int16_t      bar_min_dBm  = s0_dBm + (6 * 4);      // S4

		// ************

		const unsigned int txt_width    = 7 * 8;                 // 8 text chars
		const unsigned int bar_x        = 2 + txt_width + 4;     // X coord of bar graph
		const unsigned int bar_width    = LCD_WIDTH - 1 - bar_x;

		const int16_t      rssi_dBm     = (rssi / 2) - 160;
		const int16_t      clamped_dBm  = (rssi_dBm <= bar_min_dBm) ? bar_min_dBm : (rssi_dBm >= bar_max_dBm) ? bar_max_dBm : rssi_dBm;
		const unsigned int bar_range_dB = bar_max_dBm - bar_min_dBm;
		const unsigned int len          = ((clamped_dBm - bar_min_dBm) * bar_width) / bar_range_dB;

		const unsigned int line         = 3;
		uint8_t           *p_line        = g_frame_buffer[line];

		char               s[16];
		unsigned int       i;

		if (g_eeprom.key_lock && g_keypad_locked > 0)
			return;     // display is in use

		if (g_current_function == FUNCTION_TRANSMIT ||
		    g_screen_to_display != DISPLAY_MAIN ||
			g_dtmf_call_state != DTMF_CALL_STATE_NONE)
			return;     // display is in use

		if (now)
			memset(p_line, 0, LCD_WIDTH);

		if (rssi_dBm >= (s9_dBm + 6))
		{	// S9+XXdB, 1dB increment
			const char *fmt[] = {"%3d 9+%u  ", "%3d 9+%2u "};
			const unsigned int s9_dB = ((rssi_dBm - s9_dBm) <= 99) ? rssi_dBm - s9_dBm : 99;
			sprintf(s, (s9_dB < 10) ? fmt[0] : fmt[1], rssi_dBm, s9_dB);
		}
		else
		{	// S0 ~ S9, 6dB per S-point
			const unsigned int s_level = (rssi_dBm >= s0_dBm) ? (rssi_dBm - s0_dBm) / 6 : 0;
			sprintf(s, "%4d S%u ", rssi_dBm, s_level);
		}
		UI_PrintStringSmall(s, 2, 0, line);

		#if 1
			// solid bar
			for (i = 0; i < bar_width; i++)
				p_line[bar_x + i] = (i > len) ? ((i & 1) == 0) ? 0x41 : 0x00 : ((i & 1) == 0) ? 0x7f : 0x3e;
		#else
			// knuled bar
			for (i = 0; i < bar_width; i += 2)
				p_line[bar_x + i] = (i <= len) ? 0x7f : 0x41;
		#endif

		if (now)
			ST7565_BlitFullScreen();
	}
#endif

void UI_UpdateRSSI(const int16_t rssi, const int vfo)
{
	#ifdef ENABLE_RSSI_BAR

		(void)vfo;  // unused

		// optional larger RSSI dBm, S-point and bar level

		if (center_line != CENTER_LINE_RSSI)
			return;

		if (g_current_function == FUNCTION_RECEIVE ||
		    g_current_function == FUNCTION_MONITOR ||
		    g_current_function == FUNCTION_INCOMING)
		{
			UI_DisplayRSSIBar(rssi, true);
		}

	#else

		// original little RS bars

//		const int16_t dBm   = (rssi / 2) - 160;
		const uint8_t Line  = (vfo == 0) ? 3 : 7;
		uint8_t      *p_line = g_frame_buffer[Line - 1];

		// TODO: sort out all 8 values from the eeprom

		#if 0
			// dBm     -105  -100  -95   -90      -70   -65   -60   -55
			// RSSI     110   120   130   140      180   190   200   210
			// 0000C0   6E 00 78 00 82 00 8C 00    B4 00 BE 00 C8 00 D2 00
			//
			const unsigned int band = 1;
			const int16_t level0  = g_eeprom_rssi_calib[band][0];
			const int16_t level1  = g_eeprom_rssi_calib[band][1];
			const int16_t level2  = g_eeprom_rssi_calib[band][2];
			const int16_t level3  = g_eeprom_rssi_calib[band][3];
		#else
			const int16_t level0  = (-115 + 160) * 2;   // dB
			const int16_t level1  = ( -89 + 160) * 2;   // dB
			const int16_t level2  = ( -64 + 160) * 2;   // dB
			const int16_t level3  = ( -39 + 160) * 2;   // dB
		#endif
		const int16_t level01 = (level0 + level1) / 2;
		const int16_t level12 = (level1 + level2) / 2;
		const int16_t level23 = (level2 + level3) / 2;

		g_vfo_rssi[vfo] = rssi;

		uint8_t rssi_level = 0;

		if (rssi >= level3)  rssi_level = 7;
		else
		if (rssi >= level23) rssi_level = 6;
		else
		if (rssi >= level2)  rssi_level = 5;
		else
		if (rssi >= level12) rssi_level = 4;
		else
		if (rssi >= level1)  rssi_level = 3;
		else
		if (rssi >= level01) rssi_level = 2;
		else
		if (rssi >= level0)  rssi_level = 1;

		if (g_vfo_rssi_bar_level[vfo] == rssi_level)
			return;

		g_vfo_rssi_bar_level[vfo] = rssi_level;

		// **********************************************************

		if (g_eeprom.key_lock && g_keypad_locked > 0)
			return;    // display is in use

		if (g_current_function == FUNCTION_TRANSMIT || g_screen_to_display != DISPLAY_MAIN)
			return;    // display is in use

		p_line = g_frame_buffer[Line - 1];

		memset(p_line, 0, 23);

		// untested !!!

		if (rssi_level == 0)
			p_line = NULL;
		else
			UI_drawBars(p_line, rssi_level);

		ST7565_DrawLine(0, Line, 23, p_line);
	#endif
}

// ***************************************************************************

void UI_DisplayMain(void)
{
	const unsigned int line0 = 0;  // text screen line
	const unsigned int line1 = 4;
	char               String[16];
	unsigned int       vfo_num;

	center_line = CENTER_LINE_NONE;

//	#ifdef SINGLE_VFO_CHAN
//		const bool single_vfo = (g_eeprom.dual_watch == DUAL_WATCH_OFF && g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF) ? true : false;
//	#else
		const bool single_vfo = false;
//	#endif

	// clear the screen
	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	if (g_eeprom.key_lock && g_keypad_locked > 0)
	{	// tell user how to unlock the keyboard
		UI_PrintString("Long press #", 0, LCD_WIDTH, 1, 8);
		UI_PrintString("to unlock",    0, LCD_WIDTH, 3, 8);
		ST7565_BlitFullScreen();
		return;
	}

	for (vfo_num = 0; vfo_num < 2; vfo_num++)
	{
		const unsigned int line       = (vfo_num == 0) ? line0 : line1;
		unsigned int       channel    = g_eeprom.tx_vfo;
//		unsigned int       tx_channel = (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF) ? g_eeprom.rx_vfo : g_eeprom.tx_vfo;
		const bool         same_vfo   = (channel == vfo_num) ? true : false;
		uint8_t           *p_line0    = g_frame_buffer[line + 0];
		uint8_t           *p_line1    = g_frame_buffer[line + 1];
		unsigned int       mode       = 0;
		unsigned int       state;

		if (single_vfo)
		{	// we're in single VFO mode - screen is dedicated to just one VFO

			if (!same_vfo)
				continue;  // skip the unused vfo


		}

		if (g_eeprom.dual_watch != DUAL_WATCH_OFF && g_rx_vfo_is_active)
			channel = g_eeprom.rx_vfo;    // we're currently monitoring the other VFO

		if (channel != vfo_num)
		{
			if (g_dtmf_call_state != DTMF_CALL_STATE_NONE || g_dtmf_is_tx || g_dtmf_input_mode)
			{	// show DTMF stuff

				char Contact[16];

				if (!g_dtmf_input_mode)
				{
					memset(Contact, 0, sizeof(Contact));
					if (g_dtmf_call_state == DTMF_CALL_STATE_CALL_OUT)
						strcpy(String, (g_dtmf_state == DTMF_STATE_CALL_OUT_RSP) ? "CALL OUT(RSP)" : "CALL OUT");
					else
					if (g_dtmf_call_state == DTMF_CALL_STATE_RECEIVED || g_dtmf_call_state == DTMF_CALL_STATE_RECEIVED_STAY)
						sprintf(String, "CALL FRM:%s", (DTMF_FindContact(g_dtmf_caller, Contact)) ? Contact : g_dtmf_caller);
					else
					if (g_dtmf_is_tx)
						strcpy(String, (g_dtmf_state == DTMF_STATE_TX_SUCC) ? "DTMF TX(SUCC)" : "DTMF TX");
				}
				else
				{
					sprintf(String, ">%s", g_dtmf_input_box);
				}
				UI_PrintString(String, 2, 0, 0 + (vfo_num * 3), 8);

				memset(String,  0, sizeof(String));
				if (!g_dtmf_input_mode)
				{
					memset(Contact, 0, sizeof(Contact));
					if (g_dtmf_call_state == DTMF_CALL_STATE_CALL_OUT)
						sprintf(String, ">%s", (DTMF_FindContact(g_dtmf_string, Contact)) ? Contact : g_dtmf_string);
					else
					if (g_dtmf_call_state == DTMF_CALL_STATE_RECEIVED || g_dtmf_call_state == DTMF_CALL_STATE_RECEIVED_STAY)
						sprintf(String, ">%s", (DTMF_FindContact(g_dtmf_callee, Contact)) ? Contact : g_dtmf_callee);
					else
					if (g_dtmf_is_tx)
						sprintf(String, ">%s", g_dtmf_string);
				}
				UI_PrintString(String, 2, 0, 2 + (vfo_num * 3), 8);

				center_line = CENTER_LINE_IN_USE;
				continue;
			}

			// highlight the selected/used VFO with a marker
			if (!single_vfo && same_vfo)
				memmove(p_line0 + 0, BITMAP_VFO_DEFAULT, sizeof(BITMAP_VFO_DEFAULT));
			else
			if (g_eeprom.cross_vfo_rx_tx != CROSS_BAND_OFF)
				memmove(p_line0 + 0, BITMAP_VFO_NOT_DEFAULT, sizeof(BITMAP_VFO_NOT_DEFAULT));
		}
		else
		if (!single_vfo)
		{	// highlight the selected/used VFO with a marker
			if (same_vfo)
				memmove(p_line0 + 0, BITMAP_VFO_DEFAULT, sizeof(BITMAP_VFO_DEFAULT));
			else
			//if (g_eeprom.cross_vfo_rx_tx != CROSS_BAND_OFF)
				memmove(p_line0 + 0, BITMAP_VFO_NOT_DEFAULT, sizeof(BITMAP_VFO_NOT_DEFAULT));
		}

		if (g_current_function == FUNCTION_TRANSMIT)
		{	// transmitting

			#ifdef ENABLE_ALARM
				if (g_alarm_state == ALARM_STATE_ALARM)
					mode = 2;
				else
			#endif
			{
				channel = (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF) ? g_eeprom.rx_vfo : g_eeprom.tx_vfo;
				if (channel == vfo_num)
				{	// show the TX symbol
					mode = 1;
					#ifdef ENABLE_SMALL_BOLD
						UI_PrintStringSmallBold("TX", 14, 0, line);
					#else
						UI_PrintStringSmall("TX", 14, 0, line);
					#endif
				}
			}
		}
		else
		{	// receiving .. show the RX symbol
			mode = 2;
			if ((g_current_function == FUNCTION_RECEIVE ||
			     g_current_function == FUNCTION_MONITOR ||
			     g_current_function == FUNCTION_INCOMING) &&
			     g_eeprom.rx_vfo == vfo_num)
			{
				#ifdef ENABLE_SMALL_BOLD
					UI_PrintStringSmallBold("RX", 14, 0, line);
				#else
					UI_PrintStringSmall("RX", 14, 0, line);
				#endif
			}
		}

		if (g_eeprom.screen_channel[vfo_num] <= USER_CHANNEL_LAST)
		{	// channel mode
			const unsigned int x = 2;
			const bool inputting = (g_input_box_index == 0 || g_eeprom.tx_vfo != vfo_num) ? false : true;
			if (!inputting)
				NUMBER_ToDigits(g_eeprom.screen_channel[vfo_num] + 1, String);  // show the memory channel number
			else
				memmove(String + 5, g_input_box, 3);                            // show the input text
			UI_PrintStringSmall("M", x, 0, line + 1);
			UI_Displaysmall_digits(3, String + 5, x + 7, line + 1, inputting);
		}
		else
		if (IS_FREQ_CHANNEL(g_eeprom.screen_channel[vfo_num]))
		{	// frequency mode
			// show the frequency band number
			const unsigned int x = 2;	// was 14
//			sprintf(String, "FB%u", 1 + g_eeprom.screen_channel[vfo_num] - FREQ_CHANNEL_FIRST);
			sprintf(String, "VFO%u", 1 + g_eeprom.screen_channel[vfo_num] - FREQ_CHANNEL_FIRST);
			UI_PrintStringSmall(String, x, 0, line + 1);
		}
		#ifdef ENABLE_NOAA
			else
			{
				if (g_input_box_index == 0 || g_eeprom.tx_vfo != vfo_num)
				{	// channel number
					sprintf(String, "N%u", 1 + g_eeprom.screen_channel[vfo_num] - NOAA_CHANNEL_FIRST);
				}
				else
				{	// user entering channel number
					sprintf(String, "N%u%u", '0' + g_input_box[0], '0' + g_input_box[1]);
				}
				UI_PrintStringSmall(String, 7, 0, line + 1);
			}
		#endif

		// ************

		state = g_vfo_state[vfo_num];

		#ifdef ENABLE_ALARM
			if (g_current_function == FUNCTION_TRANSMIT && g_alarm_state == ALARM_STATE_ALARM)
			{
				channel = (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF) ? g_eeprom.rx_vfo : g_eeprom.tx_vfo;
				if (channel == vfo_num)
					state = VFO_STATE_ALARM;
			}
		#endif

		if (state != VFO_STATE_NORMAL)
		{
			const char *state_list[] = {"", "BUSY", "BAT LOW", "TX DISABLE", "TIMEOUT", "ALARM", "VOLT HIGH"};
			if (state < ARRAY_SIZE(state_list))
				UI_PrintString(state_list[state], 31, 0, line, 8);
		}
		else
		if (g_input_box_index > 0 && IS_FREQ_CHANNEL(g_eeprom.screen_channel[vfo_num]) && g_eeprom.tx_vfo == vfo_num)
		{	// user entering a frequency
			UI_DisplayFrequency(g_input_box, 32, line, true, false);

//			center_line = CENTER_LINE_IN_USE;
		}
		else
		{
			uint32_t frequency = g_eeprom.vfo_info[vfo_num].pRX->frequency;
			if (g_current_function == FUNCTION_TRANSMIT)
			{	// transmitting
				channel = (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_OFF) ? g_eeprom.rx_vfo : g_eeprom.tx_vfo;
				if (channel == vfo_num)
					frequency = g_eeprom.vfo_info[vfo_num].pTX->frequency;
			}

			if (g_eeprom.screen_channel[vfo_num] <= USER_CHANNEL_LAST)
			{	// it's a channel

				// show the channel symbols
				const uint8_t attributes = g_user_channel_attributes[g_eeprom.screen_channel[vfo_num]];
				if (attributes & USER_CH_SCANLIST1)
					memmove(p_line0 + 113, BITMAP_SCANLIST1, sizeof(BITMAP_SCANLIST1));
				if (attributes & USER_CH_SCANLIST2)
					memmove(p_line0 + 120, BITMAP_SCANLIST2, sizeof(BITMAP_SCANLIST2));
				#ifndef ENABLE_BIG_FREQ
					if ((attributes & USER_CH_COMPAND) > 0)
						memmove(p_line0 + 120 + LCD_WIDTH, BITMAP_COMPAND, sizeof(BITMAP_COMPAND));
				#else

					// TODO:  // find somewhere else to put the symbol

				#endif

				#pragma GCC diagnostic push
				#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

				switch (g_eeprom.channel_display_mode)
				{
					case MDF_FREQUENCY:	// show the channel frequency
						#ifdef ENABLE_BIG_FREQ
							NUMBER_ToDigits(frequency, String);
							// show the main large frequency digits
							UI_DisplayFrequency(String, 32, line, false, false);
							// show the remaining 2 small frequency digits
							UI_Displaysmall_digits(2, String + 6, 113, line + 1, true);
						#else
							// show the frequency in the main font
							sprintf(String, "%03u.%05u", frequency / 100000, frequency % 100000);
							UI_PrintString(String, 32, 0, line, 8);
						#endif
						break;

					case MDF_CHANNEL:	// show the channel number
						sprintf(String, "CH-%03u", g_eeprom.screen_channel[vfo_num] + 1);
						UI_PrintString(String, 32, 0, line, 8);
						break;

					case MDF_NAME:		// show the channel name
					case MDF_NAME_FREQ:	// show the channel name and frequency

						BOARD_fetchChannelName(String, g_eeprom.screen_channel[vfo_num]);
						if (String[0] == 0)
						{	// no channel name, show the channel number instead
							sprintf(String, "CH-%03u", g_eeprom.screen_channel[vfo_num] + 1);
						}

						if (g_eeprom.channel_display_mode == MDF_NAME)
						{
							UI_PrintString(String, 32, 0, line, 8);
						}
						else
						{
							#ifdef ENABLE_SMALL_BOLD
								UI_PrintStringSmallBold(String, 32 + 4, 0, line);
							#else
								UI_PrintStringSmall(String, 32 + 4, 0, line);
							#endif

							// show the channel frequency below the channel number/name
							sprintf(String, "%03u.%05u", frequency / 100000, frequency % 100000);
							UI_PrintStringSmall(String, 32 + 4, 0, line + 1);
						}

						break;
				}

				#pragma GCC diagnostic pop
			}
			else
			{	// frequency mode
				#ifdef ENABLE_BIG_FREQ
					NUMBER_ToDigits(frequency, String);  // 8 digits
					// show the main large frequency digits
					UI_DisplayFrequency(String, 32, line, false, false);
					// show the remaining 2 small frequency digits
					UI_Displaysmall_digits(2, String + 6, 113, line + 1, true);
				#else
					// show the frequency in the main font
					sprintf(String, "%03u.%05u", frequency / 100000, frequency % 100000);
					UI_PrintString(String, 32, 0, line, 8);
				#endif

				// show the channel symbols
				const uint8_t attributes = g_user_channel_attributes[g_eeprom.screen_channel[vfo_num]];
				if ((attributes & USER_CH_COMPAND) > 0)
					#ifdef ENABLE_BIG_FREQ
						memmove(p_line0 + 120, BITMAP_COMPAND, sizeof(BITMAP_COMPAND));
					#else
						memmove(p_line0 + 120 + LCD_WIDTH, BITMAP_COMPAND, sizeof(BITMAP_COMPAND));
					#endif
			}
		}

		// ************

		{	// show the TX/RX level
			uint8_t Level = 0;

			if (mode == 1)
			{	// TX power level
				switch (g_rx_vfo->output_power)
				{
					case OUTPUT_POWER_LOW:  Level = 2; break;
					case OUTPUT_POWER_MID:  Level = 4; break;
					case OUTPUT_POWER_HIGH: Level = 6; break;
				}
			}
			else
			if (mode == 2)
			{	// RX signal level
				#ifndef ENABLE_RSSI_BAR
					// bar graph
					if (g_vfo_rssi_bar_level[vfo_num] > 0)
						Level = g_vfo_rssi_bar_level[vfo_num];
				#endif
			}

			UI_drawBars(p_line1 + LCD_WIDTH, Level);
		}

		// ************

		String[0] = '\0';
		if (g_eeprom.vfo_info[vfo_num].am_mode)
		{	// show the AM symbol
			strcpy(String, "AM");
		}
		else
		{	// or show the CTCSS/DCS symbol
			const freq_config_t *pConfig = (mode == 1) ? g_eeprom.vfo_info[vfo_num].pTX : g_eeprom.vfo_info[vfo_num].pRX;
			const unsigned int code_type = pConfig->code_type;
			const char *code_list[] = {"", "CT", "DCS", "DCR"};
			if (code_type < ARRAY_SIZE(code_list))
				strcpy(String, code_list[code_type]);
		}
		UI_PrintStringSmall(String, LCD_WIDTH + 24, 0, line + 1);

		if (state == VFO_STATE_NORMAL || state == VFO_STATE_ALARM)
		{	// show the TX power
			const char pwr_list[] = "LMH";
			const unsigned int i = g_eeprom.vfo_info[vfo_num].output_power;
			String[0] = (i < ARRAY_SIZE(pwr_list)) ? pwr_list[i] : '\0';
			String[1] = '\0';
			UI_PrintStringSmall(String, LCD_WIDTH + 46, 0, line + 1);
		}

		if (g_eeprom.vfo_info[vfo_num].freq_config_rx.frequency != g_eeprom.vfo_info[vfo_num].freq_config_tx.frequency)
		{	// show the TX offset symbol
			const char dir_list[] = "\0+-";
			const unsigned int i = g_eeprom.vfo_info[vfo_num].tx_offset_freq_dir;
			String[0] = (i < sizeof(dir_list)) ? dir_list[i] : '?';
			String[1] = '\0';
			UI_PrintStringSmall(String, LCD_WIDTH + 54, 0, line + 1);
		}

		// show the TX/RX reverse symbol
		if (g_eeprom.vfo_info[vfo_num].frequency_reverse)
			UI_PrintStringSmall("R", LCD_WIDTH + 62, 0, line + 1);

		{	// show the narrow band symbol
			String[0] = '\0';
			if (g_eeprom.vfo_info[vfo_num].channel_bandwidth == BANDWIDTH_NARROW)
			{
				String[0] = 'N';
				String[1] = '\0';
			}
			UI_PrintStringSmall(String, LCD_WIDTH + 70, 0, line + 1);
		}

		// show the DTMF decoding symbol
		if (g_eeprom.vfo_info[vfo_num].dtmf_decoding_enable || g_setting_killed)
			UI_PrintStringSmall("DTMF", LCD_WIDTH + 78, 0, line + 1);

		// show the audio scramble symbol
		if (g_eeprom.vfo_info[vfo_num].scrambling_type > 0 && g_setting_scramble_enable)
			UI_PrintStringSmall("SCR", LCD_WIDTH + 106, 0, line + 1);
	}

	if (center_line == CENTER_LINE_NONE)
	{	// we're free to use the middle line

		const bool rx = (g_current_function == FUNCTION_RECEIVE ||
		                 g_current_function == FUNCTION_MONITOR ||
		                 g_current_function == FUNCTION_INCOMING);

		#ifdef ENABLE_SHOW_TX_TIMEOUT
			// show the TX timeout count down
			if (UI_DisplayTXCountdown(false))
			{
				center_line = CENTER_LINE_TX_TIMEOUT;
			}
			else
		#endif

		#ifdef ENABLE_AUDIO_BAR
			// show the TX audio level
			if (g_setting_mic_bar && g_current_function == FUNCTION_TRANSMIT)
			{
				center_line = CENTER_LINE_AUDIO_BAR;
				UI_DisplayAudioBar();
			}
			else
		#endif

		#if defined(ENABLE_AM_FIX) && defined(ENABLE_AM_FIX_SHOW_DATA)
			// show the AM-FIX debug data
			if (rx && g_eeprom.vfo_info[g_eeprom.rx_vfo].am_mode && g_setting_am_fix)
			{
				if (g_screen_to_display != DISPLAY_MAIN || g_dtmf_call_state != DTMF_CALL_STATE_NONE)
					return;

				center_line = CENTER_LINE_AM_FIX_DATA;
				AM_fix_print_data(g_eeprom.rx_vfo, String);
				UI_PrintStringSmall(String, 2, 0, 3);
			}
			else
		#endif

		#ifdef ENABLE_RSSI_BAR
			// show the RX RSSI dBm, S-point and signal strength bar graph
			if (rx)
			{
				center_line = CENTER_LINE_RSSI;
				UI_DisplayRSSIBar(g_current_rssi[g_eeprom.rx_vfo], false);
			}
			else
		#endif

		if (rx || g_current_function == FUNCTION_FOREGROUND || g_current_function == FUNCTION_POWER_SAVE)
		{
			#if 1
				if (g_setting_live_dtmf_decoder && g_dtmf_rx_live[0] != 0)
				{	// show live DTMF decode
					const unsigned int len = strlen(g_dtmf_rx_live);
					const unsigned int idx = (len > (17 - 5)) ? len - (17 - 5) : 0;  // limit to last 'n' chars

					if (g_screen_to_display != DISPLAY_MAIN || g_dtmf_call_state != DTMF_CALL_STATE_NONE)
						return;

					center_line = CENTER_LINE_DTMF_DEC;

					strcpy(String, "DTMF ");
					strcat(String, g_dtmf_rx_live + idx);
					UI_PrintStringSmall(String, 2, 0, 3);
				}
			#else
				if (g_setting_live_dtmf_decoder && g_dtmf_rx_index > 0)
				{	// show live DTMF decode
					const unsigned int len = g_dtmf_rx_index;
					const unsigned int idx = (len > (17 - 5)) ? len - (17 - 5) : 0;  // limit to last 'n' chars

					if (g_screen_to_display != DISPLAY_MAIN || g_dtmf_call_state != DTMF_CALL_STATE_NONE)
						return;

					center_line = CENTER_LINE_DTMF_DEC;

					strcpy(String, "DTMF ");
					strcat(String, g_dtmf_rx + idx);
					UI_PrintStringSmall(String, 2, 0, 3);
				}
			#endif

			#ifdef ENABLE_SHOW_CHARGE_LEVEL
				else
				if (g_charging_with_type_c)
				{	// show the battery charge state
					if (g_screen_to_display != DISPLAY_MAIN || g_dtmf_call_state != DTMF_CALL_STATE_NONE)
						return;

					center_line = CENTER_LINE_CHARGE_DATA;

					sprintf(String, "Charge %u.%02uV %u%%",
						g_battery_voltage_average / 100, g_battery_voltage_average % 100,
						BATTERY_VoltsToPercent(g_battery_voltage_average));
					UI_PrintStringSmall(String, 2, 0, 3);
				}
			#endif
		}
	}

	ST7565_BlitFullScreen();
}

// ***************************************************************************
