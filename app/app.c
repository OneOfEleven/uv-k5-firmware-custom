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
#include "app/action.h"
#ifdef ENABLE_AIRCOPY
	#include "app/aircopy.h"
#endif
#include "app/app.h"
#include "app/dtmf.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "app/generic.h"
#include "app/main.h"
#include "app/menu.h"
#include "app/search.h"
#include "app/uart.h"
#include "ARMCM0.h"
#include "audio.h"
#include "board.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/backlight.h"
#ifdef ENABLE_FMRADIO
	#include "driver/bk1080.h"
#endif
#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "driver/system.h"
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
#include "dtmf.h"
#include "external/printf/printf.h"
#include "frequencies.h"
#ifdef ENABLE_SCAN_IGNORE_LIST
	#include "freq_ignore.h"
#endif
#include "functions.h"
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
#if defined(ENABLE_OVERLAY)
	#include "sram-overlay.h"
#endif
#include "ui/battery.h"
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/menu.h"
#include "ui/status.h"
#include "ui/ui.h"

static void APP_process_key(const key_code_t Key, const bool key_pressed, const bool key_held);

static void APP_update_rssi(const int vfo, const bool force)
{
	int16_t rssi   = BK4819_GetRSSI();
	uint8_t glitch = BK4819_GetGlitchIndicator();
	uint8_t noise  = BK4819_GetExNoiceIndicator();

	#ifdef ENABLE_AM_FIX
		// add RF gain adjust compensation
		#ifdef ENABLE_PANADAPTER
			if (!PAN_scanning())
		#endif
				if (g_current_vfo->channel.mod_mode != MOD_MODE_FM && g_eeprom.config.setting.am_fix)
					rssi -= rssi_gain_diff[vfo];
	#endif

	if (g_current_rssi[vfo] == rssi && !force)
		return;     // no change

	g_current_rssi[vfo]   = rssi;
	g_current_glitch[vfo] = glitch;
	g_current_noise[vfo]  = noise;

	UI_update_rssi(rssi, glitch, noise, vfo);

	g_update_rssi = false;
}

static void APP_check_for_new_receive(void)
{
	#ifdef ENABLE_PANADAPTER
		if (PAN_scanning())
			return;
	#endif

	if (!g_squelch_open && !g_monitor_enabled)
		return;

	// squelch is open

	if (g_scan_state_dir == SCAN_STATE_DIR_OFF)
	{	// not RF scanning

		if (g_css_scan_mode != CSS_SCAN_MODE_OFF && g_rx_reception_mode == RX_MODE_NONE)
		{	// CTCSS/DTS scanning

			g_scan_tick_10ms       = scan_pause_code_10ms;
			g_scan_pause_time_mode = false;
			g_rx_reception_mode    = RX_MODE_DETECTED;
		}

		if (g_eeprom.config.setting.dual_watch == DUAL_WATCH_OFF)
		{	// dual watch is disabled

			#ifdef ENABLE_NOAA
				if (g_noaa_mode)
				{
					g_noaa_tick_10ms = noaa_tick_3_10ms;
					g_schedule_noaa  = false;
				}
			#endif

			goto done;
		}

		// dual watch is enabled and we're RX'ing a signal

		if (g_rx_reception_mode != RX_MODE_NONE)
			goto done;

		g_dual_watch_tick_10ms = g_eeprom.config.setting.scan_hold_time * 50;
		g_scan_pause_time_mode = false;

		g_update_status = true;
	}
	else
	{	// RF scanning
		if (g_rx_reception_mode != RX_MODE_NONE)
			goto done;

		g_scan_tick_10ms       = scan_pause_chan_10ms;
		g_scan_pause_time_mode = false;
	}

	g_rx_reception_mode = RX_MODE_DETECTED;

done:
	if (g_current_function != FUNCTION_NEW_RECEIVE)
	{
		FUNCTION_Select(FUNCTION_NEW_RECEIVE);

		#ifdef ENABLE_MDC1200
		{	// reset the FSK receiver
			//const uint16_t fsk_reg59 = BK4819_read_reg(0x59) & ~((1u << 15) | (1u << 14) | (1u << 12) | (1u << 11));
			//BK4819_enable_mdc1200_rx(true);
			//BK4819_write_reg(0x59, (1u << 15) | (1u << 14) | fsk_reg59);
			//BK4819_write_reg(0x59, (1u << 12) | fsk_reg59);
		}
		#endif
	}

	APP_update_rssi(g_rx_vfo_num, true);
	g_update_rssi = true;
}

static void APP_process_new_receive(void)
{
	bool flag;

	#ifdef ENABLE_PANADAPTER
		if (PAN_scanning())
		{
			BK4819_set_AFC(0);
			return;
		}
	#endif

	if (!g_squelch_open)
		BK4819_set_AFC(0);

	if (!g_squelch_open && !g_monitor_enabled)
	{	// squelch is closed

		if (g_dtmf_rx_index > 0)
			DTMF_clear_RX();

		if (g_current_function != FUNCTION_FOREGROUND)
		{
			FUNCTION_Select(FUNCTION_FOREGROUND);
			g_update_display = true;
//			g_update_status  = true;
		}

		return;
	}

	switch (g_rx_vfo->channel.mod_mode)
	{
		case MOD_MODE_FM:
		case MOD_MODE_AM:
			BK4819_set_AFC(2);
			break;
		default:
		case MOD_MODE_DSB:
			BK4819_set_AFC(0);
			break;
	}

	flag = (g_scan_state_dir == SCAN_STATE_DIR_OFF && g_current_code_type == CODE_TYPE_NONE);

	#ifdef ENABLE_NOAA
		if (IS_NOAA_CHANNEL(g_rx_vfo->channel_save) && g_noaa_tick_10ms > 0)
		{
			g_noaa_tick_10ms = 0;
			flag             = true;
		}
	#endif

	if (g_ctcss_lost && g_current_code_type == CODE_TYPE_CONTINUOUS_TONE)
	{
		g_found_ctcss = false;
		flag          = true;
	}

	if (g_cdcss_lost && g_cdcss_code_type == CDCSS_POSITIVE_CODE && (g_current_code_type == CODE_TYPE_DIGITAL || g_current_code_type == CODE_TYPE_REVERSE_DIGITAL))
	{
		g_found_cdcss = false;
	}
	else
	if (!flag)
		return;

	if (g_scan_state_dir == SCAN_STATE_DIR_OFF && g_css_scan_mode == CSS_SCAN_MODE_OFF)
	{	// not scanning

		#ifdef ENABLE_KILL_REVIVE
			if (g_rx_vfo->channel.dtmf_decoding_enable || g_eeprom.config.setting.radio_disabled)
		#else
			if (g_rx_vfo->channel.dtmf_decoding_enable)
		#endif
		{	// DTMF DCD is enabled

			DTMF_HandleRequest();

			if (g_dtmf_call_state == DTMF_CALL_STATE_NONE)
			{
				if (g_rx_reception_mode == RX_MODE_DETECTED)
				{
					g_dual_watch_tick_10ms = g_eeprom.config.setting.scan_hold_time * 50;
					g_rx_reception_mode    = RX_MODE_LISTENING;

					g_update_status  = true;
					g_update_display = true;
					return;
				}
			}
		}
	}

	APP_start_listening();
}

enum end_of_rx_mode_e {
	END_OF_RX_MODE_NONE = 0,
	END_OF_RX_MODE_END,
	END_OF_RX_MODE_TTE
};
typedef enum end_of_rx_mode_e end_of_rx_mode_t;

static void APP_process_rx(void)
{
	#ifdef ENABLE_PANADAPTER
		if (PAN_scanning())
			return;
	#endif

	end_of_rx_mode_t Mode = END_OF_RX_MODE_NONE;

//	APP_update_rssi(g_rx_vfo_num);
	g_update_rssi = true;

//	if (!g_squelch_open)
//		BK4819_set_AFC(0);

	if (g_flag_tail_tone_elimination_complete)
	{
		Mode = END_OF_RX_MODE_END;
		goto Skip;
	}

	if (g_scan_state_dir != SCAN_STATE_DIR_OFF) // && IS_FREQ_CHANNEL(g_scan_next_channel))
	{
		if (g_squelch_open || g_monitor_enabled)
		{
			switch (g_eeprom.config.setting.carrier_search_mode)
			{
				case SCAN_RESUME_TIME:     // stay only for a limited time
					break;
				case SCAN_RESUME_CARRIER:  // stay untill the carrier goes away
					g_scan_tick_10ms       = g_eeprom.config.setting.scan_hold_time * 50;
					g_scan_pause_time_mode = false;
					break;
				case SCAN_RESUME_STOP:     // stop scan once we find any signal
					APP_stop_scan();
					break;
			}
			return;
		}

		Mode = END_OF_RX_MODE_END;
		goto Skip;
	}

	switch (g_current_code_type)
	{
		default:
		case CODE_TYPE_NONE:
			break;

		case CODE_TYPE_CONTINUOUS_TONE:
			if (g_found_ctcss && g_found_ctcss_tick_10ms == 0)
			{
				g_found_ctcss = false;
				g_found_cdcss = false;
				Mode          = END_OF_RX_MODE_END;
				goto Skip;
			}
			break;

		case CODE_TYPE_DIGITAL:
		case CODE_TYPE_REVERSE_DIGITAL:
			if (g_found_cdcss && g_found_cdcss_tick_10ms == 0)
			{
				g_found_ctcss = false;
				g_found_cdcss = false;
				Mode          = END_OF_RX_MODE_END;
				goto Skip;
			}
			break;
	}

	if (g_squelch_open || g_monitor_enabled)
	{
		if (g_eeprom.config.setting.backlight_on_tx_rx >= 2)
			BACKLIGHT_turn_on(backlight_tx_rx_time_secs); // keep the backlight on while we're receiving

		if (!g_end_of_rx_detected_maybe && IS_NOT_NOAA_CHANNEL(g_rx_vfo->channel_save))
		{
			switch (g_current_code_type)
			{
				case CODE_TYPE_NONE:
					if (g_eeprom.config.setting.squelch_level > 0)
					{
						if (g_cxcss_tail_found)
						{
							Mode               = END_OF_RX_MODE_TTE;
							g_cxcss_tail_found = false;
						}
					}
					break;

				case CODE_TYPE_CONTINUOUS_TONE:
					if (g_ctcss_lost)
					{
						g_found_ctcss = false;
					}
					else
					if (!g_found_ctcss)
					{
						g_found_ctcss           = true;
						g_found_ctcss_tick_10ms = 100;   // 1 sec
					}

					if (g_cxcss_tail_found)
					{
						Mode               = END_OF_RX_MODE_TTE;
						g_cxcss_tail_found = false;
					}
					break;

				case CODE_TYPE_DIGITAL:
				case CODE_TYPE_REVERSE_DIGITAL:
					if (g_cdcss_lost && g_cdcss_code_type == CDCSS_POSITIVE_CODE)
					{
						g_found_cdcss = false;
					}
					else
					if (!g_found_cdcss)
					{
						g_found_cdcss           = true;
						g_found_cdcss_tick_10ms = 100;   // 1 sec
					}

					if (g_cxcss_tail_found)
					{
						if (BK4819_GetCTCType() == 1)
							Mode = END_OF_RX_MODE_TTE;

						g_cxcss_tail_found = false;
					}

					break;
			}
		}
	}
	else
	{
		Mode = END_OF_RX_MODE_END;
	}

	if (!g_end_of_rx_detected_maybe &&
	     Mode == END_OF_RX_MODE_NONE &&
	     g_next_time_slice_40ms &&
	     g_eeprom.config.setting.tail_tone_elimination &&
	    (g_current_code_type == CODE_TYPE_DIGITAL || g_current_code_type == CODE_TYPE_REVERSE_DIGITAL) &&
	     BK4819_GetCTCType() == 1)
	{
		Mode = END_OF_RX_MODE_TTE;
	}
	else
	{
		g_next_time_slice_40ms = false;
	}

Skip:
	switch (Mode)
	{
		case END_OF_RX_MODE_NONE:
			break;

		case END_OF_RX_MODE_END:
			RADIO_setup_registers(true);

			#ifdef ENABLE_NOAA
				if (IS_NOAA_CHANNEL(g_rx_vfo->channel_save))
					g_noaa_tick_10ms = 3000 / 10;         // 3 sec
			#endif

			g_update_display = true;
			break;

		case END_OF_RX_MODE_TTE:
			if (g_eeprom.config.setting.tail_tone_elimination)
			{
				if (!g_monitor_enabled)
					GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

				g_tail_tone_elimination_tick_10ms     = 20;   // 200ms
				g_flag_tail_tone_elimination_complete = false;
				g_end_of_rx_detected_maybe            = true;
			}

			break;
	}

	if (g_scan_state_dir != SCAN_STATE_DIR_OFF)
	{	// we're RF scanning

		switch (g_eeprom.config.setting.carrier_search_mode)
		{
			case SCAN_RESUME_TIME:     // stay only for a limited time
				break;
			case SCAN_RESUME_CARRIER:  // stay untill the carrier goes away
				g_scan_tick_10ms       = g_eeprom.config.setting.scan_hold_time * 50;
				g_scan_pause_time_mode = false;
				break;
			case SCAN_RESUME_STOP:     // stop scan once we find any signal
				APP_stop_scan();
				break;
		}
	}
}

