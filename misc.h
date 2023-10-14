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

#ifndef MISC_H
#define MISC_H

#include <stdbool.h>
#include <stdint.h>

#ifndef ARRAY_SIZE
	#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

#ifndef MAX
//	#define MAX(a, b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#endif

#ifndef MIN
//	#define MIN(a, b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
#endif

//#define IS_USER_CHANNEL(x)     ((x) >= USER_CHANNEL_FIRST   && (x) <= USER_CHANNEL_LAST)
#define IS_USER_CHANNEL(x)     ((x) <= USER_CHANNEL_LAST)
#define IS_FREQ_CHANNEL(x)     ((x) >= FREQ_CHANNEL_FIRST && (x) <= FREQ_CHANNEL_LAST)
#define IS_VALID_CHANNEL(x)    ((x) < LAST_CHANNEL)

#define IS_NOAA_CHANNEL(x)     ((x) >= NOAA_CHANNEL_FIRST && (x) <= NOAA_CHANNEL_LAST)
//#define IS_NOT_NOAA_CHANNEL(x) ((x) >= USER_CHANNEL_FIRST   && (x) <= FREQ_CHANNEL_LAST)
#define IS_NOT_NOAA_CHANNEL(x) ((x) <= FREQ_CHANNEL_LAST)

// PTT key-up/key-down audio tone freq's used in NASA's apollo rides to the moon
#define APOLLO_TONE_MS         200     // slightly shorter tone length
//#define APOLLO_TONE_MS         250   // NASA tone length
#define APOLLO_TONE1_HZ        2525
#define APOLLO_TONE2_HZ        2475

enum {
	USER_CHANNEL_FIRST = 0,
	USER_CHANNEL_LAST  = 199u,
	FREQ_CHANNEL_FIRST = 200u,
	FREQ_CHANNEL_LAST  = 206u,
	NOAA_CHANNEL_FIRST = 207u,
	NOAA_CHANNEL_LAST  = 216u,
	LAST_CHANNEL
};

enum {
	FLASHLIGHT_OFF = 0,
	FLASHLIGHT_ON,
	FLASHLIGHT_BLINK
};

enum {
	VFO_CONFIGURE_NONE = 0,
	VFO_CONFIGURE,
	VFO_CONFIGURE_RELOAD
};

enum alarm_state_e {
	ALARM_STATE_OFF = 0,
	ALARM_STATE_TXALARM,
	ALARM_STATE_ALARM,
	ALARM_STATE_TX1750
};
typedef enum alarm_state_e alarm_state_t;

enum reception_mode_e {
	RX_MODE_NONE = 0,   // squelch close ?
	RX_MODE_DETECTED,   // signal detected
	RX_MODE_LISTENING   //
};
typedef enum reception_mode_e reception_mode_t;

enum css_scan_mode_e
{
	CSS_SCAN_MODE_OFF = 0,
	CSS_SCAN_MODE_SCANNING,
	CSS_SCAN_MODE_FOUND,
};
typedef enum css_scan_mode_e css_scan_mode_t;

enum scan_next_chan_e {
	SCAN_NEXT_CHAN_SCANLIST1 = 0,
	SCAN_NEXT_CHAN_SCANLIST2,
	SCAN_NEXT_CHAN_DUAL_WATCH,
	SCAN_NEXT_CHAN_USER,
	SCAN_NEXT_NUM
};
typedef enum scan_next_chan_e scan_next_chan_t;

enum scan_state_dir_e {
	SCAN_STATE_DIR_REVERSE = -1,
	SCAN_STATE_DIR_OFF     =  0,
	SCAN_STATE_DIR_FORWARD = +1
};
typedef enum scan_state_dir_e scan_state_dir_t;

extern const uint8_t         obfuscate_array[16];

extern const uint8_t         fm_resume_countdown_500ms;
extern const uint8_t         fm_radio_countdown_500ms;
extern const uint16_t        fm_play_countdown_scan_10ms;
extern const uint16_t        fm_play_countdown_noscan_10ms;
extern const uint16_t        fm_restore_countdown_10ms;

extern const uint8_t         menu_timeout_500ms;
extern const uint16_t        menu_timeout_long_500ms;

extern const uint8_t         dtmf_rx_live_timeout_500ms;
extern const uint8_t         dtmf_rx_timeout_500ms;
extern const uint8_t         dtmf_decode_ring_countdown_500ms;
extern const uint8_t         dtmf_txstop_countdown_500ms;

extern const uint8_t         serial_config_count_down_500ms;

