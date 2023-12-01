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
#ifdef ENABLE_AM_FIX_SHOW_DATA
	#include "am_fix.h"
#endif
#include "bitmaps.h"
#include "board.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/st7565.h"
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
#include "external/printf/printf.h"
#include "font.h"
#include "functions.h"
#ifdef ENABLE_SCAN_IGNORE_LIST
	#include "freq_ignore.h"
#endif
#include "helper/battery.h"
#ifdef ENABLE_MDC1200
	#include "mdc1200.h"
#endif
#include "misc.h"
#ifdef ENABLE_PANADAPTER
	#include "panadapter.h"
#endif
#include "radio.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/menu.h"
#include "ui/ui.h"

//#define SHOW_RX_TEST_VALUES

// calibrate the RSSI reading .. roughly
//const int     rssi_offset_band_123  = 0;
//const int     rssi_offset_band_4567 = 0;
const int     rssi_offset_band_123  = -44;
const int     rssi_offset_band_4567 = -18;

int           single_vfo  = -1;
bool          pan_enabled = false;

center_line_t g_center_line = CENTER_LINE_NONE;

// ***************************************************************************

void draw_small_antenna_bars(uint8_t *p, unsigned int level)
{
	unsigned int i;

	if (level > 6)
		level = 6;

	memcpy(p, BITMAP_ANTENNA, ARRAY_SIZE(BITMAP_ANTENNA));

	for (i = 1; i <= level; i++)
	{
		const uint8_t bar = (0xff << (6 - i)) & 0x7F;
		memset(p + 2 + (i * 3), bar, 2);
	}
}

void draw_bar(uint8_t *line, const int len, const int max_width)
{
	int i;
	#if 0
		// solid bar
		for (i = 0; i < max_width; i++)
			line[i] = (i > len) ? ((i & 1) == 0) ? 0x41 : 0x00 : ((i & 1) == 0) ? 0x7f : 0x3e;
	#elif 0
		// knuled bar
		for (i = 0; i < max_width; i += 2)
			line[i] = (i <= len) ? 0x7f : 0x41;
	#else
		// segmented bar
		for (i = 0; i < max_width; i += 4)
			for (int k = i - 4; k < i && k < len; k++)
				if (k >= 0)
//					line[k] = (k < (i - 1)) ? 0x7f : 0x00;
					if (k < (i - 1))
						line[k] = 0x7f;
	#endif
}

#ifdef ENABLE_TX_AUDIO_BAR

	bool UI_DisplayAudioBar(const bool now)
	{
		if (g_current_function != FUNCTION_TRANSMIT || g_current_display_screen != DISPLAY_MAIN)
			return false;

		if (g_center_line != CENTER_LINE_NONE && g_center_line != CENTER_LINE_AUDIO_BAR)
			return false;

		if (g_dtmf_call_state != DTMF_CALL_STATE_NONE)
			return false;

		#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
			if (g_alarm_state != ALARM_STATE_OFF)
				return false;
		#endif

		if (g_eeprom.config.setting.mic_bar)
		{
			#ifdef ENABLE_SINGLE_VFO_CHAN
				const unsigned int line  = (single_vfo >= 0 && !pan_enabled) ? 6 : 3;
			#else
				const unsigned int line  = 3;
			#endif
			const unsigned int txt_width = 7 * 3;                 // 3 text chars
			const unsigned int bar_x     = 2 + txt_width + 4;     // X coord of bar graph
			const unsigned int bar_width = LCD_WIDTH - 1 - bar_x;
			const unsigned int secs      = g_tx_timer_tick_500ms / 2;
			uint8_t           *p_line    = g_frame_buffer[line];
			char               s[16];

			// clear the line
			memset(p_line, 0, LCD_WIDTH);

			// TX timeout seconds
			sprintf(s, "%3u", secs);
			#ifdef ENABLE_SMALL_BOLD
				UI_PrintStringSmallBold(s, 2, 0, line);
			#else
				UI_PrintStringSmall(s, 2, 0, line);
			#endif

			{	// TX audio level

				const unsigned int voice_amp  = BK4819_GetVoiceAmplitudeOut();  // 15:0

//				const unsigned int max        = 65535;
//				const unsigned int level      = ((voice_amp * bar_width) + (max / 2)) / max;            // with rounding
//				const unsigned int len        = (level <= bar_width) ? level : bar_width;

				// make non-linear to make more sensitive at low values
				const unsigned int level      = voice_amp * 8;
				const unsigned int sqrt_level = NUMBER_isqrt((level < 65535) ? level : 65535);
				const unsigned int len        = (sqrt_level <= bar_width) ? sqrt_level : bar_width;

				draw_bar(p_line + bar_x, len, bar_width);

				if (now)
					ST7565_BlitFullScreen();
			}
		}

		return true;
	}
#endif

bool UI_DisplayRSSIBar(const int rssi, const unsigned int glitch, const unsigned int noise, const bool now)
{
	if (g_eeprom.config.setting.enable_rssi_bar)
	{
		#ifdef SHOW_RX_TEST_VALUES

			const unsigned int line  = 3;
			char               str[22];

			#ifdef ENABLE_KEYLOCK
				if (g_eeprom.config.setting.key_lock && g_keypad_locked > 0)
					return false;     // display is in use
			#endif

			if (g_current_function == FUNCTION_TRANSMIT || g_current_display_screen != DISPLAY_MAIN || g_dtmf_call_state != DTMF_CALL_STATE_NONE)
				return false;     // display is in use

			if (now)
				memset(g_frame_buffer[line], 0, LCD_WIDTH);

			sprintf(str, "r %3d g %3u n %3u", rssi, glitch, noise);
			UI_PrintStringSmall(str, 2, 0, line);

			if (now)
				ST7565_BlitFullScreen();

			return true;

		#else

			(void)glitch;  // TODO:
			(void)noise;

			//const int          s0_dBm       = -127;                  // S0 .. base level
			const int          s0_dBm       = -147;                  // S0 .. base level

			const int          s9_dBm       = s0_dBm + (6 * 9);      // S9 .. 6dB/S-Point
			const int          bar_max_dBm  = s9_dBm + 80;           // S9+80dB
			const int          bar_min_dBm  = s0_dBm + (6 * 0);      // S0
			//const int          bar_min_dBm  = s0_dBm + (6 * 2);      // S2

			// ************

			const unsigned int txt_width    = 7 * 8;                 // 8 text chars
			const unsigned int bar_x        = 2 + txt_width + 4;     // X coord of bar graph
			const unsigned int bar_width    = LCD_WIDTH - 1 - bar_x;

			const int          rssi_dBm     = (rssi / 2) - 160;
			const int          clamped_dBm  = (rssi_dBm <= bar_min_dBm) ? bar_min_dBm : (rssi_dBm >= bar_max_dBm) ? bar_max_dBm : rssi_dBm;
			const unsigned int bar_range_dB = bar_max_dBm - bar_min_dBm;
			const unsigned int len          = ((clamped_dBm - bar_min_dBm) * bar_width) / bar_range_dB;

			#ifdef ENABLE_SINGLE_VFO_CHAN
				const unsigned int line     = (single_vfo >= 0 && !pan_enabled) ? 6 : 3;
			#else
				const unsigned int line     = 3;
			#endif

			char               s[16];

			#ifdef ENABLE_KEYLOCK
				if (g_eeprom.config.setting.key_lock && g_keypad_locked > 0)
					return false;     // display is in use
			#endif

			if (g_current_function == FUNCTION_TRANSMIT ||
				g_current_display_screen != DISPLAY_MAIN ||
				g_dtmf_call_state != DTMF_CALL_STATE_NONE)
				return false;     // display is in use

			// clear the line
			memset(g_frame_buffer[line], 0, LCD_WIDTH);

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

			draw_bar(g_frame_buffer[line] + bar_x, len, bar_width);

			if (now)
				ST7565_BlitFullScreen();

			return true;

		#endif
	}

	return false;
}