bool APP_start_listening(void)
{
	const unsigned int chan = g_rx_vfo_num;
//	const unsigned int chan = g_rx_vfo->channel_save;

	#ifdef ENABLE_KILL_REVIVE
		if (g_eeprom.config.setting.radio_disabled)
			return false;
	#endif

	if (g_squelch_open)
		BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, true);   // green LED on

	if (g_eeprom.config.setting.backlight_on_tx_rx >= 2)
		BACKLIGHT_turn_on(backlight_tx_rx_time_secs);

	#ifdef ENABLE_MDC1200
//		MDC1200_reset_rx();
	#endif

	// clear the other vfo's rssi level (to hide the antenna symbol)
	g_vfo_rssi_bar_level[(chan + 1) & 1u] = 0;

	// enable/disable AFC
	switch (g_rx_vfo->channel.mod_mode)
	{
		case MOD_MODE_FM:
		case MOD_MODE_AM:
			BK4819_set_AFC(2);   // enable a bit
			break;
		default:
		case MOD_MODE_DSB:
			BK4819_set_AFC(0);   // disable
			break;
	}

	if (g_scan_state_dir != SCAN_STATE_DIR_OFF)
	{	// we're RF scanning

		g_rx_vfo->freq_in_channel = 0xff;

		if (IS_FREQ_CHANNEL(g_scan_next_channel))
			g_rx_vfo->freq_in_channel = SETTINGS_find_channel(g_rx_vfo->freq_config_rx.frequency);

		switch (g_eeprom.config.setting.carrier_search_mode)
		{
			case SCAN_RESUME_TIME:
				if (!g_scan_pause_time_mode)
				{
					g_scan_tick_10ms       = g_eeprom.config.setting.scan_hold_time * 50;
					g_scan_pause_time_mode = true;
				}
				break;

			case SCAN_RESUME_CARRIER:
				g_scan_tick_10ms       = g_eeprom.config.setting.scan_hold_time * 50;
				g_scan_pause_time_mode = false;
				break;

			case SCAN_RESUME_STOP:
				g_scan_tick_10ms       = 0;
				g_scan_pause_time_mode = false;
				break;
		}
	}
	else
	if (g_css_scan_mode != CSS_SCAN_MODE_OFF)
	{	// we're code scanning
		g_css_scan_mode = CSS_SCAN_MODE_FOUND;
	}
	else
	if (g_eeprom.config.setting.dual_watch != DUAL_WATCH_OFF)
	{	// dual watch is enabled
		g_dual_watch_tick_10ms = g_eeprom.config.setting.scan_hold_time * 50;
		g_rx_vfo_is_active     = true;
		g_update_status        = true;
	}

	#ifdef ENABLE_NOAA
		if (IS_NOAA_CHANNEL(g_rx_vfo->channel_save) && g_noaa_mode)
		{
			g_rx_vfo->channel_save        = g_noaa_channel + NOAA_CHANNEL_FIRST;
			g_rx_vfo->p_rx->frequency     = NOAA_FREQUENCY_TABLE[g_noaa_channel];
			g_rx_vfo->p_tx->frequency     = NOAA_FREQUENCY_TABLE[g_noaa_channel];
			g_eeprom.config.setting.indices.vfo[chan].screen = g_rx_vfo->channel_save;
			g_noaa_tick_10ms              = 5000 / 10;   // 5 sec
			g_schedule_noaa               = false;
		}
	#endif

	// AF gain
//	if (g_rx_vfo->channel.mod_mode != MOD_MODE_FM)
//	{
//		BK4819_write_reg(0x48, 0xB3A8);   // 1011 0011 1010 1000
//	}
//	else
	{
		BK4819_write_reg(0x48,
			(11u << 12)                       |     // ??? .. 0 ~ 15, doesn't seem to make any difference
			( 0u << 10)                       |     // AF Rx Gain-1
			(g_eeprom.calib.volume_gain << 4) |     // AF Rx Gain-2
			(g_eeprom.calib.dac_gain    << 0));     // AF DAC Gain (after Gain-1 and Gain-2)
	}

	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_mode)
			BK1080_Init(0, false);		// disable FM radio audio
	#endif

	FUNCTION_Select(FUNCTION_RECEIVE);

	// set modulation mode FM, AM etc
	#ifdef ENABLE_VOICE
		#ifdef MUTE_AUDIO_FOR_VOICE
			if (g_voice_write_index == 0)
				AUDIO_set_mod_mode(g_rx_vfo->channel.mod_mode);
		#else
			AUDIO_set_mod_mode(g_rx_vfo->channel.mod_mode);
		#endif
	#else
		AUDIO_set_mod_mode(g_rx_vfo->channel.mod_mode);
	#endif

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		UART_printf("mod mode %u\r\n", g_rx_vfo->channel.mod_mode);
	#endif

	// speaker on
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

	if (g_current_display_screen != DISPLAY_MENU)
		GUI_SelectNextDisplay(DISPLAY_MAIN);

	APP_update_rssi(g_rx_vfo_num, true);

	g_update_status  = true;
	g_update_display = true;

	return true;
}

void APP_stop_scan(void)
{
	if (g_scan_state_dir == SCAN_STATE_DIR_OFF)
		return;   // but, but, we weren't doing anything !

	// yes we were

	g_scan_state_dir = SCAN_STATE_DIR_OFF;

	if (g_scan_pause_time_mode ||
		g_scan_tick_10ms > (200 / 10) ||
		g_monitor_enabled ||
	    g_current_function == FUNCTION_RECEIVE ||
	    g_current_function == FUNCTION_NEW_RECEIVE)
	{	// stay where we are
		g_scan_restore_channel   = 0xff;
		g_scan_restore_frequency = 0xffffffff;
	}

	if (g_scan_restore_channel != 0xff ||
	   (g_scan_restore_frequency > 0 && g_scan_restore_frequency != 0xffffffff))
	{	// revert to where we were when starting the scan

		if (g_scan_next_channel <= USER_CHANNEL_LAST)
		{	// we were channel scanning

			if (g_scan_restore_channel != 0xff)
			{
				g_eeprom.config.setting.indices.vfo[g_rx_vfo_num].user   = g_scan_restore_channel;
				g_eeprom.config.setting.indices.vfo[g_rx_vfo_num].screen = g_scan_restore_channel;

				RADIO_configure_channel(g_rx_vfo_num, VFO_CONFIGURE_RELOAD);
				RADIO_ConfigureSquelch(g_rx_vfo);
				RADIO_setup_registers(true);
			}
		}
		else
		if (g_scan_restore_frequency > 0 && g_scan_restore_frequency != 0xffffffff)
		{	// we were frequency scanning

			g_rx_vfo->freq_config_rx.frequency = g_scan_restore_frequency;

			// find the first channel that contains this frequency
			g_rx_vfo->freq_in_channel = SETTINGS_find_channel(g_rx_vfo->freq_config_rx.frequency);

			RADIO_apply_offset(g_rx_vfo, false);
			RADIO_ConfigureSquelch(g_rx_vfo);
			RADIO_setup_registers(true);
		}

		g_update_display = true;
	}
	else
	{	// stay where we are

		if (g_rx_vfo->channel_save > USER_CHANNEL_LAST)
		{	// frequency mode
			RADIO_apply_offset(g_rx_vfo, false);
			RADIO_ConfigureSquelch(g_rx_vfo);
			SETTINGS_save_channel(g_rx_vfo->channel_save, g_rx_vfo_num, g_rx_vfo, 1);
			return;
		}

		SETTINGS_save_vfo_indices();
	}

	#ifdef ENABLE_VOICE
		g_another_voice_id = VOICE_ID_SCANNING_STOP;
	#endif

	g_scan_tick_10ms      = 0;
	g_scan_pause_time_mode = false;

	g_update_status = true;
}

static void APP_next_freq(void)
{
	uint32_t freq = g_tx_vfo->freq_config_rx.frequency;

	#ifdef ENABLE_SCAN_IGNORE_LIST
		do {
	#endif
			freq += g_scan_initial_step_size * g_scan_state_dir;

			// wrap-a-round
			while (freq >= g_scan_initial_upper)
				freq -= g_scan_initial_upper - g_scan_initial_lower;
			while (freq < g_scan_initial_lower)
				freq += g_scan_initial_upper - g_scan_initial_lower;

			// round
			#ifdef ENABLE_SCAN_RANGES
				if (g_eeprom.config.setting.scan_ranges_enable)
					//freq = g_scan_initial_lower + ((((freq - g_scan_initial_lower) + (g_scan_initial_step_size / 2)) / g_scan_initial_step_size) * g_scan_initial_step_size);
					freq = FREQUENCY_floor_to_step(freq, g_scan_initial_step_size, g_scan_initial_lower, g_scan_initial_upper);
			#else
				//freq = FREQUENCY_floor_to_step(freq, g_scan_initial_step_size, g_scan_initial_lower, g_scan_initial_upper);
			#endif

	#ifdef ENABLE_SCAN_IGNORE_LIST
		} while (FI_freq_ignored(freq) >= 0);
	#endif

	g_tx_vfo->freq_in_channel = 0xff;

	g_tx_vfo->freq_config_rx.frequency = freq;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		UART_printf("APP_next_freq %u %u\r\n", freq, new_band);
	#endif

	#if 0
		// original slower method

		RADIO_apply_offset(g_tx_vfo, false);
		RADIO_setup_registers(true);

		#ifdef ENABLE_FASTER_CHANNEL_SCAN
			g_scan_tick_10ms = 10;   // 100ms
		#else
			g_scan_tick_10ms = scan_pause_freq_10ms;
		#endif

	#else
		// don't need to go through all the other stuff .. speed things up !!

		BK4819_set_rf_frequency(g_tx_vfo->freq_config_rx.frequency, true);
		BK4819_set_rf_filter_path(g_tx_vfo->freq_config_rx.frequency);

		RADIO_apply_offset(g_tx_vfo, false);

		#ifdef ENABLE_FASTER_CHANNEL_SCAN
			//g_scan_tick_10ms = 10;   // 100ms
			g_scan_tick_10ms = 6;      // 60ms
		#else
			g_scan_tick_10ms = scan_pause_freq_10ms;
		#endif

	#endif

	g_scan_pause_time_mode = false;
	g_update_display       = true;
}

