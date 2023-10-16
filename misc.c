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

#include "misc.h"
#include "settings.h"

const uint8_t obfuscate_array[16] = {
	0x16, 0x6C, 0x14, 0xE6, 0x2E, 0x91, 0x0D, 0x40, 0x21, 0x35, 0xD5, 0x40, 0x13, 0x03, 0xE9, 0x80
};

// ***********************************************

const uint8_t         fm_resume_countdown_500ms        =  2500 / 500;   // 2.5 seconds
const uint8_t         fm_radio_countdown_500ms         =  2000 / 500;   // 2 seconds
const uint16_t        fm_play_countdown_scan_10ms      =   100 / 10;    // 100ms
const uint16_t        fm_play_countdown_noscan_10ms    =  1200 / 10;    // 1.2 seconds
const uint16_t        fm_restore_countdown_10ms        =  5000 / 10;    // 5 seconds

const uint8_t         menu_timeout_500ms               =  30000 / 500;  // 30 seconds
const uint16_t        menu_timeout_long_500ms          = 120000 / 500;  // 2 minutes

const uint16_t        backlight_tx_rx_time_500ms       =  10000 / 500;  // 10 seconds

const uint8_t         dtmf_rx_live_timeout_500ms       =   6000 / 500;  // 6 seconds live decoder on screen
const uint8_t         dtmf_rx_timeout_500ms            =  10000 / 500;  // 10 seconds till we wipe the DTMF receiver
const uint8_t         dtmf_decode_ring_countdown_500ms =  15000 / 500;  // 15 seconds .. time we sound the ringing for
const uint8_t         dtmf_txstop_countdown_500ms      =   3000 / 500;  // 6 seconds

const uint8_t         serial_config_count_down_500ms   =   3000 / 500;  // 3 seconds

const uint8_t         key_input_timeout_500ms          =   6000 / 500;  // 6 seconds
const uint8_t         key_lock_timeout_500ms           =  30000 / 500;  // 30 seconds

const uint8_t         key_debounce_10ms                =     30 / 10;   // 30ms
const uint8_t         key_long_press_10ms              =    300 / 10;   // 300ms
const uint8_t         key_repeat_10ms                  =     50 / 10;   // 50ms

const uint16_t        scan_freq_css_timeout_10ms       =  10000 / 10;   // 10 seconds
const uint8_t         scan_freq_css_delay_10ms         =    210 / 10;   // 210ms .. don't reduce this

const uint16_t        dual_watch_count_after_tx_10ms   =   3600 / 10;   // 3.6 sec after TX ends
const uint16_t        dual_watch_count_after_rx_10ms   =   1000 / 10;   // 1 sec after RX ends ?
const uint16_t        dual_watch_count_after_1_10ms    =   5000 / 10;   // 5 sec
const uint16_t        dual_watch_count_after_2_10ms    =   3600 / 10;   // 3.6 sec
const uint16_t        dual_watch_count_noaa_10ms       =     70 / 10;   // 70ms
#ifdef ENABLE_VOX
	const uint16_t    dual_watch_count_after_vox_10ms  =    200 / 10;   // 200ms
#endif
const uint16_t        dual_watch_count_toggle_10ms     =    100 / 10;   // 100ms between VFO toggles

const uint16_t        scan_pause_1_10ms                =   5000 / 10;   // 5 seconds
const uint16_t        scan_pause_2_10ms                =    500 / 10;   // 500ms
const uint16_t        scan_pause_3_10ms                =    200 / 10;   // 200ms
const uint16_t        scan_pause_4_10ms                =    300 / 10;   // 300ms
const uint16_t        scan_pause_5_10ms                =   1000 / 10;   // 1 sec
const uint16_t        scan_pause_6_10ms                =    100 / 10;   // 100ms
const uint16_t        scan_pause_7_10ms                =   3600 / 10;   // 3.6 seconds

const uint16_t        battery_save_count_10ms          =  10000 / 10;   // 10 seconds

const uint16_t        power_save1_10ms                 =    100 / 10;   // 100ms
const uint16_t        power_save2_10ms                 =    200 / 10;   // 200ms

#ifdef ENABLE_VOX
	const uint16_t    vox_stop_count_down_10ms         =   1000 / 10;   // 1 second
#endif

