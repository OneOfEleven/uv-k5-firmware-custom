
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

bool    g_pan_enabled;
int     g_panadapter_vfo_mode;     // > 0 if we're currently sampling the VFO
uint8_t g_panadapter_rssi[PANADAPTER_BINS + 1 + PANADAPTER_BINS];
int     g_panadapter_rssi_index;

bool PAN_scanning(void)
{
	return (g_eeprom.config.setting.panadapter && g_pan_enabled && g_panadapter_vfo_mode <= 0) ? true : false;
}

void PAN_set_freq(void)
{	// set the frequency

	const uint32_t step_size = g_tx_vfo->step_freq;
	uint32_t       freq      = g_tx_vfo->p_rx->frequency;

	if (g_pan_enabled && g_panadapter_vfo_mode <= 0)
	{	// panadapter mode .. add the bin offset
		if (g_panadapter_rssi_index < PANADAPTER_BINS)
			freq -= step_size * (PANADAPTER_BINS - g_panadapter_rssi_index);
		else
		if (g_panadapter_rssi_index > PANADAPTER_BINS)
			freq += step_size * (g_panadapter_rssi_index - PANADAPTER_BINS);
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
	if (!g_eeprom.config.setting.panadapter         ||
	#ifdef ENABLE_FMRADIO
		 g_fm_radio_mode                            ||
	#endif
	     g_reduced_service                          ||
	     g_monitor_enabled                          ||
	     g_current_function == FUNCTION_TRANSMIT    ||
	     g_current_function == FUNCTION_POWER_SAVE  ||
	     g_current_function == FUNCTION_NEW_RECEIVE ||
	     g_current_function == FUNCTION_RECEIVE     ||
	     g_current_display_screen == DISPLAY_SEARCH ||
	     g_css_scan_mode  != CSS_SCAN_MODE_OFF      ||
	     g_scan_state_dir != SCAN_STATE_DIR_OFF)
	{
		if (g_pan_enabled)
		{	// disable the panadapter

			g_panadapter_vfo_mode = 1;
			g_pan_enabled = false;
			PAN_set_freq();

			g_update_display = true;
			//UI_DisplayMain_pan(true);
		}

		return;
	}

	if (!g_pan_enabled)
	{	// enable the panadapter

		g_panadapter_vfo_mode = 0;
		g_panadapter_rssi_index = 0;
//		memset(g_panadapter_rssi, 0, sizeof(g_panadapter_rssi));
		g_pan_enabled = true;
		PAN_set_freq();

		g_update_display = true;
		//UI_DisplayMain_pan(true);

		return;
	}

	if (g_panadapter_vfo_mode <= 0)
	{	// save the current RSSI value
		const uint16_t rssi = BK4819_GetRSSI();
		g_panadapter_rssi[g_panadapter_rssi_index] = (rssi <= 255) ? rssi : 255;
	}

	if (g_panadapter_vfo_mode <= 0)
	{	// scanning
		if (++g_panadapter_rssi_index >= (int)ARRAY_SIZE(g_panadapter_rssi))
			g_panadapter_rssi_index = 0;

		if (g_tx_vfo->channel.mod_mode == MOD_MODE_FM)
			// switch back to the VFO frequency once every 16 frequency steps .. if in FM mode
			g_panadapter_vfo_mode = ((g_panadapter_rssi_index & 15u) == 0) ? 1 : 0;
		else
			// switch back to the VFO frequency once each scan cycle if not in FM mode
			g_panadapter_vfo_mode = (g_panadapter_rssi_index == 0) ? 1 : 0;
	}
	else
	{	// checking the VFO frequency for a signal .. we do this this periodically
		if (++g_panadapter_vfo_mode >= 9)   // monitor the VFO frequency for 90ms before continuing our scan
			g_panadapter_vfo_mode = 0;
	}

	PAN_set_freq();

	// the last bin value .. draw the panadapter once each scan cycle
	if (g_panadapter_rssi_index == 0 && g_panadapter_vfo_mode <= 1)
		UI_DisplayMain_pan(true);
		//g_update_display = true;
}