static void APP_next_channel(void)
{
	static unsigned int prevChannel = 0;
	const unsigned int  index       = g_eeprom.config.setting.scan_list_default;
	const bool          enabled     = (index < 2) ? !!g_eeprom.config.setting.priority_scan_list[index].enabled    : true;
	const int           chan1       = (index < 2) ?   g_eeprom.config.setting.priority_scan_list[index].channel[0] : -1;
	const int           chan2       = (index < 2) ?   g_eeprom.config.setting.priority_scan_list[index].channel[1] : -1;
	const unsigned int  prev_chan   = g_scan_next_channel;
	unsigned int        chan        = 0;

	if (enabled)
	{
		switch (g_scan_current_scan_list)
		{
			case SCAN_NEXT_CHAN_SCANLIST1:
				prevChannel = g_scan_next_channel;

				if (chan1 >= 0)
				{
					if (RADIO_channel_valid(chan1, false, 0))
					{
						g_scan_current_scan_list = SCAN_NEXT_CHAN_SCANLIST1;
						g_scan_next_channel      = chan1;
						break;
					}
				}

			// Fallthrough

			case SCAN_NEXT_CHAN_SCANLIST2:
				if (chan2 >= 0)
				{
					if (RADIO_channel_valid(chan2, false, 0))
					{
						g_scan_current_scan_list = SCAN_NEXT_CHAN_SCANLIST2;
						g_scan_next_channel      = chan2;
						break;
					}
				}

			// Fallthrough

			default:
			case SCAN_NEXT_CHAN_USER:
				g_scan_current_scan_list = SCAN_NEXT_CHAN_USER;
				g_scan_next_channel      = prevChannel;
				chan             = 0xff;
				break;
		}
	}

	if (!enabled || chan == 0xff)
	{
		chan = RADIO_FindNextChannel(g_scan_next_channel + g_scan_state_dir, g_scan_state_dir, (index < 2) ? true : false, index);
		if (chan == 0xFF)
		{	// no valid channel found

			chan = USER_CHANNEL_FIRST;
//			return;
		}

		g_scan_next_channel = chan;
	}

	if (g_scan_next_channel != prev_chan)
	{
		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//			UART_printf("APP_next_channel %u\r\n", g_scan_next_channel);
		#endif

		g_eeprom.config.setting.indices.vfo[g_rx_vfo_num].user   = g_scan_next_channel;
		g_eeprom.config.setting.indices.vfo[g_rx_vfo_num].screen = g_scan_next_channel;

		RADIO_configure_channel(g_rx_vfo_num, VFO_CONFIGURE_RELOAD);
		RADIO_setup_registers(true);

		g_update_display = true;
	}

	#ifdef ENABLE_FASTER_CHANNEL_SCAN
		g_scan_tick_10ms = 9;  // 90ms .. <= ~60ms it misses signals (squelch response and/or PLL lock time) ?
	#else
		g_scan_tick_10ms = scan_pause_chan_10ms;
	#endif

	g_scan_pause_time_mode = false;

	if (enabled)
		if (++g_scan_current_scan_list >= SCAN_NEXT_NUM)
			g_scan_current_scan_list = SCAN_NEXT_CHAN_SCANLIST1;  // back round we go
}

#ifdef ENABLE_NOAA
	static void APP_next_noaa(void)
	{
		if (++g_noaa_channel >= ARRAY_SIZE(NOAA_FREQUENCY_TABLE))
			g_noaa_channel = 0;
	}
#endif

static bool APP_toggle_dual_watch_vfo(void)
{
	if (g_eeprom.config.setting.dual_watch == DUAL_WATCH_OFF)
		return false;
	if (g_current_function == FUNCTION_FOREGROUND && g_current_function == FUNCTION_POWER_SAVE)
		return false;
	if (g_current_display_screen == DISPLAY_SEARCH)
		return false;
	if (g_scan_state_dir != SCAN_STATE_DIR_OFF)
		return false;
	if (g_css_scan_mode != CSS_SCAN_MODE_OFF)
		return false;
	if (g_ptt_is_pressed)
		return false;
	if (g_dtmf_call_state != DTMF_CALL_STATE_NONE)
		return false;
	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_mode)
			return false;
	#endif
	if (g_dual_watch_tick_10ms > 0)
		return false;

	#ifdef ENABLE_NOAA
		if (g_noaa_mode)
		{
			if (IS_NOT_NOAA_CHANNEL(g_eeprom.config.setting.indices.vfo[0].screen) || IS_NOT_NOAA_CHANNEL(g_eeprom.config.setting.indices.vfo[1].screen))
				g_rx_vfo_num = (g_rx_vfo_num + 1) & 1u;
			else
				g_rx_vfo_num = 0;

			g_rx_vfo = &g_vfo_info[g_rx_vfo_num];

			if (g_vfo_info[0].channel_save >= NOAA_CHANNEL_FIRST)
				APP_next_noaa();
		}
		else
	#endif
	{	// toggle between VFO's
		g_rx_vfo_num    = (g_rx_vfo_num + 1) & 1u;
		g_rx_vfo        = &g_vfo_info[g_rx_vfo_num];
//		g_update_status = true;
	}

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		UART_printf("dw %u\r\n", g_rx_vfo_num);
	#endif

	RADIO_setup_registers(false);

	#ifdef ENABLE_NOAA
		g_dual_watch_tick_10ms = g_noaa_mode ? dual_watch_delay_noaa_10ms : dual_watch_delay_toggle_10ms;
	#else
		g_dual_watch_tick_10ms = dual_watch_delay_toggle_10ms;
	#endif

	return true;
}

void APP_process_radio_interrupts(void)
{
	if (g_current_display_screen == DISPLAY_SEARCH)
		return;

	while (1)
	{	// BK4819 chip interrupt request

		uint16_t int_bits;

		const uint16_t reg_c = BK4819_read_reg(0x0C);

//		if ((reg_c & (2u << 1))
//		{	// VOX
//		}

		if (reg_c & (1u << 1))
		{	// squelch is open
			if (!g_squelch_open)
			{
				#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
					UART_SendText("sq open\r\n");
				#endif
				g_squelch_open = true;
				//APP_update_rssi(g_rx_vfo_num, false);
				g_update_rssi = true;
				if (g_monitor_enabled)
					BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, true);  // green LED on
				g_update_display = true;
			}
		}
		else
		{	// squelch is closed
			if (g_squelch_open)
			{
				#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
					UART_SendText("sq close\r\n");
				#endif
				g_squelch_open = false;
				//APP_update_rssi(g_rx_vfo_num, false);
				g_update_rssi = true;
				BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, false);  // green LED off
				if (!g_monitor_enabled)
					GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
				g_update_display = true;
			}
		}

		if ((reg_c & (1u << 0)) == 0)
			break;       // no interrupt flags

		BK4819_write_reg(0x02, 0);
		int_bits = BK4819_read_reg(0x02);

		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
			#ifdef ENABLE_AIRCOPY
				if (g_current_display_screen != DISPLAY_AIRCOPY)
			#endif
				{
					int i;
					UART_printf("int bits %04X %04X ", reg_c, int_bits);
					for (i = 15; i >= 0; i--)
						UART_printf("%c", (reg_c & (1u << i)) ? '#' : '.');
					UART_SendText("  ");
					for (i = 15; i >= 0; i--)
						UART_printf("%c", (int_bits & (1u << i)) ? '#' : '.');
					UART_SendText("\r\n");
				}
		#endif

		if (int_bits & BK4819_REG_02_DTMF_5TONE_FOUND)
		{	// save the RX'ed DTMF character
			const char c = DTMF_GetCharacter(BK4819_GetDTMF_5TONE_Code());
			if (c != 0xff && g_current_function != FUNCTION_TRANSMIT)
			{
				#ifdef ENABLE_DTMF_LIVE_DECODER
					if (g_eeprom.config.setting.dtmf_live_decoder)
					{
						size_t len = strlen(g_dtmf_rx_live);
						if (len >= (sizeof(g_dtmf_rx_live) - 1))
						{	// make room
							memmove(&g_dtmf_rx_live[0], &g_dtmf_rx_live[1], sizeof(g_dtmf_rx_live) - 1);
							len--;
						}
						g_dtmf_rx_live[len++]  = c;
						g_dtmf_rx_live[len]    = 0;
						g_dtmf_rx_live_timeout = dtmf_rx_live_timeout_500ms;  // time till we delete it
						g_update_display       = true;
					}
				#endif

				#ifdef ENABLE_KILL_REVIVE
					if (g_rx_vfo->channel.dtmf_decoding_enable || g_eeprom.config.setting.radio_disabled)
				#else
					if (g_rx_vfo->channel.dtmf_decoding_enable)
				#endif
				{
					if (g_dtmf_rx_index >= (sizeof(g_dtmf_rx) - 1))
					{	// make room
						memmove(&g_dtmf_rx[0], &g_dtmf_rx[1], sizeof(g_dtmf_rx) - 1);
						g_dtmf_rx_index--;
					}
					g_dtmf_rx[g_dtmf_rx_index++] = c;
					g_dtmf_rx[g_dtmf_rx_index]   = 0;
					g_dtmf_rx_timeout            = dtmf_rx_timeout_500ms;  // time till we delete it
					g_dtmf_rx_pending            = true;

					DTMF_HandleRequest();
				}
			}
		}

		if (int_bits & BK4819_REG_02_CxCSS_TAIL)
		{
			g_cxcss_tail_found          = true;
			g_ctcss_tail_phase_shift_rx = (reg_c >> 12) & 3u;

			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_printf("cxcss tail %u\r\n", g_ctcss_tail_phase_shift_rx);
			#endif
		}

		if (int_bits & BK4819_REG_02_CDCSS_LOST)
		{
			g_cdcss_lost      = true;
			g_cdcss_code_type = BK4819_get_CDCSS_code_type();

			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_printf("cdcss lost %u\r\n", g_cdcss_code_type);
			#endif
		}

		if (int_bits & BK4819_REG_02_CDCSS_FOUND)
		{
			g_cdcss_lost = false;

			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_SendText("cdcss found\r\n");
			#endif
		}

		if (int_bits & BK4819_REG_02_CTCSS_LOST)
		{
			g_ctcss_lost = true;

			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_SendText("cdcss lost\r\n");
			#endif
		}

		if (int_bits & BK4819_REG_02_CTCSS_FOUND)
		{
			g_ctcss_lost = false;

			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_SendText("ctcss found\r\n");
			#endif
		}

		#ifdef ENABLE_VOX
			if (int_bits & BK4819_REG_02_VOX_LOST)
			{
				g_vox_lost            = true;
				g_vox_pause_tick_10ms = 10;  // 100ms

				g_update_status = true;

				#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//					UART_SendText("vox lost\r\n");
				#endif

				if (g_eeprom.config.setting.vox_enabled)
				{
					if (g_current_function == FUNCTION_POWER_SAVE && !g_rx_idle_mode)
					{
						g_power_save_tick_10ms = power_save2_10ms;
						g_power_save_expired   = false;
					}

					if (g_eeprom.config.setting.dual_watch != DUAL_WATCH_OFF &&
					   (g_dual_watch_tick_10ms == 0 || g_dual_watch_tick_10ms < dual_watch_delay_after_vox_10ms))
					{
						g_dual_watch_tick_10ms = dual_watch_delay_after_vox_10ms;
						g_update_status        = true;
					}
				}
			}

			if (int_bits & BK4819_REG_02_VOX_FOUND)
			{
				g_vox_lost            = false;
				g_vox_pause_tick_10ms = 0;

				g_update_status = true;

				#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//					UART_SendText("vox found\r\n");
				#endif
			}
		#endif
