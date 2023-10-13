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

#include <string.h>
#include <stdlib.h>  // abs()

#include "app/dtmf.h"
#include "app/menu.h"
#include "bitmaps.h"
#include "board.h"
#include "dcs.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"   // EEPROM_ReadBuffer()
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "frequencies.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/menu.h"
#include "ui/ui.h"
#include "version.h"

// ***************************************************************************************
// NOTE. the oder of menu entries you on-screen is now solely determined by the enum list order in ui/menu.h
//
// the order of entries in this list below is no longer important, no longer has to match the enum list

const t_menu_item g_menu_list[] =
{
//   text,     voice ID,                               menu ID

	{"SQL",    VOICE_ID_SQUELCH,                       MENU_SQL           },
	{"STEP",   VOICE_ID_FREQUENCY_STEP,                MENU_STEP          },
	{"W/N",    VOICE_ID_CHANNEL_BANDWIDTH,             MENU_W_N           },
	{"Tx PWR", VOICE_ID_POWER,                         MENU_TXP           }, // was "TXP"
	{"Rx DCS", VOICE_ID_DCS,                           MENU_R_DCS         }, // was "R_DCS"
	{"Rx CTS", VOICE_ID_CTCSS,                         MENU_R_CTCS        }, // was "R_CTCS"
	{"Tx DCS", VOICE_ID_DCS,                           MENU_T_DCS         }, // was "T_DCS"
	{"Tx CTS", VOICE_ID_CTCSS,                         MENU_T_CTCS        }, // was "T_CTCS"
	{"Tx DIR", VOICE_ID_TX_OFFSET_FREQ_DIR,            MENU_SFT_D         }, // was "SFT_D"
	{"Tx OFS", VOICE_ID_TX_OFFSET_FREQ,                MENU_OFFSET        }, // was "OFFSET"
	{"Tx TO",  VOICE_ID_TRANSMIT_OVER_TIME,            MENU_TOT           }, // was "TOT"
	{"Tx VFO", VOICE_ID_INVALID,                       MENU_XB            }, // was "WX"
	{"Dual W", VOICE_ID_DUAL_STANDBY,                  MENU_TDR           }, // was "TDR"
	{"SCRAM",  VOICE_ID_SCRAMBLER_ON,                  MENU_SCR           }, // was "SCR"
	{"BCL",    VOICE_ID_BUSY_LOCKOUT,                  MENU_BCL           },
	{"CH SAV", VOICE_ID_MEMORY_CHANNEL,                MENU_MEM_CH        }, // was "MEM-CH"
	{"CH NAM", VOICE_ID_INVALID,                       MENU_MEM_NAME      },
	{"CH DEL", VOICE_ID_DELETE_CHANNEL,                MENU_DEL_CH        }, // was "DEL-CH"
	{"CH DIS", VOICE_ID_INVALID,                       MENU_MDF           }, // was "MDF"
	{"BATSAV", VOICE_ID_SAVE_MODE,                     MENU_SAVE          }, // was "SAVE"
#ifdef ENABLE_VOX
	{"VOX",    VOICE_ID_VOX,                           MENU_VOX           },
#endif
	{"BLT",    VOICE_ID_INVALID,                       MENU_ABR           }, // was "ABR"
	{"BLTTRX", VOICE_ID_INVALID,                       MENU_ABR_ON_TX_RX  },
	{"CTRAST", VOICE_ID_INVALID,                       MENU_CONTRAST      },
	{"BEEP",   VOICE_ID_BEEP_PROMPT,                   MENU_BEEP          },
#ifdef ENABLE_VOICE
	{"VOICE",  VOICE_ID_VOICE_PROMPT,                  MENU_VOICE         },
#endif
	{"SC REV", VOICE_ID_INVALID,                       MENU_SC_REV        }, // was "SC_REV"
	{"KEYLOC", VOICE_ID_INVALID,                       MENU_AUTOLK        }, // was "AUTOLk"
	{"S ADD1", VOICE_ID_INVALID,                       MENU_S_ADD1        },
	{"S ADD2", VOICE_ID_INVALID,                       MENU_S_ADD2        },
	{"STE",    VOICE_ID_INVALID,                       MENU_STE           },
	{"RP STE", VOICE_ID_INVALID,                       MENU_RP_STE        },
	{"MIC",    VOICE_ID_INVALID,                       MENU_MIC           },
#ifdef ENABLE_AUDIO_BAR
	{"MICBAR", VOICE_ID_INVALID,                       MENU_MIC_BAR       },
#endif
	{"COMPND", VOICE_ID_INVALID,                       MENU_COMPAND       },
	{"1 CALL", VOICE_ID_INVALID,                       MENU_1_CALL        },
	{"SLIST",  VOICE_ID_INVALID,                       MENU_S_LIST        },
	{"SLIST1", VOICE_ID_INVALID,                       MENU_SLIST1        },
	{"SLIST2", VOICE_ID_INVALID,                       MENU_SLIST2        },
#ifdef ENABLE_ALARM
	{"SOS AL", VOICE_ID_INVALID,                       MENU_AL_MOD        }, // was "ALMODE"
#endif
	{"ANI ID", VOICE_ID_ANI_CODE,                      MENU_ANI_ID        },
	{"UPCODE", VOICE_ID_INVALID,                       MENU_UPCODE        },
	{"DWCODE", VOICE_ID_INVALID,                       MENU_DWCODE        },
	{"PTT ID", VOICE_ID_INVALID,                       MENU_PTT_ID        },
	{"D ST",   VOICE_ID_INVALID,                       MENU_D_ST          },
    {"D RSP",  VOICE_ID_INVALID,                       MENU_D_RSP         },
	{"D HOLD", VOICE_ID_INVALID,                       MENU_D_HOLD        },
	{"D PRE",  VOICE_ID_INVALID,                       MENU_D_PRE         },
	{"D DCD",  VOICE_ID_INVALID,                       MENU_D_DCD         },
	{"D LIST", VOICE_ID_INVALID,                       MENU_D_LIST        },
	{"D LIVE", VOICE_ID_INVALID,                       MENU_D_LIVE_DEC    }, // live DTMF decoder
	{"PONMSG", VOICE_ID_INVALID,                       MENU_PONMSG        },
	{"ROGER",  VOICE_ID_INVALID,                       MENU_ROGER         },
	{"BATVOL", VOICE_ID_INVALID,                       MENU_VOL           }, // was "VOL"
	{"BATTXT", VOICE_ID_INVALID,                       MENU_BAT_TXT       },
	{"MODE",   VOICE_ID_INVALID,                       MENU_AM            }, // was "AM"
#ifdef ENABLE_AM_FIX
	{"AM FIX", VOICE_ID_INVALID,                       MENU_AM_FIX        },
#endif
#ifdef ENABLE_AM_FIX_TEST1
	{"AM FT1", VOICE_ID_INVALID,                       MENU_AM_FIX_TEST1  },
#endif
#ifdef ENABLE_NOAA
	{"NOAA-S", VOICE_ID_INVALID,                       MENU_NOAA_S        },
#endif
	{"SIDE1S", VOICE_ID_INVALID,                       MENU_SIDE1_SHORT   },
	{"SIDE1L", VOICE_ID_INVALID,                       MENU_SIDE1_LONG    },
	{"SIDE2S", VOICE_ID_INVALID,                       MENU_SIDE2_SHORT   },
	{"SIDE2L", VOICE_ID_INVALID,                       MENU_SIDE2_LONG    },
	{"VER",    VOICE_ID_INVALID,                       MENU_VERSION       },
	{"RESET",  VOICE_ID_INITIALISATION,                MENU_RESET         }, // might be better to move this to the hidden menu items ?

	// hidden menu items from here on
	// enabled by pressing both the PTT and upper side button at power-on

	{"F LOCK", VOICE_ID_INVALID,                       MENU_F_LOCK        }, // country/area specific
	{"Tx 200", VOICE_ID_INVALID,                       MENU_200TX         }, // was "200TX"
	{"Tx 350", VOICE_ID_INVALID,                       MENU_350TX         }, // was "350TX"
	{"Tx 500", VOICE_ID_INVALID,                       MENU_500TX         }, // was "500TX"
	{"350 EN", VOICE_ID_INVALID,                       MENU_350EN         }, // was "350EN"
	{"SCR EN", VOICE_ID_INVALID,                       MENU_SCREN         }, // was "SCREN"
	{"Tx EN",  VOICE_ID_INVALID,                       MENU_TX_EN         }, // enable TX
#ifdef ENABLE_F_CAL_MENU
	{"F CALI", VOICE_ID_INVALID,                       MENU_F_CALI        }, // reference xtal calibration
#endif
	{"BATCAL", VOICE_ID_INVALID,                       MENU_BATCAL        }, // battery voltage calibration
};

