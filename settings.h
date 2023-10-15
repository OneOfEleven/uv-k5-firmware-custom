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
	FREQ_LOCK_NORMAL = 0,
	FREQ_LOCK_FCC,
	FREQ_LOCK_CE,
	FREQ_LOCK_GB,
	FREQ_LOCK_430,
	FREQ_LOCK_438,
#ifdef ENABLE_TX_UNLOCK
	FREQ_LOCK_TX_UNLOCK
#endif
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

// ************************************************
// this is the full eeprom structure, both config and calibration areas
//
// am going to use this to replace ALL the currently scattered values
//
// this will also make AIRCOPY safe as we'll first save the incoming transfer
// into this ram area.
// Then, IF the transfer completes withput error, we'll copy it in one go to eeprom


// if channel is  used, all unused bits are '0's
// if channel not used, all bytes are 0xff
//
// 16 bytes
typedef struct {
	// [0]
	uint32_t frequency;              //
	// [4]
	uint32_t offset;                 //
	// [8]
	uint8_t  rx_ctcss_cdcss_code;    //
	// [9]
	uint8_t  tx_ctcss_cdcss_code;    //
	// [10]
	uint8_t  rx_ctcss_cdcss_type:2;  //
	uint8_t  unused1:2;
	uint8_t  tx_ctcss_cdcss_type:2;  //
	uint8_t  unused2:2;
	// [11]
	uint8_t  tx_offset_dir:2;        //
	uint8_t  unused3:2;
	uint8_t  am_mode:1;              //
	uint8_t  unused4:3;
	// [12]
	uint8_t  frequency_reverse:1;    // reverse repeater
	uint8_t  channel_bandwidth:1;    // wide/narrow
	uint8_t  tx_power:2;             // 0, 1 or 2 .. L, M or H
	uint8_t  busy_channel_lockout:1; //
	uint8_t  unused5:3;
	// [13]
	uint8_t  dtmf_decoding_enable:1; //
	uint8_t  dtmf_ptt_id_tx_mode:3;  //
	uint8_t  unused6:4;
	// [14]
	uint8_t  step_setting:3;         //
	uint8_t  unused7:5;
	// [15]
	uint8_t  scrambler:4;            //
	uint8_t  unused8:4;
} __attribute__((packed)) t_channel;

// 512 bytes
typedef struct {

	// 0x1E00
	struct {
		uint8_t open_rssi_thresh[10];
		uint8_t unused1[6];

		uint8_t close_rssi_thresh[10];
		uint8_t unused2[6];

		uint8_t open_noise_thresh[10];
		uint8_t unused3[6];

		uint8_t close_noise_thresh[10];
		uint8_t unused4[6];

		uint8_t open_glitch_thresh[10];
		uint8_t unused5[6];

		uint8_t close_glitch_thresh[10];
		uint8_t unused6[6];
	} __attribute__((packed)) uhf_squelch[6];

	// 0x1E60
	struct {
		uint8_t open_rssi_thresh[10];
		uint8_t unused1[6];

		uint8_t close_rssi_thresh[10];
		uint8_t unused2[6];

		uint8_t open_noise_thresh[10];
		uint8_t unused3[6];

		uint8_t close_noise_thresh[10];
		uint8_t unused4[6];

		uint8_t open_glitch_thresh[10];
		uint8_t unused5[6];

		uint8_t close_glitch_thresh[10];
		uint8_t unused6[6];
	} __attribute__((packed)) vhf_squelch[6];

	// 0x1EC0
	uint16_t    rssi_uhf[4];
	uint16_t    rssi_vhf[4];

	// 0x1ED0
	struct
	{
		uint8_t low_tx_pwr[3];
		uint8_t mid_tx_pwr[3];
		uint8_t high_tx_pwr[3];
		uint8_t unused[7];
	} band_setting[7];

	// 0x1F40
	uint16_t battery[6];
	uint8_t  unused1[4];

	// 0x1F50
	struct
	{
		uint16_t threshold[10];
		uint8_t  unused[4];
	} __attribute__((packed)) vox[2];

	// 0x1F80
	uint8_t  mic_gain_dB2[5];
	uint8_t  unused4[3];
	int16_t  bk4819_xtal_freq_low;
	uint16_t unknown2;
	uint16_t unknown3;
	uint8_t  volume_gain;
	uint8_t  dac_gain;

	uint8_t  unused5[8 * 10];

} __attribute__((packed)) t_calibration;

