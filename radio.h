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

#include "dcs.h"
#include "frequencies.h"

enum {
	USER_CH_BAND_MASK = 0x0F << 0,
	USER_CH_COMPAND   =   3u << 4,  // new
	USER_CH_SCANLIST2 =   1u << 6,
	USER_CH_SCANLIST1 =   1u << 7
};

enum {
	RADIO_CHANNEL_UP   = 0x01u,
	RADIO_CHANNEL_DOWN = 0xFFu,
};

enum {
	BANDWIDTH_WIDE = 0,
	BANDWIDTH_NARROW
};

enum PTT_ID_t {
	PTT_ID_OFF = 0,    // OFF
	PTT_ID_TX_UP,      // BEGIN OF TX
	PTT_ID_TX_DOWN,    // END OF TX
	PTT_ID_BOTH,       // BOTH
	PTT_ID_APOLLO      // Apolo quindar tones
};
typedef enum PTT_ID_t PTT_ID_t;

enum VfoState_t
{
	VFO_STATE_NORMAL = 0,
	VFO_STATE_BUSY,
	VFO_STATE_BAT_LOW,
	VFO_STATE_TX_DISABLE,
	VFO_STATE_TIMEOUT,
	VFO_STATE_ALARM,
	VFO_STATE_VOLTAGE_HIGH
};
typedef enum VfoState_t VfoState_t;

typedef struct
{
	uint32_t        frequency;
	dcs_code_type_t code_type;
	uint8_t         code;
	uint8_t         padding[2];
} FREQ_Config_t;

typedef struct VFO_Info_t
{
	FREQ_Config_t  freq_config_rx;
	FREQ_Config_t  freq_config_tx;
	FREQ_Config_t *pRX;
	FREQ_Config_t *pTX;

	uint32_t       tx_offset_freq;
	uint16_t       step_freq;

	uint8_t        channel_save;

	uint8_t        tx_offset_freq_dir;

	uint8_t        squelch_open_RSSI_thresh;
	uint8_t        squelch_open_noise_thresh;
	uint8_t        squelch_close_glitch_thresh;
	uint8_t        squelch_close_RSSI_thresh;
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

	uint8_t        DTMF_decoding_enable;
	PTT_ID_t       DTMF_ptt_id_tx_mode;

	uint8_t        busy_channel_lock;

	uint8_t        am_mode;

	uint8_t        compander;

	char           name[16];
} VFO_Info_t;

extern VFO_Info_t    *gTxVfo;
extern VFO_Info_t    *gRxVfo;
extern VFO_Info_t    *gCurrentVfo;

extern dcs_code_type_t gSelectedcode_type;
extern dcs_code_type_t gCurrentcode_type;
extern uint8_t        gSelectedCode;

extern step_setting_t gStepSetting;

extern VfoState_t     VfoState[2];

bool     RADIO_CheckValidChannel(uint16_t ChNum, bool bCheckScanList, uint8_t RadioNum);
uint8_t  RADIO_FindNextChannel(uint8_t ChNum, int8_t Direction, bool bCheckScanList, uint8_t RadioNum);
void     RADIO_InitInfo(VFO_Info_t *pInfo, const uint8_t ChannelSave, const uint32_t Frequency);
void     RADIO_ConfigureChannel(const unsigned int VFO, const unsigned int configure);
void     RADIO_ConfigureSquelchAndOutputPower(VFO_Info_t *pInfo);
void     RADIO_ApplyOffset(VFO_Info_t *pInfo);
void     RADIO_SelectVfos(void);
void     RADIO_SetupRegisters(bool bSwitchToFunction0);
#ifdef ENABLE_NOAA
	void RADIO_ConfigureNOAA(void);
#endif
void     RADIO_SetTxParameters(void);

void     RADIO_SetVfoState(VfoState_t State);
void     RADIO_PrepareTX(void);
void     RADIO_EnableCxCSS(void);
void     RADIO_PrepareCssTX(void);
void     RADIO_SendEndOfTransmission(void);

#endif