extern const uint8_t         key_input_timeout_500ms;
extern const uint8_t         key_lock_timeout_500ms;

extern const uint8_t         key_debounce_10ms;
extern const uint8_t         key_long_press_10ms;
extern const uint8_t         key_repeat_10ms;

extern const uint16_t        scan_freq_css_timeout_10ms;
extern const uint8_t         scan_freq_css_delay_10ms;

extern const uint16_t        battery_save_count_10ms;

extern const uint16_t        power_save1_10ms;
extern const uint16_t        power_save2_10ms;

#ifdef ENABLE_VOX
	extern const uint16_t    vox_stop_count_down_10ms;
#endif

extern const uint16_t        noaa_count_down_10ms;
extern const uint16_t        noaa_count_down_2_10ms;
extern const uint16_t        noaa_count_down_3_10ms;

extern const uint16_t        dual_watch_count_after_tx_10ms;
extern const uint16_t        dual_watch_count_after_rx_10ms;
extern const uint16_t        dual_watch_count_after_1_10ms;
extern const uint16_t        dual_watch_count_after_2_10ms;
extern const uint16_t        dual_watch_count_toggle_10ms;
extern const uint16_t        dual_watch_count_noaa_10ms;
#ifdef ENABLE_VOX
	extern const uint16_t    dual_watch_count_after_vox_10ms;
#endif

extern const uint16_t        scan_pause_delay_in_1_10ms;
extern const uint16_t        scan_pause_delay_in_2_10ms;
extern const uint16_t        scan_pause_delay_in_3_10ms;
extern const uint16_t        scan_pause_delay_in_4_10ms;
extern const uint16_t        scan_pause_delay_in_5_10ms;
extern const uint16_t        scan_pause_delay_in_6_10ms;
extern const uint16_t        scan_pause_delay_in_7_10ms;

extern const uint8_t         g_mic_gain_dB_2[5];

extern bool                  g_setting_350_tx_enable;
extern bool                  g_setting_killed;
extern bool                  g_setting_200_tx_enable;
extern bool                  g_setting_500_tx_enable;
extern bool                  g_setting_350_enable;
extern bool                  g_setting_tx_enable;
extern uint8_t               g_setting_freq_lock;
extern bool                  g_setting_scramble_enable;

extern uint8_t               g_setting_backlight_on_tx_rx;

#ifdef ENABLE_AM_FIX
	extern bool              g_setting_am_fix;
#endif
#ifdef ENABLE_AM_FIX_TEST1
	extern uint8_t           g_setting_am_fix_test1;
#endif
#ifdef ENABLE_AUDIO_BAR
	extern bool              g_setting_mic_bar;
#endif
extern bool                  g_setting_live_dtmf_decoder;
extern uint8_t               g_setting_battery_text;

extern uint8_t               g_setting_contrast;

extern uint8_t               g_setting_side1_short;
extern uint8_t               g_setting_side1_long;
extern uint8_t               g_setting_side2_short;
extern uint8_t               g_setting_side2_long;

extern bool                  g_monitor_enabled;

extern const uint32_t        g_default_aes_key[4];
extern uint32_t              g_custom_aes_key[4];
extern bool                  g_has_custom_aes_key;
extern uint32_t              g_challenge[4];

extern uint16_t              g_eeprom_rssi_calib[7][4];
//extern uint16_t              g_eeprom_rssi_calib[2][4];

//extern uint16_t              g_eeprom_1F8A;
//extern uint16_t              g_eeprom_1F8C;

extern uint8_t               g_user_channel_attributes[207];

extern volatile uint16_t     g_battery_save_count_down_10ms;

extern volatile bool         g_power_save_expired;
extern volatile bool         g_schedule_power_save;

extern volatile bool         g_schedule_dual_watch;

extern volatile uint16_t     g_dual_watch_count_down_10ms;
extern volatile bool         g_dual_watch_count_down_expired;
extern bool                  g_dual_watch_active;

extern volatile uint8_t      g_serial_config_count_down_500ms;

extern volatile bool         g_next_time_slice_500ms;

extern volatile uint16_t     g_tx_timer_count_down_500ms;
extern volatile bool         g_tx_timeout_reached;

extern volatile uint16_t     g_tail_tone_elimination_count_down_10ms;

#ifdef ENABLE_FMRADIO
	extern volatile uint16_t g_fm_play_count_down_10ms;
#endif
#ifdef ENABLE_NOAA
	extern volatile uint16_t g_noaa_count_down_10ms;