/*
		if (int_bits & BK4819_REG_02_SQUELCH_CLOSED)
		{
			g_squelch_open = false;

			BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, false);  // LED off

			if (!g_monitor_enabled)
				GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_SendText("sq closed\r\n");
			#endif

			//APP_update_rssi(g_rx_vfo_num, false);
			g_update_rssi = true;

			g_update_display = true;
		}

		if (int_bits & BK4819_REG_02_SQUELCH_OPENED)
		{
			g_squelch_open = true;

			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_SendText("sq opened\r\n");
			#endif

			//APP_update_rssi(g_rx_vfo_num, false);
			g_update_rssi = true;

			if (g_monitor_enabled)
				BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, true);  // LED on

			g_update_display = true;
		}
*/
		#ifdef ENABLE_MDC1200
			MDC1200_process_rx(int_bits);
		#endif
	}
}

void APP_end_tx(void)
{	// back to RX mode

	RADIO_tx_eot();

	ST7565_Init(false);

	if (g_current_vfo->p_tx->code_type != CODE_TYPE_NONE)
	{	// CTCSS/CDCSS is enabled

		//if (g_eeprom.config.setting.tail_tone_elimination && g_eeprom.config.setting.repeater_tail_tone_elimination > 0)
		if (g_eeprom.config.setting.tail_tone_elimination)
		{	// send the CTCSS/DCS tail tone - allows the receivers to mute the usual FM squelch tail/crash
			RADIO_enable_CxCSS_tail();
		}
		#if 1
			else
			{	// TX a short blank carrier after disabling the CTCSS/CDCSS
				// this gives the receivers time to mute their RX audio before we drop carrier
				BK4819_disable_sub_audible();
				SYSTEM_DelayMs(200);
			}
		#endif
	}

	RADIO_setup_registers(false);

	if (g_monitor_enabled)
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
}

#ifdef ENABLE_VOX
	static void APP_process_vox(void)
	{
		if (g_vox_resume_tick_10ms == 0)
		{
			if (g_vox_pause_tick_10ms > 0)
				return;
		}
		else
		{
			g_vox_lost            = false;
			g_vox_pause_tick_10ms = 0;

			g_update_status = true;
		}

		if (g_current_function == FUNCTION_RECEIVE || g_monitor_enabled)
			return;

		if (g_scan_state_dir != SCAN_STATE_DIR_OFF || g_css_scan_mode != CSS_SCAN_MODE_OFF)
			return;

		if (g_vox_audio_detected)
		{
			if (g_vox_lost)
				g_vox_stop_tick_10ms = vox_stop_10ms;   // 1 second
			else
			if (g_vox_stop_tick_10ms == 0)
				g_vox_audio_detected = false;

			if (g_current_function == FUNCTION_TRANSMIT && !g_ptt_is_pressed && !g_vox_audio_detected)
			{
				if (g_flag_end_tx)
				{	// back to RX mode
					FUNCTION_Select(FUNCTION_FOREGROUND);
				}
				else
				{
					APP_end_tx();

					if (g_eeprom.config.setting.repeater_tail_tone_elimination == 0)
						FUNCTION_Select(FUNCTION_FOREGROUND);
					else
						g_rtte_count_down = g_eeprom.config.setting.repeater_tail_tone_elimination * 10;
				}

				g_update_display = true;

				g_flag_end_tx = false;
			}

			g_update_status  = true;
			return;
		}

		if (!g_vox_lost)
			return;

		g_vox_audio_detected = true;

		g_update_status = true;

		#ifdef ENABLE_KILL_REVIVE
			if (g_eeprom.config.setting.radio_disabled)
				return;
		#endif

		if (!g_eeprom.config.setting.tx_enable)
			return;

		#ifdef ENABLE_FMRADIO
			if (g_fm_radio_mode)
				return;            // can't do VOX while the speaker is emitting noise
		#endif

		if (g_current_display_screen == DISPLAY_MENU)
			return;

		if (g_current_function == FUNCTION_POWER_SAVE)
			FUNCTION_Select(FUNCTION_FOREGROUND);

		if (g_current_function == FUNCTION_TRANSMIT)
			return;

		#if defined(ENABLE_UART)
			if (g_serial_config_tick_500ms > 0)
				return;
		#endif

		// ************* go into TX mode

		g_dtmf_reply_state = DTMF_REPLY_NONE;

		RADIO_PrepareTX();

		g_update_display = true;
	}
#endif

// called every 10ms
void APP_process_keys(void)
{
	const bool ptt_pressed = !GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT);

	static int     key_repeat_speedup_ticks = 0;
	static uint8_t key_repeat_ticks         = 0;

	key_code_t key;

	#ifdef ENABLE_KILL_REVIVE
		if (g_eeprom.config.setting.radio_disabled)
			return;
	#endif

	// *****************
	// PTT is treated completely separately from all the other buttons

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		if (g_input_box_index > 0)
//			UART_printf("1 index %u %u\r\n", g_input_box_index, g_flash_light_blink_tick_10ms);
	#endif

	if (ptt_pressed)
	{	// PTT pressed

		if (!g_ptt_is_pressed && g_eeprom.config.setting.tx_enable && g_current_function != FUNCTION_TRANSMIT)
		{
		#if defined(ENABLE_UART)
			if (g_serial_config_tick_500ms == 0)
		#endif
			{
			#ifdef ENABLE_AIRCOPY
				if (g_current_display_screen != DISPLAY_AIRCOPY)
			#endif
				{
				#ifdef ENABLE_KILL_REVIVE
					if (!g_eeprom.config.setting.radio_disabled)
				#endif
					{
						if (++g_ptt_debounce >= 3)        // 30ms debounce
						{	// start TX'ing

							g_boot_tick_10ms   = 0;       // cancel the boot-up screen
							g_ptt_is_pressed   = ptt_pressed;
							g_ptt_was_released = false;
							g_ptt_debounce     = 3;

							APP_process_key(KEY_PTT, true, false);
						}
					}
				}
			}
		}
	}
	else
	{	// PTT released

	#if defined(ENABLE_UART)
		if (g_ptt_is_pressed || g_serial_config_tick_500ms > 0)
	#else
		if (g_ptt_is_pressed)
	#endif
		{
//			if (g_ptt_debounce > 0)
			{
				if (--g_ptt_debounce <= 0)
				{	// stop TX'ing

					g_ptt_is_pressed   = false;
					g_ptt_was_released = true;
					g_ptt_debounce     = 0;

					APP_process_key(KEY_PTT, false, false);
				}
			}
		}
	}

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		if (g_input_box_index > 0)
//			UART_printf("2 index %u %u\r\n", g_input_box_index, g_flash_light_blink_tick_10ms);
	#endif

	// *****************
	// key/button processing (non-PTT)

	// scan the hardware keys
	key = KEYBOARD_Poll();

	#if defined(ENABLE_UART)
		if (g_serial_config_tick_500ms > 0)
		{	// config upload/download in progress
			g_key_debounce_press  = 0;
			g_key_debounce_repeat = 0;
			g_key_prev            = KEY_INVALID;
			g_key_held            = false;
			g_fkey_pressed        = false;
			return;
		}
	#endif

	if (key == KEY_INVALID || (g_key_prev != KEY_INVALID && key != g_key_prev))
	{	// key not pressed or different key pressed

		if (g_key_debounce_press > 0)
		{
			if (--g_key_debounce_press == 0)
			{
				if (g_key_prev != KEY_INVALID)
				{	// key now fully released
					APP_process_key(g_key_prev, false, g_key_held);
					g_key_debounce_press  = 0;
					g_key_debounce_repeat = 0;
					g_key_prev            = KEY_INVALID;
					g_key_held            = false;
					g_boot_tick_10ms      = 0;   // cancel boot screen/beeps
					g_update_status       = true;
//					g_update_display      = true;
				}
			}

			if (g_key_debounce_repeat > 0)
				g_key_debounce_repeat--;
		}

		return;
	}

	// key pressed

	if (g_key_debounce_press < key_debounce_10ms)
	{
		if (++g_key_debounce_press >= key_debounce_10ms)
		{
			if (key != g_key_prev)
			{	// key pressed
				key_repeat_speedup_ticks = key_repeat_speedup_10ms;
				key_repeat_ticks         = key_repeat_initial_10ms;
				g_key_debounce_repeat    = key_debounce_10ms;
				g_key_prev               = key;
				g_key_held               = false;
				APP_process_key(g_key_prev, true, g_key_held);
				g_update_status          = true;
//				g_update_display         = true;
			}
		}
		return;
	}

	// long press time can be different for different keys
	const uint8_t long_press_ticks = (key == KEY_SIDE1 || key == KEY_SIDE2) ? key_side_long_press_10ms : key_long_press_10ms;

	if (g_key_debounce_repeat < long_press_ticks)
	{
		if (++g_key_debounce_repeat >= long_press_ticks)
		{	// key long press
			g_key_held = true;
			APP_process_key(g_key_prev, true, g_key_held);
		}
		return;
	}

	if (key != KEY_UP && key != KEY_DOWN)
		return;		// up and down keys are the only repeatables

	// speed up key repeat
	if (key_repeat_ticks > key_repeat_fastest_10ms)
	{
		if (--key_repeat_speedup_ticks <= 0)
		{
			key_repeat_speedup_ticks = key_repeat_speedup_10ms;
			key_repeat_ticks         = (key_repeat_ticks > (80 / 10)) ? key_repeat_ticks >> 1 : key_repeat_ticks - 1;
		}
	}
	key_repeat_ticks = (key_repeat_ticks < key_repeat_fastest_10ms) ? key_repeat_fastest_10ms : key_repeat_ticks;

	// key repeat max 10ms speed if user is moving up/down in freq/channel
//	const bool    freq_chan    = IS_FREQ_CHANNEL(g_eeprom.config.setting.indices.vfo[g_eeprom.config.setting.tx_vfo_num].screen);
//	const uint8_t repeat_ticks = (g_manual_scanning && g_monitor_enabled && freq_chan && g_current_display_screen == DISPLAY_MAIN) ? 2 : key_repeat_ticks;

	const uint8_t repeat_ticks = key_repeat_ticks;

	if (++g_key_debounce_repeat < (long_press_ticks + repeat_ticks))
		return;

	// key repeat

	g_key_debounce_repeat = long_press_ticks;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		UART_printf("rp %u\n", key_repeat_ticks);
	#endif

	APP_process_key(g_key_prev, true, g_key_held);
}

