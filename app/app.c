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
#include <stdlib.h>  // abs()

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
#include "driver/uart.h"
#include "am_fix.h"
#include "dtmf.h"
#include "external/printf/printf.h"
#include "frequencies.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
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

// original QS front end register settings
const uint8_t orig_lna_short = 3;   //   0dB
const uint8_t orig_lna       = 2;   // -14dB
const uint8_t orig_mixer     = 3;   //   0dB
const uint8_t orig_pga       = 6;   //  -3dB

static void APP_process_key(const key_code_t Key, const bool key_pressed, const bool key_held);

static void APP_update_rssi(const int vfo)
{
	int16_t rssi = BK4819_GetRSSI();

	#ifdef ENABLE_AM_FIX
		// add RF gain adjust compensation
		if (g_eeprom.vfo_info[vfo].am_mode && g_setting_am_fix)
			rssi -= rssi_gain_diff[vfo];
	#endif

	if (g_current_rssi[vfo] == rssi)
		return;     // no change

	g_current_rssi[vfo] = rssi;

	UI_update_rssi(rssi, vfo);
}

static void APP_check_for_new_receive(void)
{
	if (!g_squelch_open)
		return;

	// squelch is open

	if (g_scan_state_dir == SCAN_STATE_DIR_OFF)
	{	// not RF scanning

		if (g_css_scan_mode != CSS_SCAN_MODE_OFF && g_rx_reception_mode == RX_MODE_NONE)
		{	// CTCSS/DTS scanning

			g_scan_pause_10ms      = scan_pause_code_10ms;
			g_scan_pause_time_mode = false;
			g_rx_reception_mode    = RX_MODE_DETECTED;
		}

		if (g_eeprom.dual_watch == DUAL_WATCH_OFF)
		{	// dual watch is disabled

			#ifdef ENABLE_NOAA
				if (g_is_noaa_mode)
				{
					g_noaa_count_down_10ms = noaa_count_down_3_10ms;
					g_schedule_noaa        = false;
				}
			#endif

			goto done;
		}

		// dual watch is enabled and we're RX'ing a signal

		if (g_rx_reception_mode != RX_MODE_NONE)
			goto done;

		g_dual_watch_delay_10ms = g_eeprom.scan_hold_time_500ms * 50;
		g_scan_pause_time_mode  = false;

		g_update_status = true;
	}
	else
	{	// RF scanning
		if (g_rx_reception_mode != RX_MODE_NONE)
			goto done;

		g_scan_pause_10ms      = scan_pause_chan_10ms;
		g_scan_pause_time_mode = false;
	}

	g_rx_reception_mode = RX_MODE_DETECTED;

done:
	if (g_current_function != FUNCTION_NEW_RECEIVE)
	{
		FUNCTION_Select(FUNCTION_NEW_RECEIVE);

		APP_update_rssi(g_eeprom.rx_vfo);
		g_update_rssi = true;
	}
}

static void APP_process_new_receive(void)
{
	bool flag;

	if (!g_squelch_open)
	{	// squelch is closed

		if (g_dtmf_rx_index > 0)
			DTMF_clear_RX();

		if (g_current_function != FUNCTION_FOREGROUND)
		{
			FUNCTION_Select(FUNCTION_FOREGROUND);
			g_update_display = true;
		}

		return;
	}

	flag = (g_scan_state_dir == SCAN_STATE_DIR_OFF && g_current_code_type == CODE_TYPE_NONE);

	#ifdef ENABLE_NOAA
		if (IS_NOAA_CHANNEL(g_rx_vfo->channel_save) && g_noaa_count_down_10ms > 0)
		{
			g_noaa_count_down_10ms = 0;
			flag = true;
		}
	#endif

	if (g_ctcss_lost && g_current_code_type == CODE_TYPE_CONTINUOUS_TONE)
	{
		flag          = true;
		g_found_ctcss = false;
	}

	if (g_cdcss_lost && g_cdcss_code_type == CDCSS_POSITIVE_CODE && (g_current_code_type == CODE_TYPE_DIGITAL || g_current_code_type == CODE_TYPE_REVERSE_DIGITAL))
	{
		g_found_cdcss = false;
	}
	else
	if (!flag)
		return;

	if (g_scan_state_dir == SCAN_STATE_DIR_OFF && g_css_scan_mode == CSS_SCAN_MODE_OFF)
	{	// not code scanning

		#ifdef ENABLE_KILL_REVIVE
			if (g_rx_vfo->dtmf_decoding_enable || g_setting_radio_disabled)
		#else
			if (g_rx_vfo->dtmf_decoding_enable)
		#endif
		{	// DTMF DCD is enabled

			DTMF_HandleRequest();

			if (g_dtmf_call_state == DTMF_CALL_STATE_NONE)
			{
				if (g_rx_reception_mode == RX_MODE_DETECTED)
				{
					g_dual_watch_delay_10ms = g_eeprom.scan_hold_time_500ms * 50;
					g_rx_reception_mode     = RX_MODE_LISTENING;

					g_update_status  = true;
					g_update_display = true;

					return;
				}
			}
		}
	}

	APP_start_listening(g_monitor_enabled ? FUNCTION_MONITOR : FUNCTION_RECEIVE, false);
}

enum end_of_rx_mode_e {
	END_OF_RX_MODE_NONE = 0,
	END_OF_RX_MODE_END,
	END_OF_RX_MODE_TTE
};
typedef enum end_of_rx_mode_e end_of_rx_mode_t;

