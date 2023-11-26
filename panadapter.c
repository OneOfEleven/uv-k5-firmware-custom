
#ifdef ENABLE_AM_FIX
	#include "am_fix.h"
#endif
#include "app/dtmf.h"
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
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/ui.h"

bool          g_panadapter_enabled;
#ifdef ENABLE_PANADAPTER_PEAK_FREQ
	uint32_t  g_panadapter_peak_freq;
#endif
int           g_panadapter_vfo_tick;     // >0 if we're currently monitoring the VFO/center frequency
unsigned int  g_panadapter_cycles;       //
uint8_t       g_panadapter_max_rssi;     //
uint8_t       g_panadapter_min_rssi;     //
uint8_t       g_panadapter_rssi[PANADAPTER_BINS + 1 + PANADAPTER_BINS]; // holds the RSSI samples
int           panadapter_rssi_index;     //
int           panadapter_delay;          // used to give the VCO/PLL/RSSI time to settle

const uint8_t panadapter_min_rssi = (-147 + 160) * 2;  // -147dBm (S0) min RSSI value

inline void PAN_restart(const bool full)
{
	if (full)
		g_panadapter_cycles = 0;
	panadapter_rssi_index = 0;
	panadapter_delay      = 3;
}

bool PAN_scanning(void)
{
	return (g_eeprom.config.setting.panadapter && g_panadapter_enabled && g_panadapter_vfo_tick <= 0) ? true : false;
}

void PAN_update_min_max(void)
{	// compute the min/max RSSI values

	register unsigned int   i;
	register const uint8_t *p        = g_panadapter_rssi;
	register uint8_t        max_rssi = *p;
	register uint8_t        min_rssi = *p++;

	for (i = ARRAY_SIZE(g_panadapter_rssi) - 1; i > 0; i--)
	{
		const uint8_t rssi = *p++;
		if (max_rssi < rssi) max_rssi = rssi;
		if (min_rssi > rssi) min_rssi = rssi;
	}

	g_panadapter_max_rssi = max_rssi;
	g_panadapter_min_rssi = min_rssi;
}

#ifdef ENABLE_PANADAPTER_PEAK_FREQ
	void PAN_find_peak(void)
	{	// find the peak frequency

		const int32_t center_freq = g_tx_vfo->p_rx->frequency;
		int32_t step_size = g_tx_vfo->step_freq;

		int i;

		uint8_t  threshold_rssi;
		uint8_t  peak_rssi = 0;
		uint32_t peak_freq = 0;
		uint8_t  span_rssi = g_panadapter_max_rssi - g_panadapter_min_rssi;

		if (span_rssi < 80)
			span_rssi = 80;
		threshold_rssi = g_panadapter_min_rssi + (span_rssi / 4);

		// limit the step size
		step_size = (step_size < PANADAPTER_MIN_STEP) ? PANADAPTER_MIN_STEP : (step_size > PANADAPTER_MAX_STEP) ? PANADAPTER_MAX_STEP : step_size;

		for (i = 0; i < (int)ARRAY_SIZE(g_panadapter_rssi); i++)
		{
			const uint8_t rssi = g_panadapter_rssi[i];
			if (peak_rssi < rssi && rssi >= threshold_rssi && (i < (PANADAPTER_BINS - 1) || i > (PANADAPTER_BINS + 1)))
			{
				peak_rssi = rssi;
				peak_freq = center_freq + (step_size * (i - PANADAPTER_BINS));
			}
		}

		g_panadapter_peak_freq = peak_freq;
	}
#endif

void PAN_set_freq(void)
{	// set the VCO/PLL frequency

	int32_t freq = g_tx_vfo->p_rx->frequency;

	if (g_current_function == FUNCTION_TRANSMIT || panadapter_rssi_index < 0)
		return;

	// if not paused on the VFO/center freq, add the panadapter bin offset frequency
	if (g_panadapter_enabled && g_panadapter_vfo_tick <= 0)
	{
		int32_t step_size = g_tx_vfo->step_freq;

		// limit the step size
		step_size = (step_size < PANADAPTER_MIN_STEP) ? PANADAPTER_MIN_STEP : (step_size > PANADAPTER_MAX_STEP) ? PANADAPTER_MAX_STEP : step_size;

		freq += step_size * (panadapter_rssi_index - PANADAPTER_BINS);
	}

	BK4819_set_rf_frequency(freq, true);  // set the VCO/PLL
	//BK4819_set_rf_filter_path(freq);    // set the proper LNA/PA filter path .. don't bother, we're not moving far from the VFO/center frequency

	#ifdef ENABLE_AM_FIX
		// set front end gains
		if (g_panadapter_vfo_tick <= 0 || g_tx_vfo->channel.mod_mode == MOD_MODE_FM)
			BK4819_write_reg(0x13, (g_orig_lnas << 8) | (g_orig_lna << 5) | (g_orig_mixer << 3) | (g_orig_pga << 0));
		else
			AM_fix_set_front_end_gains(g_eeprom.config.setting.tx_vfo_num);
	#endif
}

