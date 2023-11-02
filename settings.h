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
	FREQ_LOCK_446,
#ifdef ENABLE_TX_UNLOCK
	FREQ_LOCK_TX_UNLOCK,
#endif
	FREQ_LOCK_LAST
};

enum {
	SCAN_RESUME_TIME = 0,
	SCAN_RESUME_CARRIER,
	SCAN_RESUME_STOP
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

#define MAX_TX_OFFSET   100000000
enum {
	TX_OFFSET_FREQ_DIR_OFF = 0,
	TX_OFFSET_FREQ_DIR_ADD,
	TX_OFFSET_FREQ_DIR_SUB,
	TX_OFFSET_FREQ_DIR_LAST
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

// if channel is  used, all unused bits are '0's
// if channel not used, all bytes are 0xff
//
// 16 bytes
typedef struct {
	// [0]
	uint32_t frequency;                      //
	// [4]
	uint32_t tx_offset;                      //
	// [8]
	uint8_t  rx_ctcss_cdcss_code;            //
	// [9]
	uint8_t  tx_ctcss_cdcss_code;            //
	// [10]
	uint8_t  rx_ctcss_cdcss_type:2;          //
	uint8_t  unused1:2;                      //
	uint8_t  tx_ctcss_cdcss_type:2;          //
	uint8_t  unused2:2;                      //
	// [11]
	uint8_t  tx_offset_dir:2;                //
	#ifdef ENABLE_MDC1200
		uint8_t mdc1200_mode:2;              //
	#else
		uint8_t unused3:2;                   //
	#endif
	#if 0
		uint8_t  am_mode:1;                  //
		uint8_t  unused4:3;                  //
	#else
		uint8_t  am_mode:2;                  //
		uint8_t  unused4:2;                  //
	#endif
	// [12]
	uint8_t  frequency_reverse:1;            // reverse repeater
	uint8_t  channel_bandwidth:1;            // wide/narrow
	uint8_t  tx_power:2;                     // 0, 1 or 2 .. L, M or H
	uint8_t  busy_channel_lock:1;            //
	#if 0
		// QS
		uint8_t unused5:3;                   //
	#else
		// 1of11
		uint8_t unused5:1;                   //
		uint8_t compand:2;                   // 0 = off, 1 = TX, 2 = RX, 3 = TX/RX
	#endif
	// [13]
	uint8_t  dtmf_decoding_enable:1;         //
	uint8_t  dtmf_ptt_id_tx_mode:3;          //
	uint8_t  unused6:4;                      //
	// [14]
	uint8_t  step_setting;                   //
	// [15]
	uint8_t  scrambler:4;                    //
	#if 0
		// QS
		uint8_t unused7:4;                   //
	#else
		// 1of11
		uint8_t squelch_level:4;             // 0 ~ 9 per channel squelch, 0 = use main squelch level
	#endif
} __attribute__((packed)) t_channel;         //

typedef union {
	struct {
		uint8_t    band:4;                   // why do QS have these bits ?  band can/is computed from the frequency
		uint8_t    unused:2;                 //
		uint8_t    scanlist2:1;              // set if in scan list 2
		uint8_t    scanlist1:1;              // set if in scan list 1
	};
	uint8_t attributes;
} __attribute__((packed)) t_channel_attrib;

// user configuration
typedef struct {

	union {

		struct {
			// 0x0000
			t_channel user_channel[200];     // unused channels are set to all '0xff'
			// 0x0C80
			t_channel vfo_channel[14];
		};

		// 0x0000
		t_channel channel[214];

	} __attribute__((packed));

	// 0x0D60
	t_channel_attrib channel_attributes[USER_CHANNEL_LAST - USER_CHANNEL_FIRST + 1];

	struct {
		// 0x0E28
		uint8_t        unused1[8];                      // 0xff's

		// 0x0E30
		uint8_t        unused2[16];                     // 0xff's

		// 0x0E40
		uint16_t       fm_channel[20];                  //
		uint8_t        unused3[8];                      // 0xff's

		// 0x0E70
		uint8_t        call1;                           //
		uint8_t        squelch_level;                   //
		uint8_t        tx_timeout;                      //
		uint8_t        noaa_auto_scan;                  //
		uint8_t        key_lock;                        //
		uint8_t        vox_switch;                      //
		uint8_t        vox_level;                       //
		uint8_t        mic_sensitivity;                 //
		#ifdef ENABLE_CONTRAST
			uint8_t    lcd_contrast;                    // 1of11
		#else
			uint8_t    unused4;                         // 0xff's
		#endif
		uint8_t        channel_display_mode;            //
		uint8_t        cross_vfo;                       //
		uint8_t        battery_save_ratio;              //
		uint8_t        dual_watch;                      //
		uint8_t        backlight_time;                  //
		uint8_t        tail_tone_elimination;           //
		uint8_t        vfo_open;                        //

		// 0x0E80
		struct {
			struct {
				uint8_t    screen;                      //
				uint8_t    user;                        //
				uint8_t    frequency;                   //
			} __attribute__((packed)) vfo[2];
			uint8_t        noaa_channel[2];             //
		} __attribute__((packed)) indices;

		// 0x0E88
		struct {
			uint16_t selected_frequency;                //
			uint8_t  selected_channel;                  //
			uint8_t  channel_mode;                      //
			uint8_t  unused[4];                         // 0xff's
		} __attribute__((packed)) fm_radio;

		// 0x0E90
		uint8_t        beep_control;                    //
		uint8_t        key1_short;                      //
		uint8_t        key1_long;                       //
		uint8_t        key2_short;                      //
		uint8_t        key2_long;                       //
		uint8_t        carrier_search_mode;             // sc_rev
		uint8_t        auto_key_lock;                   //
		uint8_t        power_on_display_mode;           //
		uint32_t       power_on_password;               //
		#ifdef ENABLE_MDC1200
			uint16_t   mdc1200_id;                      // 1of11
			uint8_t    unused6[2];                      // 0xff's
		#else
			uint8_t    unused6[4];                      // 0xff's
		#endif

		// 0x0EA0
		uint8_t        voice_prompt;                    //
		uint8_t        unused7[7];                      // 0xff's
		uint8_t        alarm_mode;                      //
		uint8_t        roger_mode;                      //
		uint8_t        repeater_tail_tone_elimination;  // rp_ste
		uint8_t        tx_vfo_num;                      //
		#ifdef ENABLE_AIRCOPY
			uint32_t   air_copy_freq;                   // 1of11
		#else
			uint8_t    unused8[4];                      // 0xff's
		#endif

		// 0x0EB0
		char           welcome_line[2][16];             //

		struct {
			// 0x0ED0
			uint8_t    side_tone;                       //
			uint8_t    separate_code;                   //
			uint8_t    group_call_code;                 //
			uint8_t    decode_response;                 //
			uint8_t    auto_reset_time;                 //
			uint8_t    preload_time;                    //
			uint8_t    first_code_persist_time;         //
			uint8_t    hash_code_persist_time;          //
			uint8_t    code_persist_time;               //
			uint8_t    code_interval_time;              //
			uint8_t    permit_remote_kill;              //
			uint8_t    unused[5];                       // 0xff's

			// 0x0EE0
			char       ani_id[8];                       //
			char       kill_code[8];                    //
			char       revive_code[8];                  //
			char       key_up_code[16];                 //
			char       key_down_code[16];               //
		} __attribute__((packed)) dtmf;

		// 0x0F18
		uint8_t        scan_list_default;               //
		struct {
			uint8_t    enabled;                         //
			uint8_t    channel[2];                      //
		} __attribute__((packed)) priority_scan_list[2];
		uint8_t        unused10;                        // 0xff's

		// 0x0F20
		uint8_t        unused11[16];                    // 0xff's

		// 0x0F30
		uint32_t       aes_key[4];                      // disabled = all 0xff

		// 0x0F40
		uint8_t        freq_lock;                       //
		uint8_t        enable_tx_350:1;                 // 1 = 350MHz ~ 400MHz TX is enabled
		uint8_t        unused11a:7;                     //
		uint8_t        radio_disabled:1;                // 1 = radio is disabled
		uint8_t        unused11b:7;                     //
		uint8_t        enable_tx_200:1;                 // 1 = 174MHz ~ 350MHz TX enabled
		uint8_t        unused11c:7;                     //
		uint8_t        enable_tx_470:1;                 // 1 = >= 470MHz TX enabled
		uint8_t        unused11d:7;                     //
		uint8_t        enable_350:1;                    // 1 = 350HMz ~ 400MHz enabled
		uint8_t        unused11e:7;                     //
		uint8_t        enable_scrambler:1;              //
		uint8_t        enable_rssi_bar:1;               // 1of11
		uint8_t        unused11f:6;                     //
		#if 0
			// QS
			uint8_t    unused12[9];                     // 0xff's
		#else
			// 1of11
			uint8_t    tx_enable:1;           // 0 = completely disable TX, 1 = allow TX
			uint8_t    dtmf_live_decoder:1;   // 1 = enable on-screen live DTMF decoder
			uint8_t    battery_text:2;        // 0 = no battery text, 1 = voltage, 2 = percent .. on the status bar
			uint8_t    mic_bar:1;             // 1 = on-screen TX audio level
			uint8_t    am_fix:1;              // 1 = enable RX AM fix
			uint8_t    backlight_on_tx_rx:2;  // 0 = no backlight when TX/RX, 1 = when TX, 2 = when RX, 3 = both RX/TX

			uint8_t    scan_hold_time;        // ticks we stay paused for on an RX'ed signal when scanning

			uint8_t    unused12[7];           // 0xff's
		#endif
	}  __attribute__((packed)) setting;

	// 0x0F50
	struct {
		char       name[10];
		uint8_t    unused[6];             // 0xff's
	} __attribute__((packed)) channel_name[USER_CHANNEL_LAST - USER_CHANNEL_FIRST + 1];

	// 0x1BD0
	uint8_t        unused13[16 * 3];      // 0xff's .. free to use

	// 0x1C00
	struct {
		char       name[8];
		uint8_t    number[8];
	} __attribute__((packed)) dtmf_contact[16];

} __attribute__((packed)) t_config;

// 512 bytes
typedef struct {

	// 0x1E00
	struct {
		uint8_t open_rssi_thresh[10];               //
		uint8_t unused1[6];                         // 0xff's
		uint8_t close_rssi_thresh[10];              //
		uint8_t unused2[6];                         // 0xff's
		uint8_t open_noise_thresh[10];              //
		uint8_t unused3[6];                         // 0xff's
		uint8_t close_noise_thresh[10];             //
		uint8_t unused4[6];                         // 0xff's
		uint8_t open_glitch_thresh[10];             //
		uint8_t unused5[6];                         // 0xff's
		uint8_t close_glitch_thresh[10];            //
		uint8_t unused6[6];                         // 0xff's
	} __attribute__((packed)) squelch_band[2];      // 0 = bands 4567, 1 = bands 123

	// 0x1EC0
	uint16_t rssi_band_4567[4];                     // RSSI bargraph thresholds .. (dBm + 160) * 2
	uint16_t rssi_band_123[4];                      // RSSI bargraph thresholds .. (dBm + 160) * 2

	// 0x1ED0
	struct
	{
		union {
			struct {
				uint8_t  low[3];                    //
				uint8_t  mid[3];                    //
				uint8_t high[3];                    //
			};
			uint8_t level[3][3];                    //
		};
		uint8_t unused[7];                          // 0xff's
	} __attribute__((packed)) tx_band_power[7];

	// 0x1F40
	uint16_t battery[6];                            //
	uint8_t  unused1[4];                            // 0xff's

	// 0x1F50
	struct
	{
		uint16_t threshold[10];                     //
		uint8_t  unused[4];                         // 0xff's
	} __attribute__((packed)) vox[2];

	// 0x1F80
	uint8_t  mic_gain_dB2[5];                       //
	uint8_t  unused2[3];                            //

	// 0x1F88
	int16_t  bk4819_xtal_freq_low;                  //
	uint16_t unknown2;                              //
	uint16_t unknown3;                              //
	uint8_t  volume_gain;                           //
	uint8_t  dac_gain;                              //

	// 0x1F90
	uint8_t  unused3[16 * 7];                       // 0xff's

	// 0x2000

} __attribute__((packed)) t_calibration;

// entire eeprom
typedef struct {

	// 0x0000
	t_config       config;            // radios user config

	// 0x1D00
	uint8_t        unused[16 * 16];   // does this belong to the config, or the calibration, or neither ?

	// 0x1E00
	t_calibration  calib;             // calibration settings .. we DO NOT pass this through aircopy, it's radio specific

} __attribute__((packed)) t_eeprom;   // 8192 (0x2000) bytes of eeprom

// ************************************************

extern t_eeprom         g_eeprom;
extern t_channel_attrib g_user_channel_attributes[FREQ_CHANNEL_LAST + 1];

void SETTINGS_read_eeprom(void);
void SETTINGS_write_eeprom_config(void);

#ifdef ENABLE_FMRADIO
	void SETTINGS_save_fm(void);
#endif
void SETTINGS_save_vfo_indices(void);
void SETTINGS_save(void);
void SETTINGS_save_channel(const unsigned int channel, const unsigned int vfo, const vfo_info_t *p_vfo, const unsigned int mode);
void SETTINGS_save_chan_attribs_name(const unsigned int channel, const vfo_info_t *p_vfo);

unsigned int SETTINGS_find_channel(const uint32_t frequency);
uint32_t     SETTINGS_fetch_channel_frequency(const int channel);
unsigned int SETTINGS_fetch_channel_step_setting(const int channel);
void         SETTINGS_fetch_channel_name(char *s, const int channel);
unsigned int SETTINGS_fetch_frequency_step_setting(const int channel, const int vfo);
void         SETTINGS_factory_reset(bool bIsAll);

#endif
