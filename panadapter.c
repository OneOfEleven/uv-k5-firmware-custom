
#ifdef ENABLE_AM_FIX
	#include "am_fix.h"
#endif
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
#include "panadapter.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "driver/bk4819.h"
#include "functions.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/main.h"
#include "ui/ui.h"

bool         g_panadapter_enabled;
#ifdef ENABLE_PANADAPTER_PEAK_FREQ
	uint32_t g_panadapter_peak_freq;
#endif
int          g_panadapter_vfo_mode;     // > 0 if we're currently sampling the VFO
uint8_t      g_panadapter_rssi[PANADAPTER_BINS + 1 + PANADAPTER_BINS];
uint8_t      g_panadapter_max_rssi;
uint8_t      g_panadapter_min_rssi;
unsigned int panadapter_rssi_index;

bool PAN_scanning(void)
{
	return (g_eeprom.config.setting.panadapter && g_panadapter_enabled && g_panadapter_vfo_mode <= 0) ? true : false;
}

void PAN_set_freq(void)
{	// set the frequency

	const uint32_t step_size = g_tx_vfo->step_freq;
	uint32_t       freq      = g_tx_vfo->p_rx->frequency;

	if (g_panadapter_enabled && g_panadapter_vfo_mode <= 0)
	{	// panadapter mode .. add the bin offset
		if (panadapter_rssi_index < PANADAPTER_BINS)
			freq -= step_size * (PANADAPTER_BINS - panadapter_rssi_index);
		else
		if (panadapter_rssi_index > PANADAPTER_BINS)
			freq += step_size * (panadapter_rssi_index - PANADAPTER_BINS);
	}

	BK4819_set_rf_frequency(freq, true);  // set the VCO/PLL
	//BK4819_set_rf_filter_path(freq);    // set the proper LNA/PA filter path

	// default front end gains
	#ifdef ENABLE_AM_FIX
		if (g_panadapter_vfo_mode <= 0 || g_tx_vfo->channel.mod_mode == MOD_MODE_FM)
			BK4819_write_reg(0x13, (g_orig_lnas << 8) | (g_orig_lna << 5) | (g_orig_mixer << 3) | (g_orig_pga << 0));
		else
			AM_fix_set_front_end_gains(g_eeprom.config.setting.tx_vfo_num);
	#endif
}

void PAN_process_10ms(void)
{
	uint16_t rssi;

	if (!g_eeprom.config.setting.panadapter         ||
	#ifdef ENABLE_FMRADIO
		 g_fm_radio_mode                            ||
	#endif
	     g_reduced_service                          ||
	     g_monitor_enabled                          ||
	     g_current_function == FUNCTION_TRANSMIT    ||
	     g_current_function == FUNCTION_POWER_SAVE  ||
	     g_current_display_screen == DISPLAY_SEARCH ||
	     g_css_scan_mode  != CSS_SCAN_MODE_OFF      ||
	     g_scan_state_dir != SCAN_STATE_DIR_OFF)
	{
		if (g_panadapter_enabled)
		{	// disable the panadapter

			#ifdef ENABLE_PANADAPTER_PEAK_FREQ
				g_panadapter_peak_freq = 0;
			#endif
			g_panadapter_vfo_mode  = 1;
			g_panadapter_enabled   = false;
			PAN_set_freq();

			g_update_display = true;
		}

		return;
	}

	if (!g_panadapter_enabled)
	{	// enable the panadapter

		#ifdef ENABLE_PANADAPTER_PEAK_FREQ
			g_panadapter_peak_freq = 0;
		#endif
		g_panadapter_vfo_mode  = 0;
//		g_panadapter_max_rssi  = 0;
//		g_panadapter_min_rssi  = 0;
		panadapter_rssi_index  = 0;
//		memset(g_panadapter_rssi, 0, sizeof(g_panadapter_rssi));
		g_panadapter_enabled   = true;
		PAN_set_freq();

		g_update_display = true;
		return;
	}

	if (g_panadapter_vfo_mode > 0 && g_squelch_open)
	{	// we have a signal on the VFO frequency

		// save the current RSSI value .. center bin is the VFO frequency
		rssi = g_current_rssi[g_eeprom.config.setting.tx_vfo_num];
		//if (g_tx_vfo->channel.mod_mode == MOD_MODE_FM)
			g_panadapter_rssi[PANADAPTER_BINS] = (rssi <= 255) ? rssi : 255;

//		g_update_display = true;

		g_panadapter_vfo_mode = 50;   // pause scanning for at least another 500ms
		return;
	}

	if (g_panadapter_vfo_mode <= 0)
	{	// scanning

		// save the current RSSI value
		rssi = BK4819_GetRSSI();
		g_panadapter_rssi[panadapter_rssi_index] = (rssi <= 255) ? rssi : 255;

		// next frequency
		if (++panadapter_rssi_index >= ARRAY_SIZE(g_panadapter_rssi))
			panadapter_rssi_index = 0;

		if (g_tx_vfo->channel.mod_mode == MOD_MODE_FM)
		{	// switch back to the VFO frequency for 90ms once every 250ms
			g_panadapter_vfo_mode = ((panadapter_rssi_index % 25) == 0) ? 9 : 0;
		}
		else
		{	// switch back to the VFO frequency for 90ms once each scan cycle
			g_panadapter_vfo_mode = (panadapter_rssi_index == 0) ? 9 : 0;
		}
	}
	else
	{	// checking the VFO frequency for a signal .. we periodically do this
		g_panadapter_vfo_mode--;
	}

	PAN_set_freq();

	// the last bin value .. draw the panadapter once each scan cycle
	if (panadapter_rssi_index == 0)
	{
		int i;

		g_panadapter_max_rssi = g_panadapter_rssi[0];
		g_panadapter_min_rssi = g_panadapter_rssi[0];
		for (i = 1; i < (int)ARRAY_SIZE(g_panadapter_rssi); i++)
		{
			const uint8_t rssi = g_panadapter_rssi[i];
			if (g_panadapter_max_rssi < rssi)
				g_panadapter_max_rssi = rssi;
			if (g_panadapter_min_rssi > rssi)
				g_panadapter_min_rssi = rssi;
		}

		#ifdef ENABLE_PANADAPTER_PEAK_FREQ
		{	// find the peak freq
			const int32_t step_size   = g_tx_vfo->step_freq;
			const int32_t center_freq = g_tx_vfo->p_rx->frequency;
			uint8_t peak_rssi = 0;
			uint8_t threshold_rssi;
			uint8_t span_rssi = g_panadapter_max_rssi - g_panadapter_min_rssi;
			if (span_rssi < 80)
				span_rssi = 80;
			threshold_rssi = g_panadapter_min_rssi + (span_rssi / 4);
			g_panadapter_peak_freq = 0;
			for (i = 0; i < (int)ARRAY_SIZE(g_panadapter_rssi); i++)
			{
				const uint8_t rssi = g_panadapter_rssi[i];
				if (peak_rssi < rssi && rssi >= threshold_rssi && (i < (PANADAPTER_BINS - 1) || i > (PANADAPTER_BINS + 1)))
				{
					peak_rssi = rssi;
					g_panadapter_peak_freq = center_freq + (step_size * (i - PANADAPTER_BINS));
				}
			}
		}
		#endif

		UI_DisplayMain_pan(true);
		//g_update_display = true;
	}
}