void UI_update_rssi(const int rssi, const unsigned int glitch, const unsigned int noise, const unsigned int vfo)
{
	(void)glitch;
	(void)noise;

	if (g_center_line == CENTER_LINE_RSSI)
	{	// large RSSI dBm, S-point, bar level

		const int rssi_level = (g_tx_vfo->channel_attributes.band < 3) ? rssi + rssi_offset_band_123 : rssi + rssi_offset_band_4567;

		//if (g_current_function == FUNCTION_RECEIVE && g_squelch_open)
		if (g_current_function == FUNCTION_RECEIVE)
			UI_DisplayRSSIBar(rssi_level, glitch, noise, true);
	}

	#ifdef ENABLE_SINGLE_VFO_CHAN
		if (single_vfo >= 0 && !pan_enabled)
			return;
	#endif

	{	// original little RSSI bars

		#ifdef ENABLE_SINGLE_VFO_CHAN
			const unsigned int line   = ((single_vfo >= 0 && !pan_enabled) || vfo > 0) ? 6 : 2;
		#else
			const unsigned int line   = (vfo > 0) ? 6 : 2;
		#endif
		uint8_t           *pline      = g_frame_buffer[line];
		unsigned int       rssi_level = 0;
		int                rssi_cal[7];

		#if 1
		{
			#pragma GCC diagnostic push
			#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
			const uint16_t *cal = (g_tx_vfo->channel_attributes.band < 3) ? g_eeprom.calib.rssi_cal.band_123 : g_eeprom.calib.rssi_cal.band_4567;
			rssi_cal[0] = cal[0];
			rssi_cal[2] = cal[1];
			rssi_cal[4] = cal[2];
			rssi_cal[6] = cal[3];
			#pragma GCC diagnostic pop
		}
		#else
			rssi_cal[0] = (-110 + rssi_dBm_offset) * 2;   // -110 dBm
			rssi_cal[2] = ( -90 + rssi_dBm_offset) * 2;   //  -90 dBm
			rssi_cal[4] = ( -70 + rssi_dBm_offset) * 2;   //  -70 dBm
			rssi_cal[6] = ( -50 + rssi_dBm_offset) * 2;   //  -50 dBm
		#endif
		// linear interpolate the 4 values into 7
		rssi_cal[1] = (rssi_cal[0] + rssi_cal[2]) / 2;
		rssi_cal[3] = (rssi_cal[2] + rssi_cal[4]) / 2;
		rssi_cal[5] = (rssi_cal[4] + rssi_cal[6]) / 2;

		g_vfo_rssi[vfo] = rssi;

		if (rssi >= rssi_cal[6])
			rssi_level = 7;
		else
		if (rssi >= rssi_cal[5])
			rssi_level = 6;
		else
		if (rssi >= rssi_cal[4])
			rssi_level = 5;
		else
		if (rssi >= rssi_cal[3])
			rssi_level = 4;
		else
		if (rssi >= rssi_cal[2])
			rssi_level = 3;
		else
		if (rssi >= rssi_cal[1])
			rssi_level = 2;
		else
		if (rssi >= rssi_cal[0] || g_current_function == FUNCTION_NEW_RECEIVE)
			rssi_level = 1;

		g_vfo_rssi_bar_level[vfo] = rssi_level;

		// **********************************************************

		#ifdef ENABLE_KEYLOCK
			if (g_eeprom.config.setting.key_lock && g_keypad_locked > 0)
				return;    // display is in use
		#endif

		if (g_current_function == FUNCTION_TRANSMIT || g_current_display_screen != DISPLAY_MAIN)
			return;    // display is in use

		memset(pline, 0, 23);

		if (rssi_level == 0)
			pline = NULL;
		else
			draw_small_antenna_bars(pline, rssi_level);

		ST7565_DrawLine(0, 1 + line, 23, pline);
	}
}

// ***************************************************************************

void big_freq(const uint32_t frequency, const unsigned int x, const unsigned int line)
{
	char str[9];

	NUMBER_ToDigits(frequency, str);

	// show the main large frequency digits
	UI_DisplayFrequencyBig(str, x, line, false, false, 6);

	// show the remaining 2 small frequency digits
	#ifdef ENABLE_TRIM_TRAILING_ZEROS
	{
		unsigned int small_num = 2;
		if (str[7] == 0)
		{
			small_num--;
			if (str[6] == 0)
				small_num--;
		}
		UI_Displaysmall_digits(small_num, str + 6, x + 81, line + 1, true);
	}
	#else
		UI_Displaysmall_digits(2, str + 6, x + 81, line + 1, true);
	#endif
}

