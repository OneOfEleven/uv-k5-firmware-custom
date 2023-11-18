
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

bool g_pan_enabled = false;

// a list of frequencies to ignore/skip when scanning
uint8_t g_panadapter_rssi[PANADAPTER_BINS + 1 + PANADAPTER_BINS];
int     g_panadapter_rssi_index;

int     g_panadapter_vfo_mode;     // > 0 if we're currently sampling the VFO

void PAN_set_freq(void)
{	// set the frequency

	const uint32_t step_size = g_tx_vfo->step_freq;
	uint32_t       freq      = g_tx_vfo->p_rx->frequency;

	if (g_panadapter_vfo_mode <= 0)
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
	if (g_panadapter_vfo_mode <= 0)
		BK4819_write_reg(0x13, (g_orig_lnas << 8) | (g_orig_lna << 5) | (g_orig_mixer << 3) | (g_orig_pga << 0));
}

void PAN_clear(void)
{
	g_panadapter_rssi_index = 0;
	memset(g_panadapter_rssi, 0, sizeof(g_panadapter_rssi));

	g_panadapter_vfo_mode = 1;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		UART_printf("%u\r\n", g_panadapter_rssi_index);
	#endif
}

void PAN_enable(const bool enable)
{
	if (enable && g_eeprom.config.setting.panadapter)
	{
		if (!g_pan_enabled)
		{
			PAN_clear();
			g_panadapter_vfo_mode = 0;
			PAN_set_freq();
			g_pan_enabled         = true;
			//g_update_display      = true;
			UI_DisplayMain_pan(true);
		}
	}
	else
	{
		if (g_pan_enabled)
		{
			PAN_clear();
			g_panadapter_vfo_mode = 1;
			PAN_set_freq();
			g_pan_enabled         = false;
			g_update_display      = true;
		}
	}
}

bool PAN_process_10ms(void)
{
	if (!g_pan_enabled)
		return false;

	if (g_current_function == FUNCTION_TRANSMIT    ||
	    g_current_function == FUNCTION_POWER_SAVE  ||
	    g_current_function == FUNCTION_NEW_RECEIVE ||
	    g_current_function == FUNCTION_RECEIVE)
		return false;

	if (g_current_display_screen == DISPLAY_SEARCH ||
	    g_css_scan_mode != CSS_SCAN_MODE_OFF       ||
	    g_scan_state_dir != SCAN_STATE_DIR_OFF)
		return false;

	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_mode)
			return false;
	#endif

	if (g_squelch_open || g_monitor_enabled)
		return false;

	if (g_panadapter_vfo_mode <= 0)
	{	// save the current RSSI value
		const uint16_t rssi = BK4819_GetRSSI();
		g_panadapter_rssi[g_panadapter_rssi_index] = (rssi <= 255) ? rssi : 255;
	}

	if (g_panadapter_vfo_mode <= 0)
	{
		if (++g_panadapter_rssi_index >= (int)ARRAY_SIZE(g_panadapter_rssi))
			g_panadapter_rssi_index = 0;

		// switch back to the VFO frequency once every 16 frequency steps
		g_panadapter_vfo_mode = ((g_panadapter_rssi_index & 15u) == 0) ? 1 : 0;
	}
	else
	if (++g_panadapter_vfo_mode >= 8)
	{
		g_panadapter_vfo_mode = 0;
	}

	PAN_set_freq();

	if (g_panadapter_rssi_index == 0 && g_panadapter_vfo_mode <= 1)
		UI_DisplayMain_pan(true);  // the last bin value - show the panadapter
//		g_update_display = true;

	return (g_panadapter_vfo_mode <= 0) ? true : false;
}
