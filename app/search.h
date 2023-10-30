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

#ifndef APP_SEARCH_H
#define APP_SEARCH_H

#include "dcs.h"
#include "driver/keyboard.h"
#include "frequencies.h"

enum search_css_state_e
{
	SEARCH_CSS_STATE_OFF = 0,
	SEARCH_CSS_STATE_SCANNING,
	SEARCH_CSS_STATE_FOUND,
	SEARCH_CSS_STATE_FAILED,
	SEARCH_CSS_STATE_FREQ_FAILED,
	SEARCH_CSS_STATE_REPEAT
};
typedef enum search_css_state_e search_css_state_t;

enum search_edit_state_e {
	SEARCH_EDIT_STATE_NONE = 0,
	SEARCH_EDIT_STATE_SAVE_CHAN,
	SEARCH_EDIT_STATE_SAVE_CONFIRM
};
typedef enum search_edit_state_e search_edit_state_t;

extern search_css_state_t  g_search_css_state;
extern dcs_code_type_t     g_search_css_result_type;
extern uint8_t             g_search_css_result_code;
extern bool                g_search_flag_start_scan;
extern bool                g_search_flag_stop_scan;
extern uint8_t             g_search_show_chan_prefix;
extern bool                g_search_single_frequency;
extern search_edit_state_t g_search_edit_state;
extern uint8_t             g_search_channel;
extern uint32_t            g_search_frequency;
extern step_setting_t      g_search_step_setting;
extern uint16_t            g_search_freq_css_tick_10ms;
extern uint16_t            g_search_tick_10ms;
extern uint8_t             g_search_hit_count;
extern bool                g_search_use_css_result;

void SEARCH_process_key(key_code_t Key, bool key_pressed, bool key_held);
void SEARCH_process(void);
void SEARCH_Start(void);

#endif

