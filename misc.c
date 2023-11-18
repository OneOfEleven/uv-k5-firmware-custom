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

#include "misc.h"
#include "settings.h"

const uint8_t obfuscate_array[16] = {
	0x16, 0x6C, 0x14, 0xE6, 0x2E, 0x91, 0x0D, 0x40, 0x21, 0x35, 0xD5, 0x40, 0x13, 0x03, 0xE9, 0x80
};

// ***********************************************

const uint8_t         fm_resume_500ms                  =  2500 / 500;   // 2.5 seconds
const uint8_t         fm_radio_500ms                   =  2000 / 500;   // 2 seconds
const uint16_t        fm_play_scan_10ms                =    60 / 10;    // 60ms
const uint16_t        fm_play_noscan_10ms              =  1200 / 10;    // 1.2 seconds

const uint8_t         menu_timeout_500ms               =  30000 / 500;  // 30 seconds
const uint16_t        menu_timeout_long_500ms          = 120000 / 500;  // 2 minutes

const uint16_t        backlight_tx_rx_time_secs        =  10;           // 10 seconds

const uint8_t         dtmf_rx_live_timeout_500ms       =   6000 / 500;  // 6 seconds live decoder on screen
const uint8_t         dtmf_rx_timeout_500ms            =  10000 / 500;  // 10 seconds till we wipe the DTMF receiver
const uint8_t         dtmf_decode_ring_500ms           =  15000 / 500;  // 15 seconds .. time we sound the ringing for
const uint8_t         dtmf_txstop_500ms                =   3000 / 500;  // 6 seconds

const uint8_t         serial_config_tick_500ms         =   3000 / 500;  // 3 seconds

const uint8_t         key_input_timeout_500ms          =   6000 / 500;  // 6 seconds
#ifdef ENABLE_KEYLOCK
	const uint8_t     key_lock_timeout_500ms           =  30000 / 500;  // 30 seconds
#endif

const uint8_t         key_debounce_10ms                =     30 / 10;   // 30ms
const uint8_t         key_long_press_10ms              =    300 / 10;   // 300ms
const uint8_t         key_repeat_10ms                  =     50 / 10;   // 50ms

const uint16_t        search_freq_css_10ms             =  10000 / 10;   // 10 seconds
const uint16_t        search_10ms                      =    210 / 10;   // 210ms .. don't reduce this

#ifdef ENABLE_VOX
	const uint16_t    dual_watch_delay_after_vox_10ms  =    200 / 10;   // 200ms
#endif
const uint16_t        dual_watch_delay_after_tx_10ms   =   7000 / 10;   // 7 sec after TX ends
const uint16_t        dual_watch_delay_noaa_10ms       =     70 / 10;   // 70ms
const uint16_t        dual_watch_delay_toggle_10ms     =    100 / 10;   // 100ms between VFO toggles

const uint16_t        scan_pause_code_10ms             =   1000 / 10;   // 1 sec
const uint16_t        scan_pause_css_10ms              =    500 / 10;   // 500ms
const uint16_t        scan_pause_ctcss_10ms            =    200 / 10;   // 200ms
const uint16_t        scan_pause_cdcss_10ms            =    300 / 10;   // 300ms
const uint16_t        scan_pause_freq_10ms             =    100 / 10;   // 100ms
const uint16_t        scan_pause_chan_10ms             =    200 / 10;   // 200ms

const uint16_t        power_save_pause_10ms            =  10000 / 10;   // 10 seconds
const uint16_t        power_save1_10ms                 =    100 / 10;   // 100ms
const uint16_t        power_save2_10ms                 =    200 / 10;   // 200ms

#ifdef ENABLE_VOX
	const uint16_t    vox_stop_10ms                    =   1000 / 10;   // 1 second
#endif

const uint16_t        noaa_tick_10ms                   =   5000 / 10;   // 5 seconds
const uint16_t        noaa_tick_2_10ms                 =    500 / 10;   // 500ms
const uint16_t        noaa_tick_3_10ms                 =    200 / 10;   // 200ms

// ***********************************************

const uint32_t        g_default_aes_key[4]             = {0x4AA5CC60, 0x0312CC5F, 0xFFD2DABB, 0x6BBA7F92};

