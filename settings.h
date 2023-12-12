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

#include "misc.h"
#include "dcs.h"
#include "frequencies.h"

enum {
	FM_DEV_LIMIT_LOWER   = 1000,
	FM_DEV_LIMIT_DEFAULT = 1350,
	FM_DEV_LIMIT_UPPER   = 1600
};

enum mod_mode_e {
	MOD_MODE_FM = 0,
	MOD_MODE_AM,
	MOD_MODE_DSB,
	MOD_MODE_LEN
};
typedef enum mod_mode_e mod_mode_t;

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
#ifdef ENABLE_TX_UNLOCK_MENU
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
	OUTPUT_POWER_HIGH,
	OUTPUT_POWER_USER
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
	ACTION_OPT_TX_TONE,
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
	ALARM_MODE_SITE = 0,   // TX
	ALARM_MODE_TONE        // don't TX
};
typedef enum alarm_mode_e alarm_mode_t;

enum roger_mode_e {
	ROGER_MODE_OFF = 0,
	ROGER_MODE_ROGER1,
	ROGER_MODE_ROGER2
};
typedef enum roger_mode_e roger_mode_t;

enum mdf_display_mode_e {
	MDF_FREQUENCY = 0,
	MDF_CHANNEL,
	MDF_NAME,
	MDF_NAME_FREQ
};
typedef enum mdf_display_mode_e mdf_display_mode_t;

/*
enum {
	RADIO_CHANNEL_UP   = 0x01u,
	RADIO_CHANNEL_DOWN = 0xFFu,
};
*/
enum {
	BANDWIDTH_WIDE = 0,
	BANDWIDTH_NARROW
};

enum compand_e {
	COMPAND_OFF = 0,
	COMPAND_TX,
	COMPAND_RX,
	COMPAND_TX_RX
};

enum ptt_id_e {
	PTT_ID_OFF = 0,    // OFF
	PTT_ID_BOT,        // BEGIN OF TX
	PTT_ID_EOT,        // END OF TX
	PTT_ID_BOTH,       // BOTH
	PTT_ID_APOLLO,     // Apolo quindar tones
	PTT_ID_TONE_BURST  // tone burst
};
typedef enum ptt_id_e ptt_id_t;

enum mdc1200_mode_e {
	MDC1200_MODE_OFF = 0, // OFF
	MDC1200_MODE_BOT,     // BEGIN OF TX
	MDC1200_MODE_EOT,     // END OF TX
	MDC1200_MODE_BOTH     // BOTH
};
typedef enum mdc1200_mode_e mdc1200_mode_t;

enum vfo_state_e
{
	VFO_STATE_NORMAL = 0,
	VFO_STATE_BUSY,
	VFO_STATE_BAT_LOW,
	VFO_STATE_TX_DISABLE,
	VFO_STATE_TIMEOUT,
	VFO_STATE_ALARM,
	VFO_STATE_VOLTAGE_HIGH
};
typedef enum vfo_state_e vfo_state_t;

// ************************************************
// this is the full eeprom structure, both config and calibration areas