#ifdef ENABLE_PANADAPTER

	uint8_t bit_reverse_8(uint8_t n)
	{
		n = ((n >> 1) & 0x55) | ((n << 1) & 0xAA);
		n = ((n >> 2) & 0x33) | ((n << 2) & 0xCC);
		n = ((n >> 4) & 0x0F) | ((n << 4) & 0xF0);
		return n;
	}

	void UI_DisplayMain_pan(const bool now)
	{
		const bool         valid     = (g_panadapter_cycles > 0 && !g_monitor_enabled && g_current_function != FUNCTION_TRANSMIT) ? true : false;
		const unsigned int line      = (g_eeprom.config.setting.tx_vfo_num == 0) ? 4 : 0;
		uint8_t           *base_line = g_frame_buffer[line + 2];
		uint8_t            min_rssi;
		uint8_t            span_rssi;
		unsigned int       i;

		if (!g_eeprom.config.setting.panadapter        ||
		    !g_panadapter_enabled                      ||
		    !pan_enabled                               ||
		     g_reduced_service                         ||
		     g_current_display_screen != DISPLAY_MAIN  ||
		     g_current_function == FUNCTION_POWER_SAVE ||
		     g_dtmf_call_state != DTMF_CALL_STATE_NONE ||
		     g_dtmf_input_mode                         ||
		     g_eeprom.config.setting.dual_watch != DUAL_WATCH_OFF)
		{	// don't draw the panadapter
			return;
		}

		// clear our assigned screen area
		memset(g_frame_buffer[line], 0, LCD_WIDTH * 3);

		if (valid)
		{
			// auto vertical scale
			min_rssi  = g_panadapter_min_rssi;
			span_rssi = g_panadapter_max_rssi - min_rssi;
			if (span_rssi < 30)
				span_rssi = 30;
			if (min_rssi > (255 - span_rssi))
				min_rssi =  255 - span_rssi;

			#if 0
				{	// show the min/max RSSI values
					char str[16];
					sprintf(str, "%u", min_rssi);
					UI_PrintStringSmall(str, 2, 0, line + 0);
					sprintf(str, "%u", span_rssi);
					UI_PrintStringSmall(str, LCD_WIDTH - 2 - (7 * strlen(str)), 0, line + 0);
				}
			#endif

			#ifdef ENABLE_PANADAPTER_PEAK_FREQ
				if (g_panadapter_peak_freq > 0)
				{	// print the peak frequency
					char str[16];
					sprintf(str, "%u.%05u", g_panadapter_peak_freq / 100000, g_panadapter_peak_freq % 100000);
					NUMBER_trim_trailing_zeros(str);
					UI_PrintStringSmall(str, 2 + (7 * 4), 0, line + 0);
				}
			#endif
		}

		{	// draw top & bottom horizontal dotted line
			const unsigned int top = PANADAPTER_BINS - (LCD_WIDTH * 2);
			const unsigned int bot = PANADAPTER_BINS - (LCD_WIDTH * 0);
			for (i = 0; i < PANADAPTER_BINS; i += 4)
			{
				// top line
				if (i <= 4)
				{
					base_line[top - i] |= 1u << 0;
					base_line[top + i] |= 1u << 0;
				}
				// bottom line
				base_line[bot - i] |= 1u << 6;
				base_line[bot + i] |= 1u << 6;
			}
		}

		// draw top center vertical marker (the VFO frequency)
		base_line[PANADAPTER_BINS - (LCD_WIDTH * 2)] = 0x15;

		// draw the panadapter vertical bins
		if (valid)
		{
			for (i = 0; i < ARRAY_SIZE(g_panadapter_rssi); i++)
			{
				uint32_t pixels;
				uint8_t  rssi = g_panadapter_rssi[i];

				#if 0
					rssi = (rssi < ((-129 + 160) * 2)) ? 0 : rssi - ((-129 + 160) * 2);  // min of -129dBm (S3)
					rssi = rssi >> 2;
				#else
					rssi = ((uint16_t)(rssi - min_rssi) * 22) / span_rssi;  // 0 ~ 22
				#endif

				rssi += 2;                  // offset from the bottom
				if (rssi > 24)
					rssi = 24;              // limit peak value

				pixels = (1u << rssi) - 1;  // pixels
				pixels &= 0xfffffffe;       // clear the bottom line

				base_line[i - (LCD_WIDTH * 2)] |= bit_reverse_8(pixels >> 16);
				base_line[i - (LCD_WIDTH * 1)] |= bit_reverse_8(pixels >>  8);
				base_line[i - (LCD_WIDTH * 0)] |= bit_reverse_8(pixels >>  0);
			}
		}

		if (now)
			ST7565_BlitFullScreen();
	}
#endif

void UI_DisplayCenterLine(void)
{
//	const bool rx = (g_current_function == FUNCTION_RECEIVE && g_squelch_open) ? true : false;
	const bool rx = (g_current_function == FUNCTION_RECEIVE) ? true : false;

	#ifdef ENABLE_SINGLE_VFO_CHAN
		const unsigned int line = (single_vfo >= 0 && !pan_enabled) ? 6 : 3;
	#else
		const unsigned int line = 3;
	#endif

	(void)rx;
	(void)line;

	if (g_center_line != CENTER_LINE_NONE ||
	    g_current_display_screen != DISPLAY_MAIN ||
		g_dtmf_call_state != DTMF_CALL_STATE_NONE)
	{
		return;
	}

	// we're free to use the middle line

	#ifdef ENABLE_TX_AUDIO_BAR
		// show the TX audio level
		if (UI_DisplayAudioBar(false))
		{
			g_center_line = CENTER_LINE_AUDIO_BAR;
		}
		else
	#endif

	#ifdef ENABLE_MDC1200
		if (mdc1200_rx_ready_tick_500ms > 0)
		{
			g_center_line = CENTER_LINE_MDC1200;
			#ifdef ENABLE_MDC1200_SHOW_OP_ARG
				sprintf(str, "MDC1200 %02X %02X %04X", mdc1200_op, mdc1200_arg, mdc1200_unit_id);
			#else
				sprintf(str, "MDC1200 ID %04X", mdc1200_unit_id);
			#endif
			#ifdef ENABLE_SMALL_BOLD
				UI_PrintStringSmallBold(str, 2, 0, line);
			#else
				UI_PrintStringSmall(str, 2, 0, line);
			#endif
		}
		else
	#endif

	#if defined(ENABLE_AM_FIX) && defined(ENABLE_AM_FIX_SHOW_DATA)
		// show the AM-FIX debug data
		if (rx && g_vfo_info[g_rx_vfo_num].mod_mode != MOD_MODE_FM && g_eeprom.config.setting.am_fix)
		{
			g_center_line = CENTER_LINE_AM_FIX_DATA;
			AM_fix_print_data(g_rx_vfo_num, str);
			UI_PrintStringSmall(str, 2, 0, line);
		}
		else
	#endif

	// show the RX RSSI dBm, S-point and signal strength bar graph
	if (rx && g_eeprom.config.setting.enable_rssi_bar)
	{
		const int rssi_level = (g_tx_vfo->channel_attributes.band < 3) ? g_current_rssi[g_rx_vfo_num] + rssi_offset_band_123 : g_current_rssi[g_rx_vfo_num] + rssi_offset_band_4567;
		g_center_line = CENTER_LINE_RSSI;
		UI_DisplayRSSIBar(rssi_level, g_current_glitch[g_rx_vfo_num], g_current_noise[g_rx_vfo_num], false);
	}
	else

	if (rx || g_current_function == FUNCTION_FOREGROUND || g_current_function == FUNCTION_POWER_SAVE)
	{
		#ifdef ENABLE_DTMF_LIVE_DECODER
			#if 1
				if (g_eeprom.config.setting.dtmf_live_decoder && g_dtmf_rx_live[0] != 0)
				{	// show live DTMF decode
					const unsigned int len = strlen(g_dtmf_rx_live);
					const unsigned int idx = (len > (17 - 5)) ? len - (17 - 5) : 0;  // limit to last 'n' chars

					if (g_current_display_screen != DISPLAY_MAIN || g_dtmf_call_state != DTMF_CALL_STATE_NONE)
						return;

					g_center_line = CENTER_LINE_DTMF_DEC;

					strcpy(str, "DTMF ");
					strcat(str, g_dtmf_rx_live + idx);
					UI_PrintStringSmall(str, 2, 0, line);
				}
			#else
				if (g_eeprom.config.setting.dtmf_live_decoder && g_dtmf_rx_index > 0)
				{	// show live DTMF decode
					const unsigned int len = g_dtmf_rx_index;
					const unsigned int idx = (len > (17 - 5)) ? len - (17 - 5) : 0;  // limit to last 'n' chars

					if (g_current_display_screen != DISPLAY_MAIN || g_dtmf_call_state != DTMF_CALL_STATE_NONE)
						return;

					g_center_line = CENTER_LINE_DTMF_DEC;

					strcpy(str, "DTMF ");
					strcat(str, g_dtmf_rx + idx);
					UI_PrintStringSmall(str, 2, 0, line);
				}
			#endif
		#endif

		#ifdef ENABLE_SHOW_CHARGE_LEVEL
			else
			if (g_charging_with_type_c)
			{	// show the battery charge state
				if (g_current_display_screen != DISPLAY_MAIN || g_dtmf_call_state != DTMF_CALL_STATE_NONE)
					return;

				g_center_line = CENTER_LINE_CHARGE_DATA;

				sprintf(str, "Charge %u.%02uV %u%%",
					g_battery_voltage_average / 100, g_battery_voltage_average % 100,
					BATTERY_VoltsToPercent(g_battery_voltage_average));
				UI_PrintStringSmall(str, 2, 0, line);
			}
		#endif
	}
}