const uint8_t         g_mic_gain_dB_2[5]               = {3, 8, 16, 24, 31};

uint8_t               g_mic_sensitivity_tuning;

bool                  g_monitor_enabled;

bool                  g_has_aes_key;
uint32_t              g_challenge[4];

volatile uint16_t     g_power_save_pause_tick_10ms = power_save_pause_10ms;
volatile bool         g_power_save_pause_done;
volatile bool         g_power_save_expired;

volatile uint16_t     g_dual_watch_tick_10ms;
volatile bool         g_dual_watch_delay_down_expired = true;

volatile uint8_t      g_serial_config_tick_500ms;

volatile bool         g_next_time_slice_500ms;

volatile uint16_t     g_tx_timer_tick_500ms;
volatile bool         g_tx_timeout_reached;

volatile uint16_t     g_tail_tone_elimination_tick_10ms;

#ifdef ENABLE_NOAA
	volatile uint16_t g_noaa_tick_10ms;
#endif

uint8_t               g_update_screen_tick_500ms;

uint8_t               g_key_input_count_down;
#ifdef ENABLE_KEYLOCK
	uint8_t           g_key_lock_tick_500ms;
#endif
uint8_t               g_rtte_count_down;

uint8_t               g_update_status;
bool                  g_update_display;
bool                  g_update_rssi;
bool                  g_update_menu;

bool                  g_password_locked;

uint8_t               g_found_ctcss;
uint8_t               g_found_cdcss;

bool                  g_end_of_rx_detected_maybe;

int16_t               g_vfo_rssi[2];
uint8_t               g_vfo_rssi_bar_level[2];

uint8_t               g_reduced_service;
uint8_t               g_battery_voltage_index;
#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
	alarm_state_t     g_alarm_state;
#endif
uint16_t              g_menu_tick_10ms;
bool                  g_flag_reconfigure_vfos;
uint8_t               g_vfo_configure_mode;
bool                  g_flag_reset_vfos;
bool                  g_request_save_vfo;
uint8_t               g_request_save_channel;
bool                  g_request_save_settings;
#ifdef ENABLE_FMRADIO
	bool              g_request_save_fm;
	bool              g_flag_save_fm;
#endif

bool                  g_flag_prepare_tx;

bool                  g_flag_accept_setting;

bool                  g_flag_save_vfo;
bool                  g_flag_save_settings;
bool                  g_flag_save_channel;

css_scan_mode_t       g_css_scan_mode;

bool                  g_cdcss_lost;
uint8_t               g_cdcss_code_type;
bool                  g_ctcss_lost;
bool                  g_cxcss_tail_found;
uint8_t               g_ctcss_tail_phase_shift_rx;

#ifdef ENABLE_VOX
	bool              g_vox_lost;
	bool              g_vox_audio_detected;
	uint16_t          g_vox_resume_tick_10ms;
	uint16_t          g_vox_pause_tick_10ms;
	volatile uint16_t g_vox_stop_tick_10ms;
#endif

bool                  g_squelch_open;
reception_mode_t      g_rx_reception_mode;

uint8_t               g_flash_light_state;
uint16_t              g_flash_light_blink_tick_10ms;

bool                  g_flag_end_tx;

uint16_t              g_low_battery_tick_10ms;

uint32_t              g_scan_initial_lower;
uint32_t              g_scan_initial_upper;
uint32_t              g_scan_initial_step_size;
uint8_t               g_scan_next_channel;
scan_next_chan_t      g_scan_current_scan_list;
uint8_t               g_scan_restore_channel;
uint32_t              g_scan_restore_frequency;
bool                  g_scan_pause_time_mode;      // set if we stopped in SCAN_RESUME_TIME mode
volatile uint16_t     g_scan_tick_10ms;
scan_state_dir_t      g_scan_state_dir;

uint8_t               g_rx_vfo_num;
bool                  g_rx_vfo_is_active;

#ifdef ENABLE_ALARM
	uint16_t          g_alarm_tone_counter_10ms;
	uint16_t          g_alarm_running_counter_10ms;
#endif