// if channel is  used, all unused bits are '0's
// if channel not used, all bytes are 0xff
//
// 16 bytes
typedef struct {
	// [byte 0-3]
	uint32_t frequency;                      // rx frequency / 10
	// [byte 4-7]
	uint32_t tx_offset;                      // tx offset frequency / 10
	// [byte 8]
	uint8_t  rx_ctcss_cdcss_code;            // ctcss 0 ~ 49   cdcss 0 ~ 103
	// [9]
	uint8_t  tx_ctcss_cdcss_code;            // ctcss 0 ~ 49   cdcss 0 ~ 103
	// [10]
	struct {
		uint8_t rx_ctcss_cdcss_type:2;       // 0=none  1=ctcss  2=cdcss  3=cdcss reverse
		uint8_t unused1:2;                   //
		uint8_t tx_ctcss_cdcss_type:2;       // 0=none  1=ctcss  2=cdcss  3=cdcss reverse
		uint8_t unused2:2;                   //
	};
	// [11]
	struct {
		uint8_t tx_offset_dir:2;             // 0=none  1=neg  2=pos
		uint8_t mdc1200_mode:2;              // 1of11  0=none  1=bot  2=eot  3=both
		uint8_t mod_mode:2;                  // 0=FM  1=AM  2=DSB
		uint8_t unused4:2;                   //
	};
	// [12]
	struct {
		uint8_t frequency_reverse:1;         // 0=disabled  1=enabled
		uint8_t channel_bandwidth:1;         // 0=wide (25kHz)  1=narrow (12.5kHz)
		uint8_t tx_power:2;                  // 0=low  1=medium  2=high  3=user
		uint8_t busy_channel_lock:1;         // 0=disabled  1=enabled
		uint8_t unused5:1;                   //
		uint8_t compand:2;                   // 0=off  1=TX  2=RX  3=TX/RX
	};
	// [13]
	struct {
		uint8_t dtmf_decoding_enable:1;      // 0=disabled  1=enabled
		uint8_t dtmf_ptt_id_tx_mode:3;       // 0=none  1=bot  2=eot  3=both  4=apollo
		uint8_t squelch_level:4;             // 1of11   0 = use main squelch level, 1 ~ 9 per channel squelch
	};
	// [14]
	struct {
		uint8_t step_setting:4;              // step size index 0 ~ 15
		uint8_t tx_power_user:4;             // 1of11 .. user power setting 0 ~ 15
	};
	// [15]
	struct {
		uint8_t scrambler:5;                 // voice inversion scrambler frequency index 0 ~ 31
		uint8_t unused7:3;                   //
	};
} __attribute__((packed)) t_channel;

typedef union {
	struct {
		uint8_t    band:4;                   // 0~6   otherwise channel unused
		uint8_t    unused:2;                 //
		uint8_t    scanlist2:1;              // set if in scan list 2
		uint8_t    scanlist1:1;              // set if in scan list 1
	};
	uint8_t attributes;                      //
} __attribute__((packed)) t_channel_attrib;

