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

#ifndef APP_SCANNER_H
#define APP_SCANNER_H

#include "dcs.h"
#include "driver/keyboard.h"

enum scan_css_state_e
{
	SCAN_CSS_STATE_OFF = 0,
//	SCAN_CSS_STATE_FREQ_SCANNING,
	SCAN_CSS_STATE_SCANNING,
	SCAN_CSS_STATE_FOUND,
	SCAN_CSS_STATE_FAILED,
	SCAN_CSS_STATE_FREQ_FAILED
};
typedef enum scan_css_state_e scan_css_state_t;

enum scan_state_dir_e {
	SCAN_REV = -1,
	SCAN_OFF =  0,
	SCAN_FWD = +1
};
typedef enum scan_state_dir_e scan_state_dir_t;

enum scan_edit_state_e {
	SCAN_EDIT_STATE_NONE = 0,
	SCAN_EDIT_STATE_SAVE,
	SCAN_EDIT_STATE_DONE
};
typedef enum scan_edit_state_e scan_edit_state_t;

extern dcs_code_type_t   g_scan_css_result_type;
extern uint8_t           g_scan_css_result_code;
extern bool              g_flag_start_scan;
extern bool              g_flag_stop_scan;
extern bool              g_scan_single_frequency;
extern scan_edit_state_t g_scanner_edit_state;
extern uint8_t           g_scan_channel;
extern uint32_t          g_scan_frequency;
extern bool              g_scan_pause_mode;
extern scan_css_state_t  g_scan_css_state;
extern volatile bool     g_schedule_scan_listen;
extern volatile uint16_t g_scan_pause_delay_in_10ms;
extern uint16_t          g_scan_freq_css_timer_10ms;
extern uint8_t           g_scan_hit_count;
extern bool              g_scan_use_css_result;
extern scan_state_dir_t  g_scan_state_dir;
extern bool              g_scan_keep_frequency;

void SCANNER_ProcessKeys(key_code_t Key, bool key_pressed, bool key_held);
void SCANNER_Start(void);
void SCANNER_Stop(void);

#endif