void APP_cancel_user_input_modes(void)
{
	if (g_ask_to_save)
	{
		g_ask_to_save  = false;
		g_update_display = true;
	}

	if (g_ask_to_delete)
	{
		g_ask_to_delete  = false;
		g_update_display = true;
	}

	if (g_dtmf_input_mode || g_dtmf_input_box_index > 0)
	{
		DTMF_clear_input_box();
		#ifdef ENABLE_FMRADIO
			if (g_fm_radio_mode)
				g_request_display_screen = DISPLAY_FM;
			else
				g_request_display_screen = DISPLAY_MAIN;
		#else
			g_request_display_screen = DISPLAY_MAIN;
		#endif
		g_update_display = true;
	}

	if (g_fkey_pressed || g_key_input_count_down > 0 || g_input_box_index > 0)
	{
		g_fkey_pressed         = false;
		g_input_box_index      = 0;
		g_key_input_count_down = 0;
		g_update_status        = true;
		g_update_display       = true;
	}
}

#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
	static void APP_alarm_off(void)
	{
		if (!g_squelch_open && !g_monitor_enabled)
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

		if (g_eeprom.config.setting.alarm_mode == ALARM_MODE_TONE)
		{
			RADIO_tx_eot();
			RADIO_enable_CxCSS_tail();
		}

		#ifdef ENABLE_VOX
			g_vox_resume_tick_10ms = 80;
		#endif

		g_alarm_state = ALARM_STATE_OFF;

		SYSTEM_DelayMs(5);

		RADIO_setup_registers(true);

		if (!g_squelch_open && !g_monitor_enabled)
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
		else
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

		if (g_current_display_screen != DISPLAY_MENU)     // 1of11 .. don't close the menu
			g_request_display_screen = DISPLAY_MAIN;
	}
#endif

void APP_channel_next(const bool remember_current, const scan_state_dir_t scan_direction)
{
	RADIO_select_vfos();

	g_scan_next_channel      = g_rx_vfo->channel_save;
	g_scan_current_scan_list = SCAN_NEXT_CHAN_SCANLIST1;
	g_scan_state_dir         = scan_direction;

	if (remember_current)
	{
		g_scan_restore_channel   = 0xff;
		g_scan_restore_frequency = 0xffffffff;
	}

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		UART_printf("APP_channel_next %u\r\n", g_scan_next_channel);
	#endif

	if (g_scan_next_channel <= USER_CHANNEL_LAST)
	{	// channel mode
		if (remember_current)
			g_scan_restore_channel = g_scan_next_channel;
		APP_next_channel();
	}
	else
	if (IS_FREQ_CHANNEL(g_scan_next_channel))
	{	// frequency mode
		if (remember_current)
			g_scan_restore_frequency = g_rx_vfo->freq_config_rx.frequency;
		APP_next_freq();
	}
	else
	{
		return;
	}

	g_scan_tick_10ms       = scan_pause_css_10ms;
	g_scan_pause_time_mode = false;
	g_rx_reception_mode    = RX_MODE_NONE;
}

static const uint32_t sos = __extension__ 0b10101000111011101110001010100000;

void APP_process_flash_light_10ms(void)
{
	switch (g_flash_light_state)
	{
		case FLASHLIGHT_OFF:
			break;

		case FLASHLIGHT_ON:
			break;

		case FLASHLIGHT_BLINK:
			if ((g_flash_light_blink_tick_10ms & 15u) == 0)
				GPIO_FlipBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
			break;

		case FLASHLIGHT_SOS:
			{	// 150ms tick
				// '16' sets the morse speed, lower value = faster speed
				// '+ 6' lengthens the loop time
				const unsigned int num_bits = sizeof(sos) * 8;
				const unsigned int bit = (g_flash_light_blink_tick_10ms / 16) % (num_bits + 6);
				if (bit < num_bits && (sos & (1u << (num_bits - 1 - bit))))
				{
					if (!GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT))
					{	// LED on
						GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
						#ifdef ENABLE_FLASH_LIGHT_SOS_TONE
							if (!g_squelch_open && !g_monitor_enabled && !GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER))
								BK4819_start_tone(880, 70, g_current_function == FUNCTION_TRANSMIT, false);
						#endif
					}
				}
				else
				{
					if (GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT))
					{	// LED off
						GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT); // OFF
						#ifdef ENABLE_FLASH_LIGHT_SOS_TONE
							if (!g_squelch_open && !g_monitor_enabled && GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER))
								BK4819_stop_tones(g_current_function == FUNCTION_TRANSMIT);
						#endif
					}
				}
			}
			break;
	}
}

void APP_process_scan(void)
{
	#ifdef ENABLE_VOICE
		if (g_voice_write_index != 0)
			return;
	#endif

	if ((g_current_function == FUNCTION_FOREGROUND  ||
	     g_current_function == FUNCTION_NEW_RECEIVE ||
	     g_current_function == FUNCTION_RECEIVE)    &&
		 g_current_display_screen != DISPLAY_SEARCH &&
	     g_scan_state_dir != SCAN_STATE_DIR_OFF     &&
	    !g_ptt_is_pressed)
	{	// RF scanning

		if (g_current_code_type == CODE_TYPE_NONE && g_current_function == FUNCTION_NEW_RECEIVE) // && !g_scan_pause_time_mode)
		{
			APP_start_listening();
		}
		else
		if (g_scan_tick_10ms == 0)
		{	// switch to next channel
			g_scan_pause_time_mode = false;
			g_rx_reception_mode    = RX_MODE_NONE;

			if (g_scan_next_channel <= USER_CHANNEL_LAST)
				APP_next_channel();
			else
			if (IS_FREQ_CHANNEL(g_scan_next_channel))
				APP_next_freq();
		}
/*
		if (g_scan_next_channel <= USER_CHANNEL_LAST)
		{	// channel mode

			if (g_current_code_type == CODE_TYPE_NONE && g_current_function == FUNCTION_NEW_RECEIVE && !g_scan_pause_time_mode)
			{
				APP_start_listening();
			}
			else
			{	// switch to next channel
				g_scan_pause_time_mode = false;
				g_rx_reception_mode    = RX_MODE_NONE;
				APP_next_channel();
			}
		}
		else
		if (IS_FREQ_CHANNEL(g_scan_next_channel))
		{	// frequency mode

			if (g_current_function == FUNCTION_NEW_RECEIVE && !g_scan_pause_time_mode)
			{
				APP_start_listening();
			}
			else
			{	// switch to next frequency
				g_scan_pause_time_mode = false;
				g_rx_reception_mode    = RX_MODE_NONE;
				APP_next_freq();
			}
		}
*/

	}

	if (g_css_scan_mode == CSS_SCAN_MODE_SCANNING && g_scan_tick_10ms == 0)
		MENU_SelectNextCode();

	#ifdef ENABLE_NOAA
		if (g_eeprom.config.setting.dual_watch == DUAL_WATCH_OFF && g_noaa_mode && g_schedule_noaa)
		{
			APP_next_noaa();

			RADIO_setup_registers(false);

			g_noaa_tick_10ms = 7;      // 70ms
			g_schedule_noaa  = false;
		}
	#endif

	if (APP_toggle_dual_watch_vfo())
	{
		if (g_rx_vfo_is_active && g_current_display_screen == DISPLAY_MAIN)
			GUI_SelectNextDisplay(DISPLAY_MAIN);

		g_rx_vfo_is_active  = false;
		g_rx_reception_mode = RX_MODE_NONE;
	}
}

void APP_process_search(void)
{
	if (g_current_display_screen != DISPLAY_SEARCH)
		return;

	g_search_freq_css_tick_10ms++;

	if (g_search_tick_10ms > 0)
	{
		if (--g_search_tick_10ms > 0)
		{
			APP_process_keys();
			return;
		}
	}

	if (g_search_edit_state != SEARCH_EDIT_STATE_NONE)
	{	// waiting for user input choice
		APP_process_keys();
		return;
	}

	g_update_display = true;
	GUI_SelectNextDisplay(DISPLAY_SEARCH);

	SEARCH_process();
}

void APP_process_transmit(void)
{
	if (g_current_function != FUNCTION_TRANSMIT)
		return;

	#ifdef ENABLE_ALARM
		if (g_alarm_state == ALARM_STATE_TXALARM || g_alarm_state == ALARM_STATE_ALARM)
		{	// TX alarm tone

			uint16_t Tone;

			g_alarm_running_counter_10ms++;

			// loop alarm tone frequency 300Hz ~ 1500Hz ~ 300Hz
			Tone = 300 + (g_alarm_tone_counter_10ms++ * 50);
			if (Tone >= ((1500 * 2) - 300))
			{
				Tone = 300;
				g_alarm_tone_counter_10ms = 0;
			}

			BK4819_SetScrambleFrequencyControlWord((Tone <= 1500) ? Tone : (1500 * 2) - Tone);

			if (g_eeprom.config.setting.alarm_mode == ALARM_MODE_TONE && g_alarm_running_counter_10ms == 512)
			{
				g_alarm_running_counter_10ms = 0;

				if (g_alarm_state == ALARM_STATE_TXALARM)
				{
					g_alarm_state = ALARM_STATE_ALARM;

					RADIO_enable_CxCSS_tail();
					RADIO_ConfigureTXPower(g_tx_vfo);
					BK4819_SetupPowerAmplifier(0, 0);
					BK4819_set_GPIO_pin(BK4819_GPIO1_PIN29_PA_ENABLE, false);   // PA off
					BK4819_Enable_AfDac_DiscMode_TxDsp();
					BK4819_set_GPIO_pin(BK4819_GPIO5_PIN1_RED, false);          // LED off

					GUI_DisplayScreen();
				}
				else
				{
					g_alarm_state = ALARM_STATE_TXALARM;

					GUI_DisplayScreen();

					RADIO_enableTX(false);
					BK4819_TransmitTone(true, 500);
					SYSTEM_DelayMs(2);

					GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

					g_alarm_tone_counter_10ms = 0;
				}
			}
		}
	#endif

	// repeater tail tone elimination
	if (g_rtte_count_down > 0)
	{
		if (--g_rtte_count_down == 0)
		{
			FUNCTION_Select(FUNCTION_FOREGROUND);
			g_update_status  = true;
			g_update_display = true;
		}
	}
}

void APP_process_functions(void)
{
	switch (g_current_function)
	{
		case FUNCTION_FOREGROUND:
			APP_check_for_new_receive();
			break;

		case FUNCTION_TRANSMIT:
			if (g_eeprom.config.setting.backlight_on_tx_rx == 1 || g_eeprom.config.setting.backlight_on_tx_rx == 3)
				BACKLIGHT_turn_on(backlight_tx_rx_time_secs);
			break;

		case FUNCTION_NEW_RECEIVE:
			APP_process_new_receive();
			break;

		case FUNCTION_RECEIVE:
			APP_process_rx();
			break;

		case FUNCTION_POWER_SAVE:
			if (!g_rx_idle_mode)
				APP_check_for_new_receive();
			break;

		default:
			break;
	}
}

