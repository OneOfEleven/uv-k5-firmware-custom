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

#ifndef UI_MENU_H
#define UI_MENU_H

#include <stdbool.h>
#include <stdint.h>

#include "audio.h"     // voice_id_t

typedef struct {
	const char name[7];    // menu display area only has room for 6 characters
	voice_id_t voice_id;
	uint8_t    menu_id;
} t_menu_item;

// currently this list MUST be in exactly the same order
// as the other menu list "g_menu_list[]" in "ui/menu.c", otherwise
// you'll have big problems
//
// I'm going to fix that so that you can reorder the menu items
// anyway you like simply by editing this list only (the other list
// you just leave as is, or any which way, it won't matter)
//
enum
{
	// ************************************

	MENU_SQL = 0,
	MENU_CHAN_SQL,
	MENU_STEP,
	MENU_BANDWIDTH,
	MENU_TX_POWER,
	MENU_RX_CDCSS,
	MENU_RX_CTCSS,
	MENU_TX_CDCSS,
	MENU_TX_CTCSS,
	MENU_SHIFT_DIR,
	MENU_OFFSET,
	MENU_TX_TO,
	MENU_CROSS_VFO,
	MENU_DUAL_WATCH,
	MENU_SCAN_CAR_RESUME,
	MENU_SCAN_HOLD,
	MENU_SCRAMBLER,
	MENU_BUSY_CHAN_LOCK,
	MENU_MEM_SAVE,
	MENU_MEM_NAME,
	MENU_MEM_DEL,
	MENU_MEM_DISP,
	MENU_BAT_SAVE,
#ifdef ENABLE_VOX
	MENU_VOX,
#endif
	MENU_AUTO_BACKLITE,
	MENU_AUTO_BACKLITE_ON_TX_RX,
#ifdef ENABLE_CONTRAST
	MENU_CONTRAST,
#endif
	MENU_S_ADD1,
	MENU_S_ADD2,
#ifdef ENABLE_NOAA
	MENU_NOAA_SCAN,
#endif
	MENU_1_CALL,
	MENU_STE,
	MENU_RP_STE,
	MENU_MIC_GAIN,
	MENU_COMPAND,
#ifdef ENABLE_TX_AUDIO_BAR
	MENU_TX_BAR,
#endif
#ifdef ENABLE_RX_SIGNAL_BAR
	MENU_RX_BAR,
#endif
	MENU_S_LIST,
	MENU_SLIST1,
	MENU_SLIST2,
	MENU_ANI_ID,
	MENU_UP_CODE,
	MENU_DN_CODE,
	MENU_DTMF_ST,
	MENU_DTMF_RSP,
	MENU_DTMF_HOLD,
	MENU_DTMF_PRE,
	MENU_DTMF_DCD,
	MENU_DTMF_LIST,
	MENU_DTMF_LIVE_DEC,
#ifdef ENABLE_MDC1200
	MENU_MDC1200_MODE,
	MENU_MDC1200_ID,
#endif
	MENU_PTT_ID,
	MENU_ROGER_MODE,
#ifdef ENABLE_ALARM
	MENU_ALARM_MODE,
#endif
	MENU_PON_MSG,
	MENU_VOLTAGE,
	MENU_BAT_TXT,
	MENU_MOD_MODE,
#ifdef ENABLE_AM_FIX
//	MENU_AM_FIX,
#endif
#ifdef ENABLE_AM_FIX_TEST1
	MENU_AM_FIX_TEST1,
#endif
	MENU_BEEP,
#ifdef ENABLE_VOICE
	MENU_VOICE,
#endif
#ifdef ENABLE_KEYLOCK
	MENU_AUTO_KEY_LOCK,
#endif
#ifdef ENABLE_SIDE_BUTT_MENU
	MENU_SIDE1_SHORT,
	MENU_SIDE1_LONG,
	MENU_SIDE2_SHORT,
	MENU_SIDE2_LONG,
#endif
	MENU_VERSION,
	MENU_RESET,

	// ************************************
	// ************************************
	// ************************************
	// items after here are normally hidden

	MENU_BAT_CAL,      // battery voltage calibration

#ifdef ENABLE_F_CAL_MENU
	MENU_F_CALI,       // 26MHz reference xtal calibration
#endif

	MENU_SCRAMBLER_EN, // scrambler enable/disable
	MENU_FREQ_LOCK,    // lock to a selected region
	MENU_350_EN,       // 350~400MHz enable/disable
	MENU_174_TX,       // 174~350MHz TX enable/disable
	MENU_350_TX,       // 350~400MHz TX enable/disable
	MENU_470_TX,       // 470MHz and up TX enable/disable
	MENU_TX_EN,        // disable the TX entirely

	// ************************************
	// ************************************
	// ************************************
};

extern const t_menu_item  g_menu_list[];
extern uint8_t            g_menu_list_sorted[];

extern const char         g_sub_menu_mod_mode[3][4];
extern const char         g_sub_menu_tx_power[3][7];
extern const char         g_sub_menu_shift_dir[3][4];
extern const char         g_sub_menu_bandwidth[2][7];
extern const char         g_sub_menu_off_on[2][4];
extern const char         g_sub_menu_bat_save[5][9];
extern const char         g_sub_menu_tx_timeout[11][7];
extern const char         g_sub_menu_dual_watch[3][10];
extern const char         g_sub_menu_cross_vfo[3][10];
#ifdef ENABLE_VOICE
	extern const char     g_sub_menu_voice[3][4];
#endif
extern const char         g_sub_menu_scan_car_resume[3][8];
extern const char         g_sub_menu_mem_disp[4][15];
#ifdef ENABLE_ALARM
	extern const char     g_sub_menu_alarm_mode[2][5];
#endif
extern const char         g_sub_menu_dtmf_rsp[4][9];
extern const char         g_sub_menu_ptt_id[5][15];
#ifdef ENABLE_MDC1200
	extern const char     g_sub_menu_mdc1200_mode[4][8];
#endif
extern const char         g_sub_menu_pwr_on_msg[4][14];
extern const char         g_sub_menu_roger_mode[2][16];
extern const char         g_sub_menu_reset[2][4];
extern const char         g_sub_menu_backlight[8][7];
extern const char         g_sub_menu_rx_tx[4][6];
#ifdef ENABLE_AM_FIX_TEST1
	extern const char     g_sub_menu_AM_FIX_test1[4][8];
#endif
extern const char         g_sub_menu_bat_text[3][8];
extern const char         g_sub_menu_dis_en[2][9];
extern const char         g_sub_menu_scrambler[11][7];
#ifdef ENABLE_SIDE_BUTT_MENU
	extern const char     g_sub_menu_side_butt[9][16];
#endif

extern bool               g_in_sub_menu;

extern uint8_t            g_menu_cursor;
extern int8_t             g_menu_scroll_direction;
extern int32_t            g_sub_menu_selection;

extern char               g_edit_original[17];
extern char               g_edit[17];
extern int                g_edit_index;

void UI_SortMenu(const bool hide_hidden);
void UI_DisplayMenu(void);

#endif