const char *state_list[] = {"", "BUSY", "BAT LOW", "TX DISABLE", "TIMEOUT", "ALARM", "VOLT HIGH"};

#ifdef ENABLE_SINGLE_VFO_CHAN
	void UI_DisplayMainSingle(void)
	{
		const int          vfo_num   = g_eeprom.config.setting.tx_vfo_num;
		const unsigned int scrn_chan = g_eeprom.config.setting.indices.vfo[vfo_num].screen;
		const unsigned int state     = g_vfo_state[vfo_num];
		uint8_t           *p_line1   = g_frame_buffer[1];
		char               str[22];

		#ifdef ENABLE_ALARM
			if (g_current_function == FUNCTION_TRANSMIT && g_alarm_state == ALARM_STATE_ALARM)
				state = VFO_STATE_ALARM;
		#endif

		// ********************

		{	// top line

			unsigned int y = 0;
			unsigned int x = 0;

			#ifdef ENABLE_KILL_REVIVE
				if (g_eeprom.config.setting.radio_disabled)
				{
					#ifdef ENABLE_SMALL_BOLD
						UI_PrintStringSmallBold("DISABLED", x + 10, 0, y);
					#else
						UI_PrintStringSmall("DISABLED", x + 10, 0, y);
					#endif
				}
				else
			#endif
			{
				{	// VFO number
					sprintf(str, "VFO%d", 1 + vfo_num);
					UI_PrintStringSmall(str, x, 0, y);
				}

				x += 7 * 5;

				if (state == VFO_STATE_NORMAL)
				{	// frequency band/channel number

					if (scrn_chan <= USER_CHANNEL_LAST)
					{	// channel mode
						const bool inputting = (g_input_box_index == 0 || g_eeprom.config.setting.tx_vfo_num != vfo_num) ? false : true;
						if (!inputting)
							NUMBER_ToDigits(1 + scrn_chan, str);  // show the memory channel number
						else
							memcpy(str + 5, g_input_box, 3);      // show the input text
						UI_PrintStringSmall("M", x, 0, y);
						UI_Displaysmall_digits(3, str + 5, x + 7, y, inputting);
					}
					else
					if (IS_FREQ_CHANNEL(scrn_chan))
					{	// frequency mode
						sprintf(str, "F%u", 1 + scrn_chan - FREQ_CHANNEL_FIRST);
						UI_PrintStringSmall(str, x, 0, y);
					}
					#ifdef ENABLE_NOAA
						else
						{
							if (g_input_box_index == 0 || g_eeprom.config.setting.tx_vfo_num != vfo_num)
							{	// channel number
								sprintf(str, "N%u", 1 + scrn_chan - NOAA_CHANNEL_FIRST);
							}
							else
							{	// user entering channel number
								sprintf(str, "N%u%u", '0' + g_input_box[0], '0' + g_input_box[1]);
							}
							UI_PrintStringSmall(str, x, 0, y);
						}
					#endif

					x += 7 * 5;

					{	// RX or TX or state message
						str[0] = 0;
						if (g_current_function == FUNCTION_TRANSMIT)
							strcpy(str, "TX");
						else
						if (g_current_function == FUNCTION_RECEIVE && g_squelch_open)
							strcpy(str, "RX");
						#ifdef ENABLE_SMALL_BOLD
							UI_PrintStringSmallBold(str, x, 0, y);
						#else
							UI_PrintStringSmall(str, x, 0, y);
						#endif
					}

					x += 7 * 3;

					#if 1  // not quite enough room to fit this in :(
					{	// step size
						const uint32_t step = g_vfo_info[vfo_num].step_freq * 10;
						if (step < 1000)
						{	// Hz
							sprintf(str, "%u", step);
						}
						else
						{	// kHz
							sprintf(str, "%u.%03u", step / 1000, step % 1000);
							NUMBER_trim_trailing_zeros(str);
							strcat(str, "k");
						}
						UI_PrintStringSmall(str, x, 0, y);
					}
					#endif
				}
				else
				{
					#ifdef ENABLE_SMALL_BOLD
						UI_PrintStringSmallBold(state_list[state], x, 0, y);
					#else
						UI_PrintStringSmall(state_list[state], x, 0, y);
					#endif
				}

			}
		}

		// ********************

		{
			unsigned int y = 1;
			unsigned int x = 6;

			if (g_input_box_index > 0 && IS_FREQ_CHANNEL(scrn_chan) && g_eeprom.config.setting.tx_vfo_num == vfo_num)
			{	// user is entering a frequency
				UI_DisplayFrequency(g_input_box, x, y, true, 8);
			}
			else
			{
				const uint32_t frequency = (g_current_function == FUNCTION_TRANSMIT) ? g_vfo_info[vfo_num].p_tx->frequency : g_vfo_info[vfo_num].p_rx->frequency;

				#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//					UART_printf("%u.%05u MHz\n", frequency / 100000, frequency % 100000);
				#endif

				if (scrn_chan <= USER_CHANNEL_LAST)
				{	// a user channel

					// channel name
					SETTINGS_fetch_channel_name(str, scrn_chan);
					if (str[0] == 0)
						sprintf(str, "CH %u", scrn_chan);
					UI_PrintString(str, x, 0, y, 8);

					// frequency
					sprintf(str, "%u.%05u", frequency / 100000, frequency % 100000);
					#ifdef ENABLE_TRIM_TRAILING_ZEROS
						NUMBER_trim_trailing_zeros(str);
					#endif
					UI_PrintString(str, x, 0, y + 2, 8);
				}
				else
//				if (IS_FREQ_CHANNEL(scrn_chan))
				{	// frequency mode
					#ifdef ENABLE_BIG_FREQ
						big_freq(frequency, x, y);
					#else

						#ifdef ENABLE_SHOW_FREQS_CHAN
							const unsigned int chan = g_vfo_info[vfo_num].freq_in_channel;
						#endif

						//sprintf(str, "%03u.%05u", frequency / 100000, frequency % 100000);
						sprintf(str, "%u.%05u", frequency / 100000, frequency % 100000);
						#ifdef ENABLE_TRIM_TRAILING_ZEROS
							NUMBER_trim_trailing_zeros(str);
						#endif
/*
						#ifdef ENABLE_SHOW_FREQS_CHAN
							//g_vfo_info[vfo_num].freq_in_channel = SETTINGS_find_channel(frequency);
							if (chan <= USER_CHANNEL_LAST)
							{	// the frequency has a channel - show the channel name below the frequency

								// frequency
								#ifdef ENABLE_SMALL_BOLD
									UI_PrintStringSmallBold(str, x + 4, 0, line + 0);
								#else
									UI_PrintStringSmall(str, x + 4, 0, y);
								#endif

								// channel name, if not then channel number
								SETTINGS_fetch_channel_name(str, chan);
								if (str[0] == 0)
									//sprintf(str, "CH-%03u", 1 + chan);
									sprintf(str, "CH-%u", 1 + chan);
								UI_PrintStringSmall(str, x + 4, 0, y + 1);
							}
							else
						#endif
*/						{	// show the frequency in the main font
//							UI_PrintString(str, x, 0, y, 8);
							UI_PrintString(str, x, 0, y + 1, 8);
						}
					#endif
				}

				// channel symbols

				if (scrn_chan <= USER_CHANNEL_LAST)
				//if (IS_NOT_NOAA_CHANNEL(scrn_chan))
				{	// it's a user channel or VFO

					unsigned int x = LCD_WIDTH - 1 - sizeof(BITMAP_SCANLIST2) - sizeof(BITMAP_SCANLIST1);

					if (g_vfo_info[vfo_num].channel_attributes.scanlist1)
						memcpy(p_line1 + x, BITMAP_SCANLIST1, sizeof(BITMAP_SCANLIST1));
					x += sizeof(BITMAP_SCANLIST1);

					if (g_vfo_info[vfo_num].channel_attributes.scanlist2)
						memcpy(p_line1 + x, BITMAP_SCANLIST2, sizeof(BITMAP_SCANLIST2));
					//x += sizeof(BITMAP_SCANLIST2);
				}

				{
					#ifdef ENABLE_SHOW_FREQS_CHAN
						strcpy(str, "  ");

						#ifdef ENABLE_SCAN_IGNORE_LIST
							if (FI_freq_ignored(frequency) >= 0)
								str[0] = 'I';  // frequency is in the ignore list
						#endif

						if (g_vfo_info[vfo_num].channel.compand != COMPAND_OFF)
							str[1] = 'C';  // compander is enabled

						UI_PrintStringSmall(str, LCD_WIDTH - (7 * 2), 0, y + 1);
					#else
						const bool is_freq_chan       = IS_FREQ_CHANNEL(scrn_chan);
						const uint8_t freq_in_channel = g_vfo_info[vfo_num].freq_in_channel;
//						const uint8_t freq_in_channel = SETTINGS_find_channel(frequency);  // was way to slow

						strcpy(str, "   ");

						#ifdef ENABLE_SCAN_IGNORE_LIST
							if (FI_freq_ignored(frequency) >= 0)
								str[0] = 'I';  // frequency is in the ignore list
						#endif

						if (is_freq_chan && freq_in_channel <= USER_CHANNEL_LAST)
						{	// this VFO frequency is also found in a channel
							str[1] = 'F';


							// TODO: show the channel name this frequency is found in


						}

						if (g_vfo_info[vfo_num].channel.compand != COMPAND_OFF)
							str[2] = 'C';  // compander is enabled

						UI_PrintStringSmall(str, LCD_WIDTH - (7 * 3), 0, y + 1);
					#endif
				}
			}

			y += 2;
			x = LCD_WIDTH - (7 * 6);

			{	// audio scramble symbol
				if (g_vfo_info[vfo_num].channel.scrambler > 0 && g_eeprom.config.setting.enable_scrambler)
					UI_PrintStringSmall("SCR", x, 0, y);
			}

			{	// modulation mode
				const char *mode_list[] = {"FM", "AM", "SB", ""};
				strcpy(str, mode_list[g_vfo_info[vfo_num].channel.mod_mode]);
				UI_PrintStringSmall(str, x + (7 * 4), 0, y);
			}

			y++;
			x = LCD_WIDTH - (7 * 5);

			{	// CTCSS/CDCSS code
				str[0] = 0;
				if (g_vfo_info[vfo_num].channel.mod_mode == MOD_MODE_FM)
				{	// show the CTCSS/CDCSS symbol
					const freq_config_t *pConfig   = (g_current_function == FUNCTION_TRANSMIT) ? g_vfo_info[vfo_num].p_tx : g_vfo_info[vfo_num].p_rx;
					const unsigned int   code_type = pConfig->code_type;
					unsigned int         code      = pConfig->code;
					switch (code_type)
					{
						case CODE_TYPE_NONE:
							//str[0] = 0;
							break;
						case CODE_TYPE_CONTINUOUS_TONE:
							sprintf(str, "%3u.%u", CTCSS_TONE_LIST[code] / 10, CTCSS_TONE_LIST[code] % 10);
							break;
						case CODE_TYPE_DIGITAL:
						case CODE_TYPE_REVERSE_DIGITAL:
							sprintf(str, "D%03o%c", DCS_CODE_LIST[code], (code_type == CODE_TYPE_DIGITAL) ? 'N' : 'I');
							break;
					}
				}
				UI_PrintStringSmall(str, x, 0, y);
			}

			// ***************************************

			x = 2;
			y++;

			#ifdef ENABLE_TX_WHEN_AM
				if (state == VFO_STATE_NORMAL || state == VFO_STATE_ALARM)
			#else
				if ((state == VFO_STATE_NORMAL || state == VFO_STATE_ALARM) && g_vfo_info[vfo_num].channel.mod_mode == MOD_MODE_FM) // TX allowed only when FM
			#endif
			{
				if (FREQUENCY_tx_freq_check(g_vfo_info[vfo_num].p_tx->frequency) == 0)
				{	// TX power
					const char *pwr_list[] = {"LOW", "MID", "HIGH", "U"};
					const unsigned int i = g_vfo_info[vfo_num].channel.tx_power;
					strcpy(str, pwr_list[i]);
					if (i == OUTPUT_POWER_USER)
						sprintf(str + strlen(str), "%03u", g_tx_vfo->channel.tx_power_user);
					UI_PrintStringSmall(str, x, 0, y);

					if (g_vfo_info[vfo_num].freq_config_rx.frequency != g_vfo_info[vfo_num].freq_config_tx.frequency)
					{	// TX offset symbol
						const char *dir_list[] = {"", "+", "-"};
						const unsigned int i = g_vfo_info[vfo_num].channel.tx_offset_dir;
						UI_PrintStringSmall(dir_list[i], x + (7 * 5), 0, y);
					}
				}
			}

			{	// TX/RX reverse symbol
				x += 7 * 7;
				if (g_vfo_info[vfo_num].channel.frequency_reverse)
					UI_PrintStringSmall("R", x, 0, y);
			}

			{	// wide/narrow band symbol
				x += 7 * 2;
				strcpy(str, " ");
				if (g_vfo_info[vfo_num].channel.channel_bandwidth == BANDWIDTH_WIDE)
					str[0] = 'W';
				else
				if (g_vfo_info[vfo_num].channel.channel_bandwidth == BANDWIDTH_NARROW)
					str[0] = 'N';
				UI_PrintStringSmall(str, x, 0, y);
			}

			{	// DTMF decoding symbol
				str[0] = 0;
				if (g_vfo_info[vfo_num].channel.dtmf_decoding_enable)
					strcpy(str, "DTMF");
				x += 7 * 2;
				UI_PrintStringSmall(str, x, 0, y);
			}
		}

		UI_DisplayCenterLine();

		ST7565_BlitFullScreen();
	}