void APP_process_power_save(void)
{
	bool power_save = true;

	#ifdef ENABLE_PANADAPTER
		if (g_eeprom.config.setting.panadapter)
			power_save = false;
	#endif

	if (g_monitor_enabled ||
		#ifdef ENABLE_FMRADIO
			g_fm_radio_mode ||
		#endif
		g_ptt_is_pressed                          ||
		g_fkey_pressed                            ||
		g_key_pressed != KEY_INVALID              ||
		g_eeprom.config.setting.battery_save_ratio == 0  ||
		g_scan_state_dir != SCAN_STATE_DIR_OFF    ||
		g_css_scan_mode != CSS_SCAN_MODE_OFF      ||
		g_current_display_screen != DISPLAY_MAIN  ||
		g_dtmf_call_state != DTMF_CALL_STATE_NONE ||
		g_flash_light_state == FLASHLIGHT_SOS)
	{
		power_save = false;
	}

	#ifdef ENABLE_NOAA
		if (IS_NOAA_CHANNEL(g_eeprom.config.setting.indices.vfo[0].screen) ||
			IS_NOAA_CHANNEL(g_eeprom.config.setting.indices.vfo[1].screen) ||
			g_noaa_mode)
		{
			power_save = false;
		}
	#endif

	if (!power_save)
	{
//		if (g_current_function == FUNCTION_POWER_SAVE && g_rx_idle_mode)
//			BK4819_RX_TurnOn();

		if (g_current_function == FUNCTION_POWER_SAVE)
			FUNCTION_Select(FUNCTION_RECEIVE);   // come out of power save mode

		g_power_save_pause_tick_10ms = power_save_pause_10ms;  // stay out of power save mode
	}
	else
	if (g_power_save_pause_done)
	{	// enter power save
		FUNCTION_Select(FUNCTION_POWER_SAVE);
	}

	g_power_save_pause_done = false;

	// **************************

	#ifdef ENABLE_VOICE
		if (g_voice_write_index != 0)
			return;
	#endif

	if (!g_power_save_expired || g_current_function != FUNCTION_POWER_SAVE)
		return;

	// wake up, enable RX then go back to sleep

	if (g_rx_idle_mode)
	{
		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
			//UART_SendText("ps wake up\r\n");
		#endif

		BK4819_Conditional_RX_TurnOn();

		#ifdef ENABLE_VOX
			if (g_eeprom.config.setting.vox_enabled)
				RADIO_enable_vox(g_eeprom.config.setting.vox_level);
		#endif

		if (APP_toggle_dual_watch_vfo())
			g_update_rssi = false;

		FUNCTION_Init();

		g_power_save_tick_10ms = power_save1_10ms; // come back here in a bit
		g_rx_idle_mode         = false;            // RX is awake
	}
	else
	if (g_eeprom.config.setting.dual_watch == DUAL_WATCH_OFF  ||
		g_scan_state_dir != SCAN_STATE_DIR_OFF ||
		g_css_scan_mode != CSS_SCAN_MODE_OFF   ||
		g_update_rssi)
	{	// go back to sleep

		#ifdef ENABLE_PANADAPTER
			if (!g_eeprom.config.setting.panadapter)
		#endif
		{
			APP_update_rssi(g_rx_vfo_num, false);

			// go back to sleep

			g_power_save_tick_10ms = g_eeprom.config.setting.battery_save_ratio * 10;
			g_rx_idle_mode         = true;

			BK4819_DisableVox();
			BK4819_Sleep();
			BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_RX_ENABLE, false);
		}
	}
	else
	if (APP_toggle_dual_watch_vfo())
	{
		g_update_rssi          = true;
		g_power_save_tick_10ms = power_save1_10ms;
	}

	g_power_save_expired = false;
}

// this is called once every 500ms
void APP_time_slice_500ms(void)
{
	bool exit_menu = false;

	if (g_key_input_count_down > 0)
	{
		if (--g_key_input_count_down == 0)
		{
			APP_cancel_user_input_modes();

			if (g_beep_to_play != BEEP_NONE)
			{
			#if defined(ENABLE_UART)
				if (g_serial_config_tick_500ms == 0)
			#endif
					AUDIO_PlayBeep(g_beep_to_play);
				g_beep_to_play = BEEP_NONE;
			}
		}
	}

	#ifdef ENABLE_AIRCOPY
		if (g_current_display_screen == DISPLAY_AIRCOPY)
		{	// we're in AIRCOPY mode

			BOARD_ADC_GetBatteryInfo(&g_usb_current_voltage, &g_usb_current);

			return;
		}
	#endif

	if (g_keypad_locked > 0)
		if (--g_keypad_locked == 0)
			g_update_display = true;

	#if defined(ENABLE_UART)
		if (g_serial_config_tick_500ms > 0)
			return;
	#endif

	if (g_current_function == FUNCTION_TRANSMIT)
	{
		if (g_tx_timer_tick_500ms < 6)
		{	// <= 3 seconds left
			if (g_tx_timer_tick_500ms & 1u)
				BK4819_start_tone(880, 10, true, true);
			else
				BK4819_stop_tones(true);
		}

		if (g_tx_timer_tick_500ms & 1u)
			g_update_display = true;
	}

	if (g_update_screen_tick_500ms > 0)
	{	// update display once every 500ms
		if (--g_update_screen_tick_500ms == 0)
		{
			RADIO_set_vfo_state(VFO_STATE_NORMAL);

			g_update_status  = true;
			g_update_display = true;
		}
		//g_update_status  = true;
		//g_update_display = true;
	}

	#ifdef ENABLE_MDC1200
		if (mdc1200_rx_ready_tick_500ms > 0)
		{
			if (--mdc1200_rx_ready_tick_500ms == 0)
			{
				if (g_center_line == CENTER_LINE_MDC1200)
					g_center_line = CENTER_LINE_NONE;
				g_update_display = true;
			}
		}
	#endif

	#ifdef ENABLE_DTMF_LIVE_DECODER
		if (g_dtmf_rx_live_timeout > 0)
		{
			if (g_center_line == CENTER_LINE_DTMF_DEC ||
				g_center_line == CENTER_LINE_NONE)  // wait till the center line is free for us to use before timing out

			if (--g_dtmf_rx_live_timeout == 0)
			{
				if (g_dtmf_rx_live[0] != 0)
				{
					memset(g_dtmf_rx_live, 0, sizeof(g_dtmf_rx_live));
					g_update_display   = true;
				}
			}
		}
	#endif

	if (g_menu_tick_10ms > 0)
		if (--g_menu_tick_10ms == 0)
			exit_menu = (g_current_display_screen == DISPLAY_MENU);	// exit menu mode

	if (g_dtmf_rx_timeout > 0)
		if (--g_dtmf_rx_timeout == 0)
			DTMF_clear_RX();

	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_tick_500ms > 0)
			g_fm_radio_tick_500ms--;

//		if (g_fm_radio_mode && g_current_display_screen == DISPLAY_FM && g_fm_scan_state_dir != FM_SCAN_STATE_DIR_OFF)
//			g_update_display = true;  // can't do this if not FM scanning, it causes audio clicks
	#endif

	if (g_reduced_service)
	{
		BOARD_ADC_GetBatteryInfo(&g_usb_current_voltage, &g_usb_current);

		if (g_usb_current > 500 || g_eeprom.calib.battery[3] < g_usb_current_voltage)
		{
			#ifdef ENABLE_OVERLAY
				overlay_FLASH_RebootToBootloader();
			#else
				NVIC_SystemReset();
			#endif
		}

		return;
	}

	g_battery_check_counter++;
	if ((g_battery_check_counter & 1u) == 0)
	{
		BOARD_ADC_GetBatteryInfo(&g_battery_voltages[g_battery_voltage_index++], &g_usb_current);
		if (g_battery_voltage_index > 3)
			g_battery_voltage_index = 0;
		BATTERY_GetReadings(true);
	}

	// update every 2 sec
	if ((g_battery_check_counter & 3) == 0)
	{
		if (g_charging_with_type_c || g_eeprom.config.setting.battery_text > 0)
			g_update_status = true;

		#ifdef ENABLE_SHOW_CHARGE_LEVEL
			if (g_charging_with_type_c)
				g_update_display = true;
		#endif
	}

#ifdef ENABLE_FMRADIO
	if (g_fm_scan_state_dir == FM_SCAN_STATE_DIR_OFF || g_ask_to_save)
#endif
	{
		if (g_css_scan_mode == CSS_SCAN_MODE_OFF   &&
		    g_scan_state_dir == SCAN_STATE_DIR_OFF &&
		   (g_current_display_screen != DISPLAY_SEARCH         ||
			g_search_css_state == SEARCH_CSS_STATE_FOUND  ||
			g_search_css_state == SEARCH_CSS_STATE_FAILED ||
			g_search_css_state == SEARCH_CSS_STATE_REPEAT))
		{
			#ifdef ENABLE_KEYLOCK
				if (g_eeprom.config.setting.auto_key_lock != 0 &&
					g_key_lock_tick_500ms > 0 &&
				   !g_dtmf_input_mode         &&
					g_input_box_index == 0    &&
					g_current_display_screen != DISPLAY_MENU)
				{
					if (--g_key_lock_tick_500ms == 0)
					{	// lock the keyboard
						g_eeprom.config.setting.key_lock = true;
						g_update_status = true;
					}
				}
			#endif

			if (exit_menu)
			{
				g_menu_tick_10ms = 0;

				if (g_eeprom.config.setting.backlight_time == 0)
					BACKLIGHT_turn_off();

				if (g_input_box_index > 0 || g_dtmf_input_mode)
					AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
/*
				if (g_current_display_screen == DISPLAY_SEARCH)
				{
					BK4819_StopScan();

					RADIO_configure_channel(0, VFO_CONFIGURE_RELOAD);
					RADIO_configure_channel(1, VFO_CONFIGURE_RELOAD);

					RADIO_setup_registers(true);
				}
*/
				DTMF_clear_input_box();

				g_fkey_pressed    = false;
				g_input_box_index = 0;

				g_ask_to_save     = false;
				g_ask_to_delete   = false;

				g_update_status   = true;
				g_update_display  = true;

				{
					gui_display_type_t disp = DISPLAY_INVALID;

					#ifdef ENABLE_FMRADIO
						if (g_fm_radio_mode &&
							g_current_function != FUNCTION_RECEIVE &&
							g_current_function != FUNCTION_TRANSMIT &&
							!g_monitor_enabled)
						{
							disp = DISPLAY_FM;
						}
					#endif

					if (disp == DISPLAY_INVALID)
					{
						#ifndef ENABLE_CODE_SEARCH_TIMEOUT
							if (g_current_display_screen != DISPLAY_SEARCH)
						#endif
								disp = DISPLAY_MAIN;
					}

					if (disp != DISPLAY_INVALID)
						GUI_SelectNextDisplay(disp);
				}
			}
		}
	}