// number of hidden menu items at the end of the list - KEEP THIS UP-TO-DATE
const unsigned int g_hidden_menu_count = 9;

// ***************************************************************************************

const char g_sub_menu_txp[3][5] =
{
	"LOW",
	"MID",
	"HIGH"
};

const char g_sub_menu_shift_dir[3][4] =
{
	"OFF",
	"+",
	"-"
};

const char g_sub_menu_w_n[2][7] =
{
	"WIDE",
	"NARROW"
};

const char g_sub_menu_off_on[2][4] =
{
	"OFF",
	"ON"
};

const char g_sub_menu_SAVE[5][9] =
{
	"OFF",
	"1:1 50%",
	"1:2 66%",
	"1:3 75%",
	"1:4 80%"
};

const char g_sub_menu_TOT[11][7] =
{
	"30 sec",
	"1 min",
	"2 min",
	"3 min",
	"4 min",
	"5 min",
	"6 min",
	"7 min",
	"8 min",
	"9 min",
	"15 min"
};

const char g_sub_menu_tdr[3][10] =
{
	"OFF",
	"LOWER\nVFO",
	"UPPER\nVFO",
};

const char g_sub_menu_xb[3][10] =
{
	"RX\nVFO",
	"UPPER\nVFO",
	"LOWER\nVFO"
};

#ifdef ENABLE_VOICE
	const char g_sub_menu_voice[3][4] =
	{
		"OFF",
		"CHI",
		"ENG"
	};
#endif

const char g_sub_menu_sc_rev[3][13] =
{
	"TIME",
	"CARRIER",
	"SEARCH"
};

const char g_sub_menu_mdf[4][15] =
{
	"FREQ",
	"CHANNEL\nNUMBER",
	"NAME",
	"NAME\n+\nFREQ"
};

