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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#include "frequencies.h"
#include "radio.h"

enum pwr_on_display_mode_e {
	PWR_ON_DISPLAY_MODE_FULL_SCREEN = 0,
	PWR_ON_DISPLAY_MODE_MESSAGE,
	PWR_ON_DISPLAY_MODE_VOLTAGE,
	PWR_ON_DISPLAY_MODE_NONE
};
typedef enum pwr_on_display_mode_e pwr_on_display_mode_t;

enum {
	F_LOCK_OFF = 0,
	F_LOCK_FCC,
	F_LOCK_CE,
	F_LOCK_GB,
	F_LOCK_430,
	F_LOCK_438
};

enum {
	SCAN_RESUME_TO = 0,
	SCAN_RESUME_CO,
	SCAN_RESUME_SE
};

enum {
	CROSS_BAND_OFF = 0,
	CROSS_BAND_CHAN_A,
	CROSS_BAND_CHAN_B
};

enum {
	DUAL_WATCH_OFF = 0,
	DUAL_WATCH_CHAN_A,
	DUAL_WATCH_CHAN_B
};

enum {
	TX_OFFSET_FREQ_DIR_OFF = 0,
	TX_OFFSET_FREQ_DIR_ADD,
	TX_OFFSET_FREQ_DIR_SUB
};

enum {
	OUTPUT_POWER_LOW = 0,
	OUTPUT_POWER_MID,
	OUTPUT_POWER_HIGH
};

enum {
	ACTION_OPT_NONE = 0,
	ACTION_OPT_FLASHLIGHT,
	ACTION_OPT_POWER,
	ACTION_OPT_MONITOR,
	ACTION_OPT_SCAN,
	ACTION_OPT_VOX,
	ACTION_OPT_ALARM,
	ACTION_OPT_FM,
	ACTION_OPT_1750,
	ACTION_OPT_LEN
};

#ifdef ENABLE_VOICE
	enum voice_prompt_e
	{
		VOICE_PROMPT_OFF = 0,
		VOICE_PROMPT_CHINESE,
		VOICE_PROMPT_ENGLISH
	};
	typedef enum voice_prompt_e voice_prompt_t;
#endif

enum alarm_mode_e {
	ALARM_MODE_SITE = 0,
	ALARM_MODE_TONE
};
typedef enum alarm_mode_e alarm_mode_t;

enum roger_mode_e {
	ROGER_MODE_OFF = 0,
	ROGER_MODE_ROGER,
	ROGER_MODE_MDC
};
typedef enum roger_mode_e roger_mode_t;

enum mdf_display_mode_e {
	MDF_FREQUENCY = 0,
	MDF_CHANNEL,
	MDF_NAME,
	MDF_NAME_FREQ
};
typedef enum mdf_display_mode_e mdf_display_mode_t;

typedef struct {
	uint8_t               screen_channel[2];
	uint8_t               freq_channel[2];
	uint8_t               user_channel[2];
	#ifdef ENABLE_NOAA
		uint8_t           noaa_channel[2];
	#endif
	uint8_t               rx_vfo;
	uint8_t               tx_vfo;

	uint8_t               field7_0xa;
	uint8_t               field8_0xb;

	#ifdef ENABLE_FMRADIO
		uint16_t          fm_selected_frequency;
		uint8_t           fm_selected_channel;
		bool              fm_is_channel_mode;
		uint16_t          fm_frequency_playing;
		uint16_t          fm_lower_limit;
		uint16_t          fm_upper_limit;
	#endif

	uint8_t               squelch_level;
	uint8_t               tx_timeout_timer;
	bool                  key_lock;
	bool                  vox_switch;
	uint8_t               vox_level;
	#ifdef ENABLE_VOICE
		voice_prompt_t    voice_prompt;
	#endif
	bool                  beep_control;
	uint8_t               channel_display_mode;
	bool                  tail_note_elimination;
	bool                  vfo_open;
	uint8_t               dual_watch;
	uint8_t               cross_vfo_rx_tx;
	uint8_t               battery_save;
	uint8_t               backlight;
	uint8_t               scan_resume_mode;
	uint8_t               scan_list_default;
	bool                  scan_list_enabled[2];
	uint8_t               scan_list_priority_ch1[2];
	uint8_t               scan_list_priority_ch2[2];

	uint8_t               field29_0x26;
	uint8_t               field30_0x27;
	
	uint8_t               field37_0x32;
	uint8_t               field38_0x33;

	bool                  auto_keypad_lock;
	
	#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
		alarm_mode_t      alarm_mode;
	#endif
	pwr_on_display_mode_t pwr_on_display_mode;
	roger_mode_t          roger_mode;
	uint8_t               repeater_tail_tone_elimination;
	uint8_t               key1_short_press_action;
	uint8_t               key1_long_press_action;
	uint8_t               key2_short_press_action;
	uint8_t               key2_long_press_action;
	uint8_t               mic_sensitivity;
	uint8_t               mic_sensitivity_tuning;
	uint8_t               chan_1_call;
	char                  ani_dtmf_id[8];
	char                  kill_code[8];
	char                  revive_code[8];
	char                  dtmf_up_code[16];
	char                  dtmf_down_code[16];

	uint8_t               field57_0x6c;
	uint8_t               field58_0x6d;

	uint8_t               field60_0x7e;
	uint8_t               field61_0x7f;

	char                  dtmf_separate_code;
	char                  dtmf_group_call_code;
	uint8_t               dtmf_decode_response;
	uint8_t               dtmf_auto_reset_time;
	uint16_t              dtmf_preload_time;
	uint16_t              dtmf_first_code_persist_time;
	uint16_t              dtmf_hash_code_persist_time;
	uint16_t              dtmf_code_persist_time;
	uint16_t              dtmf_code_interval_time;
	bool                  dtmf_side_tone;
	bool                  permit_remote_kill;
	int16_t               BK4819_xtal_freq_low;
	#ifdef ENABLE_NOAA
		bool              noaa_auto_scan;
	#endif
	uint8_t               volume_gain;
	uint8_t               dac_gain;
	vfo_info_t            vfo_info[2];
	uint32_t              power_on_password;
	uint16_t              vox1_threshold;
	uint16_t              vox0_threshold;

	uint8_t               field77_0x95;
	uint8_t               field78_0x96;
	uint8_t               field79_0x97;

	uint8_t _pad[1];
} eeprom_config_t;

extern eeprom_config_t g_eeprom;

#ifdef ENABLE_FMRADIO
	void SETTINGS_SaveFM(void);
#endif
void SETTINGS_SaveVfoIndices(void);
void SETTINGS_SaveSettings(void);
void SETTINGS_SaveChannel(uint8_t Channel, uint8_t VFO, const vfo_info_t *pVFO, uint8_t Mode);
void SETTINGS_UpdateChannel(uint8_t Channel, const vfo_info_t *pVFO, bool keep);

#endif