// entire eeprom
typedef struct {

	// 0x0000
	t_channel channel[200];   // unused channels are set to all '0xff'

	// 0x0C80
	#if 0
		t_channel vfo[14];        // 2 VFO's (upper/lower) per band, 7 frequency bands
	#else
		union {                   // 2 VFO's (upper/lower) per band, 7 frequency bands
			t_channel vfo[14];
			struct {
				t_channel a;
				t_channel b;
			} __attribute__((packed)) vfo_band[7];
		} __attribute__((packed));
	#endif
	
	// 0x0D60
	struct {                  // all these channel settings could have been in the t_channel structure !
		uint8_t band:4;       // why do QS have these 4 bits ? .. band can/is computed from the frequency
		uint8_t compander:2;  // TODO: move this to the t_channel structure
		uint8_t scanlist2:1;  // set if is in scan list 2
		uint8_t scanlist1:1;  // set if is in scan list 1
	} __attribute__((packed)) channel_attr[200];

	uint8_t        unused1[8];
	uint8_t        unused2[16];

	// 0x0E40
	uint16_t       fm_channel[20];
	uint8_t        unused3[8];

	// 0x0E70
	uint8_t        call1;
	uint8_t        squelch;
	uint8_t        tx_timeout;
	uint8_t        noaa_auto_scan;
	uint8_t        key_lock;
	uint8_t        vox_switch;
	uint8_t        vox_level;
	uint8_t        mic_sensitivity;
	uint8_t        unused4;
	uint8_t        mdf;
	uint8_t        wx;
	uint8_t        battery_save;
	uint8_t        tdr;
	uint8_t        backlight;
	uint8_t        site;
	uint8_t        vfo_open;

	// 0x0E80
	uint8_t        screen_channel_a;
	uint8_t        channel_a;
	uint8_t        freq_channel_a;
	uint8_t        screen_channel_b;
	uint8_t        channel_b;
	uint8_t        freq_channel_b;
	uint8_t        noaa_channel_a;
	uint8_t        noaa_channel_b;
	uint8_t        fm_selected_frequency;
	uint8_t        fm_selected_channel;
	uint8_t        fm_is_channel_mode;
	uint8_t        unused5[5];

	// 0x0E90
	uint8_t        beep_control;
	uint8_t        key1_short;
	uint8_t        key1_long;
	uint8_t        key2_short;
	uint8_t        key2_long;
	uint8_t        sc_rev;
	uint8_t        auto_lock;
	uint8_t        display_mode;
	uint32_t       power_on_password;
	uint8_t        unused6[4];

	// 0x0EA0
	uint8_t        voice_prompt;
	uint8_t        unused7[7];
	uint8_t        alarm_mode;
	uint8_t        roger_mode;
	uint8_t        rp_ste;
	uint8_t        tx_channel;
	uint8_t        unused8[4];

	// 0x0EB0
	char           welcome_line1[16];
	char           welcome_line2[16];

	// 0x0ED0
	uint8_t        dtmf_side_tone;
	uint8_t        dtmf_separate_code;
	uint8_t        dtmf_group_call_code;
	uint8_t        dtmf_rsp;
	uint8_t        dtmf_auto_reset_time;
	uint8_t        dtmf_preload_time;
	uint8_t        dtmf_first_code_time;
	uint8_t        dtmf_hash_code_time;
	uint8_t        dtmf_code_time;
	uint8_t        dtmf_code_interval;
	uint8_t        dtmf_permit_kill;
	uint8_t        unused9[5];

	// 0x0EE0
	uint8_t        dtmf_ani_id[8];
	uint8_t        dtmf_kill_code[8];
	uint8_t        dtmf_revive_code[8];
	uint8_t        dtmf_key_up_code[16];
	uint8_t        dtmf_key_down_code[16];
	uint8_t        s_list_default;
	uint8_t        priority1_enable;
	uint8_t        priority1_channel1;
	uint8_t        priority1_channel2;
	uint8_t        priority2_enable;
	uint8_t        priority2_channel1;
	uint8_t        priority2_channel2;
	uint8_t        unused10;

	// 0x0F20
	uint8_t        unused11[8];

	// 0x0F30
	uint8_t        aes_key[16];       // disabled = all 0xff

	// 0x0F40
	uint8_t        freq_lock;             // 
	uint8_t        enable_tx_350;         // 350MHz ~ 400MHz
	uint8_t        killed;                //
	uint8_t        enable_tx_200;         //
	uint8_t        enable_tx_500;         //
	uint8_t        enable_350;            //
	uint8_t        enable_scrambler;      //
	#if 0
		// QS
		uint8_t    unused12[9];
	#else
		// 1of11 .. some of my additional settings
		uint8_t    tx_enable:1;           // 0 = completely disable TX, 1 = allow TX
		uint8_t    dtmf_live_decoder:1;   // 1 = enable on-screen live DTMF decoder
		uint8_t    battery_text:2;        // 0 = no battery text, 1 = voltage, 2 = percent .. on the status bar
		uint8_t    mic_bar:1;             // 1 = on-screen TX audio level
		uint8_t    am_fix:1;              // 1 = RX AM fix
		uint8_t    backlight_on_tx_rx:2;  // 0 = no backlight when TX/RX, 1 = when RX, 2 = when TX, 3 = both RX/TX

		uint8_t    unused12[8];
	#endif

	// 0x0F50
	char           channel_name[200][16]; // each channels name text

	// 0x1BD0
	uint8_t        unused13[16];
	uint8_t        unused14[16];
	uint8_t        unused15[16];

	// 0x1C00
	uint8_t        dtmf_contact[16][16];

	// 0x1D00
	uint8_t        unused16[256];         // lots of unused area we could make use of

	// 0x1E00
	t_calibration  calibration;           // the radios calibration/general settings

} __attribute__((packed)) t_eeprom;

// ************************************************
// this and all the other variables are going to be replaced with the above t_eeprom

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
	char                  dtmf_key_up_code[16];
	char                  dtmf_key_down_code[16];

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

//	uint8_t               field29_0x26;
//	uint8_t               field30_0x27;

//	uint8_t               field37_0x32;
//	uint8_t               field38_0x33;

//	uint8_t               field57_0x6c;
//	uint8_t               field58_0x6d;

//	uint8_t               field60_0x7e;
//	uint8_t               field61_0x7f;

//	uint8_t               field77_0x95;
//	uint8_t               field78_0x96;
//	uint8_t               field79_0x97;

} eeprom_config_t;

extern eeprom_config_t g_eeprom;

#ifdef ENABLE_FMRADIO
	void SETTINGS_SaveFM(void);
#endif
void SETTINGS_SaveVfoIndices(void);
//void SETTINGS_restore_calibration(void);
void SETTINGS_SaveSettings(void);
void SETTINGS_SaveChannel(uint8_t Channel, uint8_t VFO, const vfo_info_t *pVFO, uint8_t Mode);
void SETTINGS_UpdateChannel(uint8_t Channel, const vfo_info_t *pVFO, bool keep);

#endif