const uint16_t        noaa_count_down_10ms              =  5000 / 10;   // 5 seconds
const uint16_t        noaa_count_down_2_10ms            =   500 / 10;   // 500ms
const uint16_t        noaa_count_down_3_10ms            =   200 / 10;   // 200ms

// ***********************************************

const uint32_t        g_default_aes_key[4]                = {0x4AA5CC60, 0x0312CC5F, 0xFFD2DABB, 0x6BBA7F92};

const uint8_t         g_mic_gain_dB_2[5]                  = {3, 8, 16, 24, 31};

#ifdef ENABLE_KILL_REVIVE
	bool              g_setting_radio_disabled;
#endif

bool                  g_setting_350_tx_enable;
bool                  g_setting_174_tx_enable;
bool                  g_setting_470_tx_enable;
bool                  g_setting_350_enable;
bool                  g_setting_tx_enable;
uint8_t               g_setting_freq_lock;
bool                  g_setting_scramble_enable;

uint8_t               g_setting_backlight_on_tx_rx;

#ifdef ENABLE_AM_FIX
	bool              g_setting_am_fix;
#endif
#ifdef ENABLE_AM_FIX_TEST1
	uint8_t           g_setting_am_fix_test1 = 0;
#endif
#ifdef ENABLE_AUDIO_BAR
	bool              g_setting_mic_bar;
#endif
bool                  g_setting_live_dtmf_decoder;
uint8_t               g_setting_battery_text;

uint8_t               g_setting_contrast;

uint8_t               g_setting_side1_short;
uint8_t               g_setting_side1_long;
uint8_t               g_setting_side2_short;
uint8_t               g_setting_side2_long;

bool                  g_monitor_enabled = false;           // true opens the squelch

uint32_t              g_custom_aes_key[4];
bool                  g_has_custom_aes_key;
uint32_t              g_challenge[4];

uint16_t              g_eeprom_rssi_calib[7][4];

//uint16_t              g_eeprom_1F8A;
//uint16_t              g_eeprom_1F8C;

uint8_t               g_user_channel_attributes[FREQ_CHANNEL_LAST + 1];

volatile uint16_t     g_battery_save_count_down_10ms = battery_save_count_10ms;

volatile bool         g_power_save_expired;
volatile bool         g_schedule_power_save;

volatile bool         g_schedule_dual_watch = true;

volatile uint16_t     g_dual_watch_count_down_10ms;
volatile bool         g_dual_watch_count_down_expired = true;

volatile uint8_t      g_serial_config_count_down_500ms;

volatile bool         g_next_time_slice_500ms;

volatile uint16_t     g_tx_timer_count_down_500ms;
volatile bool         g_tx_timeout_reached;

volatile uint16_t     g_tail_tone_elimination_count_down_10ms;

#ifdef ENABLE_NOAA
	volatile uint16_t g_noaa_count_down_10ms;
#endif

bool                  g_enable_speaker;
uint8_t               g_key_input_count_down;
uint8_t               g_key_lock_count_down_500ms;
uint8_t               g_rtte_count_down;
bool                  g_password_locked;
uint8_t               g_update_status;
uint8_t               g_found_ctcss;
uint8_t               g_found_cdcss;
bool                  g_end_of_rx_detected_maybe;

int16_t               g_vfo_rssi[2];
uint8_t               g_vfo_rssi_bar_level[2];

uint8_t               g_reduced_service;
uint8_t               g_battery_voltage_index;
css_scan_mode_t       g_css_scan_mode;
bool                  g_update_rssi;
#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
	alarm_state_t     g_alarm_state;
#endif
uint16_t              g_menu_count_down;
bool                  g_flag_reconfigure_vfos;
uint8_t               g_vfo_configure_mode;
bool                  g_flag_reset_vfos;
bool                  g_request_save_vfo;
uint8_t               g_request_save_channel;
bool                  g_request_save_settings;
#ifdef ENABLE_FMRADIO
	bool              g_request_save_fm;
#endif
bool                  g_flag_prepare_tx;

bool                  g_flag_accept_setting;
bool                  g_flag_refresh_menu;

bool                  g_flag_save_vfo;
bool                  g_flag_save_settings;
bool                  g_flag_save_channel;
#ifdef ENABLE_FMRADIO
	bool              g_flag_save_fm;