#endif

void UI_DisplayMain(void)
{
	#if !defined(ENABLE_BIG_FREQ) && defined(ENABLE_SMALLEST_FONT)
		const unsigned int smallest_char_spacing = ARRAY_SIZE(g_font3x5[0]) + 1;
	#endif
	const unsigned int line0           = 0;  // text screen line
	const unsigned int line1           = 4;
	int                main_vfo_num    = g_eeprom.config.setting.tx_vfo_num;
	int                current_vfo_num = g_eeprom.config.setting.tx_vfo_num;
	char               str[22];
	int                vfo_num;

	g_center_line = CENTER_LINE_NONE;

	pan_enabled = false;
	single_vfo  = -1;

	if (g_eeprom.config.setting.dual_watch == DUAL_WATCH_OFF && g_eeprom.config.setting.cross_vfo == CROSS_BAND_OFF)
	{
		single_vfo = main_vfo_num;
	}
	else
	if (g_eeprom.config.setting.dual_watch != DUAL_WATCH_OFF && g_rx_vfo_is_active)
		current_vfo_num = g_rx_vfo_num;

	// clear the screen buffer
	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	#if defined(ENABLE_UART)
		if (g_serial_config_tick_500ms > 0)
		{	// tell user the serial comms is in use
			BACKLIGHT_turn_on(5);		// 5 seconds
			UI_PrintString("UART", 0, LCD_WIDTH, 1, 8);
			UI_PrintString("CONFIG COMMS", 0, LCD_WIDTH, 3, 8);
			ST7565_BlitFullScreen();
			g_center_line = CENTER_LINE_IN_USE;
			return;
		}
	#endif

	#ifdef ENABLE_KEYLOCK
		if (g_eeprom.config.setting.key_lock && g_keypad_locked > 0)
		{	// tell user how to unlock the keyboard
			BACKLIGHT_turn_on(5);     // 5 seconds
			UI_PrintString("Long press #", 0, LCD_WIDTH, 1, 8);
			UI_PrintString("to unlock",    0, LCD_WIDTH, 3, 8);
			ST7565_BlitFullScreen();
			g_center_line = CENTER_LINE_IN_USE;
			return;
		}
	#endif

	#ifdef ENABLE_PANADAPTER
		if (g_eeprom.config.setting.panadapter && g_panadapter_enabled && single_vfo >= 0)
			pan_enabled = true;
		#ifndef ENABLE_SINGLE_VFO_CHAN
			else
				single_vfo = -1;
		#endif
	#endif

	#if ENABLE_SINGLE_VFO_CHAN
		if (g_dtmf_input_mode)
			single_vfo = -1;

		#ifdef ENABLE_PANADAPTER
			if (!pan_enabled)
		#endif
			{
				if (single_vfo >= 0)
				{
					UI_DisplayMainSingle();
					return;
				}
			}
	#endif

	for (vfo_num = 0; vfo_num < 2; vfo_num++)
	{
		const unsigned int scrn_chan = g_eeprom.config.setting.indices.vfo[vfo_num].screen;
		const unsigned int line      = (vfo_num == 0) ? line0 : line1;
		uint8_t           *p_line0   = g_frame_buffer[line + 0];
		uint8_t           *p_line1   = g_frame_buffer[line + 1];
		unsigned int       mode      = 0;
		unsigned int       state;

		if (single_vfo >= 0 && single_vfo != vfo_num)
			continue;	// we're in single VFO mode - screen is dedicated to just one VFO

		if (current_vfo_num != vfo_num)
		{
			if (g_dtmf_call_state != DTMF_CALL_STATE_NONE || g_dtmf_is_tx || g_dtmf_input_mode)
			{	// show DTMF stuff

				char contact[17];

				if (!g_dtmf_input_mode)
				{
					memset(contact, 0, sizeof(contact));
					if (g_dtmf_call_state == DTMF_CALL_STATE_CALL_OUT)
					{
						strcpy(str, (g_dtmf_state == DTMF_STATE_CALL_OUT_RSP) ? "CALL OUT RESP" : "CALL OUT");
					}
					else
					if (g_dtmf_call_state == DTMF_CALL_STATE_RECEIVED || g_dtmf_call_state == DTMF_CALL_STATE_RECEIVED_STAY)
					{
						const bool found = DTMF_FindContact(g_dtmf_caller, contact);
						contact[8] = 0;
						sprintf(str, "FROM %s", found ? contact : g_dtmf_caller);
					}
					else
					if (g_dtmf_is_tx)
					{
						strcpy(str, (g_dtmf_state == DTMF_STATE_TX_SUCC) ? "DTMF TX SUCC" : "DTMF TX");
					}
				}
				else
				{
					sprintf(str, "DTMF entry");
				}
				str[16] = 0;
				UI_PrintString(str, 2, 0, 0 + (vfo_num * 3), 8);

				memset(str,  0, sizeof(str));
				if (!g_dtmf_input_mode)
				{
					memset(contact, 0, sizeof(contact));
					if (g_dtmf_call_state == DTMF_CALL_STATE_CALL_OUT)
					{
						const bool found = DTMF_FindContact(g_dtmf_string, contact);
						contact[15] = 0;
						sprintf(str, ">%s", found ? contact : g_dtmf_string);
					}
					else
					if (g_dtmf_call_state == DTMF_CALL_STATE_RECEIVED || g_dtmf_call_state == DTMF_CALL_STATE_RECEIVED_STAY)
					{
						const bool found = DTMF_FindContact(g_dtmf_callee, contact);
						contact[15] = 0;
						sprintf(str, ">%s", found ? contact : g_dtmf_callee);
					}
					else
					if (g_dtmf_is_tx)
					{
						sprintf(str, ">%s", g_dtmf_string);
					}
				}
				else
				{
					sprintf(str, ">%s", g_dtmf_input_box);
				}
				str[16] = 0;
				UI_PrintString(str, 2, 0, 2 + (vfo_num * 3), 8);

				pan_enabled = false;

				g_center_line = CENTER_LINE_IN_USE;
				continue;
			}
		}

		if (single_vfo < 0)
		{
			if (vfo_num == main_vfo_num)
				memcpy(p_line0, BITMAP_VFO_DEFAULT, sizeof(BITMAP_VFO_DEFAULT));
			else
			if (g_eeprom.config.setting.cross_vfo != CROSS_BAND_OFF || vfo_num == current_vfo_num)
				memcpy(p_line0, BITMAP_VFO_NOT_DEFAULT, sizeof(BITMAP_VFO_NOT_DEFAULT));
		}

		if (g_current_function == FUNCTION_TRANSMIT)
		{	// transmitting
			#ifdef ENABLE_ALARM
				if (g_alarm_state == ALARM_STATE_ALARM)
					mode = 1;
				else
			#endif
			{
				current_vfo_num = (g_eeprom.config.setting.cross_vfo == CROSS_BAND_OFF) ? g_rx_vfo_num : g_eeprom.config.setting.tx_vfo_num;
				if (current_vfo_num == vfo_num)
				{	// show the TX symbol
					mode = 1;
					const int x = 14;
					#ifdef ENABLE_SMALL_BOLD
						UI_PrintStringSmallBold("TX", x, 0, line);
					#else
						UI_PrintStringSmall("TX", x, 0, line);
					#endif
				}
			}
		}
		else
		{	// receiving .. show the RX symbol
			mode = 2;
			if ((g_current_function == FUNCTION_RECEIVE && g_squelch_open) && g_rx_vfo_num == vfo_num)
			{
				const int x = 14;
				#ifdef ENABLE_SMALL_BOLD
					UI_PrintStringSmallBold("RX", x, 0, line);
				#else
					UI_PrintStringSmall("RX", x, 0, line);
				#endif
			}
		}

		if (scrn_chan <= USER_CHANNEL_LAST)
		{	// channel mode
			const unsigned int x = 2;
			const bool inputting = (g_input_box_index == 0 || g_eeprom.config.setting.tx_vfo_num != vfo_num) ? false : true;
			if (!inputting)
				NUMBER_ToDigits(scrn_chan + 1, str);  // show the memory channel number
			else
				memcpy(str + 5, g_input_box, 3);      // show the input text
			UI_PrintStringSmall("M", x, 0, line + 1);
			UI_Displaysmall_digits(3, str + 5, x + 7, line + 1, inputting);
		}
		else
		if (IS_FREQ_CHANNEL(scrn_chan))
		{	// frequency mode
			// show the frequency band number
			const unsigned int x = 2;	// was 14
			sprintf(str, "F%u", 1 + scrn_chan - FREQ_CHANNEL_FIRST);
			UI_PrintStringSmall(str, x, 0, line + 1);
		}
		#ifdef ENABLE_NOAA
			else
			{
				const int x = 7;
				if (g_input_box_index == 0 || g_eeprom.config.setting.tx_vfo_num != vfo_num)
				{	// channel number
					sprintf(str, "N%u", 1 + scrn_chan - NOAA_CHANNEL_FIRST);
				}
				else
				{	// user entering channel number
					sprintf(str, "N%u%u", '0' + g_input_box[0], '0' + g_input_box[1]);
				}
				UI_PrintStringSmall(str, x, 0, line + 1);
			}
		#endif

		// ************

		state = g_vfo_state[vfo_num];

		#ifdef ENABLE_ALARM
			if (g_current_function == FUNCTION_TRANSMIT && g_alarm_state == ALARM_STATE_ALARM)
			{
				channel = (g_eeprom.config.setting.cross_vfo == CROSS_BAND_OFF) ? g_rx_vfo_num : g_eeprom.config.setting.tx_vfo_num;
				if (channel == vfo_num)
					state = VFO_STATE_ALARM;
			}
		#endif

		{
			const unsigned int x = 32;

			if (state != VFO_STATE_NORMAL)
			{
				if (state < ARRAY_SIZE(state_list))
					UI_PrintString(state_list[state], x - 1, 0, line, 8);
			}
			else
			if (g_input_box_index > 0 && IS_FREQ_CHANNEL(scrn_chan) && g_eeprom.config.setting.tx_vfo_num == vfo_num)
			{	// user is entering a frequency
//				UI_DisplayFrequencyBig(g_input_box, x, line, true, false, 6);
//				UI_DisplayFrequencyBig(g_input_box, x, line, true, false, 7);
				UI_DisplayFrequency(g_input_box, x, line, true, 8);
//				g_center_line = CENTER_LINE_IN_USE;
			}
			else
			{
				uint32_t frequency = g_vfo_info[vfo_num].p_rx->frequency;

				if (g_current_function == FUNCTION_TRANSMIT)
				{	// transmitting
					current_vfo_num = (g_eeprom.config.setting.cross_vfo == CROSS_BAND_OFF) ? g_rx_vfo_num : g_eeprom.config.setting.tx_vfo_num;
					if (current_vfo_num == vfo_num)
						frequency = g_vfo_info[vfo_num].p_tx->frequency;
				}

				if (scrn_chan <= USER_CHANNEL_LAST)
				{	// a user channel

					switch (g_eeprom.config.setting.channel_display_mode)
					{
						case MDF_FREQUENCY:	// just channel frequency

							#ifdef ENABLE_BIG_FREQ
								big_freq(frequency, x, line);
							#else
								// show the frequency in the main font
								sprintf(str, "%03u.%05u", frequency / 100000, frequency % 100000);
								#ifdef ENABLE_TRIM_TRAILING_ZEROS
									NUMBER_trim_trailing_zeros(str);
								#endif
								UI_PrintString(str, x, 0, line, 8);
							#endif

							break;

						case MDF_CHANNEL:	// just channel number

							sprintf(str, "CH-%03u", scrn_chan + 1);
							UI_PrintString(str, x, 0, line, 8);

							break;

						case MDF_NAME:		// channel name
						case MDF_NAME_FREQ:	// channel name and frequency

							SETTINGS_fetch_channel_name(str, scrn_chan);
							if (str[0] == 0)
							{	// no channel name, use channel number
								sprintf(str, "CH-%u", 1 + scrn_chan);
							}

							if (g_eeprom.config.setting.channel_display_mode == MDF_NAME)
							{	// just the name
								UI_PrintString(str, x + 4, 0, line, 8);
							}
							else
							{	// name & frequency

								// name
								#ifdef ENABLE_SMALL_BOLD
									UI_PrintStringSmallBold(str, x + 4, 0, line + 0);
								#else
									UI_PrintStringSmall(str, x + 4, 0, line + 0);
								#endif

								// frequency
//								sprintf(str, "%03u.%05u", frequency / 100000, frequency % 100000);
								sprintf(str, "%u.%05u", frequency / 100000, frequency % 100000);
								#ifdef ENABLE_TRIM_TRAILING_ZEROS
									NUMBER_trim_trailing_zeros(str);
								#endif
								UI_PrintStringSmall(str, x + 4, 0, line + 1);
							}

							break;
					}
				}
				else
//				if (IS_FREQ_CHANNEL(scrn_chan))
				{	// frequency mode
					#ifdef ENABLE_BIG_FREQ
						big_freq(frequency, x, line);
					#else

						#ifdef ENABLE_SHOW_FREQS_CHAN
							const unsigned int chan = g_vfo_info[vfo_num].freq_in_channel;
						#endif

//						sprintf(str, "%03u.%05u", frequency / 100000, frequency % 100000);
						sprintf(str, "%u.%05u", frequency / 100000, frequency % 100000);
						#ifdef ENABLE_TRIM_TRAILING_ZEROS
							NUMBER_trim_trailing_zeros(str);
						#endif

						#ifdef ENABLE_SHOW_FREQS_CHAN
							//g_vfo_info[vfo_num].freq_in_channel = SETTINGS_find_channel(frequency);
							if (chan <= USER_CHANNEL_LAST)
							{	// the frequency has a channel - show the channel name below the frequency

								// frequency
								#ifdef ENABLE_SMALL_BOLD
									UI_PrintStringSmallBold(str, x + 4, 0, line + 0);
								#else
									UI_PrintStringSmall(str, x + 4, 0, line + 0);
								#endif

								// channel name, if not then channel number
								SETTINGS_fetch_channel_name(str, chan);
								if (str[0] == 0)
//									sprintf(str, "CH-%03u", 1 + chan);
									sprintf(str, "CH-%u", 1 + chan);
								UI_PrintStringSmall(str, x + 4, 0, line + 1);
							}
							else
						#endif
						{	// show the frequency in the main font
							UI_PrintString(str, x, 0, line, 8);
						}

					#endif
				}

				// show channel symbols

				if (scrn_chan <= USER_CHANNEL_LAST)
				//if (IS_NOT_NOAA_CHANNEL(scrn_chan))
				{	// it's a user channel or VFO

					unsigned int x = LCD_WIDTH - 1 - sizeof(BITMAP_SCANLIST2) - sizeof(BITMAP_SCANLIST1);

					if (g_vfo_info[vfo_num].channel_attributes.scanlist1)
						memcpy(p_line0 + x, BITMAP_SCANLIST1, sizeof(BITMAP_SCANLIST1));
					x += sizeof(BITMAP_SCANLIST1);

					if (g_vfo_info[vfo_num].channel_attributes.scanlist2)
						memcpy(p_line0 + x, BITMAP_SCANLIST2, sizeof(BITMAP_SCANLIST2));
					//x += sizeof(BITMAP_SCANLIST2);
				}

				#ifdef ENABLE_BIG_FREQ

					// no room for these symbols

				#elif defined(ENABLE_SMALLEST_FONT)
				{
					unsigned int x = LCD_WIDTH + LCD_WIDTH - 1 - (smallest_char_spacing * 1) - (smallest_char_spacing * 4);

					if (IS_FREQ_CHANNEL(scrn_chan))
					{
						//g_vfo_info[vfo_num].freq_in_channel = SETTINGS_find_channel(frequency);
						if (g_vfo_info[vfo_num].freq_in_channel <= USER_CHANNEL_LAST)
						{	// the channel number that contains this VFO frequency
							sprintf(str, "%03u", 1 + g_vfo_info[vfo_num].freq_in_channel);
							UI_PrintStringSmallest(str, x, (line + 0) * 8, false, true);
						}
					}
					x += smallest_char_spacing * 4;

					if (g_vfo_info[vfo_num].channel.compand != COMPAND_OFF)
						UI_PrintStringSmallest("C", x, (line + 0) * 8, false, true);
					//x += smallest_char_spacing * 1;
				}
				#else
				{
					#ifdef ENABLE_SHOW_FREQS_CHAN
						strcpy(str, "  ");

						#ifdef ENABLE_SCAN_IGNORE_LIST
							if (FI_freq_ignored(frequency) >= 0)
								str[0] = 'I';  // frequency is in the ignore list
						#endif

						if (g_vfo_info[vfo_num].channel.compand != COMPAND_OFF)
							str[1] = 'C';  // compander is enabled

						UI_PrintStringSmall(str, LCD_WIDTH - (7 * 2), 0, line + 1);
					#else
						const bool is_freq_chan       = IS_FREQ_CHANNEL(scrn_chan);
						const uint8_t freq_in_channel = g_vfo_info[vfo_num].freq_in_channel;
//						const uint8_t freq_in_channel = SETTINGS_find_channel(frequency);  // was way to slow

						strcpy(str, "   ");

						#ifdef ENABLE_SCAN_IGNORE_LIST
							if (FI_freq_ignored(frequency) >= 0)
								str[0] = 'I';  // frequency is in the ignore list
						#endif

						if (is_freq_chan && freq_in_channel <= USER_CHANNEL_LAST)
							str[1] = 'F';  // this VFO frequency is also found in a channel

						if (g_vfo_info[vfo_num].channel.compand != COMPAND_OFF)
							str[2] = 'C';  // compander is enabled

						UI_PrintStringSmall(str, LCD_WIDTH - (7 * 3), 0, line + 1);
					#endif
				}
				#endif
			}
		}

		// ************

		{	// show the TX/RX level
			uint8_t Level = 0;

			if (mode == 1)
			{	// TX power level
				switch (g_rx_vfo->channel.tx_power)
				{
					case OUTPUT_POWER_LOW:  Level = 2; break;
					case OUTPUT_POWER_MID:  Level = 4; break;
					case OUTPUT_POWER_HIGH: Level = 6; break;
					case OUTPUT_POWER_USER: Level = 2; break;
				}
			}
			else
			if (mode == 2)
			{	// RX signal level
				if (g_vfo_rssi_bar_level[vfo_num] > 0)
					Level = g_vfo_rssi_bar_level[vfo_num];
			}

			draw_small_antenna_bars(p_line1 + LCD_WIDTH, Level);
		}

		// ************

		str[0] = '\0';
		if (g_vfo_info[vfo_num].channel.mod_mode != MOD_MODE_FM)
		{	// show the modulation mode
			const char *mode_list[] = {"FM", "AM", "SB", "??"};
			const unsigned int mode = g_vfo_info[vfo_num].channel.mod_mode;
			if (mode < ARRAY_SIZE(mode_list))
				strcpy(str, mode_list[mode]);
		}
		else
		{	// or show the CTCSS/DCS symbol (when in FM mode)
			const freq_config_t *pConfig = (mode == 1) ? g_vfo_info[vfo_num].p_tx : g_vfo_info[vfo_num].p_rx;
			const unsigned int code_type = pConfig->code_type;
			const char *code_list[] = {"FM", "CTC", "DCS", "DCR"};
			if (code_type < ARRAY_SIZE(code_list))
				strcpy(str, code_list[code_type]);
		}
		UI_PrintStringSmall(str, 24, 0, line + 2);

		#ifdef ENABLE_TX_WHEN_AM
			if (state == VFO_STATE_NORMAL || state == VFO_STATE_ALARM)
		#else
			if ((state == VFO_STATE_NORMAL || state == VFO_STATE_ALARM) && g_vfo_info[vfo_num].channel.mod_mode == MOD_MODE_FM) // TX allowed only when FM
		#endif
		{
			if (FREQUENCY_tx_freq_check(g_vfo_info[vfo_num].p_tx->frequency) == 0)
			{
				// show the TX power
				const char pwr_list[] = "LMHU";  //  low, medium, high, user
				const unsigned int i = g_vfo_info[vfo_num].channel.tx_power;
				str[0] = (i < ARRAY_SIZE(pwr_list)) ? pwr_list[i] : '\0';
				str[1] = '\0';
				UI_PrintStringSmall(str, 46, 0, line + 2);

				if (g_vfo_info[vfo_num].freq_config_rx.frequency != g_vfo_info[vfo_num].freq_config_tx.frequency)
				{	// show the TX offset symbol
					const char dir_list[] = "\0+-";
					const unsigned int i = g_vfo_info[vfo_num].channel.tx_offset_dir;
					str[0] = (i < sizeof(dir_list)) ? dir_list[i] : '?';
					str[1] = '\0';
					UI_PrintStringSmall(str, 54, 0, line + 2);
				}
			}
		}

		// show the TX/RX reverse symbol
		if (g_vfo_info[vfo_num].channel.frequency_reverse)
			UI_PrintStringSmall("R", 62, 0, line + 2);

		// show the narrow band symbol
		strcpy(str, " ");
		if (g_vfo_info[vfo_num].channel.channel_bandwidth == BANDWIDTH_WIDE)
			str[0] = 'W';
		else
		if (g_vfo_info[vfo_num].channel.channel_bandwidth == BANDWIDTH_NARROW)
			str[0] = 'N';
		UI_PrintStringSmall(str, 70, 0, line + 2);

		// show the DTMF decoding symbol
		#ifdef ENABLE_KILL_REVIVE
			if (g_vfo_info[vfo_num].channel.dtmf_decoding_enable || g_eeprom.config.setting.radio_disabled)
				UI_PrintStringSmall("DTMF", 78, 0, line + 2);
		#else
			if (g_vfo_info[vfo_num].channel.dtmf_decoding_enable)
				UI_PrintStringSmall("DTMF", 78, 0, line + 2);
				//UI_PrintStringSmallest("DTMF", 78, (line + 2) * 8, false, true);
		#endif

		// show the audio scramble symbol
		if (g_vfo_info[vfo_num].channel.scrambler > 0 && g_eeprom.config.setting.enable_scrambler)
			UI_PrintStringSmall("SCR", 106, 0, line + 2);
	}

	// *************************************************

	UI_DisplayCenterLine();

	#ifdef ENABLE_PANADAPTER
		UI_DisplayMain_pan(false);
	#endif

	ST7565_BlitFullScreen();
}

// ***************************************************************************
