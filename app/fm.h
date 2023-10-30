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

#ifndef APP_FM_H
#define APP_FM_H

#include "driver/keyboard.h"

#define FM_CHANNEL_UP	0x01
#define FM_CHANNEL_DOWN	0xFF

enum fm_scan_state_dir_e {
	FM_SCAN_STATE_DIR_DOWN = -1,
	FM_SCAN_STATE_DIR_OFF  = 0,
	FM_SCAN_STATE_DIR_UP,
};
typedef enum fm_scan_state_dir_e fm_scan_state_dir_t;

extern uint16_t            g_fm_channels[20];
extern bool                g_fm_radio_mode;
extern fm_scan_state_dir_t g_fm_scan_state_dir;
extern bool                g_fm_auto_scan;
extern uint8_t             g_fm_channel_position;
// Doubts about whether this should be signed or not
extern uint16_t            g_fm_frequency_deviation;
extern bool                g_fm_found_frequency;
extern bool                g_fm_auto_scan;
extern uint8_t             g_fm_resume_tick_500ms;
extern uint16_t            g_fm_restore_tick_10ms;
extern uint8_t             g_fm_radio_tick_500ms;
extern volatile uint16_t   g_fm_play_tick_10ms;
extern volatile bool       g_fm_schedule;

bool         FM_check_valid_channel(const unsigned int Channel);
unsigned int FM_find_next_channel(unsigned int Channel, const fm_scan_state_dir_t scan_state_dir);
int          FM_configure_channel_state(void);
void         FM_erase_channels(void);
void         FM_tune(uint16_t frequency, const fm_scan_state_dir_t scan_state_dir, const bool flag);
void         FM_stop_scan(void);
int          FM_check_frequency_lock(uint16_t Frequency, uint16_t LowerLimit);
void         FM_scan(void);
void         FM_turn_on(void);
void         FM_turn_off(void);
void         FM_process_key(key_code_t Key, bool bKeyPressed, bool bKeyHeld);

#endif