#endif
bool                  g_cdcss_lost;
uint8_t               g_cdcss_code_type;
bool                  g_ctcss_lost;
bool                  g_cxcss_tail_found;
#ifdef ENABLE_VOX
	bool              g_vox_lost;
	bool              g_vox_noise_detected;
	uint16_t          g_vox_resume_count_down;
	uint16_t          g_vox_pause_count_down;
#endif
bool                  g_squelch_lost;

uint8_t               g_flash_light_state;
volatile uint16_t     g_flash_light_blink_counter;
#ifdef ENABLE_SOS_FLASHLIGHT
	uint8_t               g_flash_light_SOS_Sequence[18] = {1,1,1,1,1,3,3,1,3,1,3,3,1,1,1,1,1,7};
	volatile uint8_t      g_flash_light_SOS_Index = 0;
	volatile uint8_t      g_flash_light_SOS_Wait_Length = 0;
	volatile uint8_t      g_flash_light_SOS_Wait_Index = 0;
#endif

bool                  g_flag_end_tx;
uint16_t              g_low_batteryCountdown;

reception_mode_t      g_rx_reception_mode;

uint8_t               g_scan_next_channel;
uint8_t               g_scan_restore_channel;
scan_next_chan_t      g_scan_current_scan_list;
uint32_t              g_scan_restore_frequency;
bool                  g_scan_keep_frequency;
bool                  g_scan_pause_mode;
volatile uint16_t     g_scan_pause_10ms;
scan_state_dir_t      g_scan_state_dir;

bool                  g_rx_vfo_is_active;
#ifdef ENABLE_ALARM
	uint8_t           g_alarm_tone_counter;
	uint16_t          g_alarm_running_counter;
#endif
uint8_t               g_menu_list_count;
uint8_t               g_backup_cross_vfo_rx_tx;

#ifdef ENABLE_NOAA
	bool              g_is_noaa_mode;
	uint8_t           g_noaa_channel;
#endif

bool                  g_update_display;

bool                  g_unhide_hidden = false;

volatile bool         g_next_time_slice;
volatile uint8_t      g_found_cdcss_count_down_10ms;
volatile uint8_t      g_found_ctcss_count_down_10ms;
#ifdef ENABLE_VOX
	volatile uint16_t g_vox_stop_count_down_10ms;
#endif
volatile bool         g_next_time_slice_40ms;
#ifdef ENABLE_NOAA
	volatile uint16_t g_noaa_count_down_10ms = 0;
	volatile bool     g_schedule_noaa       = true;
#endif
volatile bool         g_flag_tail_tone_elimination_complete;
#ifdef ENABLE_FMRADIO
	volatile bool     g_schedule_fm;
#endif

volatile uint16_t     g_boot_counter_10ms = 4000 / 10;   // 4 seconds

int16_t               g_current_rssi[2] = {0, 0};  // now one per VFO

unsigned int get_RX_VFO(void)
{
	unsigned int rx_vfo = g_eeprom.tx_vfo;
	if (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_CHAN_B)
		rx_vfo = 0;
	else
	if (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_CHAN_A)
		rx_vfo = 1;
	else
	if (g_eeprom.dual_watch == DUAL_WATCH_CHAN_B)
		rx_vfo = 1;
	else
	if (g_eeprom.dual_watch == DUAL_WATCH_CHAN_A)
		rx_vfo = 0;
	return rx_vfo;
}

unsigned int get_TX_VFO(void)
{
	unsigned int tx_vfo = g_eeprom.tx_vfo;
	if (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_CHAN_B)
		tx_vfo = 1;
	else
	if (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_CHAN_A)
		tx_vfo = 0;
	else
	if (g_eeprom.dual_watch == DUAL_WATCH_CHAN_B)
		tx_vfo = 1;
	else
	if (g_eeprom.dual_watch == DUAL_WATCH_CHAN_A)
		tx_vfo = 0;
	return tx_vfo;
}

void NUMBER_Get(char *pDigits, uint32_t *pInteger)
{
	unsigned int i;
	uint32_t     Multiplier = 10000000;
	uint32_t     Value      = 0;
	for (i = 0; i < 8; i++)
	{
		if (pDigits[i] > 9)
			break;
		Value += pDigits[i] * Multiplier;
		Multiplier /= 10U;
	}
	*pInteger = Value;
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