#ifdef ENABLE_ALARM
	const char g_sub_menu_AL_MOD[2][5] =
	{
		"SITE",
		"TONE"
	};
#endif

const char g_sub_menu_D_RSP[4][9] =
{
	"NONE",
	"RING",
	"REPLY",
	"RNG RPLY"
};

const char g_sub_menu_PTT_ID[5][15] =
{
	"OFF",
	"BEGIN",
	"END",
	"BEGIN +\nEND",
	"APOLLO\nQUINDAR"
};

const char g_sub_menu_pwr_on_msg[4][14] =
{
	"ALL\nPIXELS\nON",
	"MESSAGE",
	"VOLTAGE",
	"NONE"
};

const char g_sub_menu_roger_mode[3][16] =
{
	"OFF",
	"TX END\nROGER",
	"TX END\nMDC\n1200"
};

const char g_sub_menu_RESET[2][4] =
{
	"VFO",
	"ALL"
};

const char g_sub_menu_f_lock[7][9] =
{
	"OFF",
	"FCC",
	"CE",
	"GB",
	"430 MHz",
	"438 MHz",
	"Extended"
};

const char g_sub_menu_backlight[8][7] =
{
	"OFF",
	"5 sec",
	"10 sec",
	"20 sec",
	"1 min",
	"2 min",
	"4 min",
	"ON"
};

const char g_sub_menu_rx_tx[4][6] =
{
	"OFF",
	"TX",
	"RX",
	"TX/RX"
};

#ifdef ENABLE_AM_FIX_TEST1
	const char g_sub_menu_AM_fix_test1[4][8] =
	{
		"LNA-S 0",
		"LNA-S 1",
		"LNA-S 2",
		"LNA-S 3"
	};
#endif

const char g_sub_menu_BAT_TXT[3][8] =
{
	"NONE",
	"VOLTAGE",
	"PERCENT"
};

const char g_sub_menu_DIS_EN[2][9] =
{
	"DISABLED",
	"ENABLED"
};

const char g_sub_menu_SCRAMBLER[11][7] =
{
	"OFF",
	"2600Hz",
	"2700Hz",
	"2800Hz",
	"2900Hz",
	"3000Hz",
	"3100Hz",
	"3200Hz",
	"3300Hz",
	"3400Hz",
	"3500Hz"
};

const char g_sub_menu_SIDE_BUTT[9][16] =
//const char g_sub_menu_SIDE_BUTT[10][16] =
{
	"NONE",
	"FLASH\nLIGHT",
	"TX\nPOWER",
	"MONITOR",
	"SCAN\non\\off",
	"VOX\non\\off",
	"ALARM\non\\off",
	"FM RADIO\non\\off",
	"TX\n1750Hz",
//	"2nd PTT",
};

// ***************************************************************************************

uint8_t g_menu_list_sorted[ARRAY_SIZE(g_menu_list)];

bool    g_is_in_sub_menu;
uint8_t g_menu_cursor;
int8_t  g_menu_scroll_direction;
int32_t g_sub_menu_selection;

// edit box
char    g_edit_original[17]; // a copy of the text before editing so that we can easily test for changes/differences
char    g_edit[17];
int     g_edit_index;

// ***************************************************************************************

void UI_SortMenu(const bool hide_hidden)
{
	// sort the menu order according to the MENU-ID value (enum list in id/menu.h)
	//
	// this means the menu order is entirely determined by the enum list (found in id/menu.h)
	// it now no longer depends on the order of entries in the above const list (they can be any order)

	unsigned int i;

	unsigned int hidden_menu_count = g_hidden_menu_count;

	#ifndef ENABLE_F_CAL_MENU
		hidden_menu_count--;
	#endif

	g_menu_list_count = ARRAY_SIZE(g_menu_list_sorted);

	for (i = 0; i < g_menu_list_count; i++)
		g_menu_list_sorted[i] = g_menu_list[i].menu_id;

	// don't sort the hidden entries at the end, keep them at the end of the list

	for (i = 0; i < (g_menu_list_count - hidden_menu_count - 1); i++)
	{
		unsigned int k;
		unsigned int menu_id1 = g_menu_list_sorted[i];
		for (k = i + 1; k < (g_menu_list_count - hidden_menu_count); k++)
		{
			unsigned int menu_id2 = g_menu_list_sorted[k];
			if (menu_id2 < menu_id1)
			{	// swap
				const unsigned int id = menu_id1;
				menu_id1 = menu_id2;
				menu_id2 = id;
				g_menu_list_sorted[i] = menu_id1;
				g_menu_list_sorted[k] = menu_id2;
			}
		}
	}

	if (hide_hidden)
		g_menu_list_count -= hidden_menu_count;  // hide the hidden menu items
}

