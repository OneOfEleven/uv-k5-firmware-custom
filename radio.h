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

#include "frequencies.h"
#include "settings.h"

extern vfo_info_t      g_vfo_info[2];

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
#ifdef ENABLE_VOX
	void RADIO_enable_vox(unsigned int level);
#endif
void     RADIO_ConfigureSquelchAndOutputPower(vfo_info_t *p_vfo);
void     RADIO_ApplyOffset(vfo_info_t *p_vfo, const bool set_pees);
void     RADIO_select_vfos(void);
void     RADIO_setup_registers(bool switch_to_function_foreground);
#ifdef ENABLE_NOAA
	void RADIO_ConfigureNOAA(void);
#endif
void     RADIO_enableTX(const bool fsk_tx);

void     RADIO_set_vfo_state(vfo_state_t State);
void     RADIO_PrepareTX(void);
void     RADIO_enable_CxCSS_tail(void);
void     RADIO_PrepareCssTX(void);
void     RADIO_tx_eot(void);

#endif
