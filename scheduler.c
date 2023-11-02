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

#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "app/search.h"
#include "audio.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"

#include "driver/backlight.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/gpio.h"

#define DECREMENT(cnt) \
	do {               \
		if (cnt > 0)   \
			cnt--;     \
	} while (0)

#define DECREMENT_AND_TRIGGER(cnt, flag) \
	do {                                 \
		if (cnt > 0)                     \
			if (--cnt == 0)              \
				flag = true;             \
	} while (0)

static volatile uint32_t g_global_sys_tick_counter;

void SystickHandler(void);

// we come here every 10ms
void SystickHandler(void)
{
	g_global_sys_tick_counter++;
	
	g_next_time_slice = true;

	if ((g_global_sys_tick_counter % 50) == 0)
	{	// 500ms tick

		g_next_time_slice_500ms = true;
		
		DECREMENT_AND_TRIGGER(g_tx_timer_tick_500ms, g_tx_timeout_reached);
		DECREMENT(g_serial_config_tick_500ms);
	}

	if ((g_global_sys_tick_counter & 3) == 0)
		g_next_time_slice_40ms = true;

	#ifdef ENABLE_NOAA
		DECREMENT(g_noaa_tick_10ms);
	#endif

	DECREMENT(g_found_cdcss_tick_10ms);

	DECREMENT(g_found_ctcss_tick_10ms);

	if (g_current_function == FUNCTION_FOREGROUND)
		DECREMENT_AND_TRIGGER(g_schedule_power_save_tick_10ms, g_schedule_power_save);

	if (g_current_function == FUNCTION_POWER_SAVE)
		DECREMENT_AND_TRIGGER(g_power_save_tick_10ms, g_power_save_expired);

	if (g_eeprom.config.setting.dual_watch != DUAL_WATCH_OFF &&
	    g_scan_state_dir == SCAN_STATE_DIR_OFF &&
	    g_css_scan_mode == CSS_SCAN_MODE_OFF)
	{
		if (g_current_function == FUNCTION_FOREGROUND || g_current_function == FUNCTION_POWER_SAVE)
			DECREMENT(g_dual_watch_tick_10ms);
	}
		
	#ifdef ENABLE_NOAA
		if (g_scan_state_dir == SCAN_STATE_DIR_OFF &&
		    g_css_scan_mode == CSS_SCAN_MODE_OFF &&
		    g_eeprom.config.setting.dual_watch == DUAL_WATCH_OFF &&
		    g_noaa_mode &&
		   !g_monitor_enabled &&
		    g_current_function != FUNCTION_TRANSMIT)
		{
			if (g_current_function != FUNCTION_RECEIVE)
				DECREMENT_AND_TRIGGER(g_noaa_tick_10ms, g_schedule_noaa);
		}
	#endif

	if (g_scan_state_dir != SCAN_STATE_DIR_OFF || g_css_scan_mode == CSS_SCAN_MODE_SCANNING)
		if (!g_monitor_enabled && g_current_function != FUNCTION_TRANSMIT)
			DECREMENT(g_scan_pause_tick_10ms);

	DECREMENT_AND_TRIGGER(g_tail_tone_elimination_tick_10ms, g_flag_tail_tone_elimination_complete);

	#ifdef ENABLE_VOICE
		DECREMENT_AND_TRIGGER(g_play_next_voice_tick_10ms, g_flag_play_queued_voice);
	#endif
	
	#ifdef ENABLE_FMRADIO
		if (g_fm_scan_state_dir != FM_SCAN_STATE_DIR_OFF &&
		   !g_monitor_enabled &&
		    g_current_function != FUNCTION_TRANSMIT &&
		    g_current_function != FUNCTION_RECEIVE)
		{
			DECREMENT_AND_TRIGGER(g_fm_play_tick_10ms, g_fm_schedule);
		}
	#endif

	#ifdef ENABLE_VOX
		DECREMENT(g_vox_stop_tick_10ms);
	#endif

	DECREMENT(g_boot_tick_10ms);
}