void UI_DisplayMenu(void)
{
	const unsigned int menu_list_width = 6; // max no. of characters on the menu list (left side)
	const unsigned int menu_item_x1    = (8 * menu_list_width) + 2;
	const unsigned int menu_item_x2    = LCD_WIDTH - 1;
	unsigned int       i;
	char               String[64];  // bigger cuz we can now do multi-line in one string (use '\n' char)
	char               Contact[16];

	// clear the screen buffer
	memset(g_frame_buffer, 0, sizeof(g_frame_buffer));

	#if 0
		// original menu layout

		for (i = 0; i < 3; i++)
			if (g_menu_cursor > 0 || i > 0)
				if ((g_menu_list_count - 1) != g_menu_cursor || i != 2)
					UI_PrintString(g_menu_list[g_menu_list_sorted[g_menu_cursor + i - 1]].name, 0, 0, i * 2, 8);

		// invert the current menu list item pixels
		for (i = 0; i < (8 * menu_list_width); i++)
		{
			g_frame_buffer[2][i] ^= 0xFF;
			g_frame_buffer[3][i] ^= 0xFF;
		}

		// draw vertical separating dotted line
		for (i = 0; i < 7; i++)
			g_frame_buffer[i][(8 * menu_list_width) + 1] = 0xAA;

		// draw the little sub-menu triangle marker
		if (g_is_in_sub_menu)
			memmove(g_frame_buffer[0] + (8 * menu_list_width) + 1, BITMAP_CurrentIndicator, sizeof(BITMAP_CurrentIndicator));

		// draw the menu index number/count
		sprintf(String, "%2u.%u", 1 + g_menu_cursor, g_menu_list_count);
		UI_PrintStringSmall(String, 2, 0, 6);

	#else
	{	// new menu layout .. experimental & unfinished

		const int menu_index = g_menu_cursor;  // current selected menu item
		i = 1;

		if (!g_is_in_sub_menu)
		{
			while (i < 2)
			{	// leading menu items - small text
				const int k = menu_index + i - 2;
				if (k < 0)
					UI_PrintStringSmall(g_menu_list[g_menu_list_sorted[g_menu_list_count + k]].name, 0, 0, i);  // wrap-a-round
				else
				if (k >= 0 && k < (int)g_menu_list_count)
					UI_PrintStringSmall(g_menu_list[g_menu_list_sorted[k]].name, 0, 0, i);
				i++;
			}

			// current menu item - keep big n fat
			if (menu_index >= 0 && menu_index < (int)g_menu_list_count)
				UI_PrintString(g_menu_list[g_menu_list_sorted[menu_index]].name, 0, 0, 2, 8);
			i++;

			while (i < 4)
			{	// trailing menu item - small text
				const int k = menu_index + i - 2;
				if (k >= 0 && k < (int)g_menu_list_count)
					UI_PrintStringSmall(g_menu_list[g_menu_list_sorted[k]].name, 0, 0, 1 + i);
				else
				if (k >= (int)g_menu_list_count)
					UI_PrintStringSmall(g_menu_list[g_menu_list_sorted[g_menu_list_count - k]].name, 0, 0, 1 + i);  // wrap-a-round
				i++;
			}

			// draw the menu index number/count
			sprintf(String, "%2u.%u", 1 + g_menu_cursor, g_menu_list_count);
			UI_PrintStringSmall(String, 2, 0, 6);
		}
		else
		if (menu_index >= 0 && menu_index < (int)g_menu_list_count)
		{	// current menu item
			strcpy(String, g_menu_list[g_menu_list_sorted[menu_index]].name);
//			strcat(String, ":");
			UI_PrintString(String, 0, 0, 0, 8);
//			UI_PrintStringSmall(String, 0, 0, 0);
		}
	}
	#endif

	// **************

	memset(String, 0, sizeof(String));

	bool already_printed = false;

	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

	switch (g_menu_cursor)
	{
		case MENU_SQL:
			sprintf(String, "%d", g_sub_menu_selection);
			break;

		case MENU_MIC:
			{	// display the mic gain in actual dB rather than just an index number
				const uint8_t mic = g_mic_gain_dB_2[g_sub_menu_selection];
				sprintf(String, "+%u.%01udB", mic / 2, mic % 2);
			}
			break;

		#ifdef ENABLE_AUDIO_BAR
			case MENU_MIC_BAR:
				strcpy(String, g_sub_menu_off_on[g_sub_menu_selection]);
				break;
		#endif

		case MENU_STEP:
			sprintf(String, "%d.%02ukHz", STEP_FREQ_TABLE[g_sub_menu_selection] / 100, STEP_FREQ_TABLE[g_sub_menu_selection] % 100);
			break;

		case MENU_TXP:
			strcpy(String, g_sub_menu_txp[g_sub_menu_selection]);
			break;

		case MENU_R_DCS:
		case MENU_T_DCS:
			strcpy(String, "CDCSS\n");
			if (g_sub_menu_selection == 0)
				strcat(String, "OFF");
			else
			if (g_sub_menu_selection < 105)
				sprintf(String + strlen(String), "D%03oN", DCS_OPTIONS[g_sub_menu_selection -   1]);
			else
				sprintf(String + strlen(String), "D%03oI", DCS_OPTIONS[g_sub_menu_selection - 105]);
			break;

		case MENU_R_CTCS:
		case MENU_T_CTCS:
		{
			strcpy(String, "CTCSS\n");
			#if 1
				// set CTCSS as the user adjusts it
				unsigned int Code;
				freq_config_t *pConfig = (g_menu_cursor == MENU_R_CTCS) ? &g_tx_vfo->freq_config_rx : &g_tx_vfo->freq_config_tx;
				if (g_sub_menu_selection == 0)
				{
					strcat(String, "OFF");

					if (pConfig->code_type != CODE_TYPE_CONTINUOUS_TONE)
						break;
					
					Code = 0;
					pConfig->code_type = CODE_TYPE_NONE;
					pConfig->code = Code;

					BK4819_ExitSubAu();
				}
				else
				{
					sprintf(String + strlen(String), "%u.%uHz", CTCSS_OPTIONS[g_sub_menu_selection - 1] / 10, CTCSS_OPTIONS[g_sub_menu_selection - 1] % 10);

					pConfig->code_type = CODE_TYPE_CONTINUOUS_TONE;
					Code = g_sub_menu_selection - 1;
					pConfig->code = Code;

					BK4819_SetCTCSSFrequency(CTCSS_OPTIONS[Code]);
				}
			#else
				if (g_sub_menu_selection == 0)
					strcat(String, "OFF");
				else
					sprintf(String + strlen(String), "%u.%uHz", CTCSS_OPTIONS[g_sub_menu_selection - 1] / 10, CTCSS_OPTIONS[g_sub_menu_selection - 1] % 10);
			#endif

			break;
		}

		case MENU_SFT_D:
			strcpy(String, g_sub_menu_shift_dir[g_sub_menu_selection]);
			break;

		case MENU_OFFSET:
			if (!g_is_in_sub_menu || g_input_box_index == 0)
			{
				sprintf(String, "%d.%05u", g_sub_menu_selection / 100000, abs(g_sub_menu_selection) % 100000);
				UI_PrintString(String, menu_item_x1, menu_item_x2, 1, 8);
			}
			else
			{
				for (i = 0; i < 3; i++)
					String[i    ] = (g_input_box[i] == 10) ? '-' : g_input_box[i] + '0';
				String[3] = '.';
				for (i = 3; i < 6; i++)
					String[i + 1] = (g_input_box[i] == 10) ? '-' : g_input_box[i] + '0';
				String[ 7] = '-';
				String[ 8] = '-';
				String[ 9] = 0;
				String[10] = 0;
				String[11] = 0;
				UI_PrintString(String, menu_item_x1, menu_item_x2, 1, 8);
			}

			UI_PrintString("MHz",  menu_item_x1, menu_item_x2, 3, 8);

			already_printed = true;
			break;

		case MENU_W_N:
			strcpy(String, g_sub_menu_w_n[g_sub_menu_selection]);
			break;

		case MENU_SCR:
			strcpy(String, "INVERT\n");
			strcat(String, g_sub_menu_SCRAMBLER[g_sub_menu_selection]);
			
			#if 1
				if (g_sub_menu_selection > 0 && g_setting_scramble_enable)
					BK4819_EnableScramble(g_sub_menu_selection - 1);
				else
					BK4819_DisableScramble();
			#endif
			break;

		#ifdef ENABLE_VOX
			case MENU_VOX:
				if (g_sub_menu_selection == 0)
					strcpy(String, "OFF");
				else
					sprintf(String, "%d", g_sub_menu_selection);
				break;
		#endif

		case MENU_ABR:
			strcpy(String, "BACKLITE\n");
			strcat(String, g_sub_menu_backlight[g_sub_menu_selection]);
			break;

		case MENU_ABR_ON_TX_RX:
			strcpy(String, "BACKLITE\n");
			strcat(String, g_sub_menu_rx_tx[g_sub_menu_selection]);
			break;

		case MENU_AM:
			strcpy(String, (g_sub_menu_selection == 0) ? "FM" : "AM");
			break;

		#ifdef ENABLE_AM_FIX_TEST1
			case MENU_AM_FIX_TEST1:
				strcpy(String, g_sub_menu_AM_fix_test1[g_sub_menu_selection]);
//				g_setting_am_fix = g_sub_menu_selection;
				break;
		#endif

		case MENU_AUTOLK:
			if (g_sub_menu_selection == 0)
				strcpy(String, "OFF");
			else
				sprintf(String, "%u secs", key_lock_timeout_500ms / 2);
			break;

		case MENU_COMPAND:
			strcpy(String, g_sub_menu_rx_tx[g_sub_menu_selection]);
			break;

		case MENU_CONTRAST:
			strcpy(String, "DISPLAY\nCONTRAST\n");
			sprintf(String, "%d", g_sub_menu_selection);
			//g_setting_contrast = g_sub_menu_selection
			ST7565_SetContrast(g_sub_menu_selection);
			g_update_display = true;
			break;
			
		#ifdef ENABLE_AM_FIX
			case MENU_AM_FIX:
		#endif
		case MENU_S_ADD1:
		case MENU_S_ADD2:
			strcpy(String, g_sub_menu_off_on[g_sub_menu_selection]);
			break;

		case MENU_BCL:
			strcpy(String, "BSY CH TX\nLOCKOUT\n");
			strcat(String, g_sub_menu_off_on[g_sub_menu_selection]);
			break;

		case MENU_D_DCD:
		case MENU_D_LIVE_DEC:
			strcpy(String, "DTMF\nDECODE\n");
			strcat(String, g_sub_menu_off_on[g_sub_menu_selection]);
			break;

		case MENU_STE:
			strcpy(String, "SUB TAIL\nELIMIN\n");
			strcat(String, g_sub_menu_off_on[g_sub_menu_selection]);
			break;

		case MENU_BEEP:
			strcpy(String, "KEY BEEP\n");
			strcat(String + strlen(String), g_sub_menu_off_on[g_sub_menu_selection]);
			break;

		case MENU_D_ST:
			strcpy(String, "DTMF\nSIDETONE\n");
			strcat(String, g_sub_menu_off_on[g_sub_menu_selection]);
			break;

		#ifdef ENABLE_NOAA
			case MENU_NOAA_S:
				strcpy(String, "SCAN\n");
				strcat(String, g_sub_menu_DIS_EN[g_sub_menu_selection]);
				break;
		#endif

		case MENU_350TX:
		case MENU_200TX:
		case MENU_500TX:
		case MENU_350EN:
			strcpy(String, g_sub_menu_DIS_EN[g_sub_menu_selection]);
			break;

		case MENU_SCREN:
			strcpy(String, "SCRAMBLER\n");
			strcat(String, g_sub_menu_DIS_EN[g_sub_menu_selection]);
			break;

		case MENU_TX_EN:
			strcpy(String, "TX\n");
			strcat(String, g_sub_menu_DIS_EN[g_sub_menu_selection]);
			break;
			
		case MENU_MEM_CH:
		case MENU_1_CALL:
		case MENU_DEL_CH:
		{
			char s[11];
			const bool valid = RADIO_CheckValidChannel(g_sub_menu_selection, false, 0);

			UI_GenerateChannelStringEx(String, valid ? "CH-" : "", g_sub_menu_selection);

			// channel name
			BOARD_fetchChannelName(s, g_sub_menu_selection);
			strcat(String, "\n");
			strcat(String, (s[0] == 0) ? "--" : s);
			
			if (valid && !g_ask_for_confirmation)
			{	// show the frequency so that the user knows the channels frequency
				const uint32_t frequency = BOARD_fetchChannelFrequency(g_sub_menu_selection);
				sprintf(String + strlen(String), "\n%u.%05u", frequency / 100000, frequency % 100000);
			}

			break;
		}

		case MENU_MEM_NAME:
		{
			const bool valid = RADIO_CheckValidChannel(g_sub_menu_selection, false, 0);
			const unsigned int y = (!g_is_in_sub_menu || g_edit_index < 0) ? 1 : 0;

			UI_GenerateChannelStringEx(String, valid ? "CH-" : "", g_sub_menu_selection);
			UI_PrintString(String, menu_item_x1, menu_item_x2, y, 8);

			if (valid)
			{
				const uint32_t frequency = BOARD_fetchChannelFrequency(g_sub_menu_selection);

				if (!g_is_in_sub_menu || g_edit_index < 0)
				{	// show the channel name
					BOARD_fetchChannelName(String, g_sub_menu_selection);
					if (String[0] == 0)
						strcpy(String, "--");
					UI_PrintString(String, menu_item_x1, menu_item_x2, y + 2, 8);
				}
				else
				{	// show the channel name being edited
					UI_PrintString(g_edit, menu_item_x1, 0, y + 2, 8);
					if (g_edit_index < 10)
						UI_PrintString("^", menu_item_x1 + (8 * g_edit_index), 0, y + 4, 8);  // show the cursor
				}

				if (!g_ask_for_confirmation)
				{	// show the frequency so that the user knows the channels frequency
					sprintf(String, "%u.%05u", frequency / 100000, frequency % 100000);
					if (!g_is_in_sub_menu || g_edit_index < 0)
						UI_PrintString(String, menu_item_x1, menu_item_x2, y + 4, 8);
					else
						UI_PrintString(String, menu_item_x1, menu_item_x2, y + 5, 8);
				}
			}

			already_printed = true;
			break;
		}

		case MENU_SAVE:
			strcpy(String, g_sub_menu_SAVE[g_sub_menu_selection]);
			break;

		case MENU_TDR:
//			strcpy(String, g_sub_menu_tdr[g_sub_menu_selection]);
			strcpy(String, g_sub_menu_off_on[g_sub_menu_selection]);
			break;

		case MENU_XB:
			strcpy(String, g_sub_menu_xb[g_sub_menu_selection]);
			break;

		case MENU_TOT:
			strcpy(String, g_sub_menu_TOT[g_sub_menu_selection]);
			break;

		#ifdef ENABLE_VOICE
			case MENU_VOICE:
				strcpy(String, g_sub_menu_voice[g_sub_menu_selection]);
				break;
		#endif

		case MENU_SC_REV:
			strcpy(String, "SCAN\nRESUME\n");
			strcat(String, g_sub_menu_sc_rev[g_sub_menu_selection]);
			break;

		case MENU_MDF:
			strcpy(String, g_sub_menu_mdf[g_sub_menu_selection]);
			break;

		case MENU_RP_STE:
			if (g_sub_menu_selection == 0)
				strcpy(String, "OFF");
			else
				sprintf(String, "%d*100ms", g_sub_menu_selection);
			break;

		case MENU_S_LIST:
			if (g_sub_menu_selection < 2)
				sprintf(String, "LIST%u", 1 + g_sub_menu_selection);
			else
				strcpy(String, "ALL");
			break;

		#ifdef ENABLE_ALARM
			case MENU_AL_MOD:
				strcpy(String, "TX ALARM\n");
				sprintf(String + strlen(String), g_sub_menu_AL_MOD[g_sub_menu_selection]);
				break;
		#endif

		case MENU_ANI_ID:
			strcpy(String, "YOUR ID\n");
			strcat(String, g_eeprom.ani_dtmf_id);
			break;

		case MENU_UPCODE:
			strcpy(String, "PTT DTMF\nBEGIN\n");
			strcat(String, g_eeprom.dtmf_key_up_code);
			break;

		case MENU_DWCODE:
			strcpy(String, "PTT DTMF\nEND\n");
			strcat(String, g_eeprom.dtmf_key_down_code);
			break;

		case MENU_D_RSP:
			strcpy(String, "DTMF\nRESPONSE\n");
			strcat(String, g_sub_menu_D_RSP[g_sub_menu_selection]);
			break;

		case MENU_D_HOLD:
			// only allow 5, 10, 20, 30, 40, 50 or "STAY ON SCREEN" (60)
			switch (g_sub_menu_selection)
			{
				case  4: g_sub_menu_selection = 60; break;
				case  6: g_sub_menu_selection = 10; break;
				case  9: g_sub_menu_selection =  5; break;
				case 11: g_sub_menu_selection = 20; break;
				case 19: g_sub_menu_selection = 10; break;
				case 21: g_sub_menu_selection = 30; break;
				case 29: g_sub_menu_selection = 20; break;
				case 31: g_sub_menu_selection = 40; break;
				case 39: g_sub_menu_selection = 30; break;
				case 41: g_sub_menu_selection = 50; break;
				case 49: g_sub_menu_selection = 40; break;
				case 51: g_sub_menu_selection = 60; break;
				case 59: g_sub_menu_selection = 50; break;
				case 61: g_sub_menu_selection =  5; break;
			}

			strcpy(String, "DTMF MSG\n");
			if (g_sub_menu_selection < DTMF_HOLD_MAX)
				sprintf(String + strlen(String), "%d sec", g_sub_menu_selection);
			else
				strcat(String, "STAY ON\nSCREEN");  // 60

			break;

		case MENU_D_PRE:
			strcpy(String, "TX DTMF\nDELAY\n");
//			sprintf(String + strlen(String), "%d*10ms", g_sub_menu_selection);
			sprintf(String + strlen(String), "%dms", 10 * g_sub_menu_selection);
			break;

		case MENU_PTT_ID:
			strcpy(String, (g_sub_menu_selection > 0) ? "TX ID\n" : "");
			strcat(String, g_sub_menu_PTT_ID[g_sub_menu_selection]);
			break;

		case MENU_BAT_TXT:
			strcpy(String, g_sub_menu_BAT_TXT[g_sub_menu_selection]);
			break;

		case MENU_D_LIST:
			g_dtmf_is_contact_valid = DTMF_GetContact((int)g_sub_menu_selection - 1, Contact);
			strcpy(String, "DTMF\n");
			if (!g_dtmf_is_contact_valid)
			{
				strcat(String, "NULL");
			}
			else
			{
				memmove(String + strlen(String), Contact, 8);
				Contact[11] = 0;
				memmove(&g_dtmf_id, Contact + 8, 4);
				sprintf(String + strlen(String), "\nID:%s", Contact + 8);
			}
			break;

		case MENU_PONMSG:
			strcpy(String, g_sub_menu_pwr_on_msg[g_sub_menu_selection]);
			break;

		case MENU_ROGER:
			strcpy(String, g_sub_menu_roger_mode[g_sub_menu_selection]);
			break;

		case MENU_VOL:
			sprintf(String, "%u.%02uV\n%u%%\ncurr %u",
				g_battery_voltage_average / 100, g_battery_voltage_average % 100,
				BATTERY_VoltsToPercent(g_battery_voltage_average),
				g_usb_current);
			break;

		case MENU_SIDE1_SHORT:
		case MENU_SIDE1_LONG:
		case MENU_SIDE2_SHORT:
		case MENU_SIDE2_LONG:
			strcpy(String, g_sub_menu_SIDE_BUTT[g_sub_menu_selection]);
			break;

		case MENU_VERSION:
		{	// show the version string on multiple lines - if need be
			const unsigned int slen = strlen(Version_str);
			unsigned int m = 0;
			unsigned int k = 0;
			i = 0;
			while (i < (sizeof(String) - 1) && k < slen)
			{
				const char c = Version_str[k++];
				if (c == ' ' || c == '-' || c == '_')
				{
					if (m >= 3)
					{
						String[i++] = '\n';
						m = 0;
					}
					else
						String[i++] = c;
				}
				else
				{
					String[i++] = c;
					if (++m >= 9 && k < slen && i < (sizeof(String) - 1))
					{
						if (m > 0)
						{
							m = 0;
							String[i++] = '\n';
						}
					}
				}
			}
			// add the date and time
			strcat(String, "\n" __DATE__);
			strcat(String, "\n" __TIME__);
			break;
		}
			
		case MENU_RESET:
			strcpy(String, g_sub_menu_RESET[g_sub_menu_selection]);
			break;

		case MENU_F_LOCK:
			strcpy(String, g_sub_menu_f_lock[g_sub_menu_selection]);
			break;

		#ifdef ENABLE_F_CAL_MENU
			case MENU_F_CALI:
				{
					const uint32_t value   = 22656 + g_sub_menu_selection;
					const uint32_t xtal_Hz = (0x4f0000u + value) * 5;

					writeXtalFreqCal(g_sub_menu_selection, false);

					sprintf(String, "%d\n%u.%06u\nMHz",
						g_sub_menu_selection,
						xtal_Hz / 1000000, xtal_Hz % 1000000);
				}
				break;
		#endif

		case MENU_BATCAL:
		{
			const uint16_t vol = (uint32_t)g_battery_voltage_average * g_battery_calibration[3] / g_sub_menu_selection;
			sprintf(String, "%u.%02uV\n%u", vol / 100, vol % 100, g_sub_menu_selection);
			break;
		}
	}

	#pragma GCC diagnostic pop

	if (!already_printed)
	{	// we now do multi-line text in a single string

		unsigned int y;
		unsigned int lines = 1;
		unsigned int len   = strlen(String);
		bool         small = false;

		if (len > 0)
		{
			// count number of lines
			for (i = 0; i < len; i++)
			{
				if (String[i] == '\n' && i < (len - 1))
				{	// found new line char
					lines++;
					String[i] = 0;  // null terminate the line
				}
			}

			if (lines > 3)
			{	// use small text
				small = true;
				if (lines > 7)
					lines = 7;
			}

			// center vertically'ish
			if (small)
				y = 3 - ((lines + 0) / 2);  // untested
			else
				y = 2 - ((lines + 0) / 2);

			// draw the text lines
			for (i = 0; i < len && lines > 0; lines--)
			{
				if (small)
					UI_PrintStringSmall(String + i, menu_item_x1, menu_item_x2, y);
				else
					UI_PrintString(String + i, menu_item_x1, menu_item_x2, y, 8);

				// look for start of next line
				while (i < len && String[i] >= 32)
					i++;

				// hop over the null term char(s)
				while (i < len && String[i] < 32)
					i++;

				y += small ? 1 : 2;
			}
		}
	}

	if (g_menu_cursor == MENU_SLIST1 || g_menu_cursor == MENU_SLIST2)
	{
		i = (g_menu_cursor == MENU_SLIST1) ? 0 : 1;

//		if (g_sub_menu_selection == 0xFF)
		if (g_sub_menu_selection < 0)
			strcpy(String, "NULL");
		else
			UI_GenerateChannelStringEx(String, "CH-", g_sub_menu_selection);

//		if (g_sub_menu_selection == 0xFF || !g_eeprom.scan_list_enabled[i])
		if (g_sub_menu_selection < 0 || !g_eeprom.scan_list_enabled[i])
		{
			// channel number
			UI_PrintString(String, menu_item_x1, menu_item_x2, 0, 8);

			// channel name
			BOARD_fetchChannelName(String, g_sub_menu_selection);
			if (String[0] == 0)
				strcpy(String, "--");
			UI_PrintString(String, menu_item_x1, menu_item_x2, 2, 8);
		}
		else
		{
			// channel number
			UI_PrintString(String, menu_item_x1, menu_item_x2, 0, 8);

			// channel name
			BOARD_fetchChannelName(String, g_sub_menu_selection);
			if (String[0] == 0)
				strcpy(String, "--");
			UI_PrintStringSmall(String, menu_item_x1, menu_item_x2, 2);

			if (IS_USER_CHANNEL(g_eeprom.scan_list_priority_ch1[i]))
			{
				sprintf(String, "PRI1:%u", g_eeprom.scan_list_priority_ch1[i] + 1);
				UI_PrintString(String, menu_item_x1, menu_item_x2, 3, 8);
			}

			if (IS_USER_CHANNEL(g_eeprom.scan_list_priority_ch2[i]))
			{
				sprintf(String, "PRI2:%u", g_eeprom.scan_list_priority_ch2[i] + 1);
				UI_PrintString(String, menu_item_x1, menu_item_x2, 5, 8);
			}
		}
	}

	if ((g_menu_cursor == MENU_R_CTCS || g_menu_cursor == MENU_R_DCS) && g_css_scan_mode != CSS_SCAN_MODE_OFF)
		UI_PrintString("SCAN", menu_item_x1, menu_item_x2, 4, 8);

	if (g_menu_cursor == MENU_UPCODE)
		if (strlen(g_eeprom.dtmf_key_up_code) > 8)
			UI_PrintString(g_eeprom.dtmf_key_up_code + 8, menu_item_x1, menu_item_x2, 4, 8);

	if (g_menu_cursor == MENU_DWCODE)
		if (strlen(g_eeprom.dtmf_key_down_code) > 8)
			UI_PrintString(g_eeprom.dtmf_key_down_code + 8, menu_item_x1, menu_item_x2, 4, 8);

	if (g_menu_cursor == MENU_R_CTCS ||
	    g_menu_cursor == MENU_T_CTCS ||
	    g_menu_cursor == MENU_R_DCS  ||
	    g_menu_cursor == MENU_T_DCS  ||
	    g_menu_cursor == MENU_D_LIST)
	{
		unsigned int Offset;
		NUMBER_ToDigits(g_sub_menu_selection, String);
		Offset = (g_menu_cursor == MENU_D_LIST) ? 2 : 3;
		UI_Displaysmall_digits(Offset, String + (8 - Offset), 105, 0, false);
	}

	if ((g_menu_cursor == MENU_RESET    ||
	     g_menu_cursor == MENU_MEM_CH   ||
	     g_menu_cursor == MENU_MEM_NAME ||
	     g_menu_cursor == MENU_DEL_CH) && g_ask_for_confirmation)
	{	// display confirmation
		strcpy(String, (g_ask_for_confirmation == 1) ? "SURE?" : "WAIT!");
		UI_PrintString(String, menu_item_x1, menu_item_x2, 5, 8);
	}

	ST7565_BlitFullScreen();
}