typedef struct {
	char         name[10];
	uint8_t      unused[6];
} __attribute__((packed)) t_channel_name;

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
	t_channel_attrib   channel_attributes[200 + 7 + 1]; // last byte = 0x00

	struct {
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
		uint8_t        vox_enabled;                     //
		uint8_t        vox_level;                       //
		uint8_t        mic_sensitivity;                 //

		// 0x0E78
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
			uint8_t    unused6[1];                      // 0xff's
		#else
			uint8_t    unused6[3];                      // 0xff's
		#endif
		#ifdef ENABLE_PANADAPTER
			struct {
				uint8_t panadapter:1;                   // 1 = enable panadapter
				uint8_t unused6a:7;                     // 0xff
			};
		#else
			uint8_t     unused6a;                       // 0xff
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
		struct {
			uint8_t    enable_tx_350:1;                 // 1 = 350MHz ~ 400MHz TX is enabled
			uint8_t    unused11a:7;                     //
		};
		struct {
			uint8_t    radio_disabled:1;                // 1 = radio is disabled
			uint8_t    unused11b:7;                     //
		};
		struct {
			uint8_t    enable_tx_200:1;                 // 1 = 174MHz ~ 350MHz TX enabled
			uint8_t    unused11c:7;                     //
		};
		struct {
			uint8_t    enable_tx_470:1;                 // 1 = >= 470MHz TX enabled
			uint8_t    unused11d:7;                     //
		};
		struct {
			uint8_t    enable_350:1;                    // 1 = 350HMz ~ 400MHz enabled
			uint8_t    unused11e:7;                     //
		};
		struct {
			uint8_t    enable_scrambler:1;              //
			uint8_t    enable_rssi_bar:1;               // 1of11
			uint8_t    unused11f:6;                     //
		};

		#if 0
			// QS
			uint8_t    unused12[9];                     // 0xff's
		#else
			// 1of11
			struct {
				uint8_t tx_enable:1;                 // 0 = completely disable TX, 1 = allow TX
				uint8_t dtmf_live_decoder:1;         // 1 = enable on-screen live DTMF decoder
				uint8_t battery_text:2;              // 0 = no battery text, 1 = voltage, 2 = percent .. on the status bar
				uint8_t mic_bar:1;                   // 1 = on-screen TX audio level
				uint8_t am_fix:1;                    // 1 = enable RX AM fix
				uint8_t backlight_on_tx_rx:2;        // 0 = no backlight when TX/RX, 1 = when TX, 2 = when RX, 3 = both RX/TX
			};

			uint8_t     scan_hold_time;                  // ticks we stay paused for on an RX'ed signal when scanning

			struct {
				uint8_t scan_ranges_enable:1;        // enable/disable auto scan ranges
				uint8_t unused11g:7;                 // 0xff's
			};

			uint8_t     unused12[6];                 // 0xff's
		#endif

	}  __attribute__((packed)) setting;

	// 0x0F50
	t_channel_name channel_name[USER_CHANNEL_LAST - USER_CHANNEL_FIRST + 1];

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

	// 0x1EC0 .. mine = 006E 0078 0082 008C 0086 00AA 00CE 00F2
	struct {	// RSSI bargraph thresholds .. (dBm + 160) * 2
		uint16_t band_4567[4];                      //
		uint16_t band_123[4];                       //
	} __attribute__((packed)) rssi_cal;

	// 0x1ED0
	struct {
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
	#if 1
		// QS
		uint8_t unused1[4];                         // 0xff's
	#else
		// 1of11
		struct {
			uint16_t wide;                          // 0 ~ 4095
			uint16_t narrow;                        // 0 ~ 4095
		} __attribute__((packed)) tx_deviation;
	#endif

	// 0x1F50
	uint16_t vox_threshold_enable[10];              //
	uint8_t  unused[4];                             // 0xff's

	// 0x1F68
	uint16_t vox_threshold_disable[10];             //
//	#ifdef ENABLE_FM_DEV_CAL_MENU
		// 1of11
		uint16_t deviation;                         //
		uint8_t  unused1a[2];                       // 0xff's
//	#else
//		// QS
//		uint8_t  unused1a[4];                       // 0xff's
//	#endif

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

typedef struct
{
	uint32_t        frequency;
	dcs_code_type_t code_type;
	uint8_t         code;
} freq_config_t;

typedef struct vfo_info_t
{
	t_channel        channel;
	t_channel_attrib channel_attributes;
	t_channel_name   channel_name;

	uint8_t          channel_save;

	freq_config_t    freq_config_rx;
	freq_config_t    freq_config_tx;
	freq_config_t   *p_rx;
	freq_config_t   *p_tx;

	uint16_t         step_freq;

	uint8_t          freq_in_channel; // first channel number we found this VFO's frequency in

	uint8_t          squelch_open_rssi_thresh;
	uint8_t          squelch_close_rssi_thresh;

	uint8_t          squelch_open_noise_thresh;
	uint8_t          squelch_close_noise_thresh;

	uint8_t          squelch_open_glitch_thresh;
	uint8_t          squelch_close_glitch_thresh;

	uint8_t          txp_reg_value;

} vfo_info_t;

// ************************************************

extern t_eeprom g_eeprom;

void SETTINGS_read_eeprom(void);
void SETTINGS_write_eeprom_config(void);
void SETTINGS_write_eeprom_calib(void);

#ifdef ENABLE_FMRADIO
	void SETTINGS_save_fm(void);
#endif
void SETTINGS_save_vfo_indices(void);
void SETTINGS_save(void);
void SETTINGS_save_channel(const unsigned int channel, const unsigned int vfo, vfo_info_t *p_vfo, const unsigned int mode);
void SETTINGS_save_chan_name(const unsigned int channel);
void SETTINGS_save_chan_attribs_name(const unsigned int channel, const vfo_info_t *p_vfo);

unsigned int SETTINGS_find_channel(const uint32_t frequency);
uint32_t     SETTINGS_fetch_channel_frequency(const int channel);
unsigned int SETTINGS_fetch_channel_step_setting(const int channel);
void         SETTINGS_fetch_channel_name(char *s, const int channel);
unsigned int SETTINGS_fetch_frequency_step_setting(const int channel, const int vfo);
void         SETTINGS_factory_reset(bool bIsAll);

#endif