static void APP_process_rx(void)
{
	end_of_rx_mode_t Mode = END_OF_RX_MODE_NONE;

	if (g_flag_tail_tone_elimination_complete)
	{
		Mode = END_OF_RX_MODE_END;
		goto Skip;
	}

	if (g_scan_state_dir != SCAN_STATE_DIR_OFF) // && IS_FREQ_CHANNEL(g_scan_next_channel))  
	{
		if (g_squelch_open)
		{
			switch (g_eeprom.scan_resume_mode)
			{
				case SCAN_RESUME_TIME:     // stay only for a limited time
					break;
				case SCAN_RESUME_CARRIER:  // stay untill the carrier goes away
					g_scan_pause_10ms      = g_eeprom.scan_hold_time_500ms * 50;
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
			if (g_found_ctcss && g_found_ctcss_count_down_10ms == 0)
			{
				g_found_ctcss = false;
				g_found_cdcss = false;
				Mode          = END_OF_RX_MODE_END;
				goto Skip;
			}
			break;

		case CODE_TYPE_DIGITAL:
		case CODE_TYPE_REVERSE_DIGITAL:
			if (g_found_cdcss && g_found_cdcss_count_down_10ms == 0)
			{
				g_found_ctcss = false;
				g_found_cdcss = false;
				Mode          = END_OF_RX_MODE_END;
				goto Skip;
			}
			break;
	}

	if (g_squelch_open)
	{
		if (g_setting_backlight_on_tx_rx >= 2)
			backlight_turn_on(backlight_tx_rx_time_500ms); // keep the backlight on while we're receiving

		if (!g_end_of_rx_detected_maybe && IS_NOT_NOAA_CHANNEL(g_rx_vfo->channel_save))
		{
			switch (g_current_code_type)
			{
				case CODE_TYPE_NONE:
					if (g_eeprom.squelch_level)
					{
						if (g_cxcss_tail_found)
						{
							Mode = END_OF_RX_MODE_TTE;
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
						g_found_ctcss = true;
						g_found_ctcss_count_down_10ms = 100;   // 1 sec
					}

					if (g_cxcss_tail_found)
					{
						Mode = END_OF_RX_MODE_TTE;
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
						g_found_cdcss = true;
						g_found_cdcss_count_down_10ms = 100;   // 1 sec
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
	     g_eeprom.tail_note_elimination &&
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
					g_noaa_count_down_10ms = 3000 / 10;         // 3 sec
			#endif

			g_update_display = true;

			break;

		case END_OF_RX_MODE_TTE:
		
			if (g_eeprom.tail_note_elimination)
			{
				GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

				g_tail_tone_elimination_count_down_10ms = 20;
				g_flag_tail_tone_elimination_complete   = false;
				g_end_of_rx_detected_maybe              = true;
				g_speaker_enabled                        = false;
			}
			
			break;
	}

	if (g_scan_state_dir != SCAN_STATE_DIR_OFF)
	{	// we're RF scanning

		switch (g_eeprom.scan_resume_mode)
		{
			case SCAN_RESUME_TIME:     // stay only for a limited time
				break;
			case SCAN_RESUME_CARRIER:  // stay untill the carrier goes away
				g_scan_pause_10ms      = g_eeprom.scan_hold_time_500ms * 50;
				g_scan_pause_time_mode = false;
				break;
			case SCAN_RESUME_STOP:     // stop scan once we find any signal
				APP_stop_scan();
				break;
		}
	}
}

static void APP_process_function(void)
{
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

	switch (g_current_function)
	{
		case FUNCTION_FOREGROUND:
			APP_check_for_new_receive();
			break;

		case FUNCTION_TRANSMIT:
			if (g_setting_backlight_on_tx_rx == 1 || g_setting_backlight_on_tx_rx == 3)
				backlight_turn_on(backlight_tx_rx_time_500ms);
			break;

		case FUNCTION_NEW_RECEIVE:
			APP_process_new_receive();

		case FUNCTION_MONITOR:
			break;

		case FUNCTION_RECEIVE:
			APP_process_rx();
			break;

		case FUNCTION_POWER_SAVE:
			if (!g_rx_idle_mode)
				APP_check_for_new_receive();
			break;

		case FUNCTION_PANADAPTER:
			break;
	}

	#pragma GCC diagnostic pop
}

bool APP_start_listening(function_type_t Function, const bool reset_am_fix)
{
	const unsigned int chan = g_eeprom.rx_vfo;
//	const unsigned int chan = g_rx_vfo->channel_save;

	#ifdef ENABLE_KILL_REVIVE
		if (g_setting_radio_disabled)
			return false;
	#endif

	BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, true);   // LED on

	if (g_setting_backlight_on_tx_rx >= 2)
		backlight_turn_on(backlight_tx_rx_time_500ms);

	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_mode)
			BK1080_Init(0, false);
	#endif

	// clear the other vfo's rssi level (to hide the antenna symbol)
	g_vfo_rssi_bar_level[(chan + 1) & 1u] = 0;

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
	g_speaker_enabled = true;

	if (g_scan_state_dir != SCAN_STATE_DIR_OFF)
	{	// we're RF scanning

		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

		switch (g_eeprom.scan_resume_mode)
		{
			case SCAN_RESUME_TIME:
				if (!g_scan_pause_time_mode)
				{
					g_scan_pause_10ms      = g_eeprom.scan_hold_time_500ms * 50;
					g_scan_pause_time_mode = true;
				}
				break;
			case SCAN_RESUME_CARRIER:
				g_scan_pause_10ms      = g_eeprom.scan_hold_time_500ms * 50;
				g_scan_pause_time_mode = false;
				break;
			case SCAN_RESUME_STOP:
				g_scan_pause_10ms      = 0;
				g_scan_pause_time_mode = false;
				break;
		}

		#pragma GCC diagnostic pop
	}

	#ifdef ENABLE_NOAA
		if (IS_NOAA_CHANNEL(g_rx_vfo->channel_save) && g_is_noaa_mode)
		{
			g_rx_vfo->channel_save        = g_noaa_channel + NOAA_CHANNEL_FIRST;
			g_rx_vfo->p_rx->frequency     = NOAA_FREQUENCY_TABLE[g_noaa_channel];
			g_rx_vfo->p_tx->frequency     = NOAA_FREQUENCY_TABLE[g_noaa_channel];
			g_eeprom.screen_channel[chan] = g_rx_vfo->channel_save;
			g_noaa_count_down_10ms        = 5000 / 10;   // 5 sec
			g_schedule_noaa               = false;
		}
	#endif

	if (g_css_scan_mode != CSS_SCAN_MODE_OFF)
		g_css_scan_mode = CSS_SCAN_MODE_FOUND;

	if (g_scan_state_dir == SCAN_STATE_DIR_OFF &&
	    g_css_scan_mode == CSS_SCAN_MODE_OFF &&
	    g_eeprom.dual_watch != DUAL_WATCH_OFF)
	{	// dual watch is active

		g_dual_watch_delay_10ms = g_eeprom.scan_hold_time_500ms * 50;
		g_rx_vfo_is_active      = true;

		g_update_status = true;
	}

#ifdef ENABLE_AM_FIX
	{	// RF RX front end gain

		// original setting
		uint16_t lna_short = orig_lna_short;
		uint16_t lna       = orig_lna;
		uint16_t mixer     = orig_mixer;
		uint16_t pga       = orig_pga;

		if (g_rx_vfo->am_mode && g_setting_am_fix)
		{	// AM RX mode
			if (reset_am_fix)
				AM_fix_reset(chan);   // TODO: only reset it when moving channel/frequency .. or do we ???
			AM_fix_10ms(chan);
		}
		else
			BK4819_WriteRegister(0x13, (lna_short << 8) | (lna << 5) | (mixer << 3) | (pga << 0));
	}
#else
	(void)reset_am_fix;
#endif

	// AF gain - original QS values
	BK4819_WriteRegister(0x48,
		(11u << 12)                 |     // ??? .. 0 to 15, doesn't seem to make any difference
		( 0u << 10)                 |     // AF Rx Gain-1
		(g_eeprom.volume_gain << 4) |     // AF Rx Gain-2
		(g_eeprom.dac_gain    << 0));     // AF DAC Gain (after Gain-1 and Gain-2)

	#ifdef ENABLE_VOICE
		#ifdef MUTE_AUDIO_FOR_VOICE
			if (g_voice_write_index == 0)
				BK4819_SetAF(g_rx_vfo->am_mode ? BK4819_AF_AM : BK4819_AF_FM);
		#else
			BK4819_SetAF(g_rx_vfo->am_mode ? BK4819_AF_AM : BK4819_AF_FM);
		#endif
	#else
		BK4819_SetAF(g_rx_vfo->am_mode ? BK4819_AF_AM : BK4819_AF_FM);
	#endif

	FUNCTION_Select(Function);

	#ifdef ENABLE_FMRADIO
		if (Function == FUNCTION_MONITOR || g_fm_radio_mode)
	#else
		if (Function == FUNCTION_MONITOR)
	#endif
	{	// monitor mode (open squelch)
		if (g_screen_to_display != DISPLAY_MENU)     // 1of11 .. don't close the menu
			GUI_SelectNextDisplay(DISPLAY_MAIN);
	}
	else
		g_update_display = true;

	g_update_status = true;
	
	return true;
}

uint32_t APP_set_frequency_by_step(vfo_info_t *pInfo, int8_t Step)
{
	uint32_t Frequency = pInfo->freq_config_rx.frequency + (Step * pInfo->step_freq);

	if (pInfo->step_freq == 833)
	{
		const uint32_t Lower = FREQ_BAND_TABLE[pInfo->band].lower;
		const uint32_t Delta = Frequency - Lower;
		uint32_t       Base  = (Delta / 2500) * 2500;
		const uint32_t Index = ((Delta - Base) % 2500) / 833;

		if (Index == 2)
			Base++;

		Frequency = Lower + Base + (Index * 833);
	}

//	if (Frequency >= FREQ_BAND_TABLE[pInfo->band].upper)
//		Frequency =  FREQ_BAND_TABLE[pInfo->band].lower;
//	else
//	if (Frequency < FREQ_BAND_TABLE[pInfo->band].lower)
//		Frequency = FREQUENCY_floor_to_step(FREQ_BAND_TABLE[pInfo->band].upper, pInfo->step_freq, FREQ_BAND_TABLE[pInfo->band].lower);
	Frequency = FREQUENCY_wrap_to_step_band(Frequency, pInfo->step_freq, pInfo->band);

	return Frequency;
}

void APP_stop_scan(void)
{
	if (g_scan_state_dir == SCAN_STATE_DIR_OFF)
		return;   // but, but, we weren't doing anything !

	// yes we were

	g_scan_state_dir = SCAN_STATE_DIR_OFF;

	// 1of11
	if (g_scan_pause_time_mode ||
		g_scan_pause_10ms > (200 / 10) ||
	    g_current_function == FUNCTION_RECEIVE ||
	    g_current_function == FUNCTION_MONITOR ||
	    g_current_function == FUNCTION_NEW_RECEIVE)
	{	// stay where we are
		g_scan_restore_channel   = 0xff;
		g_scan_restore_frequency = 0xffffffff;
	}

	if (g_scan_restore_channel != 0xff ||
	   (g_scan_restore_frequency > 0 && g_scan_restore_frequency != 0xffffffff))
	{	// revert to where we were when starting the scan

		if (g_scan_next_channel <= USER_CHANNEL_LAST)
		{	// we were channel hopping

			if (g_scan_restore_channel != 0xff)
			{
				g_eeprom.user_channel[g_eeprom.rx_vfo]   = g_scan_restore_channel;
				g_eeprom.screen_channel[g_eeprom.rx_vfo] = g_scan_restore_channel;

				RADIO_configure_channel(g_eeprom.rx_vfo, VFO_CONFIGURE_RELOAD);
				RADIO_setup_registers(true);
			}
		}
		else
		if (g_scan_restore_frequency > 0 && g_scan_restore_frequency != 0xffffffff)
		{	// we were frequency scanning

			g_rx_vfo->freq_config_rx.frequency = g_scan_restore_frequency;

			RADIO_ApplyOffset(g_rx_vfo);
			RADIO_ConfigureSquelchAndOutputPower(g_rx_vfo);
			RADIO_setup_registers(true);
		}

		g_update_display = true;
	}
	else
	{	// stay where we are

		if (g_rx_vfo->channel_save > USER_CHANNEL_LAST)
		{	// frequency mode
			RADIO_ApplyOffset(g_rx_vfo);
			RADIO_ConfigureSquelchAndOutputPower(g_rx_vfo);
			SETTINGS_save_channel(g_rx_vfo->channel_save, g_eeprom.rx_vfo, g_rx_vfo, 1);
			return;
		}

		SETTINGS_save_vfo_indices();
	}

	#ifdef ENABLE_VOICE
		g_another_voice_id = VOICE_ID_SCANNING_STOP;
	#endif

	g_scan_pause_10ms      = 0;
	g_scan_pause_time_mode = false;

	g_update_status = true;
}

static void APP_next_freq(void)
{
	frequency_band_t       new_band;
	const frequency_band_t old_band  = FREQUENCY_GetBand(g_rx_vfo->freq_config_rx.frequency);
	const uint32_t         frequency = APP_set_frequency_by_step(g_rx_vfo, g_scan_state_dir);

	new_band = FREQUENCY_GetBand(frequency);

	g_rx_vfo->freq_config_rx.frequency = frequency;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		UART_printf("APP_next_freq %u %u\r\n", frequency, new_band);
	#endif

	if (new_band != old_band)
	{	// original slow method

		RADIO_ApplyOffset(g_rx_vfo);
		RADIO_ConfigureSquelchAndOutputPower(g_rx_vfo);
		RADIO_setup_registers(true);

		#ifdef ENABLE_FASTER_CHANNEL_SCAN
			g_scan_pause_10ms = 10;   // 100ms
		#else
			g_scan_pause_10ms = scan_pause_freq_10ms;
		#endif
	}
	else
	{	// don't need to go through all the other stuff .. lets speed things up !!

		BK4819_set_rf_frequency(frequency, true);
		BK4819_set_rf_filter_path(frequency);

		#ifdef ENABLE_FASTER_CHANNEL_SCAN
			g_scan_pause_10ms = 10;   // 100ms
		#else
			g_scan_pause_10ms = scan_pause_freq_10ms;
		#endif
	}

	g_scan_pause_time_mode = false;
	g_update_display       = true;
}

static void APP_next_channel(void)
{
	static unsigned int prevChannel = 0;
	const bool          enabled     = (g_eeprom.scan_list_default < 2) ? g_eeprom.scan_list_enabled[g_eeprom.scan_list_default] : true;
	const int           chan1       = (g_eeprom.scan_list_default < 2) ? g_eeprom.scan_list_priority_ch1[g_eeprom.scan_list_default] : -1;
	const int           chan2       = (g_eeprom.scan_list_default < 2) ? g_eeprom.scan_list_priority_ch2[g_eeprom.scan_list_default] : -1;
	const unsigned int  prev_chan   = g_scan_next_channel;
	unsigned int        chan        = 0;

	if (enabled)
	{
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

		switch (g_scan_current_scan_list)
		{
			case SCAN_NEXT_CHAN_SCANLIST1:
				prevChannel = g_scan_next_channel;

				if (chan1 >= 0)
				{
					if (RADIO_CheckValidChannel(chan1, false, 0))
					{
						g_scan_current_scan_list = SCAN_NEXT_CHAN_SCANLIST1;
						g_scan_next_channel      = chan1;
						break;
					}
				}

			case SCAN_NEXT_CHAN_SCANLIST2:
				if (chan2 >= 0)
				{
					if (RADIO_CheckValidChannel(chan2, false, 0))
					{
						g_scan_current_scan_list = SCAN_NEXT_CHAN_SCANLIST2;
						g_scan_next_channel      = chan2;
						break;
					}
				}

			// this bit doesn't yet work if the other VFO is a frequency
			case SCAN_NEXT_CHAN_DUAL_WATCH:
				// dual watch is enabled - include the other VFO in the scan
//				if (g_eeprom.dual_watch != DUAL_WATCH_OFF)
//				{
//					chan = (g_eeprom.rx_vfo + 1) & 1u;
//					chan = g_eeprom.screen_channel[chan];
//					if (chan <= USER_CHANNEL_LAST)
//					{
//						g_scan_current_scan_list = SCAN_NEXT_CHAN_DUAL_WATCH;
//						g_scan_next_channel   = chan;
//						break;
//					}
//				}

			default:
			case SCAN_NEXT_CHAN_USER:
				g_scan_current_scan_list = SCAN_NEXT_CHAN_USER;
				g_scan_next_channel      = prevChannel;
				chan             = 0xff;
				break;
		}

		#pragma GCC diagnostic pop
	}

	if (!enabled || chan == 0xff)
	{
		chan = RADIO_FindNextChannel(g_scan_next_channel + g_scan_state_dir, g_scan_state_dir, (g_eeprom.scan_list_default < 2) ? true : false, g_eeprom.scan_list_default);
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

		g_eeprom.user_channel[g_eeprom.rx_vfo]   = g_scan_next_channel;
		g_eeprom.screen_channel[g_eeprom.rx_vfo] = g_scan_next_channel;

		RADIO_configure_channel(g_eeprom.rx_vfo, VFO_CONFIGURE_RELOAD);
		RADIO_setup_registers(true);

		g_update_display = true;
	}

	#ifdef ENABLE_FASTER_CHANNEL_SCAN
		g_scan_pause_10ms = 9;  // 90ms .. <= ~60ms it misses signals (squelch response and/or PLL lock time) ?
	#else
		g_scan_pause_10ms = scan_pause_chan_10ms;
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

static void APP_toggle_dual_watch_vfo(void)
{
	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		UART_SendText("dual watch\r\n");
	#endif

	#ifdef ENABLE_NOAA
		if (g_is_noaa_mode)
		{
			if (IS_NOT_NOAA_CHANNEL(g_eeprom.screen_channel[0]) || IS_NOT_NOAA_CHANNEL(g_eeprom.screen_channel[1]))
				g_eeprom.rx_vfo = (g_eeprom.rx_vfo + 1) & 1;
			else
				g_eeprom.rx_vfo = 0;

			g_rx_vfo = &g_eeprom.vfo_info[g_eeprom.rx_vfo];

			if (g_eeprom.vfo_info[0].channel_save >= NOAA_CHANNEL_FIRST)
				APP_next_noaa();
		}
		else
	#endif
	{	// toggle between VFO's
		g_eeprom.rx_vfo = (g_eeprom.rx_vfo + 1) & 1;
		g_rx_vfo        = &g_eeprom.vfo_info[g_eeprom.rx_vfo];
		g_update_status = true;
	}

	RADIO_setup_registers(false);

	#ifdef ENABLE_NOAA
		g_dual_watch_delay_10ms = g_is_noaa_mode ? dual_watch_delay_noaa_10ms : dual_watch_delay_toggle_10ms;
	#else
		g_dual_watch_delay_10ms = dual_watch_delay_toggle_10ms;
	#endif
}

void APP_process_radio_interrupts(void)
{
	if (g_screen_to_display == DISPLAY_SEARCH)
		return;

	while (BK4819_ReadRegister(0x0C) & (1u << 0))
	{	// BK chip interrupt request

		BK4819_WriteRegister(0x02, 0);
		const uint16_t interrupt_bits = BK4819_ReadRegister(0x02);

		if (interrupt_bits & BK4819_REG_02_DTMF_5TONE_FOUND)
		{	// save the RX'ed DTMF character
			const char c = DTMF_GetCharacter(BK4819_GetDTMF_5TONE_Code());
			if (c != 0xff)
			{
				if (g_current_function != FUNCTION_TRANSMIT)
				{
					if (g_setting_live_dtmf_decoder)
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
						g_update_display        = true;
					}

					#ifdef ENABLE_KILL_REVIVE
						if (g_rx_vfo->dtmf_decoding_enable || g_setting_radio_disabled)
					#else
						if (g_rx_vfo->dtmf_decoding_enable)
					#endif
					{
						if (g_dtmf_rx_index >= (sizeof(g_dtmf_rx) - 1))
						{	// make room
							memmove(&g_dtmf_rx[0], &g_dtmf_rx[1], sizeof(g_dtmf_rx) - 1);
							g_dtmf_rx_index--;
						}
						g_dtmf_rx[g_dtmf_rx_index++] = c;
						g_dtmf_rx[g_dtmf_rx_index]   = 0;
						g_dtmf_rx_timeout           = dtmf_rx_timeout_500ms;  // time till we delete it
						g_dtmf_rx_pending           = true;

						DTMF_HandleRequest();
					}
				}
			}
		}

		if (interrupt_bits & BK4819_REG_02_CxCSS_TAIL)
			g_cxcss_tail_found = true;

		if (interrupt_bits & BK4819_REG_02_CDCSS_LOST)
		{
			g_cdcss_lost = true;
			g_cdcss_code_type = BK4819_get_CDCSS_code_type();
		}

		if (interrupt_bits & BK4819_REG_02_CDCSS_FOUND)
			g_cdcss_lost = false;

		if (interrupt_bits & BK4819_REG_02_CTCSS_LOST)
			g_ctcss_lost = true;

		if (interrupt_bits & BK4819_REG_02_CTCSS_FOUND)
			g_ctcss_lost = false;

		#ifdef ENABLE_VOX
			if (interrupt_bits & BK4819_REG_02_VOX_LOST)
			{
				g_vox_lost = true;
				g_vox_pause_count_down = 10;

				if (g_eeprom.vox_switch)
				{
					if (g_current_function == FUNCTION_POWER_SAVE && !g_rx_idle_mode)
					{
						g_power_save_10ms    = power_save2_10ms;
						g_power_save_expired = false;
					}

					if (g_eeprom.dual_watch != DUAL_WATCH_OFF &&
					   (g_dual_watch_delay_10ms == 0 || g_dual_watch_delay_10ms < dual_watch_delay_after_vox_10ms))
					{
						g_dual_watch_delay_10ms = dual_watch_delay_after_vox_10ms;
						g_update_status = true;
					}
				}
			}

			if (interrupt_bits & BK4819_REG_02_VOX_FOUND)
			{
				g_vox_lost         = false;
				g_vox_pause_count_down = 0;
			}
		#endif

		if (interrupt_bits & BK4819_REG_02_SQUELCH_CLOSED)
		{
			BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, false);  // LED off
			g_squelch_open = false;

			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_printf("squelch closed\r\n");
			#endif
		}

		if (interrupt_bits & BK4819_REG_02_SQUELCH_OPENED)
		{
//			BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, true);   // LED on
			g_squelch_open = true;

			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_printf("squelch opened\r\n");
			#endif
		}
	}
}

void APP_end_tx(void)
{	// back to RX mode

	RADIO_tx_eot();

	if (g_current_vfo->p_tx->code_type != CODE_TYPE_NONE)
	{	// CTCSS/DCS is enabled

		//if (g_eeprom.tail_note_elimination && g_eeprom.repeater_tail_tone_elimination > 0)
		if (g_eeprom.tail_note_elimination)
		{	// send the CTCSS/DCS tail tone - allows the receivers to mute the usual FM squelch tail/crash
			RADIO_EnableCxCSS();
		}
		#if 0
			else
			{	// TX a short blank carrier
				// this gives the receivers time to mute RX audio before we drop carrier
				BK4819_ExitSubAu();
				SYSTEM_DelayMs(200);
			}
		#endif
	}

	RADIO_setup_registers(false);
}

#ifdef ENABLE_VOX
	static void APP_process_vox(void)
	{
		#ifdef ENABLE_KILL_REVIVE
			if (g_setting_radio_disabled)
				return;
		#endif

		if (g_vox_resume_count_down == 0)
		{
			if (g_vox_pause_count_down)
				return;
		}
		else
		{
			g_vox_lost         = false;
			g_vox_pause_count_down = 0;
		}

		#ifdef ENABLE_FMRADIO
			if (g_fm_radio_mode)
				return;
		#endif

		if (g_current_function == FUNCTION_RECEIVE || g_current_function == FUNCTION_MONITOR)
			return;

		if (g_scan_state_dir != SCAN_STATE_DIR_OFF || g_css_scan_mode != CSS_SCAN_MODE_OFF)
			return;

		if (g_vox_noise_detected)
		{
			if (g_vox_lost)
				g_vox_stop_count_down_10ms = vox_stop_count_down_10ms;
			else
			if (g_vox_stop_count_down_10ms == 0)
				g_vox_noise_detected = false;

			if (g_current_function == FUNCTION_TRANSMIT &&
			   !g_ptt_is_pressed &&
			   !g_vox_noise_detected)
			{
				if (g_flag_end_tx)
				{
					//if (g_current_function != FUNCTION_FOREGROUND)
						FUNCTION_Select(FUNCTION_FOREGROUND);
				}
				else
				{
					APP_end_tx();

					if (g_eeprom.repeater_tail_tone_elimination == 0)
					{
						//if (g_current_function != FUNCTION_FOREGROUND)
							FUNCTION_Select(FUNCTION_FOREGROUND);
					}
					else
						g_rtte_count_down = g_eeprom.repeater_tail_tone_elimination * 10;
				}

				g_update_status        = true;
				g_update_display       = true;
				g_flag_end_tx = false;
			}
			return;
		}

		if (g_vox_lost)
		{
			g_vox_noise_detected = true;

			if (g_current_function == FUNCTION_POWER_SAVE)
				FUNCTION_Select(FUNCTION_FOREGROUND);

			if (g_current_function != FUNCTION_TRANSMIT && g_serial_config_count_down_500ms == 0)
			{
				g_dtmf_reply_state = DTMF_REPLY_NONE;
				RADIO_PrepareTX();
				g_update_display = true;
			}
		}
	}
#endif

void APP_process(void)
{
	#ifdef ENABLE_VOICE
		if (g_flag_play_queued_voice)
		{
			g_flag_play_queued_voice = false;
			AUDIO_PlayQueuedVoice();
		}
	#endif

	if (g_current_function == FUNCTION_TRANSMIT && (g_tx_timeout_reached || g_serial_config_count_down_500ms > 0))
	{	// transmitter timed out or must de-key

		g_tx_timeout_reached = false;
		g_flag_end_tx        = true;

		APP_end_tx();
		AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);
		RADIO_Setg_vfo_state(VFO_STATE_TIMEOUT);

		GUI_DisplayScreen();
	}

	if (g_reduced_service || g_serial_config_count_down_500ms > 0)
		return;

	APP_process_function();

	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_mode && g_fm_radio_count_down_500ms > 0)
			return;
	#endif

	#ifdef ENABLE_VOICE
		if (g_voice_write_index == 0)
	#endif
	{
		if ((g_current_function == FUNCTION_FOREGROUND  ||
		     g_current_function == FUNCTION_NEW_RECEIVE ||
		     g_current_function == FUNCTION_RECEIVE)    &&
			g_screen_to_display != DISPLAY_SEARCH       &&
		    g_scan_state_dir != SCAN_STATE_DIR_OFF      &&
		    !g_ptt_is_pressed)
		{	// RF scanning

			if (g_current_code_type == CODE_TYPE_NONE && g_current_function == FUNCTION_NEW_RECEIVE) // && !g_scan_pause_time_mode)
			{
				APP_start_listening(g_monitor_enabled ? FUNCTION_MONITOR : FUNCTION_RECEIVE, true);
			}
			else
			if (g_scan_pause_10ms == 0)
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
					APP_start_listening(g_monitor_enabled ? FUNCTION_MONITOR : FUNCTION_RECEIVE, true);
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
					APP_start_listening(g_monitor_enabled ? FUNCTION_MONITOR : FUNCTION_RECEIVE, true);
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
	}

	#ifdef ENABLE_VOICE
		if (g_voice_write_index == 0)
	#endif
	{
		if (g_css_scan_mode == CSS_SCAN_MODE_SCANNING && g_scan_pause_10ms == 0)
			MENU_SelectNextCode();
	}

	#ifdef ENABLE_NOAA
		#ifdef ENABLE_VOICE
			if (g_voice_write_index == 0)
		#endif
		{
			if (g_eeprom.dual_watch == DUAL_WATCH_OFF && g_is_noaa_mode && g_schedule_noaa)
			{
				APP_next_noaa();
				RADIO_setup_registers(false);

				g_noaa_count_down_10ms = 7;      // 70ms
				g_schedule_noaa        = false;
			}
		}
	#endif

	// toggle between the VFO's if dual watch is enabled
	if (g_eeprom.dual_watch != DUAL_WATCH_OFF &&
	    g_dual_watch_delay_10ms == 0 &&
	   !g_ptt_is_pressed &&
		#ifdef ENABLE_VOICE
			g_voice_write_index == 0 &&
		#endif
		#ifdef ENABLE_FMRADIO
			!g_fm_radio_mode &&
		#endif
	    g_dtmf_call_state == DTMF_CALL_STATE_NONE &&
	    g_screen_to_display != DISPLAY_SEARCH &&
		g_scan_state_dir == SCAN_STATE_DIR_OFF &&
		g_css_scan_mode == CSS_SCAN_MODE_OFF &&
		g_current_function != FUNCTION_POWER_SAVE &&
		(g_current_function == FUNCTION_FOREGROUND || g_current_function == FUNCTION_POWER_SAVE))
	{
		APP_toggle_dual_watch_vfo();    // toggle between the two VFO's

		if (g_rx_vfo_is_active && g_screen_to_display == DISPLAY_MAIN)
			GUI_SelectNextDisplay(DISPLAY_MAIN);

		g_rx_vfo_is_active  = false;
		g_rx_reception_mode = RX_MODE_NONE;
	}

#ifdef ENABLE_FMRADIO
	if (g_schedule_fm                          &&
		g_fm_scan_state    != FM_SCAN_OFF      &&
		g_current_function != FUNCTION_MONITOR &&
		g_current_function != FUNCTION_RECEIVE &&
		g_current_function != FUNCTION_TRANSMIT)
	{	// switch to FM radio mode
		FM_Play();
		g_schedule_fm = false;
	}
#endif

#ifdef ENABLE_VOX
	if (g_eeprom.vox_switch)
		APP_process_vox();
#endif

	if (g_schedule_power_save)
	{
		#ifdef ENABLE_NOAA
			if (
			#ifdef ENABLE_FMRADIO
			    g_fm_radio_mode                        ||
			#endif
				g_ptt_is_pressed                       ||
			    g_key_held                             ||
				g_eeprom.battery_save == 0             ||
			    g_scan_state_dir != SCAN_STATE_DIR_OFF ||
			    g_css_scan_mode != CSS_SCAN_MODE_OFF   ||
			    g_screen_to_display != DISPLAY_MAIN    ||
			    g_dtmf_call_state != DTMF_CALL_STATE_NONE)
			{
				g_battery_save_count_down_10ms   = battery_save_count_10ms;
			}
			else
			if ((IS_NOT_NOAA_CHANNEL(g_eeprom.screen_channel[0]) &&
			     IS_NOT_NOAA_CHANNEL(g_eeprom.screen_channel[1])) ||
			     !g_is_noaa_mode)
			{
				FUNCTION_Select(FUNCTION_POWER_SAVE);
			}
			else
			{
				g_battery_save_count_down_10ms = battery_save_count_10ms;
			}
		#else
			if (
				#ifdef ENABLE_FMRADIO
					g_fm_radio_mode                    ||
			    #endif
				g_ptt_is_pressed                       ||
			    g_key_held                             ||
				g_eeprom.battery_save == 0             ||
			    g_scan_state_dir != SCAN_STATE_DIR_OFF ||
			    g_css_scan_mode != CSS_SCAN_MODE_OFF   ||
			    g_screen_to_display != DISPLAY_MAIN    ||
			    g_dtmf_call_state != DTMF_CALL_STATE_NONE)
			{
				g_battery_save_count_down_10ms = battery_save_count_10ms;
			}
			else
			{
				FUNCTION_Select(FUNCTION_POWER_SAVE);
			}
		#endif

		g_schedule_power_save = false;
	}

#ifdef ENABLE_VOICE
	if (g_voice_write_index == 0)
#endif
	{
		if (g_power_save_expired && g_current_function == FUNCTION_POWER_SAVE)
		{	// wake up, enable RX then go back to sleep
			if (g_rx_idle_mode)
			{
				#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//					UART_SendText("ps wake up\r\n");
				#endif

				BK4819_Conditional_RX_TurnOn_and_GPIO6_Enable();

				#ifdef ENABLE_VOX
					if (g_eeprom.vox_switch)
						BK4819_EnableVox(g_eeprom.vox1_threshold, g_eeprom.vox0_threshold);
				#endif

				if (g_eeprom.dual_watch != DUAL_WATCH_OFF &&
					g_scan_state_dir == SCAN_STATE_DIR_OFF &&
					g_css_scan_mode == CSS_SCAN_MODE_OFF)
				{	// dual watch mode, toggle between the two VFO's
					APP_toggle_dual_watch_vfo();
					g_update_rssi = false;
				}

				FUNCTION_Init();

				g_power_save_10ms = power_save1_10ms; // come back here in a bit
				g_rx_idle_mode    = false;            // RX is awake
			}
			else
			if (g_eeprom.dual_watch == DUAL_WATCH_OFF  ||
				g_scan_state_dir != SCAN_STATE_DIR_OFF ||
				g_css_scan_mode != CSS_SCAN_MODE_OFF   ||
				g_update_rssi)
			{	// dual watch mode, go back to sleep

				APP_update_rssi(g_eeprom.rx_vfo);

				// go back to sleep

				g_power_save_10ms = g_eeprom.battery_save * 10;
				g_rx_idle_mode    = true;

				BK4819_DisableVox();
				BK4819_Sleep();
				BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_RX_ENABLE, false);

				// Authentic device checked removed

			}
			else
			{
				// toggle between the two VFO's
				APP_toggle_dual_watch_vfo();

				g_update_rssi     = true;
				g_power_save_10ms = power_save1_10ms;
			}

			g_power_save_expired = false;
		}
	}
}

// called every 10ms
void APP_check_keys(void)
{
	const bool ptt_pressed = !GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT) && (g_serial_config_count_down_500ms == 0) && g_setting_tx_enable;

	key_code_t key;

	#ifdef ENABLE_KILL_REVIVE
		if (g_setting_radio_disabled)
			return;
	#endif

	// *****************
	// PTT is treated completely separately from all the other buttons

	if (ptt_pressed)
	{	// PTT pressed
	#ifdef ENABLE_KILL_REVIVE
		if (!g_setting_radio_disabled)
	#endif
		{
		#ifdef ENABLE_AIRCOPY
			if (!g_ptt_is_pressed && g_screen_to_display != DISPLAY_AIRCOPY)
		#else
			if (!g_ptt_is_pressed)
		#endif
			{
				if (++g_ptt_debounce >= 3)      // 30ms
				{	// start TX'ing

					g_boot_counter_10ms = 0;    // cancel the boot-up screen
					g_ptt_is_pressed    = true;
					g_ptt_was_released  = false;
					g_ptt_debounce      = 0;

					APP_process_key(KEY_PTT, true, false);

					#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//						UART_printf(" ptt key %3u %u %u\r\n", KEY_PTT, g_ptt_is_pressed, g_ptt_was_released);
					#endif
				}
			}
			else
				g_ptt_debounce = 0;
		}
	}
	else
	{	// PTT released
		if (g_ptt_is_pressed)
		{
			if (++g_ptt_debounce >= 3)  // 30ms
			{	// stop TX'ing

				g_ptt_is_pressed   = false;
				g_ptt_was_released = true;
				g_ptt_debounce     = 0;

				APP_process_key(KEY_PTT, false, false);

				#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//					UART_printf(" ptt key %3u %u %u\r\n", KEY_PTT, g_ptt_is_pressed, g_ptt_was_released);
				#endif
			}
		}
		else
			g_ptt_debounce = 0;
	}

	// *****************
	// button processing (non-PTT)

	// scan the hardware keys
	key = KEYBOARD_Poll();

	g_boot_counter_10ms = 0;   // cancel boot screen/beeps

	if (g_serial_config_count_down_500ms > 0)
	{	// config upload/download in progress
		g_key_debounce_press  = 0;
		g_key_debounce_repeat = 0;
		g_key_prev            = KEY_INVALID;
		g_key_held            = false;
		g_fkey_pressed        = false;
		return;
	}

	if (key == KEY_INVALID || (g_key_prev != KEY_INVALID && key != g_key_prev))
	{	// key not pressed or different key pressed
		if (g_key_debounce_press > 0)
		{
			if (--g_key_debounce_press == 0)
			{
				if (g_key_prev != KEY_INVALID)
				{	// key now fully released

					#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//						UART_printf(" old key %3u %3u, %3u %3u, %u\r\n", key, g_key_prev, g_key_debounce_press, g_key_debounce_repeat, g_key_held);
					#endif

					#ifdef ENABLE_AIRCOPY
						if (g_screen_to_display != DISPLAY_AIRCOPY)
							APP_process_key(g_key_prev, false, g_key_held);
						else
							AIRCOPY_process_key(g_key_prev, false, g_key_held);
					#else
						APP_process_key(g_key_prev, false, g_key_held);
					#endif

					g_key_debounce_press  = 0;
					g_key_debounce_repeat = 0;
					g_key_prev            = KEY_INVALID;
					g_key_held            = false;
					g_boot_counter_10ms   = 0;         // cancel the boot-up screen

					g_update_status       = true;
					g_update_display      = true;
				}
			}
			if (g_key_debounce_repeat > 0)
				g_key_debounce_repeat--;
		}
	}
	else
	{	// key pressed
		if (g_key_debounce_press < key_debounce_10ms)
		{
			if (++g_key_debounce_press >= key_debounce_10ms)
			{
				if (key != g_key_prev)
				{	// key now fully pressed
					g_key_debounce_repeat = key_debounce_10ms;
					g_key_held            = false;

					#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//						UART_printf("\r\n new key %3u %3u, %3u %3u, %u\r\n", key, g_key_prev, g_key_debounce_press, g_key_debounce_repeat, g_key_held);
					#endif

					g_key_prev = key;

					#ifdef ENABLE_AIRCOPY
						if (g_screen_to_display != DISPLAY_AIRCOPY)
							APP_process_key(g_key_prev, true, g_key_held);
						else
							AIRCOPY_process_key(g_key_prev, true, g_key_held);
					#else
						APP_process_key(g_key_prev, true, g_key_held);
					#endif

					g_update_status  = true;
					g_update_display = true;
				}
			}
		}
		else
		if (g_key_debounce_repeat < key_long_press_10ms)
		{
			if (++g_key_debounce_repeat >= key_long_press_10ms)
			{	// key long press
				g_key_held = true;

				#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//					UART_printf("long key %3u %3u, %3u %3u, %u\r\n", key, g_key_prev, g_key_debounce_press, g_key_debounce_repeat, g_key_held);
				#endif

				#ifdef ENABLE_AIRCOPY
					if (g_screen_to_display != DISPLAY_AIRCOPY)
						APP_process_key(g_key_prev, true, g_key_held);
					else
						AIRCOPY_process_key(g_key_prev, true, g_key_held);
				#else
					APP_process_key(g_key_prev, true, g_key_held);
				#endif

				//g_update_status  = true;
				//g_update_display = true;
			}
		}
		else
		if (key == KEY_UP || key == KEY_DOWN)
		{	// only the up and down keys are repeatable
			if (++g_key_debounce_repeat >= (key_long_press_10ms + key_repeat_10ms))
			{	// key repeat
				g_key_debounce_repeat -= key_repeat_10ms;

				#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//					UART_printf("rept key %3u %3u, %3u %3u, %u\r\n", key, g_key_prev, g_key_debounce_press, g_key_debounce_repeat, g_key_held);
				#endif

				#ifdef ENABLE_AIRCOPY
					if (g_screen_to_display != DISPLAY_AIRCOPY)
						APP_process_key(g_key_prev, true, g_key_held);
					else
						AIRCOPY_process_key(g_key_prev, true, g_key_held);
				#else
					APP_process_key(g_key_prev, true, g_key_held);
				#endif

				//g_update_status  = true;
				//g_update_display = true;
			}
		}
	}

	// *****************
}