//	if (g_current_function != FUNCTION_POWER_SAVE && g_current_function != FUNCTION_TRANSMIT)
//		APP_update_rssi(g_rx_vfo_num, false);

	if (g_low_battery)
	{
		g_low_battery_blink = ++g_low_battery_tick_10ms & 1;

		UI_DisplayBattery(0, g_low_battery_blink);

		if (g_current_function != FUNCTION_TRANSMIT)
		{	// not transmitting

			if (g_low_battery_tick_10ms < 30)
			{
				if (g_low_battery_tick_10ms == 29 && !g_charging_with_type_c)
					AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP);
			}
			else
			{
				g_low_battery_tick_10ms = 0;

				if (!g_charging_with_type_c)
				{	// not on charge

					AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP);

					#ifdef ENABLE_VOICE
						AUDIO_SetVoiceID(0, VOICE_ID_LOW_VOLTAGE);
					#endif

					if (g_battery_display_level == 0)
					{
						#ifdef ENABLE_VOICE
							AUDIO_PlaySingleVoice(true);
						#endif

						g_reduced_service = true;

						#ifdef ENABLE_PANADAPTER
							PAN_process_10ms();      // disable the panadapter
						#endif

						FUNCTION_Select(FUNCTION_POWER_SAVE);

						ST7565_HardwareReset();

						if (g_eeprom.config.setting.backlight_time < (ARRAY_SIZE(g_sub_menu_backlight) - 1))
							BACKLIGHT_turn_off();
					}
					#ifdef ENABLE_VOICE
						else
							AUDIO_PlaySingleVoice(false);
					#endif
				}
			}
		}
	}

	#ifdef ENABLE_FMRADIO
		if (g_current_function != FUNCTION_TRANSMIT)
		{
			if (g_fm_resume_tick_500ms > 0)
			{
				if (g_fm_radio_mode)
				{
					if (--g_fm_resume_tick_500ms == 0)
					{
						if (g_current_function != FUNCTION_RECEIVE && g_fm_radio_mode)
						{	// switch back to FM radio mode
							if (g_current_display_screen != DISPLAY_FM)
							{
								FM_turn_on();
								GUI_SelectNextDisplay(DISPLAY_FM);
							}
						}
					}
					GUI_SelectNextDisplay(DISPLAY_FM);
				}
				else
					FM_turn_off();
			}
		}

		if (g_fm_radio_mode && g_fm_radio_tick_500ms == 0)
			return;
	#endif

	if (g_current_function != FUNCTION_TRANSMIT)
	{
		if (g_dtmf_decode_ring_tick_500ms > 0)
		{	// make "ring-ring" sound
			g_dtmf_decode_ring_tick_500ms--;

			#ifdef ENABLE_DTMF_CALL_FLASH_LIGHT
				GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);    // light on
			#endif

			AUDIO_PlayBeep(BEEP_880HZ_200MS);

			#ifdef ENABLE_DTMF_CALL_FLASH_LIGHT
				GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);  // light off
			#endif
		}
	}
	else
		g_dtmf_decode_ring_tick_500ms = 0;

	if (g_dtmf_call_state  != DTMF_CALL_STATE_NONE &&
	    g_current_function != FUNCTION_TRANSMIT &&
	    g_current_function != FUNCTION_RECEIVE)
	{
		if (g_dtmf_auto_reset_time_500ms > 0)
		{
			if (--g_dtmf_auto_reset_time_500ms == 0)
			{
				if (g_dtmf_call_state == DTMF_CALL_STATE_RECEIVED && g_eeprom.config.setting.dtmf.auto_reset_time >= DTMF_HOLD_MAX)
					g_dtmf_call_state = DTMF_CALL_STATE_RECEIVED_STAY;     // keep message on-screen till a key is pressed
				else
					g_dtmf_call_state = DTMF_CALL_STATE_NONE;
				g_update_display  = true;
			}
		}

//		if (g_dtmf_call_state != DTMF_CALL_STATE_RECEIVED_STAY)
//		{
//			g_dtmf_call_state = DTMF_CALL_STATE_NONE;
//			g_update_display  = true;
//		}
	}

	if (g_dtmf_is_tx && g_dtmf_tx_stop_tick_500ms > 0)
	{
		if (--g_dtmf_tx_stop_tick_500ms == 0)
		{
			g_dtmf_is_tx     = false;
			g_update_display = true;
		}
	}
}

void APP_time_slice_10ms(void)
{
	g_flash_light_blink_tick_10ms++;

	if (g_backlight_tick_10ms > 0 &&
	   !g_ask_to_save &&
	    g_css_scan_mode == CSS_SCAN_MODE_OFF &&
	    g_current_display_screen != DISPLAY_AIRCOPY)
	{	// don't turn off backlight if user is in backlight menu option
		if (g_current_display_screen != DISPLAY_MENU || g_menu_cursor != MENU_AUTO_BACKLITE)
			if (g_eeprom.config.setting.backlight_time < (ARRAY_SIZE(g_sub_menu_backlight) - 1))
				if (--g_backlight_tick_10ms <= BACKLIGHT_MAX_BRIGHTNESS)
					BACKLIGHT_set_brightness(g_backlight_tick_10ms);
	}

	if (g_flag_save_vfo)
	{
		SETTINGS_save_vfo_indices();
		g_flag_save_vfo = false;
	}

	if (g_flag_save_settings)
	{
		SETTINGS_save();
		g_flag_save_settings = false;
	}

	if (g_request_display_screen != DISPLAY_INVALID)
	{
		GUI_SelectNextDisplay(g_request_display_screen);
		g_request_display_screen = DISPLAY_INVALID;
	}

	if (g_update_display)
		GUI_DisplayScreen();

	if (g_update_status)
		UI_DisplayStatus(false);

	#ifdef ENABLE_AIRCOPY
		if (g_current_display_screen == DISPLAY_AIRCOPY)
		{	// we're in AIRCOPY mode

			if (g_aircopy_state == AIRCOPY_TX)
				AIRCOPY_process_fsk_tx_10ms();

			AIRCOPY_process_fsk_rx_10ms();

			APP_process_keys();
			return;
		}
	#endif

	if (g_flag_save_channel)
	{
		SETTINGS_save_channel(g_tx_vfo->channel_save, g_eeprom.config.setting.tx_vfo_num, g_tx_vfo, g_flag_save_channel ? 1 : 0);
		g_flag_save_channel = false;

		RADIO_configure_channel(g_eeprom.config.setting.tx_vfo_num, VFO_CONFIGURE);

		RADIO_setup_registers(true);

		if (g_monitor_enabled)
			APP_start_listening();

		GUI_SelectNextDisplay(DISPLAY_MAIN);
	}

	#ifdef ENABLE_UART
		if (UART_IsCommandAvailable())
		{
			__disable_irq();
			UART_HandleCommand();
			__enable_irq();
		}
	#endif

#if defined(ENABLE_UART)
	if (g_current_function == FUNCTION_TRANSMIT && (g_tx_timeout_reached || g_serial_config_tick_500ms > 0))
#else
	if (g_current_function == FUNCTION_TRANSMIT && g_tx_timeout_reached)
#endif
	{	// transmitter timed out or must de-key

		BK4819_stop_tones(true);

		g_tx_timeout_reached = false;
		g_flag_end_tx        = true;

		APP_end_tx();

		AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);

		RADIO_set_vfo_state(VFO_STATE_TIMEOUT);

		GUI_DisplayScreen();
	}

	#ifdef ENABLE_UART
		if (g_serial_config_tick_500ms > 0)
			return;
	#endif

	#ifdef ENABLE_AM_FIX
		#ifdef ENABLE_PANADAPTER
			if (!PAN_scanning())
		#endif
				if (g_rx_vfo->channel.mod_mode != MOD_MODE_FM && g_eeprom.config.setting.am_fix)
					AM_fix_10ms(g_rx_vfo_num);
	#endif

	#ifdef ENABLE_FMRADIO
		if (g_flag_save_fm)
		{
			SETTINGS_save_fm();
			g_flag_save_fm = false;
		}
	#endif

#if defined(ENABLE_UART)
	if (g_reduced_service || g_serial_config_tick_500ms > 0)
#else
	if (g_reduced_service)
#endif
	{
		if (g_current_function == FUNCTION_TRANSMIT)
			g_tx_timeout_reached = true;

		return;
	}

	// ***************************************************

	#ifdef ENABLE_BOOT_BEEPS
		if (g_boot_tick_10ms > 0 && (g_boot_tick_10ms % 25) == 0)
			AUDIO_PlayBeep(BEEP_880HZ_40MS_OPTIONAL);
	#endif

#ifdef ENABLE_PANADAPTER
	if (!PAN_scanning())
#endif
	{
		// 1of11
//		if (g_update_rssi)
		if (g_current_function != FUNCTION_POWER_SAVE && g_current_function != FUNCTION_TRANSMIT)
			if (!g_flag_save_channel)
				APP_update_rssi(g_rx_vfo_num, false);

		if (g_current_function != FUNCTION_POWER_SAVE || !g_rx_idle_mode)
			APP_process_radio_interrupts();
	}

	APP_process_functions();

	APP_process_flash_light_10ms();

	if (g_current_function == FUNCTION_TRANSMIT)
	{	// transmitting
		#ifdef ENABLE_TX_AUDIO_BAR
			if (g_eeprom.config.setting.mic_bar && (g_flash_light_blink_tick_10ms % (150 / 10)) == 0 && !g_update_display) // once every 150ms
				UI_DisplayAudioBar(true);
		#endif
	}

	#ifdef ENABLE_VOICE
		if (g_flag_play_queued_voice)
		{
			g_flag_play_queued_voice = false;
			AUDIO_PlayQueuedVoice();
		}
	#endif

	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_mode && g_fm_radio_tick_500ms > 0)
			return;
	#endif

	#ifdef ENABLE_VOX
		if (g_vox_resume_tick_10ms > 0)
			g_vox_resume_tick_10ms--;

		if (g_vox_pause_tick_10ms > 0)
			g_vox_pause_tick_10ms--;

		if (g_eeprom.config.setting.vox_enabled)
			APP_process_vox();
	#endif

	APP_process_transmit();

	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_mode && g_fm_restore_tick_10ms > 0)
		{
			if (--g_fm_restore_tick_10ms == 0)
			{	// switch back to FM radio mode
				FM_turn_on();
				GUI_SelectNextDisplay(DISPLAY_FM);
			}
		}

		if (g_fm_schedule                            &&
			g_fm_scan_state_dir != FM_SCAN_STATE_DIR_OFF &&
		   !g_monitor_enabled                        &&
			g_current_function != FUNCTION_RECEIVE   &&
			g_current_function != FUNCTION_TRANSMIT)
		{	// switch to FM radio mode
			FM_scan();
			g_fm_schedule = false;
		}
	#endif

	#ifdef ENABLE_PANADAPTER
		PAN_process_10ms();
	#endif

	APP_process_power_save();

	APP_process_scan();

	APP_process_search();

	APP_process_keys();
}

