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
	MENU_STEP,
	MENU_W_N,
	MENU_TXP,
	MENU_R_DCS,
	MENU_R_CTCS,
	MENU_T_DCS,
	MENU_T_CTCS,
	MENU_SFT_D,
	MENU_OFFSET,
	MENU_TOT,
	MENU_XB,
	MENU_TDR,
	MENU_SCR,
	MENU_BCL,
	MENU_MEM_CH,
	MENU_MEM_NAME,
	MENU_DEL_CH,
	MENU_MDF,
	MENU_SAVE,
#ifdef ENABLE_VOX
	MENU_VOX,
#endif
	MENU_ABR,
	MENU_ABR_ON_TX_RX,
	MENU_CONTRAST,
	MENU_BEEP,
#ifdef ENABLE_VOICE
	MENU_VOICE,
#endif
	MENU_SC_REV,
	MENU_AUTOLK,
	MENU_S_ADD1,
	MENU_S_ADD2,
	MENU_STE,
	MENU_RP_STE,
	MENU_MIC,
#ifdef ENABLE_AUDIO_BAR
	MENU_MIC_BAR,
#endif
	MENU_COMPAND,
	MENU_1_CALL,
	MENU_S_LIST,
	MENU_SLIST1,
	MENU_SLIST2,
#ifdef ENABLE_ALARM
	MENU_AL_MOD,
#endif
	MENU_ANI_ID,
	MENU_UPCODE,
	MENU_DWCODE,
	MENU_PTT_ID,
	MENU_D_ST,
	MENU_D_RSP,
	MENU_D_HOLD,
	MENU_D_PRE,
	MENU_D_DCD,
	MENU_D_LIST,
	MENU_D_LIVE_DEC,
	MENU_PONMSG,
	MENU_ROGER,
	MENU_VOL,
	MENU_BAT_TXT,
	MENU_AM,
#ifdef ENABLE_AM_FIX
	MENU_AM_FIX,
#endif
#ifdef ENABLE_AM_FIX_TEST1
	MENU_AM_FIX_TEST1,
#endif
#ifdef ENABLE_NOAA
	MENU_NOAA_S,
#endif
	MENU_SIDE1_SHORT,
	MENU_SIDE1_LONG,
	MENU_SIDE2_SHORT,
	MENU_SIDE2_LONG,
	MENU_VERSION,
	MENU_RESET,

	// ************************************
	// items after here are normally hidden

	MENU_FREQ_LOCK,
	MENU_174TX,
	MENU_350TX,
	MENU_470TX,
	MENU_350EN,
	MENU_SCREN,

	MENU_TX_EN,   // enable TX
#ifdef ENABLE_F_CAL_MENU
	MENU_F_CALI,  // reference xtal calibration
#endif
	MENU_BATCAL,  // battery voltage calibration
	
	// ************************************
};

extern const unsigned int g_hidden_menu_count;

extern const t_menu_item  g_menu_list[];
extern uint8_t            g_menu_list_sorted[];

extern const char         g_sub_menu_txp[3][5];
extern const char         g_sub_menu_shift_dir[3][4];
extern const char         g_sub_menu_w_n[2][7];
extern const char         g_sub_menu_off_on[2][4];
extern const char         g_sub_menu_SAVE[5][9];
extern const char         g_sub_menu_TOT[11][7];
extern const char         g_sub_menu_tdr[3][10];
extern const char         g_sub_menu_xb[3][10];
#ifdef ENABLE_VOICE       
	extern const char     g_sub_menu_voice[3][4];
#endif                    
extern const char         g_sub_menu_sc_rev[3][13];
extern const char         g_sub_menu_mdf[4][15];
#ifdef ENABLE_ALARM       
	extern const char     g_sub_menu_AL_MOD[2][5];
#endif                    
extern const char         g_sub_menu_D_RSP[4][9];
extern const char         g_sub_menu_PTT_ID[5][15];
extern const char         g_sub_menu_pwr_on_msg[4][14];
extern const char         g_sub_menu_roger_mode[3][16];
extern const char         g_sub_menu_RESET[2][4];
#ifdef ENABLE_TX_UNLOCK
	extern const char     g_sub_menu_freq_lock[7][9];
#else
	extern const char     g_sub_menu_freq_lock[6][8];
#endif
extern const char         g_sub_menu_backlight[8][7];
extern const char         g_sub_menu_rx_tx[4][6];
#ifdef ENABLE_AM_FIX_TEST1
	extern const char     g_sub_menu_AM_fix_test1[4][8];
#endif                    
extern const char         g_sub_menu_BAT_TXT[3][8];
extern const char         g_sub_menu_DIS_EN[2][9];
extern const char         g_sub_menu_SCRAMBLER[11][7];
extern const char         g_sub_menu_SIDE_BUTT[9][16];
						  
extern bool               g_is_in_sub_menu;
						  
extern uint8_t            g_menu_cursor;
extern int8_t             g_menu_scroll_direction;
extern int32_t            g_sub_menu_selection;
						  
extern char               g_edit_original[17];
extern char               g_edit[17];
extern int                g_edit_index;

void UI_SortMenu(const bool hide_hidden);
void UI_DisplayMenu(void);

#endif