void APP_time_slice_10ms(void)
{
	g_flash_light_blink_counter++;

#ifdef ENABLE_UART
	if (UART_IsCommandAvailable())
	{
		__disable_irq();
		UART_HandleCommand();
		__enable_irq();
	}
#endif

	// ***********

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

	#ifdef ENABLE_FMRADIO
		if (g_flag_save_fm)
		{
			SETTINGS_save_fm();
			g_flag_save_fm = false;
		}
	#endif

	if (g_flag_save_channel)
	{
		SETTINGS_save_channel(g_tx_vfo->channel_save, g_eeprom.tx_vfo, g_tx_vfo, g_flag_save_channel ? 1 : 0);
		g_flag_save_channel = false;

		RADIO_configure_channel(g_eeprom.tx_vfo, VFO_CONFIGURE);
		RADIO_setup_registers(true);

		GUI_SelectNextDisplay(DISPLAY_MAIN);
	}

	// ***********

	if (g_serial_config_count_down_500ms > 0)
	{	// config upload/download is running
		if (g_update_display)
			GUI_DisplayScreen();
		if (g_update_status)
			UI_DisplayStatus(false);
		return;
	}

	// ***********

	#ifdef ENABLE_BOOT_BEEPS
		if (g_boot_counter_10ms > 0 && (g_boot_counter_10ms % 25) == 0)
			AUDIO_PlayBeep(BEEP_880HZ_40MS_OPTIONAL);
	#endif

	if (g_reduced_service)
		return;

	#ifdef ENABLE_AIRCOPY
		if (g_screen_to_display == DISPLAY_AIRCOPY)
		{	// we're in AIRCOPY mode

			if (g_aircopy_state == AIRCOPY_TX)
				AIRCOPY_process_fsk_tx_10ms();

			AIRCOPY_process_fsk_rx_10ms();

			APP_check_keys();

			if (g_update_display)
				GUI_DisplayScreen();

			if (g_update_status)
				UI_DisplayStatus(false);

			return;
		}
	#endif

	#ifdef ENABLE_AM_FIX
//		if (g_eeprom.vfo_info[g_eeprom.rx_vfo].am_mode && g_setting_am_fix)
		if (g_rx_vfo->am_mode && g_setting_am_fix)
			AM_fix_10ms(g_eeprom.rx_vfo);
	#endif

	if (g_current_function != FUNCTION_POWER_SAVE || !g_rx_idle_mode)
		APP_process_radio_interrupts();

	if (g_current_function == FUNCTION_TRANSMIT)
	{	// transmitting
		#ifdef ENABLE_TX_AUDIO_BAR
			if (g_setting_mic_bar && (g_flash_light_blink_counter % (150 / 10)) == 0) // once every 150ms
				UI_DisplayAudioBar(true);
		#endif
	}

	if (g_update_display)
		GUI_DisplayScreen();

	if (g_update_status)
		UI_DisplayStatus(false);

	// Skipping authentic device checks

	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_mode && g_fm_radio_count_down_500ms > 0)
			return;
	#endif

	if (g_flash_light_state == FLASHLIGHT_BLINK && (g_flash_light_blink_counter & 15u) == 0)
		GPIO_FlipBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);

	#ifdef ENABLE_VOX
		if (g_vox_resume_count_down > 0)
			g_vox_resume_count_down--;

		if (g_vox_pause_count_down > 0)
			g_vox_pause_count_down--;
	#endif

	if (g_current_function == FUNCTION_TRANSMIT)
	{
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

				if (g_eeprom.alarm_mode == ALARM_MODE_TONE && g_alarm_running_counter_10ms == 512)
				{
					g_alarm_running_counter_10ms = 0;

					if (g_alarm_state == ALARM_STATE_TXALARM)
					{
						g_alarm_state = ALARM_STATE_ALARM;

						RADIO_EnableCxCSS();
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
						g_speaker_enabled = true;

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

	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_mode && g_fm_restore_count_down_10ms > 0)
		{
			if (--g_fm_restore_count_down_10ms == 0)
			{	// switch back to FM radio mode
				FM_Start();
				GUI_SelectNextDisplay(DISPLAY_FM);
			}
		}
	#endif

	if (g_screen_to_display == DISPLAY_SEARCH)
	{
		uint32_t                 Result;
		int32_t                  Delta;
		uint16_t                 CtcssFreq;
		BK4819_CSS_scan_result_t ScanResult;

		g_search_freq_css_timer_10ms++;

		if (g_search_delay_10ms > 0)
		{
			if (--g_search_delay_10ms > 0)
			{
				APP_check_keys();
				return;
			}
		}

		if (g_search_edit_state != SEARCH_EDIT_STATE_NONE)
		{	// waiting for user input choice
			APP_check_keys();
			return;
		}

		g_update_display = true;
		GUI_SelectNextDisplay(DISPLAY_SEARCH);

		switch (g_search_css_state)
		{
			case SEARCH_CSS_STATE_OFF:

				if (g_search_freq_css_timer_10ms >= scan_freq_css_timeout_10ms)
				{	// FREQ/CTCSS/CDCSS search timeout

					if (!g_search_single_frequency)
					{	// FREQ search timeout
						#ifdef ENABLE_FREQ_SEARCH_TIMEOUT
							BK4819_DisableFrequencyScan();

							g_search_css_state = SEARCH_CSS_STATE_FREQ_FAILED;

							AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);
							g_update_status  = true;
							g_update_display = true;
							break;
						#endif
					}
					else
					{	// CTCSS/CDCSS search timeout
						#ifdef ENABLE_CODE_SEARCH_TIMEOUT
							BK4819_DisableFrequencyScan();

							g_search_css_state = SEARCH_CSS_STATE_FREQ_FAILED;

							AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);
							g_update_status  = true;
							g_update_display = true;
							break;
						#endif
					}
				}

				if (!BK4819_GetFrequencyScanResult(&Result))
					break;   // still scanning

				// accept only within 1kHz
				Delta = Result - g_search_frequency;
				g_search_hit_count = (abs(Delta) < 100) ? g_search_hit_count + 1 : 0;

				BK4819_DisableFrequencyScan();

				g_search_frequency = Result;

				if (g_search_hit_count < 3)
				{	// keep scanning for an RF carrier
					BK4819_EnableFrequencyScan();
				}
				else
				{	// RF carrier found
					//
					// stop RF search and start CTCSS/CDCSS search

					BK4819_SetScanFrequency(g_search_frequency);

					g_search_css_result_type     = CODE_TYPE_NONE;
					g_search_css_result_code     = 0xff;
					g_search_hit_count           = 0;
					g_search_use_css_result      = false;
					g_search_freq_css_timer_10ms = 0;
					g_search_css_state           = SEARCH_CSS_STATE_SCANNING;

					g_update_status  = true;
					g_update_display = true;
					GUI_SelectNextDisplay(DISPLAY_SEARCH);
				}

				g_search_delay_10ms = scan_freq_css_delay_10ms;
				break;

			case SEARCH_CSS_STATE_SCANNING:

				if (g_search_freq_css_timer_10ms >= scan_freq_css_timeout_10ms)
				{	// CTCSS/CDCSS search timeout

					#if defined(ENABLE_CODE_SEARCH_TIMEOUT)
						g_search_css_state = SEARCH_CSS_STATE_FAILED;

						BK4819_Disable();

						AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);
						g_update_status  = true;
						g_update_display = true;
						break;

					#else
						if (!g_search_single_frequency)
						{
							g_search_css_state = SEARCH_CSS_STATE_FAILED;

							BK4819_Disable();

							AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);
							g_update_status  = true;
							g_update_display = true;
							break;
						}
					#endif
				}

				ScanResult = BK4819_GetCxCSSScanResult(&Result, &CtcssFreq);
				if (ScanResult == BK4819_CSS_RESULT_NOT_FOUND)
					break;

				BK4819_Disable();

				if (ScanResult == BK4819_CSS_RESULT_CDCSS)
				{	// found a CDCSS code
					const uint8_t code = DCS_GetCdcssCode(Result);
					if (code != 0xFF)
					{
						g_search_hit_count       = 0;
						g_search_css_result_type = CODE_TYPE_DIGITAL;
						g_search_css_result_code = code;
						g_search_css_state       = SEARCH_CSS_STATE_FOUND;
						g_search_use_css_result  = true;

						AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);
						g_update_status  = true;
						g_update_display = true;
					}
					else
					{
						g_search_hit_count       = 0;
						g_search_css_result_type = CODE_TYPE_NONE;
						g_search_css_result_code = code;
						g_search_use_css_result  = false;
					}
				}
				else
				if (ScanResult == BK4819_CSS_RESULT_CTCSS)
				{	// found a CTCSS tone
					const uint8_t code = DCS_GetCtcssCode(CtcssFreq);
					if (code != 0xFF)
					{
						if (code == g_search_css_result_code &&
						    g_search_css_result_type == CODE_TYPE_CONTINUOUS_TONE)
						{
							if (++g_search_hit_count >= 3)
							{
								g_search_css_state      = SEARCH_CSS_STATE_FOUND;
								g_search_use_css_result = true;

								AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);
								g_update_status  = true;
								g_update_display = true;
							}
						}
						else
						{
							g_search_hit_count       = 1;
							g_search_css_result_type = CODE_TYPE_CONTINUOUS_TONE;
							g_search_css_result_code = code;
							g_search_use_css_result  = false;
						}
					}
					else
					{
						g_search_hit_count       = 0;
						g_search_css_result_type = CODE_TYPE_NONE;
						g_search_css_result_code = 0xff;
						g_search_use_css_result  = false;
					}
				}

				if (g_search_css_state == SEARCH_CSS_STATE_OFF ||
				    g_search_css_state == SEARCH_CSS_STATE_SCANNING)
				{	// re-start scan
					BK4819_SetScanFrequency(g_search_frequency);
					g_search_delay_10ms = scan_freq_css_delay_10ms;
				}

				GUI_SelectNextDisplay(DISPLAY_SEARCH);
				break;

			//case SEARCH_CSS_STATE_FOUND:
			//case SEARCH_CSS_STATE_FAILED:
			//case SEARCH_CSS_STATE_REPEAT:
			default:
				break;
		}
	}

	APP_check_keys();
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
		g_update_display         = true;
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

