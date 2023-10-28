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

#ifndef RADIO_H
#define RADIO_H

#include <stdbool.h>
#include <stdint.h>

#include "misc.h"
#include "dcs.h"
#include "frequencies.h"

enum {
	USER_CH_BAND_MASK = 0x0F << 0,
	USER_CH_SPARE     =   3u << 4,
	USER_CH_SCANLIST2 =   1u << 6,
	USER_CH_SCANLIST1 =   1u << 7
};
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

enum ptt_id_e {
	PTT_ID_OFF = 0,    // OFF
	PTT_ID_TX_UP,      // BEGIN OF TX
	PTT_ID_TX_DOWN,    // END OF TX
	PTT_ID_BOTH,       // BOTH
	PTT_ID_APOLLO      // Apolo quindar tones
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

typedef struct
{
	uint32_t        frequency;
	dcs_code_type_t code_type;
	uint8_t         code;
	uint8_t         padding[2];
} freq_config_t;

typedef struct vfo_info_t
{
	freq_config_t  freq_config_rx;
	freq_config_t  freq_config_tx;
	freq_config_t *p_rx;
	freq_config_t *p_tx;

	uint32_t       tx_offset_freq;
	uint16_t       step_freq;

	uint8_t        channel_save;

	uint8_t        tx_offset_freq_dir;

	uint8_t        squelch_level;   // per channel squelch level
	
	uint8_t        squelch_open_rssi_thresh;
	uint8_t        squelch_open_noise_thresh;
	uint8_t        squelch_close_glitch_thresh;
	uint8_t        squelch_close_rssi_thresh;
	uint8_t        squelch_close_noise_thresh;
	uint8_t        squelch_open_glitch_thresh;

	step_setting_t step_setting;
	uint8_t        output_power;
	uint8_t        txp_calculated_setting;
	bool           frequency_reverse;

	uint8_t        scrambling_type;
	uint8_t        channel_bandwidth;

	uint8_t        scanlist_1_participation;
	uint8_t        scanlist_2_participation;

	uint8_t        band;

	uint8_t        dtmf_decoding_enable;
	ptt_id_t       dtmf_ptt_id_tx_mode;

	#ifdef ENABLE_MDC1200
		mdc1200_mode_t mdc1200_mode;
	#endif
	
	uint8_t        busy_channel_lock;

	uint8_t        am_mode;

	uint8_t        compand;

	uint8_t        freq_in_channel; // channel number if the VFO's frequency is found stored in a channel

	char           name[16];
} vfo_info_t;

extern vfo_info_t     *g_tx_vfo;
extern vfo_info_t     *g_rx_vfo;
extern vfo_info_t     *g_current_vfo;

extern dcs_code_type_t g_selected_code_type;
extern dcs_code_type_t g_current_code_type;
extern uint8_t         g_selected_code;

extern vfo_state_t     g_vfo_state[2];

bool     RADIO_CheckValidChannel(uint16_t ChNum, bool bCheckScanList, uint8_t RadioNum);
uint8_t  RADIO_FindNextChannel(uint8_t ChNum, scan_state_dir_t Direction, bool bCheckScanList, uint8_t RadioNum);
void     RADIO_InitInfo(vfo_info_t *p_vfo, const uint8_t ChannelSave, const uint32_t Frequency);
void     RADIO_configure_channel(const unsigned int VFO, const unsigned int configure);
void     RADIO_ConfigureSquelchAndOutputPower(vfo_info_t *p_vfo);
void     RADIO_ApplyOffset(vfo_info_t *p_vfo, const bool set_pees);
void     RADIO_select_vfos(void);
void     RADIO_setup_registers(bool switch_to_function_foreground);
#ifdef ENABLE_NOAA
	void RADIO_ConfigureNOAA(void);
#endif
void     RADIO_enableTX(const bool fsk_tx);

void     RADIO_Setg_vfo_state(vfo_state_t State);
void     RADIO_PrepareTX(void);
void     RADIO_EnableCxCSS(void);
void     RADIO_PrepareCssTX(void);
void     RADIO_tx_eot(void);

#endif