#endif
extern bool                  g_enable_speaker;
extern uint8_t               g_key_input_count_down;
extern uint8_t               g_key_lock_count_down_500ms;
extern uint8_t               g_rtte_count_down;
extern bool                  g_password_locked;
extern uint8_t               g_update_status;
extern uint8_t               g_found_ctcss;
extern uint8_t               g_found_cdcss;
extern bool                  g_end_of_rx_detected_maybe;

extern int16_t               g_vfo_rssi[2];
extern uint8_t               g_vfo_rssi_bar_level[2];

extern uint8_t               g_reduced_service;
extern uint8_t               g_battery_voltage_index;
extern css_scan_mode_t       g_css_scan_mode;
extern bool                  g_update_rssi;
extern alarm_state_t         g_alarm_state;
extern uint16_t              g_menu_count_down;
extern bool                  g_flag_reconfigure_vfos;
extern uint8_t               g_vfo_configure_mode;
extern bool                  g_flag_reset_vfos;
extern bool                  g_request_save_vfo;
extern uint8_t               g_request_save_channel;
extern bool                  g_request_save_settings;
#ifdef ENABLE_FMRADIO
	extern bool              g_request_save_fm;
#endif
extern bool                  g_flag_prepare_tx;

extern bool                  g_flag_accept_setting;   // accept menu setting
extern bool                  g_flag_refresh_menu;  // refresh menu display

extern bool                  g_flag_save_vfo;
extern bool                  g_flag_save_settings;
extern bool                  g_flag_save_channel;
#ifdef ENABLE_FMRADIO
	extern bool              g_flag_save_fm;
#endif
extern bool                  g_cdcss_lost;
extern uint8_t               g_cdcss_code_type;
extern bool                  g_ctcss_lost;
extern bool                  g_cxcss_tail_found;
#ifdef ENABLE_VOX
	extern bool              g_vox_lost;
	extern bool              g_vox_noise_detected;
	extern uint16_t          g_vox_resume_count_down;
	extern uint16_t          g_vox_pause_count_down;
#endif
extern bool                  g_squelch_lost;
extern uint8_t               g_flash_light_state;
extern volatile uint16_t     g_flash_light_blink_counter;
extern bool                  g_flag_end_tx;
extern uint16_t              g_low_batteryCountdown;
extern reception_mode_t      g_rx_reception_mode;

extern uint8_t               g_scan_next_channel;
extern uint8_t               g_scan_restore_channel;
extern scan_next_chan_t      g_scan_current_scan_list;
extern uint32_t              g_scan_restore_frequency;
extern bool                  g_scan_keep_frequency;
extern bool                  g_scan_pause_mode;
extern volatile bool         g_scan_schedule_scan_listen;
extern volatile uint16_t     g_scan_pause_delay_in_10ms;
extern scan_state_dir_t      g_scan_state_dir;


extern bool                  g_rx_vfo_is_active;
extern uint8_t               g_alarm_tone_counter;
extern uint16_t              g_alarm_running_counter;
extern uint8_t               g_menu_list_count;
extern uint8_t               g_backup_cross_vfo_rx_tx;
#ifdef ENABLE_NOAA
	extern bool              g_is_noaa_mode;
	extern uint8_t           g_noaa_channel;
#endif
extern volatile bool         g_next_time_slice;
extern bool                  g_update_display;
extern bool                  g_unhide_hidden;
#ifdef ENABLE_FMRADIO
	extern uint8_t           g_fm_channel_position;
#endif
extern volatile uint8_t      g_found_cdcss_count_down_10ms;
extern volatile uint8_t      g_found_ctcss_count_down_10ms;
#ifdef ENABLE_VOX
	extern volatile uint16_t g_vox_stop_count_down_10ms;
#endif
extern volatile bool         g_next_time_slice_40ms;
#ifdef ENABLE_NOAA
	extern volatile uint16_t g_noaa_count_down_10ms;
	extern volatile bool     g_schedule_noaa;
#endif
extern volatile bool         g_flag_tail_tone_elimination_complete;
#ifdef ENABLE_FMRADIO
	extern volatile bool     g_schedule_fm;
#endif
extern int16_t               g_current_rssi[2];   // now one per VFO
extern volatile uint8_t      g_boot_counter_10ms;

unsigned int get_TX_VFO(void);
unsigned int get_RX_VFO(void);
void         NUMBER_Get(char *pDigits, uint32_t *pInteger);
void         NUMBER_ToDigits(uint32_t Value, char *pDigits);
int32_t      NUMBER_AddWithWraparound(int32_t Base, int32_t Add, int32_t LowerLimit, int32_t UpperLimit);

#endif