// this is called once every 500ms
void APP_time_slice_500ms(void)
{
	bool exit_menu = false;

	// Skipped authentic device check

	if (g_serial_config_count_down_500ms > 0)
	{	// config upload/download is running
		return;
	}

	if (g_keypad_locked > 0)
		if (--g_keypad_locked == 0)
			g_update_display = true;

	if (g_key_input_count_down > 0)
	{
		if (--g_key_input_count_down == 0)
		{
			APP_cancel_user_input_modes();

			if (g_beep_to_play != BEEP_NONE)
			{
				AUDIO_PlayBeep(g_beep_to_play);
				g_beep_to_play = BEEP_NONE;
			}
		}
	}

	if (g_dtmf_rx_live_timeout > 0)
	{
		#ifdef ENABLE_RX_SIGNAL_BAR
			if (center_line == CENTER_LINE_DTMF_DEC ||
				center_line == CENTER_LINE_NONE)  // wait till the center line is free for us to use before timing out
		#endif
		{
			if (--g_dtmf_rx_live_timeout == 0)
			{
				if (g_dtmf_rx_live[0] != 0)
				{
					memset(g_dtmf_rx_live, 0, sizeof(g_dtmf_rx_live));
					g_update_display   = true;
				}
			}
		}
	}

	if (g_menu_count_down > 0)
		if (--g_menu_count_down == 0)
			exit_menu = (g_screen_to_display == DISPLAY_MENU);	// exit menu mode

	if (g_dtmf_rx_timeout > 0)
		if (--g_dtmf_rx_timeout == 0)
			DTMF_clear_RX();

	// Skipped authentic device check

	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_count_down_500ms > 0)
		{
			g_fm_radio_count_down_500ms--;
			if (g_fm_radio_mode)           // 1of11
				return;
		}
	#endif

	if (g_backlight_count_down > 0 &&
	   !g_ask_to_save &&
	    g_css_scan_mode == CSS_SCAN_MODE_OFF &&
	    g_screen_to_display != DISPLAY_AIRCOPY)
	{
		if (g_screen_to_display != DISPLAY_MENU || g_menu_cursor != MENU_AUTO_BACKLITE) // don't turn off backlight if user is in backlight menu option
			if (--g_backlight_count_down == 0)
				if (g_eeprom.backlight < (ARRAY_SIZE(g_sub_menu_backlight) - 1))
					GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);   // turn backlight off
	}

	if (g_reduced_service)
	{
		BOARD_ADC_GetBatteryInfo(&g_usb_current_voltage, &g_usb_current);

		if (g_usb_current > 500 || g_battery_calibration[3] < g_usb_current_voltage)
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

	// Skipped authentic device check

	if ((g_battery_check_counter & 1) == 0)
	{
		BOARD_ADC_GetBatteryInfo(&g_battery_voltages[g_battery_voltage_index++], &g_usb_current);
		if (g_battery_voltage_index > 3)
			g_battery_voltage_index = 0;
		BATTERY_GetReadings(true);
	}

	// regular display updates (once every 2 sec) - if need be
	if ((g_battery_check_counter & 3) == 0)
	{
		if (g_charging_with_type_c || g_setting_battery_text > 0)
			g_update_status = true;
		#ifdef ENABLE_SHOW_CHARGE_LEVEL
			if (g_charging_with_type_c)
				g_update_display = true;
		#endif
	}

#ifdef ENABLE_FMRADIO
	if (g_fm_scan_state == FM_SCAN_OFF || g_ask_to_save)
#endif
	{
	#ifdef ENABLE_AIRCOPY
		if (g_screen_to_display != DISPLAY_AIRCOPY)
	#endif
		{
			if (g_css_scan_mode == CSS_SCAN_MODE_OFF   &&
			    g_scan_state_dir == SCAN_STATE_DIR_OFF &&
			   (g_screen_to_display != DISPLAY_SEARCH         ||
				g_search_css_state == SEARCH_CSS_STATE_FOUND  ||
				g_search_css_state == SEARCH_CSS_STATE_FAILED ||
				g_search_css_state == SEARCH_CSS_STATE_REPEAT))
			{

				#ifdef ENABLE_KEYLOCK
				if (g_eeprom.auto_keypad_lock       &&
				    g_key_lock_count_down_500ms > 0 &&
				   !g_dtmf_input_mode               &&
				    g_input_box_index == 0          &&
				    g_screen_to_display != DISPLAY_MENU)
				{
					if (--g_key_lock_count_down_500ms == 0)
					{	// lock the keyboard
						g_eeprom.key_lock = true;
						g_update_status   = true;
					}
				}
				#endif

				if (exit_menu)
				{
					g_menu_count_down = 0;

					if (g_eeprom.backlight == 0)
					{
						g_backlight_count_down = 0;
						GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);	// turn the backlight OFF
					}

					if (g_input_box_index > 0 || g_dtmf_input_mode)
						AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL);
/*
					if (g_screen_to_display == DISPLAY_SEARCH)
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
								g_current_function != FUNCTION_MONITOR &&
								g_current_function != FUNCTION_TRANSMIT)
							{
								disp = DISPLAY_FM;
							}
						#endif

						if (disp == DISPLAY_INVALID)
						{
							#ifndef ENABLE_CODE_SEARCH_TIMEOUT
								if (g_screen_to_display != DISPLAY_SEARCH)
							#endif
									disp = DISPLAY_MAIN;
						}

						if (disp != DISPLAY_INVALID)
							GUI_SelectNextDisplay(disp);
					}
				}
			}
		}
	}

	if (g_current_function != FUNCTION_POWER_SAVE && g_current_function != FUNCTION_TRANSMIT)
		APP_update_rssi(g_eeprom.rx_vfo);

	#ifdef ENABLE_FMRADIO
		if (!g_ptt_is_pressed && g_fm_resume_count_down_500ms > 0)
		{
			if (--g_fm_resume_count_down_500ms == 0)
			{
				RADIO_Setg_vfo_state(VFO_STATE_NORMAL);

				if (g_current_function != FUNCTION_RECEIVE  &&
				    g_current_function != FUNCTION_TRANSMIT &&
				    g_current_function != FUNCTION_MONITOR  &&
					g_fm_radio_mode)
				{	// switch back to FM radio mode
					FM_Start();
					GUI_SelectNextDisplay(DISPLAY_FM);
				}
			}
		}
	#endif

	if (g_low_battery)
	{
		g_low_battery_blink = ++g_low_batteryCountdown & 1;

		UI_DisplayBattery(0, g_low_battery_blink);

		if (g_current_function != FUNCTION_TRANSMIT)
		{	// not transmitting

			if (g_low_batteryCountdown < 30)
			{
				if (g_low_batteryCountdown == 29 && !g_charging_with_type_c)
					AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP);
			}
			else
			{
				g_low_batteryCountdown = 0;

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

						FUNCTION_Select(FUNCTION_POWER_SAVE);

						ST7565_HardwareReset();

						if (g_eeprom.backlight < (ARRAY_SIZE(g_sub_menu_backlight) - 1))
							GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);  // turn the backlight off
					}
					#ifdef ENABLE_VOICE
						else
							AUDIO_PlaySingleVoice(false);
					#endif
				}
			}
		}
	}

	if (g_current_function != FUNCTION_TRANSMIT)
	{
		if (g_dtmf_decode_ring_count_down_500ms > 0)
		{	// make "ring-ring" sound
			g_dtmf_decode_ring_count_down_500ms--;

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
		g_dtmf_decode_ring_count_down_500ms = 0;

	if (g_dtmf_call_state  != DTMF_CALL_STATE_NONE &&
	    g_current_function != FUNCTION_TRANSMIT &&
	    g_current_function != FUNCTION_RECEIVE)
	{
		if (g_dtmf_auto_reset_time_500ms > 0)
		{
			if (--g_dtmf_auto_reset_time_500ms == 0)
			{
				if (g_dtmf_call_state == DTMF_CALL_STATE_RECEIVED && g_eeprom.dtmf_auto_reset_time >= DTMF_HOLD_MAX)
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

	if (g_dtmf_is_tx && g_dtmf_tx_stop_count_down_500ms > 0)
	{
		if (--g_dtmf_tx_stop_count_down_500ms == 0)
		{
			g_dtmf_is_tx     = false;
			g_update_display = true;
		}
	}

	#ifdef ENABLE_TX_TIMEOUT_BAR
		if (g_current_function == FUNCTION_TRANSMIT && (g_tx_timer_count_down_500ms & 1))
			UI_DisplayTXCountdown(true);
	#endif
}

#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
	static void APP_alarm_off(void)
	{
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
		g_speaker_enabled = false;

		if (g_eeprom.alarm_mode == ALARM_MODE_TONE)
		{
			RADIO_tx_eot();
			RADIO_EnableCxCSS();
		}
		
		#ifdef ENABLE_VOX
			g_vox_resume_count_down = 80;
		#endif

		g_alarm_state = ALARM_STATE_OFF;

		SYSTEM_DelayMs(5);

		RADIO_setup_registers(true);

		if (g_screen_to_display != DISPLAY_MENU)     // 1of11 .. don't close the menu
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
		UART_printf("APP_channel_next %u\r\n", g_scan_next_channel);
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

	g_scan_pause_10ms      = scan_pause_css_10ms;
	g_scan_pause_time_mode = false;
	g_rx_reception_mode    = RX_MODE_NONE;
}

static void APP_process_key(const key_code_t Key, const bool key_pressed, const bool key_held)
{
	bool flag = false;

	if (Key == KEY_INVALID && !key_pressed && !key_held)
		return;

	// reset the state so as to remove it from the screen
	if (Key != KEY_INVALID && Key != KEY_PTT)
		RADIO_Setg_vfo_state(VFO_STATE_NORMAL);
#if 0
	// remember the current backlight state (on / off)
	const bool backlight_was_on = GPIO_CheckBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);

	if (Key == KEY_EXIT && !backlight_was_on && g_eeprom.backlight > 0)
	{	// just turn the back light on for now so the user can see what's what
		if (!key_pressed && !key_held)
		{	// key has been released
			backlight_turn_on(0);
		}
		g_beep_to_play = BEEP_NONE;
		return;
	}
#endif

	// turn the backlight on
	if (key_pressed)
		if (Key != KEY_PTT)
			backlight_turn_on(0);

	if (g_current_function == FUNCTION_POWER_SAVE)
		FUNCTION_Select(FUNCTION_FOREGROUND);

	// stay awake - for now
	g_battery_save_count_down_10ms = battery_save_count_10ms;

	#ifdef ENABLE_KEYLOCK
	// keep the auto keylock at bay
	if (g_eeprom.auto_keypad_lock)
		g_key_lock_count_down_500ms = key_lock_timeout_500ms;
	#endif

	if (g_fkey_pressed && (Key == KEY_PTT || Key == KEY_EXIT || Key == KEY_SIDE1 || Key == KEY_SIDE2))
	{	// cancel the F-key
		g_fkey_pressed  = false;
		g_update_status = true;
	}

	// ********************

	#ifdef ENABLE_KEYLOCK
	if (g_eeprom.key_lock && g_current_function != FUNCTION_TRANSMIT && Key != KEY_PTT)
	{	// keyboard is locked

		if (Key == KEY_F)
		{	// function/key-lock key

			if (!key_pressed)
				return;

			if (key_held)
			{	// unlock the keypad
				g_eeprom.key_lock       = false;
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

	// key beep
//	if (Key != KEY_PTT && !key_held && key_pressed)
//		g_beep_to_play = BEEP_1KHZ_60MS_OPTIONAL;

	// ********************

	if (Key == KEY_EXIT && key_held && key_pressed)
	{	// exit key held pressed

		// clear the live DTMF decoder
		if (g_dtmf_rx_live[0] != 0)
		{
			memset(g_dtmf_rx_live, 0, sizeof(g_dtmf_rx_live));
			g_dtmf_rx_live_timeout = 0;
			g_update_display       = true;
		}

		// cancel user input
		APP_cancel_user_input_modes();
	}

	if (key_pressed && g_screen_to_display == DISPLAY_MENU)
		g_menu_count_down = menu_timeout_500ms;

	// cancel the ringing
	if (key_pressed && g_dtmf_decode_ring_count_down_500ms > 0)
		g_dtmf_decode_ring_count_down_500ms = 0;

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

						g_speaker_enabled = false;

						BK4819_ExitDTMF_TX(false);

						if (g_current_vfo->scrambling_type == 0 || !g_setting_scramble_enable)
							BK4819_DisableScramble();
						else
							BK4819_EnableScramble(g_current_vfo->scrambling_type - 1);
					}
				}
				else
				{
					if (g_eeprom.dtmf_side_tone)
					{	// user will here the DTMF tones in speaker
						GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
						g_speaker_enabled = true;
					}

					BK4819_DisableScramble();

					if (Code == 0xFE)
						BK4819_TransmitTone(g_eeprom.dtmf_side_tone, 1750);
					else
						BK4819_PlayDTMFEx(g_eeprom.dtmf_side_tone, Code);
				}
			}
			#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
				else
				if ((!key_held && key_pressed) || (g_alarm_state == ALARM_STATE_TX1750 && key_held && !key_pressed))
				{
					APP_alarm_off();

					if (g_eeprom.repeater_tail_tone_elimination == 0)
						FUNCTION_Select(FUNCTION_FOREGROUND);
					else
						g_rtte_count_down = g_eeprom.repeater_tail_tone_elimination * 10;

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
			switch (g_screen_to_display)
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
			if (g_screen_to_display != DISPLAY_SEARCH && g_screen_to_display != DISPLAY_AIRCOPY)
		#else
			if (g_screen_to_display != DISPLAY_SEARCH)
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
		g_menu_count_down = menu_timeout_500ms;

		MENU_AcceptSetting();

		g_flag_refresh_menu   = true;
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
			SETTINGS_save_channel(g_tx_vfo->channel_save, g_eeprom.tx_vfo, g_tx_vfo, g_request_save_channel);

			if (g_screen_to_display != DISPLAY_SEARCH)
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
			RADIO_configure_channel(g_eeprom.tx_vfo, g_vfo_configure_mode);
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

//		g_tx_vfo->freq_in_channel = BOARD_find_channel(frequency);

		g_dtmf_auto_reset_time_500ms    = 0;
		g_dtmf_call_state               = DTMF_CALL_STATE_NONE;
		g_dtmf_tx_stop_count_down_500ms = 0;
		g_dtmf_is_tx                    = false;

		g_vfo_rssi_bar_level[0] = 0;
		g_vfo_rssi_bar_level[1] = 0;

		g_flag_reconfigure_vfos = false;

		if (g_monitor_enabled)
			ACTION_Monitor();   // 1of11
	}

	if (g_flag_refresh_menu)
	{
		g_flag_refresh_menu = false;
		g_menu_count_down   = menu_timeout_500ms;

		MENU_ShowCurrentSetting();
	}

	if (g_search_flag_start_scan)
	{
		g_search_flag_start_scan = false;
		g_monitor_enabled        = false;

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

	GUI_SelectNextDisplay(g_request_display_screen);

	g_request_display_screen = DISPLAY_INVALID;
	g_update_display         = true;
}