uint8_t               g_menu_list_count;

uint8_t               g_backup_cross_vfo;

#ifdef ENABLE_NOAA
	bool              g_noaa_mode;
	uint8_t           g_noaa_channel;
	volatile uint16_t g_noaa_tick_10ms;
	volatile bool     g_schedule_noaa  = true;
#endif

bool                  g_unhide_hidden;

volatile bool         g_next_time_slice;
volatile bool         g_next_time_slice_40ms;

volatile uint8_t      g_found_cdcss_tick_10ms;
volatile uint8_t      g_found_ctcss_tick_10ms;

volatile bool         g_flag_tail_tone_elimination_complete;

volatile uint16_t     g_boot_tick_10ms = 4000 / 10;   // 4 seconds

int16_t               g_current_rssi[2];
uint16_t              g_current_glitch[2];
uint16_t              g_current_noise[2];

// original QS front end register settings
// 0x03BE   00000 011 101 11 110
const uint8_t         g_orig_lnas  = 3;   //   0dB
const uint8_t         g_orig_lna   = 5;   //  -4dB
const uint8_t         g_orig_mixer = 3;   //   0dB
const uint8_t         g_orig_pga   = 6;   //  -3dB

// ***************************

unsigned int get_RX_VFO(void)
{
	unsigned int rx_vfo = g_eeprom.config.setting.tx_vfo_num;
	if (g_eeprom.config.setting.cross_vfo == CROSS_BAND_CHAN_B)
		rx_vfo = 0;
	else
	if (g_eeprom.config.setting.cross_vfo == CROSS_BAND_CHAN_A)
		rx_vfo = 1;
	else
	if (g_eeprom.config.setting.dual_watch == DUAL_WATCH_CHAN_B)
		rx_vfo = 1;
	else
	if (g_eeprom.config.setting.dual_watch == DUAL_WATCH_CHAN_A)
		rx_vfo = 0;
	return rx_vfo;
}

unsigned int get_TX_VFO(void)
{
	unsigned int tx_vfo = g_eeprom.config.setting.tx_vfo_num;
	if (g_eeprom.config.setting.cross_vfo == CROSS_BAND_CHAN_B)
		tx_vfo = 1;
	else
	if (g_eeprom.config.setting.cross_vfo == CROSS_BAND_CHAN_A)
		tx_vfo = 0;
	else
	if (g_eeprom.config.setting.dual_watch == DUAL_WATCH_CHAN_B)
		tx_vfo = 1;
	else
	if (g_eeprom.config.setting.dual_watch == DUAL_WATCH_CHAN_A)
		tx_vfo = 0;
	return tx_vfo;
}

void NUMBER_Get(char *pDigits, uint32_t *pInteger)
{
	unsigned int i;
	uint32_t     mul = 10000000;
	uint32_t     val = 0;
	for (i = 0; i < 8; i++)
	{
		if (pDigits[i] > 9)
			break;
		val += pDigits[i] * mul;
		mul /= 10u;
	}
	*pInteger = val;
}

void NUMBER_ToDigits(uint32_t Value, char *pDigits)
{
	unsigned int i;
	for (i = 0; i < 8; i++)
	{
		const uint32_t Result = Value / 10U;
		pDigits[7 - i] = Value - (Result * 10U);
		Value = Result;
	}
	pDigits[8] = 0;
}

int32_t NUMBER_AddWithWraparound(int32_t Base, int32_t Add, int32_t LowerLimit, int32_t UpperLimit)
{
	Base += Add;

	if (Base == 0x7fffffff || Base < LowerLimit)
		return UpperLimit;

	if (Base > UpperLimit)
		return LowerLimit;

	return Base;
}

void NUMBER_trim_trailing_zeros(char *str)
{
	if (str != NULL)
	{
		bool found_dp = false;
		int i = 0;
		while (i < 16 && str[i] != 0)
		{
			if (str[i] == '.')
				found_dp = true;
			i++;
		}
		if (found_dp)
		{
			i--;
			while (i > 0 && (str[i] == '0' || str[i] == ' ') && str[i - 1] != '.')
				str[i--] = 0;
		}
	}
}