void PAN_process_10ms(void)
{
	if (!g_eeprom.config.setting.panadapter         ||
	     #ifdef ENABLE_FMRADIO
			g_fm_radio_mode                         ||
	     #endif
	     g_reduced_service                          ||
	     g_current_function == FUNCTION_POWER_SAVE  ||
	     g_current_display_screen == DISPLAY_SEARCH ||
	     g_css_scan_mode   != CSS_SCAN_MODE_OFF     ||
	     g_scan_state_dir  != SCAN_STATE_DIR_OFF    ||
		 g_dtmf_call_state != DTMF_CALL_STATE_NONE  ||
	     g_eeprom.config.setting.dual_watch != DUAL_WATCH_OFF)
	{
		if (g_panadapter_enabled)
		{	// disable the panadapter
			g_panadapter_enabled = false;
			PAN_restart(true);
			PAN_set_freq();
			g_update_display = true;
		}
		return;
	}

	if (g_current_function == FUNCTION_TRANSMIT || g_monitor_enabled)
	{
		panadapter_rssi_index = -1;
		return;
	}

	if (!g_panadapter_enabled)
	{	// enable the panadapter
		PAN_restart(false);
		g_panadapter_vfo_tick = 0;
		g_panadapter_enabled  = true;
		PAN_set_freq();
		g_update_display = true;
		return;
	}

	if (panadapter_rssi_index < 0)
	{	// guess we've just come out of TX mode
		g_panadapter_vfo_tick = 100;  // 1 sec - stay on the VFO frequency for at least this amount of time after PTT release
		PAN_restart(false);
//		PAN_set_freq();
		return;
	}

	if (g_panadapter_vfo_tick > 0)
	{	// we're paused on/monitoring the VFO/center frequency

		// save the current RSSI value into the center of the panadapter
		const int16_t rssi = g_current_rssi[g_eeprom.config.setting.tx_vfo_num];
		g_panadapter_rssi[PANADAPTER_BINS] = (rssi > 255) ? 255 : (rssi < panadapter_min_rssi) ? panadapter_min_rssi : rssi;

		PAN_update_min_max();

		// stay on the VFO/center frequency for a further 400ms after carrier drop
		g_panadapter_vfo_tick = g_squelch_open ? 40 : g_panadapter_vfo_tick - 1;

		if (g_panadapter_vfo_tick > 0)
		{
			if (--panadapter_delay <= 0)
			{	// update the screen every 200ms while on the VFO/center frequency
				panadapter_delay = 20;
				if (!g_dtmf_input_mode)
					UI_DisplayMain_pan(true);
				//else
				//	g_update_display = true;
			}
			return;
		}

		// back to scan/sweep mode
		PAN_set_freq();
		panadapter_delay = 3;  // give the VCO/PLL/RSSI a little more time to settle
	}

	// scanning/sweeping

	// let the VCO/PLL/RSSI settle before sampling the RSSI
	if (--panadapter_delay >= 0)
		return;
	panadapter_delay = 0;

	// save the current RSSI value into the panadapter
	const uint16_t rssi = BK4819_GetRSSI();
	g_panadapter_rssi[panadapter_rssi_index] = (rssi > 255) ? 255 : (rssi < panadapter_min_rssi) ? panadapter_min_rssi : rssi;

	// next scan/sweep frequency
	if (++panadapter_rssi_index >= (int)ARRAY_SIZE(g_panadapter_rssi))
	{
		panadapter_rssi_index = 0;
		panadapter_delay      = 3;  // give the VCO/PLL/RSSI a little more time to settle
	}

//	if (g_tx_vfo->channel.mod_mode == MOD_MODE_FM && g_panadapter_cycles > 0)
//	{	// switch back to the VFO/center frequency for 100ms once every 400ms
//		g_panadapter_vfo_tick = ((panadapter_rssi_index % 40) == 0) ? 10 : 0;
//	}
//	else
	{	// switch back to the VFO/center frequency for 100ms once per full sweep/scan cycle
		g_panadapter_vfo_tick = (panadapter_rssi_index == 0) ? 10 : 0;
	}

	// set the VCO/PLL frequency
	PAN_set_freq();

	if (panadapter_rssi_index != 0)
		return;

	// completed a full sweep/scan, draw the panadapter on-screen

	if (g_panadapter_cycles + 1)  // prevent wrap-a-round/over-flow
		g_panadapter_cycles++;

	PAN_update_min_max();

	#ifdef ENABLE_PANADAPTER_PEAK_FREQ
		PAN_find_peak();
	#endif

	if (!g_dtmf_input_mode)
		UI_DisplayMain_pan(true);
//	else
//		g_update_display = true;
}