static void APP_process_key(const key_code_t Key, const bool key_pressed, const bool key_held)
{
	bool flag = false;

	#ifdef ENABLE_AIRCOPY
		if (g_current_display_screen == DISPLAY_AIRCOPY)
		{
			AIRCOPY_process_key(key, key_pressed, key_held);
			return;
		}
	#endif

	if (Key == KEY_INVALID && !key_pressed && !key_held)
		return;

	// reset the state so as to remove it from the screen
	if (Key != KEY_INVALID && Key != KEY_PTT)
		RADIO_set_vfo_state(VFO_STATE_NORMAL);

#if 1
	// remember the current backlight state (on / off)
	if (Key == KEY_EXIT && !BACKLIGHT_is_on() && g_eeprom.config.setting.backlight_time > 0)
	{	// just turn the back light on for now so the user can see what's what
		if (!key_pressed && !key_held)
		{	// key has been released
			BACKLIGHT_turn_on(0);
		}
		g_beep_to_play = BEEP_NONE;
		return;
	}
#endif

	// turn the backlight on
	if (key_pressed)
		if (Key != KEY_PTT)
			BACKLIGHT_turn_on(0);

	if (g_current_function == FUNCTION_POWER_SAVE)
		FUNCTION_Select(FUNCTION_FOREGROUND);

	// stay awake - for now
	g_power_save_pause_tick_10ms = power_save_pause_10ms;
	g_power_save_pause_done      = false;

	#ifdef ENABLE_KEYLOCK
		// keep the auto keylock at bay
		if (g_eeprom.config.setting.auto_key_lock != 0)
			g_key_lock_tick_500ms = key_lock_timeout_500ms;
	#endif

	if (g_fkey_pressed && (Key == KEY_PTT || Key == KEY_EXIT || Key == KEY_SIDE1 || Key == KEY_SIDE2))
	{	// cancel the F-key
		g_fkey_pressed  = false;
		g_update_status = true;
	}

	// ********************

#ifdef ENABLE_KEYLOCK
	if (g_eeprom.config.setting.key_lock && g_current_function != FUNCTION_TRANSMIT && Key != KEY_PTT)
	{	// keyboard is locked

		if (Key == KEY_F)
		{	// function/key-lock key

			if (!key_pressed)
				return;

			if (key_held)
			{	// unlock the keypad
				g_eeprom.config.setting.key_lock = false;
				g_request_save_settings = true;
				g_update_status         = true;

				#ifdef ENABLE_VOICE
					g_another_voice_id = VOICE_ID_UNLOCK;
				#endif

				AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
			}

			return;
		}

		if (Key != KEY_SIDE1 && Key != KEY_SIDE2)
		{
			if (!key_pressed || key_held)
				return;

			// keypad is locked, let the user know
			g_keypad_locked  = 4;          // 2 second pop-up
			g_update_display = true;

			#ifdef ENABLE_FMRADIO
				if (!g_fm_radio_mode)  // don't beep when the FM radio is on, it cause bad gaps and loud clicks
			#endif
					g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;

			return;
		}
	}
#endif

	// ********************

	if (Key == KEY_EXIT && key_held && key_pressed)
	{	// exit key held pressed

		#ifdef ENABLE_DTMF_LIVE_DECODER
			// clear the live DTMF decoder
			if (g_dtmf_rx_live[0] != 0)
			{
				memset(g_dtmf_rx_live, 0, sizeof(g_dtmf_rx_live));
				g_dtmf_rx_live_timeout = 0;
				g_update_display       = true;
			}
		#endif

		// cancel user input
		APP_cancel_user_input_modes();
	}

	if (key_pressed && g_current_display_screen == DISPLAY_MENU)
		g_menu_tick_10ms = menu_timeout_500ms;

	// cancel the ringing
	if (key_pressed && g_dtmf_decode_ring_tick_500ms > 0)
		g_dtmf_decode_ring_tick_500ms = 0;

	// ********************

	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wtype-limits"

//	if (g_scan_state_dir != SCAN_STATE_DIR_OFF || g_css_scan_mode != CSS_SCAN_MODE_OFF)
	if (g_css_scan_mode != CSS_SCAN_MODE_OFF)
	{	// FREQ/CTCSS/CDCSS scanning

		if ((Key >= KEY_0 && Key <= KEY_9) || Key == KEY_F)
		{
			if (key_pressed && !key_held)
				AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
			return;
		}
	}

	#pragma GCC diagnostic pop

	// ********************

	if (g_ptt_was_pressed)
	{
		if (Key == KEY_PTT)
		{
			flag = key_held;
			if (!key_pressed)
			{
				flag = true;
				g_ptt_was_pressed = false;
			}
		}
//		g_ptt_was_pressed = false;

		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//			UART_printf("proc key 1 %3u %u %u %u %u\r\n", Key, key_pressed, key_held, g_fkey_pressed, flag);
		#endif
	}

	// this bit of code has caused soooooo many problems due
	// to this causing key releases to be ignored :( .. 1of11
	if (g_ptt_was_released)
	{
//		if (Key != KEY_PTT)
		if (Key == KEY_PTT)
		{
			if (key_held)
				flag = true;
			if (key_pressed)	// I now use key released for button press detections
			{
				flag = true;
				g_ptt_was_released = false;
			}
		}
//		g_ptt_was_released = false;

		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//			UART_printf("proc key 2 %3u %u %u %u %u\r\n", Key, key_pressed, key_held, g_fkey_pressed, flag);
		#endif
	}

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		UART_printf("proc key 3 %3u %u %u %u %u\r\n", Key, key_pressed, key_held, g_fkey_pressed, flag);
	#endif

	if (!flag)  // this flag is responsible for keys being ignored :(
	{
		if (g_current_function == FUNCTION_TRANSMIT)
		{	// transmitting

			#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
				if (g_alarm_state == ALARM_STATE_OFF)
			#endif
			{
				char Code;

				if (Key == KEY_PTT)
				{
					GENERIC_Key_PTT(key_pressed);
					goto Skip;
				}

				if (Key == KEY_SIDE2)
				{	// transmit 1750Hz tone
					Code = 0xFE;
				}
				else
				{
					Code = DTMF_GetCharacter(Key - KEY_0);
					if (Code == 0xFF)
						goto Skip;

					// transmit DTMF keys
				}

				if (!key_pressed || key_held)
				{
					if (!key_pressed)
					{
						GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

						BK4819_ExitDTMF_TX(false);

						if (g_eeprom.config.setting.enable_scrambler)
							BK4819_set_scrambler(g_current_vfo->channel.scrambler);
						else
							BK4819_set_scrambler(0);
					}
				}
				else
				{
					if (g_eeprom.config.setting.dtmf.side_tone)
					{	// user will here the DTMF tones in speaker
						GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
					}

					BK4819_set_scrambler(0);

					if (Code == 0xFE)
						BK4819_TransmitTone(g_eeprom.config.setting.dtmf.side_tone, 1750);
					else
						BK4819_PlayDTMFEx(g_eeprom.config.setting.dtmf.side_tone, Code);
				}
			}
			#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
				else
				if ((!key_held && key_pressed) || (g_alarm_state == ALARM_STATE_TX1750 && key_held && !key_pressed))
				{
					APP_alarm_off();

					if (g_eeprom.config.setting.repeater_tail_tone_elimination == 0)
						FUNCTION_Select(FUNCTION_FOREGROUND);
					else
						g_rtte_count_down = g_eeprom.config.setting.repeater_tail_tone_elimination * 10;

					if (Key == KEY_PTT)
						g_ptt_was_pressed  = true;
					else
//					if (!key_held)
						g_ptt_was_released = true;
				}
			#endif
		}
		else
		if (Key != KEY_SIDE1 && Key != KEY_SIDE2)
		{
			switch (g_current_display_screen)
			{
				case DISPLAY_MAIN:
					MAIN_process_key(Key, key_pressed, key_held);
					break;

				#ifdef ENABLE_FMRADIO
					case DISPLAY_FM:
						FM_process_key(Key, key_pressed, key_held);
						break;
				#endif

				case DISPLAY_MENU:
					MENU_process_key(Key, key_pressed, key_held);
					break;

				case DISPLAY_SEARCH:
					SEARCH_process_key(Key, key_pressed, key_held);
					break;

				#ifdef ENABLE_AIRCOPY
					case DISPLAY_AIRCOPY:
						AIRCOPY_process_key(Key, key_pressed, key_held);
						break;
				#endif

				case DISPLAY_INVALID:
				default:
					break;
			}
		}
		else
		#ifdef ENABLE_AIRCOPY
			if (g_current_display_screen != DISPLAY_SEARCH && g_current_display_screen != DISPLAY_AIRCOPY)
		#else
			if (g_current_display_screen != DISPLAY_SEARCH)
		#endif
		{
			ACTION_process(Key, key_pressed, key_held);
		}
		else
		{
			#ifdef ENABLE_FMRADIO
				if (!g_fm_radio_mode)
			#endif
					if (!key_held && key_pressed)
						g_beep_to_play = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		}
	}

Skip:
	if (g_beep_to_play != BEEP_NONE)
	{
		AUDIO_PlayBeep(g_beep_to_play);
		g_beep_to_play = BEEP_NONE;
	}

	if (g_flag_accept_setting)
	{
		g_menu_tick_10ms = menu_timeout_500ms;

		MENU_AcceptSetting();

		g_update_menu   = true;
		g_flag_accept_setting = false;
	}

	if (g_search_flag_stop_scan)
	{
		BK4819_StopScan();
		g_search_flag_stop_scan = false;
	}

	if (g_request_save_settings)
	{
		if (!key_held)
			SETTINGS_save();
		else
			g_flag_save_settings = 1;

		g_request_save_settings = false;
		g_update_status         = true;
	}

	#ifdef ENABLE_FMRADIO
		if (g_request_save_fm)
		{
			if (!key_held)
				SETTINGS_save_fm();
			else
				g_flag_save_fm = true;

			g_request_save_fm = false;
		}
	#endif

	if (g_request_save_vfo)
	{
		if (!key_held)
			SETTINGS_save_vfo_indices();
		else
			g_flag_save_vfo = true;

		g_request_save_vfo = false;
	}

	if (g_request_save_channel > 0)
	{
		if (!key_held)
		{
			SETTINGS_save_channel(g_tx_vfo->channel_save, g_eeprom.config.setting.tx_vfo_num, g_tx_vfo, g_request_save_channel);

			if (g_current_display_screen != DISPLAY_SEARCH)
				if (g_vfo_configure_mode == VFO_CONFIGURE_NONE)  // don't wipe previous variable setting
					g_vfo_configure_mode = VFO_CONFIGURE;
		}
		else
		{
			g_flag_save_channel = g_request_save_channel;

			if (g_request_display_screen == DISPLAY_INVALID)
				g_request_display_screen = DISPLAY_MAIN;
		}

		g_request_save_channel = 0;
	}

	if (g_vfo_configure_mode != VFO_CONFIGURE_NONE)
	{
		if (g_flag_reset_vfos)
		{
			RADIO_configure_channel(0, g_vfo_configure_mode);
			RADIO_configure_channel(1, g_vfo_configure_mode);
		}
		else
		{
			RADIO_configure_channel(g_eeprom.config.setting.tx_vfo_num, g_vfo_configure_mode);
		}

		if (g_request_display_screen == DISPLAY_INVALID)
			g_request_display_screen = DISPLAY_MAIN;

		g_flag_reconfigure_vfos = true;
		g_vfo_configure_mode    = VFO_CONFIGURE_NONE;
		g_flag_reset_vfos       = false;
	}

	if (g_flag_reconfigure_vfos)
	{
		RADIO_select_vfos();

		#ifdef ENABLE_NOAA
			RADIO_ConfigureNOAA();
		#endif

		RADIO_setup_registers(true);

//		g_tx_vfo->freq_in_channel = SETTINGS_find_channel(frequency);

		g_dtmf_auto_reset_time_500ms = 0;
		g_dtmf_call_state            = DTMF_CALL_STATE_NONE;
		g_dtmf_tx_stop_tick_500ms    = 0;
		g_dtmf_is_tx                 = false;

		g_vfo_rssi_bar_level[0] = 0;
		g_vfo_rssi_bar_level[1] = 0;

		if (g_squelch_open || g_monitor_enabled)
			APP_start_listening();

		g_flag_reconfigure_vfos = false;
	}

	if (g_update_menu)
	{
		g_update_menu = false;
		g_menu_tick_10ms   = menu_timeout_500ms;

		MENU_ShowCurrentSetting();
	}

	if (g_search_flag_start_scan)
	{
		g_search_flag_start_scan = false;

		#ifdef ENABLE_VOICE
			AUDIO_SetVoiceID(0, VOICE_ID_SCANNING_BEGIN);
			AUDIO_PlaySingleVoice(true);
		#endif

		SEARCH_Start();
	}

	if (g_flag_prepare_tx)
	{
		RADIO_PrepareTX();
		g_flag_prepare_tx = false;
	}

	#ifdef ENABLE_VOICE
		if (g_another_voice_id != VOICE_ID_INVALID)
		{
			if (g_another_voice_id < 76)
				AUDIO_SetVoiceID(0, g_another_voice_id);
			AUDIO_PlaySingleVoice(false);
			g_another_voice_id = VOICE_ID_INVALID;
		}
	#endif
}
