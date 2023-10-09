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

enum SCAN_CssState_e
{
	SCAN_CSS_STATE_OFF = 0,
	SCAN_CSS_STATE_SCANNING,
	SCAN_CSS_STATE_FOUND,
	SCAN_CSS_STATE_FAILED
};
typedef enum SCAN_CssState_e SCAN_CssState_t;

enum {
	SCAN_REV = -1,
	SCAN_OFF =  0,
	SCAN_FWD = +1
};

enum SCAN_edit_state_e {
	SCAN_EDIT_STATE_NONE = 0,
	SCAN_EDIT_STATE_BUSY,
	SCAN_EDIT_STATE_DONE
};
typedef enum SCAN_edit_state_e SCAN_edit_state_t;

extern dcs_code_type_t    gScanCssResultType;
extern uint8_t           gScanCssResultCode;
extern bool              g_flag_start_scan;
extern bool              g_flag_stop_scan;
extern bool              g_scan_single_frequency;
extern SCAN_edit_state_t gScannerEditState;
extern uint8_t           gScanChannel;
extern uint32_t          gScanFrequency;
extern bool              gScanPauseMode;
extern SCAN_CssState_t   gScanCssState;
extern volatile bool     g_schedule_scan_listen;
extern volatile uint16_t g_scan_pause_delay_in_10ms;
extern uint8_t           gScanProgressIndicator;
extern uint8_t           gScanHitCount;
extern bool              gScanUseCssResult;
extern int8_t            g_scan_state_dir;
extern bool              bScanKeepFrequency;

void SCANNER_ProcessKeys(key_code_t Key, bool key_pressed, bool key_held);
void SCANNER_Start(void);
void SCANNER_Stop(void);

#endif

