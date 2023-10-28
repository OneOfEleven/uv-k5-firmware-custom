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
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
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

	{"SQL",    VOICE_ID_SQUELCH,                       MENU_SQL                   },
	{"CH SQL", VOICE_ID_SQUELCH,                       MENU_CHAN_SQL              },
	{"STEP",   VOICE_ID_FREQUENCY_STEP,                MENU_STEP                  },
	{"W/N",    VOICE_ID_CHANNEL_BANDWIDTH,             MENU_BANDWIDTH             },
	{"Tx PWR", VOICE_ID_POWER,                         MENU_TX_POWER              }, // was "TXP"
	{"Rx DCS", VOICE_ID_DCS,                           MENU_RX_CDCSS              }, // was "R_DCS"
	{"Rx CTS", VOICE_ID_CTCSS,                         MENU_RX_CTCSS              }, // was "R_CTCS"
	{"Tx DCS", VOICE_ID_DCS,                           MENU_TX_CDCSS              }, // was "T_DCS"
	{"Tx CTS", VOICE_ID_CTCSS,                         MENU_TX_CTCSS              }, // was "T_CTCS"
	{"Tx DIR", VOICE_ID_TX_OFFSET_FREQ_DIR,            MENU_SHIFT_DIR             }, // was "SFT_D"
	{"Tx OFS", VOICE_ID_TX_OFFSET_FREQ,                MENU_OFFSET                }, // was "OFFSET"
	{"Tx TO",  VOICE_ID_TRANSMIT_OVER_TIME,            MENU_TX_TO                 }, // was "TOT"
	{"Tx VFO", VOICE_ID_INVALID,                       MENU_CROSS_VFO             }, // was "WX"
	{"Dual W", VOICE_ID_DUAL_STANDBY,                  MENU_DUAL_WATCH            }, // was "TDR"
	{"SC REV", VOICE_ID_INVALID,                       MENU_SCAN_CAR_RESUME       }, // was "SC_REV"
	{"S HOLD", VOICE_ID_INVALID,                       MENU_SCAN_HOLD             },
	{"SCRAM",  VOICE_ID_SCRAMBLER_ON,                  MENU_SCRAMBLER             }, // was "SCR"
	{"BCL",    VOICE_ID_BUSY_LOCKOUT,                  MENU_BUSY_CHAN_LOCK        },
	{"CH SAV", VOICE_ID_MEMORY_CHANNEL,                MENU_MEM_SAVE              }, // was "MEM-CH"
	{"CH NAM", VOICE_ID_INVALID,                       MENU_MEM_NAME              },
	{"CH DEL", VOICE_ID_DELETE_CHANNEL,                MENU_MEM_DEL               }, // was "DEL-CH"
	{"CH DIS", VOICE_ID_INVALID,                       MENU_MEM_DISP              }, // was "MDF"
	{"BatSAV", VOICE_ID_SAVE_MODE,                     MENU_BAT_SAVE              }, // was "SAVE"
#ifdef ENABLE_VOX
	{"VOX",    VOICE_ID_VOX,                           MENU_VOX                   },
#endif
	{"BL ",    VOICE_ID_INVALID,                       MENU_AUTO_BACKLITE         }, // was "ABR"
	{"BL TRX", VOICE_ID_INVALID,                       MENU_AUTO_BACKLITE_ON_TX_RX},
#ifdef ENABLE_CONTRAST
	{"CTRAST", VOICE_ID_INVALID,                       MENU_CONTRAST              },
#endif
	{"BEEP",   VOICE_ID_BEEP_PROMPT,                   MENU_BEEP                  },
#ifdef ENABLE_VOICE
	{"VOICE",  VOICE_ID_VOICE_PROMPT,                  MENU_VOICE                 },
#endif
#ifdef ENABLE_KEYLOCK
	{"KeyLOC", VOICE_ID_INVALID,                       MENU_AUTO_KEY_LOCK         }, // was "AUTOLk"
#endif
	{"S ADD1", VOICE_ID_INVALID,                       MENU_S_ADD1                },
	{"S ADD2", VOICE_ID_INVALID,                       MENU_S_ADD2                },
	{"STE",    VOICE_ID_INVALID,                       MENU_STE                   },
	{"RP STE", VOICE_ID_INVALID,                       MENU_RP_STE                },
	{"MIC GN", VOICE_ID_INVALID,                       MENU_MIC_GAIN              },
	{"COMPND", VOICE_ID_INVALID,                       MENU_COMPAND               },
#ifdef ENABLE_TX_AUDIO_BAR
	{"Tx BAR", VOICE_ID_INVALID,                       MENU_TX_BAR                },
#endif
#ifdef ENABLE_RX_SIGNAL_BAR
	{"Rx BAR", VOICE_ID_INVALID,                       MENU_RX_BAR                },
#endif
	{"1 CALL", VOICE_ID_INVALID,                       MENU_1_CALL                },
	{"SLIST",  VOICE_ID_INVALID,                       MENU_S_LIST                },
	{"SLIST1", VOICE_ID_INVALID,                       MENU_SLIST1                },
	{"SLIST2", VOICE_ID_INVALID,                       MENU_SLIST2                },
#ifdef ENABLE_ALARM
	{"SOS AL", VOICE_ID_INVALID,                       MENU_ALARM_MODE            }, // was "ALMODE"
#endif
	{"ANI ID", VOICE_ID_ANI_CODE,                      MENU_ANI_ID                },
	{"UpCODE", VOICE_ID_INVALID,                       MENU_UP_CODE               },
	{"DnCODE", VOICE_ID_INVALID,                       MENU_DN_CODE               }, // was "DWCODE"
#ifdef ENABLE_MDC1200
	{"MDCPTT", VOICE_ID_INVALID,                       MENU_MDC1200_MODE          },
	{"MDC ID", VOICE_ID_INVALID,                       MENU_MDC1200_ID            },
#endif
	{"PTT ID", VOICE_ID_INVALID,                       MENU_PTT_ID                },
	{"D ST",   VOICE_ID_INVALID,                       MENU_DTMF_ST               },
    {"D RSP",  VOICE_ID_INVALID,                       MENU_DTMF_RSP              },
	{"D HOLD", VOICE_ID_INVALID,                       MENU_DTMF_HOLD             },
	{"D PRE",  VOICE_ID_INVALID,                       MENU_DTMF_PRE              },
	{"D DCD",  VOICE_ID_INVALID,                       MENU_DTMF_DCD              },
	{"D LIST", VOICE_ID_INVALID,                       MENU_DTMF_LIST             },
	{"D LIVE", VOICE_ID_INVALID,                       MENU_DTMF_LIVE_DEC         }, // live DTMF decoder
	{"PonMSG", VOICE_ID_INVALID,                       MENU_PON_MSG               },
	{"ROGER",  VOICE_ID_INVALID,                       MENU_ROGER_MODE            },
	{"BatVOL", VOICE_ID_INVALID,                       MENU_VOLTAGE               }, // was "VOL"
	{"BatTXT", VOICE_ID_INVALID,                       MENU_BAT_TXT               },
	{"MODE",   VOICE_ID_INVALID,                       MENU_MOD_MODE              }, // was "AM"
#ifdef ENABLE_AM_FIX
//	{"AM FIX", VOICE_ID_INVALID,                       MENU_AM_FIX                },
#endif
#ifdef ENABLE_AM_FIX_TEST1
	{"AM FT1", VOICE_ID_INVALID,                       MENU_AM_FIX_TEST1          },
#endif
#ifdef ENABLE_NOAA
	{"NOAA-S", VOICE_ID_INVALID,                       MENU_NOAA_SCAN             },
#endif
#ifdef ENABLE_SIDE_BUTT_MENU
	{"Side1S", VOICE_ID_INVALID,                       MENU_SIDE1_SHORT           },
	{"Side1L", VOICE_ID_INVALID,                       MENU_SIDE1_LONG            },
	{"Side2S", VOICE_ID_INVALID,                       MENU_SIDE2_SHORT           },
	{"Side2L", VOICE_ID_INVALID,                       MENU_SIDE2_LONG            },
#endif
	{"VER",    VOICE_ID_INVALID,                       MENU_VERSION               },
	{"RESET",  VOICE_ID_INITIALISATION,                MENU_RESET                 }, // might be better to move this to the hidden menu items ?

	// ************************************
	// ************************************
	// ************************************
	// hidden menu items from here on
	// enabled by pressing both the PTT and upper side button at power-on

	{"BatCAL", VOICE_ID_INVALID,                       MENU_BAT_CAL               }, // battery voltage calibration

#ifdef ENABLE_F_CAL_MENU
	{"F CAL",  VOICE_ID_INVALID,                       MENU_F_CALI                }, // reference xtal calibration
#endif

	{"F LOCK", VOICE_ID_INVALID,                       MENU_FREQ_LOCK             }, // country/area specific
	{"Tx 174", VOICE_ID_INVALID,                       MENU_174_TX                }, // was "200TX"
	{"Tx 350", VOICE_ID_INVALID,                       MENU_350_TX                }, // was "350TX"
	{"Tx 470", VOICE_ID_INVALID,                       MENU_470_TX                }, // was "500TX"
	{"350 EN", VOICE_ID_INVALID,                       MENU_350_EN                }, // was "350EN"
	{"SCR EN", VOICE_ID_INVALID,                       MENU_SCRAMBLER_EN          }, // was "SCREN"
	{"Tx EN",  VOICE_ID_INVALID,                       MENU_TX_EN                 }, // enable TX

	// ************************************
	// ************************************
	// ************************************
};

// number of hidden menu items at the end of the list - KEEP THIS CORRECT
const unsigned int g_hidden_menu_count = 9;

// ***************************************************************************************

const char g_sub_menu_tx_power[3][7] =
{
	"LOW",
	"MEDIUM",
	"HIGH"
};

const char g_sub_menu_shift_dir[3][4] =
{
	"OFF",
	"+",
	"-"
};

const char g_sub_menu_bandwidth[2][7] =
{
	"WIDE",
	"NARROW"
};

const char g_sub_menu_off_on[2][4] =
{
	"OFF",
	"ON"
};

const char g_sub_menu_bat_save[5][9] =
{
	"OFF",
	"1:1 50%",
	"1:2 66%",
	"1:3 75%",
	"1:4 80%"
};

const char g_sub_menu_tx_timeout[11][7] =
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

const char g_sub_menu_dual_watch[3][10] =
{
	"OFF",
	"LOWER\nVFO",
	"UPPER\nVFO",
};

const char g_sub_menu_cross_vfo[3][10] =
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

const char g_sub_menu_scan_car_resume[3][8] =
{
	"TIME ",
	"CARRIER",
//	"SEARCH"
	"NO"
};

const char g_sub_menu_mem_disp[4][15] =
{
	"FREQ",
	"CHANNEL\nNUMBER",
	"NAME",
	"NAME\n+\nFREQ"
};

#ifdef ENABLE_ALARM
	const char g_sub_menu_alarm_mode[2][5] =
	{
		"SITE",
		"TONE"
	};
#endif

const char g_sub_menu_dtmf_rsp[4][9] =
{
	"NONE",
	"RING",
	"REPLY",
	"RNG RPLY"
};

const char g_sub_menu_ptt_id[5][15] =
{
	"OFF",
	"BOT",
	"EOT",
	"BOT+EOT",
	"APOLLO\nQUINDAR"
};

#ifdef ENABLE_MDC1200
	const char g_sub_menu_mdc1200_mode[4][8] =
	{
		"OFF",
		"BOT",
		"EOT",
		"BOT+EOT"
	};
#endif

const char g_sub_menu_pwr_on_msg[4][14] =
{
	"ALL\nPIXELS\nON",
	"MESSAGE",
	"VOLTAGE",
	"NONE"
};

const char g_sub_menu_roger_mode[2][16] =
{
	"OFF",
	"TX END\nROGER",
//	"TX END\nMDC1200"
};

const char g_sub_menu_reset[2][4] =
{
	"VFO",
	"ALL"
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
	const char g_sub_menu_AM_FIX_test1[4][8] =
	{
		"LNA-S 0",
		"LNA-S 1",
		"LNA-S 2",
		"LNA-S 3"
	};
#endif

const char g_sub_menu_bat_text[3][8] =
{
	"NONE",
	"VOLTAGE",
	"PERCENT"
};

const char g_sub_menu_dis_en[2][9] =
{
	"DISABLED",
	"ENABLED"
};

const char g_sub_menu_scrambler[11][7] =
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

#ifdef ENABLE_SIDE_BUTT_MENU
const char g_sub_menu_side_butt[9][16] =
//const char g_sub_menu_side_butt[10][16] =
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
#endif

// ***************************************************************************************

uint8_t g_menu_list_sorted[ARRAY_SIZE(g_menu_list)];

bool    g_in_sub_menu;
uint8_t g_menu_cursor;
int8_t  g_menu_scroll_direction;
int32_t g_sub_menu_selection;

// edit box
char    g_edit_original[17]; // a copy of the text before editing so that we can easily test for changes/differences
char    g_edit[17];
int     g_edit_index;

// ***************************************************************************************

void sort_list(const unsigned int start, const unsigned int length)
{
	const unsigned int end = start + length;
	unsigned int i;
	for (i = start; i < end; i++)
	{
		unsigned int k;
		for (k = 0; k < end; k++)
		{
			if (g_menu_list[k].menu_id == i)
			{
				g_menu_list_sorted[i] = k;
				break;
			}
		}
	}
}

void UI_SortMenu(const bool hide_hidden)
{
	// sort the menu order according to the MENU-ID value (enum list in id/menu.h)
	//
	// this means the menu order is entirely determined by the enum list (found in id/menu.h)
	// it now no longer depends on the order of entries in the above const list (they can be any order)

	unsigned int hidden_menu_count = g_hidden_menu_count;

	#ifndef ENABLE_F_CAL_MENU
		hidden_menu_count--;
	#endif

	g_menu_list_count = ARRAY_SIZE(g_menu_list);

	// sort non-hidden entries at the beginning
	sort_list(0, g_menu_list_count - hidden_menu_count);

	// sort the hidden entries at the end
	sort_list(g_menu_list_count - hidden_menu_count, hidden_menu_count);
/*
	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	{
		unsigned int i;
		UART_SendText("menu ..\r\n");
		for (i = 0; i < ARRAY_SIZE(g_menu_list_sorted); i++)
			UART_printf("%3u %3u %3u\r\n", i, g_menu_list_sorted[i], g_menu_list[g_menu_list_sorted[i]].menu_id);
		UART_SendText("\r\n");
	}
	#endif
*/
	if (hide_hidden)
		g_menu_list_count -= hidden_menu_count;  // hide the hidden menu items
}

void UI_DisplayMenu(void)
{
	const unsigned int menu_list_width = 6;            // max no. of characters on the menu list (left side)
	const unsigned int sub_menu_x1     = (8 * menu_list_width) + 2;  // start x corrd
	const unsigned int sub_menu_x2     = LCD_WIDTH - 1;              //   end x coord
	unsigned int       i;
	char               str[64];  // bigger cuz we can now do multi-line in one string (use '\n' char)

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
		if (g_in_sub_menu)
			memcpy(g_frame_buffer[0] + (8 * menu_list_width) + 1, BITMAP_CurrentIndicator, sizeof(BITMAP_CurrentIndicator));

		// draw the menu index number/count
		sprintf(str, "%2u.%u", 1 + g_menu_cursor, g_menu_list_count);
		UI_PrintStringSmall(str, 2, 0, 6);

	#else
	{	// new menu layout .. experimental & unfinished

		const int menu_index = g_menu_cursor;  // current selected menu item
		i = 1;

		if (!g_in_sub_menu)
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
			sprintf(str, "%2u.%u", 1 + g_menu_cursor, g_menu_list_count);
			UI_PrintStringSmall(str, 2, 0, 6);
		}
		else
		if (menu_index >= 0 && menu_index < (int)g_menu_list_count)
		{	// current menu item
			strcpy(str, g_menu_list[g_menu_list_sorted[menu_index]].name);
			UI_PrintString(str, 0, 0, 0, 8);
		}
	}
	#endif

	// **************

	memset(str, 0, sizeof(str));

	bool already_printed = false;

	switch (g_menu_cursor)
	{
		case MENU_SQL:
			strcpy(str, "MAIN SQL\n");
			sprintf(str + strlen(str), "%d\n ", g_sub_menu_selection);
			break;

		case MENU_CHAN_SQL:
			if (g_sub_menu_selection == 0)
				strcpy(str, "USE\nMAIN SQL");
			else
				sprintf(str, "%d", g_sub_menu_selection);
//			g_tx_vfo->squelch_level = g_sub_menu_selection;
//			RADIO_ConfigureSquelchAndOutputPower(g_tx_vfo);
			break;

		case MENU_MIC_GAIN:
			{	// display the mic gain in actual dB rather than just an index number
				const uint8_t mic = g_mic_gain_dB_2[g_sub_menu_selection];
				sprintf(str, "+%u.%udB", mic / 2, mic % 2);
			}
			break;

		case MENU_STEP:
		{
//			const uint32_t step = (uint32_t)STEP_FREQ_TABLE[g_sub_menu_selection] * 10;
			const uint32_t step = (uint32_t)STEP_FREQ_TABLE[step_freq_table_sorted[g_sub_menu_selection]] * 10;
			if (step < 1000)
			{	// Hz
				sprintf(str, "%uHz", step);
			}
			else
			{	// kHz
				sprintf(str, "%u.%03u", step / 1000, step % 1000);
				NUMBER_trim_trailing_zeros(str);
				strcat(str, "kHz");
			}
			break;
		}

		case MENU_TX_POWER:
			strcpy(str, g_sub_menu_tx_power[g_sub_menu_selection]);
			break;

		case MENU_RX_CDCSS:
		case MENU_TX_CDCSS:
			strcpy(str, "CDCSS\n");
			if (g_sub_menu_selection == 0)
				strcat(str, "OFF");
			else
			if (g_sub_menu_selection < 105)
				sprintf(str + strlen(str), "D%03oN", DCS_OPTIONS[g_sub_menu_selection -   1]);
			else
				sprintf(str + strlen(str), "D%03oI", DCS_OPTIONS[g_sub_menu_selection - 105]);
			break;

		case MENU_RX_CTCSS:
		case MENU_TX_CTCSS:
		{
			strcpy(str, "CTCSS\n");
			#if 1
				// set CTCSS as the user adjusts it
				unsigned int Code;
				freq_config_t *pConfig = (g_menu_cursor == MENU_RX_CTCSS) ? &g_tx_vfo->freq_config_rx : &g_tx_vfo->freq_config_tx;
				if (g_sub_menu_selection == 0)
				{
					strcat(str, "OFF");

					if (pConfig->code_type != CODE_TYPE_CONTINUOUS_TONE)
						break;

					Code = 0;
					pConfig->code_type = CODE_TYPE_NONE;
					pConfig->code = Code;

					BK4819_ExitSubAu();
				}
				else
				{
					sprintf(str + strlen(str), "%u.%uHz", CTCSS_OPTIONS[g_sub_menu_selection - 1] / 10, CTCSS_OPTIONS[g_sub_menu_selection - 1] % 10);

					pConfig->code_type = CODE_TYPE_CONTINUOUS_TONE;
					Code = g_sub_menu_selection - 1;
					pConfig->code = Code;

					BK4819_SetCTCSSFrequency(CTCSS_OPTIONS[Code]);
				}
			#else
				if (g_sub_menu_selection == 0)
					strcat(str, "OFF");
				else
					sprintf(str + strlen(str), "%u.%uHz", CTCSS_OPTIONS[g_sub_menu_selection - 1] / 10, CTCSS_OPTIONS[g_sub_menu_selection - 1] % 10);
			#endif

			break;
		}

		case MENU_SHIFT_DIR:
			strcpy(str, g_sub_menu_shift_dir[g_sub_menu_selection]);
			break;

		case MENU_OFFSET:
			if (!g_in_sub_menu || g_input_box_index == 0)
			{
				sprintf(str, "%d.%05u", g_sub_menu_selection / 100000, abs(g_sub_menu_selection) % 100000);
				#ifdef ENABLE_TRIM_TRAILING_ZEROS
					NUMBER_trim_trailing_zeros(str);
				#endif
				UI_PrintString(str, sub_menu_x1, sub_menu_x2, 1, 8);
			}
			else
			{
				memset(str, 0, sizeof(str));
				i = 0;
				while (i < 3)
				{
					str[i] = (g_input_box[i] == 10) ? '-' : g_input_box[i] + '0';
					i++;
				}
				str[i] = '.';
				while (i < 8)
				{
					str[1 + i] = (g_input_box[i] == 10) ? '-' : g_input_box[i] + '0';			
					i++;
				}
				UI_PrintString(str, sub_menu_x1, sub_menu_x2, 1, 8);
			}
			UI_PrintString("MHz",  sub_menu_x1, sub_menu_x2, 3, 8);
			already_printed = true;
			break;

		case MENU_BANDWIDTH:
			strcpy(str, g_sub_menu_bandwidth[g_sub_menu_selection]);
			break;

		case MENU_SCRAMBLER:
			strcpy(str, "INVERT\n");
			strcat(str, g_sub_menu_scrambler[g_sub_menu_selection]);

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
					strcpy(str, "OFF");
				else
					sprintf(str, "%d", g_sub_menu_selection);
				break;
		#endif

		case MENU_AUTO_BACKLITE:
			strcpy(str, "BACKLITE\n");
			strcat(str, g_sub_menu_backlight[g_sub_menu_selection]);
			break;

		case MENU_AUTO_BACKLITE_ON_TX_RX:
			strcpy(str, "BACKLITE\n");
			strcat(str, g_sub_menu_rx_tx[g_sub_menu_selection]);
			break;

		case MENU_MOD_MODE:
			strcpy(str, (g_sub_menu_selection == 0) ? "FM" : "AM");
			break;

		#ifdef ENABLE_AM_FIX_TEST1
			case MENU_AM_FIX_TEST1:
				strcpy(str, g_sub_menu_AM_FIX_test1[g_sub_menu_selection]);
//				g_setting_am_fix = g_sub_menu_selection;
				break;
		#endif

		#ifdef ENABLE_KEYLOCK
		case MENU_AUTO_KEY_LOCK:
			if (g_sub_menu_selection == 0)
				strcpy(str, "OFF");
			else
				sprintf(str, "%u secs", key_lock_timeout_500ms / 2);
			break;
		#endif

		case MENU_COMPAND:
			strcpy(str, g_sub_menu_rx_tx[g_sub_menu_selection]);
			break;

		#ifdef ENABLE_CONTRAST
			case MENU_CONTRAST:
				strcpy(str, "CONTRAST\n");
				sprintf(str + strlen(str), "%d", g_sub_menu_selection);
				//g_setting_contrast = g_sub_menu_selection
				ST7565_SetContrast(g_sub_menu_selection);
				g_update_display = true;
				break;
		#endif

		#ifdef ENABLE_TX_AUDIO_BAR
			case MENU_TX_BAR:
		#endif
		#ifdef ENABLE_RX_SIGNAL_BAR
			case MENU_RX_BAR:
		#endif
		#ifdef ENABLE_AM_FIX
//			case MENU_AM_FIX:
		#endif
		case MENU_S_ADD1:
		case MENU_S_ADD2:
			strcpy(str, g_sub_menu_off_on[g_sub_menu_selection]);
			break;

		case MENU_BUSY_CHAN_LOCK:
			strcpy(str, "BSY CH TX\nLOCKOUT\n");
			strcat(str, g_sub_menu_off_on[g_sub_menu_selection]);
			break;

		case MENU_DTMF_DCD:
		case MENU_DTMF_LIVE_DEC:
			strcpy(str, "DTMF\nDECODE\n");
			strcat(str, g_sub_menu_off_on[g_sub_menu_selection]);
			break;

		case MENU_STE:
			strcpy(str, "SUB TAIL\nELIMIN\n");
			strcat(str, g_sub_menu_off_on[g_sub_menu_selection]);
			break;

		case MENU_BEEP:
			strcpy(str, "KEY BEEP\n");
			strcat(str + strlen(str), g_sub_menu_off_on[g_sub_menu_selection]);
			break;

		case MENU_DTMF_ST:
			strcpy(str, "DTMF\nSIDETONE\n");
			strcat(str, g_sub_menu_off_on[g_sub_menu_selection]);
			break;

		#ifdef ENABLE_NOAA
			case MENU_NOAA_SCAN:
				strcpy(str, "SCAN\n");
				strcat(str, g_sub_menu_dis_en[g_sub_menu_selection]);
				break;
		#endif

		case MENU_350_EN:
			strcpy(str, "350~400\n");
			strcat(str, g_sub_menu_dis_en[g_sub_menu_selection]);
			break;

		case MENU_350_TX:
			strcpy(str, "TX\n350~400\n");
			strcat(str, g_sub_menu_dis_en[g_sub_menu_selection]);
			break;

		case MENU_174_TX:
			strcpy(str, "TX\n174~350\n");
			strcat(str, g_sub_menu_dis_en[g_sub_menu_selection]);
			break;

		case MENU_470_TX:
			strcpy(str, "TX\n470~600\n");
			strcat(str, g_sub_menu_dis_en[g_sub_menu_selection]);
			break;

		case MENU_SCRAMBLER_EN:
			strcpy(str, "SCRAMBLER\n");
			strcat(str, g_sub_menu_dis_en[g_sub_menu_selection]);
			break;

		case MENU_TX_EN:
			strcpy(str, "TX\n");
			strcat(str, g_sub_menu_dis_en[g_sub_menu_selection]);
			break;

		case MENU_MEM_SAVE:
		case MENU_1_CALL:
		case MENU_MEM_DEL:
		{
			char s[11];
			const bool valid = RADIO_CheckValidChannel(g_sub_menu_selection, false, 0);

			UI_GenerateChannelStringEx(str, valid ? "CH-" : "", g_sub_menu_selection);

			// channel name
			BOARD_fetchChannelName(s, g_sub_menu_selection);
			strcat(str, "\n");
			strcat(str, (s[0] == 0) ? "--" : s);

			if (valid && !g_ask_for_confirmation)
			{	// show the frequency so that the user knows the channels frequency
				const uint32_t frequency = BOARD_fetchChannelFrequency(g_sub_menu_selection);
				sprintf(str + strlen(str), "\n%u.%05u", frequency / 100000, frequency % 100000);

				#ifdef ENABLE_TRIM_TRAILING_ZEROS
					NUMBER_trim_trailing_zeros(str);
				#endif
			}

			break;
		}

		case MENU_MEM_NAME:
		{
			const bool valid = RADIO_CheckValidChannel(g_sub_menu_selection, false, 0);
			const unsigned int y = (!g_in_sub_menu || g_edit_index < 0) ? 1 : 0;

			UI_GenerateChannelStringEx(str, valid ? "CH-" : "", g_sub_menu_selection);
			UI_PrintString(str, sub_menu_x1, sub_menu_x2, y, 8);

			if (valid)
			{
				const uint32_t frequency = BOARD_fetchChannelFrequency(g_sub_menu_selection);

				if (!g_in_sub_menu || g_edit_index < 0)
				{	// show the channel name
					BOARD_fetchChannelName(str, g_sub_menu_selection);
					if (str[0] == 0)
						strcpy(str, "--");
					UI_PrintString(str, sub_menu_x1, sub_menu_x2, y + 2, 8);
				}
				else
				{	// show the channel name being edited
					UI_PrintString(g_edit, sub_menu_x1, 0, y + 2, 8);
					if (g_edit_index < 10)
						UI_PrintString("^", sub_menu_x1 + (8 * g_edit_index), 0, y + 4, 8);  // show the cursor
				}

				if (!g_ask_for_confirmation)
				{	// show the frequency so that the user knows the channels frequency
					sprintf(str, "%u.%05u", frequency / 100000, frequency % 100000);
					if (!g_in_sub_menu || g_edit_index < 0)
						UI_PrintString(str, sub_menu_x1, sub_menu_x2, y + 4, 8);
					else
						UI_PrintString(str, sub_menu_x1, sub_menu_x2, y + 5, 8);
				}
			}

			already_printed = true;
			break;
		}

		case MENU_BAT_SAVE:
			strcpy(str, g_sub_menu_bat_save[g_sub_menu_selection]);
			break;

		case MENU_CROSS_VFO:
			strcpy(str, g_sub_menu_cross_vfo[g_sub_menu_selection]);
			break;

		case MENU_TX_TO:
			strcpy(str, g_sub_menu_tx_timeout[g_sub_menu_selection]);
			break;

		#ifdef ENABLE_VOICE
			case MENU_VOICE:
				strcpy(str, g_sub_menu_voice[g_sub_menu_selection]);
				break;
		#endif

		case MENU_DUAL_WATCH:
//			strcpy(String, g_sub_menu_dual_watch[g_sub_menu_selection]);
			strcpy(str, g_sub_menu_off_on[g_sub_menu_selection]);
			break;

		case MENU_SCAN_CAR_RESUME:
			strcpy(str, "SCAN\nRESUME\n");
			strcat(str, g_sub_menu_scan_car_resume[g_sub_menu_selection]);
			if (g_sub_menu_selection == SCAN_RESUME_TIME)
				sprintf(str + strlen(str), "%d.%ds", g_eeprom.scan_hold_time_500ms / 2, 5 * (g_eeprom.scan_hold_time_500ms % 2));
			break;

		case MENU_SCAN_HOLD:
			strcpy(str, "SCAN HOLD\n");
			sprintf(str + strlen(str), "%d.%d sec", g_sub_menu_selection / 2, 5 * (g_sub_menu_selection % 2));
			break;

		case MENU_MEM_DISP:
			strcpy(str, g_sub_menu_mem_disp[g_sub_menu_selection]);
			break;

		case MENU_RP_STE:
			if (g_sub_menu_selection == 0)
				strcpy(str, "OFF");
			else
				sprintf(str, "%d*100ms", g_sub_menu_selection);
			break;

		case MENU_S_LIST:
			if (g_sub_menu_selection < 2)
				sprintf(str, "LIST%u", 1 + g_sub_menu_selection);
			else
				strcpy(str, "ALL");
			break;

		#ifdef ENABLE_ALARM
			case MENU_ALARM_MODE:
				strcpy(str, "TX ALARM\n");
				sprintf(str + strlen(str), g_sub_menu_alarm_mode[g_sub_menu_selection]);
				break;
		#endif

		case MENU_ANI_ID:
			strcpy(str, "YOUR ID\n");
			strcat(str, g_eeprom.ani_dtmf_id);
			break;

		case MENU_UP_CODE:
			strcpy(str, "PTT DTMF\nBEGIN\n");
			strcat(str, g_eeprom.dtmf_key_up_code);
			break;

		case MENU_DN_CODE:
			strcpy(str, "PTT DTMF\nEND\n");
			strcat(str, g_eeprom.dtmf_key_down_code);
			break;

		case MENU_DTMF_RSP:
			strcpy(str, "DTMF\nRESPONSE\n");
			strcat(str, g_sub_menu_dtmf_rsp[g_sub_menu_selection]);
			break;

		case MENU_DTMF_HOLD:
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

			strcpy(str, "DTMF MSG\n");
			if (g_sub_menu_selection < DTMF_HOLD_MAX)
				sprintf(str + strlen(str), "%d sec", g_sub_menu_selection);
			else
				strcat(str, "STAY ON\nSCREEN");  // 60

			break;

		case MENU_DTMF_PRE:
			strcpy(str, "TX DTMF\nDELAY\n");
//			sprintf(String + strlen(String), "%d*10ms", g_sub_menu_selection);
			sprintf(str + strlen(str), "%dms", 10 * g_sub_menu_selection);
			break;

		#ifdef ENABLE_MDC1200
			case MENU_MDC1200_MODE:
				strcpy(str, "MDC1200\nMODE\n");
				strcat(str, g_sub_menu_mdc1200_mode[g_sub_menu_selection]);
				break;
				
			case MENU_MDC1200_ID:
				sprintf(str, "MDC1200\nID\n%04X", g_sub_menu_selection);
				break;
		#endif

		case MENU_PTT_ID:
			strcpy(str, (g_sub_menu_selection > 0) ? "TX ID\n" : "");
			strcat(str, g_sub_menu_ptt_id[g_sub_menu_selection]);
			break;

		case MENU_BAT_TXT:
			strcpy(str, g_sub_menu_bat_text[g_sub_menu_selection]);
			break;

		case MENU_DTMF_LIST:
		{
			char Contact[17];
			g_dtmf_is_contact_valid = DTMF_GetContact((int)g_sub_menu_selection - 1, Contact);
			strcpy(str, "DTMF\n");
			if (!g_dtmf_is_contact_valid)
			{
				strcat(str, "NULL");
			}
			else
			{
				memcpy(str + strlen(str), Contact, 8);
				Contact[11] = 0;
				memcpy(&g_dtmf_id, Contact + 8, sizeof(g_dtmf_id));
				sprintf(str + strlen(str), "\nID:%s", Contact + 8);
			}
			break;
		}
		
		case MENU_PON_MSG:
			strcpy(str, g_sub_menu_pwr_on_msg[g_sub_menu_selection]);
			break;

		case MENU_ROGER_MODE:
			strcpy(str, g_sub_menu_roger_mode[g_sub_menu_selection]);
			break;

		case MENU_VOLTAGE:
			sprintf(str, "%u.%02uV\n%u%%\ncurr %u",
				g_battery_voltage_average / 100, g_battery_voltage_average % 100,
				BATTERY_VoltsToPercent(g_battery_voltage_average),
				g_usb_current);
			break;

		#ifdef ENABLE_SIDE_BUTT_MENU
			case MENU_SIDE1_SHORT:
			case MENU_SIDE1_LONG:
			case MENU_SIDE2_SHORT:
			case MENU_SIDE2_LONG:
				strcpy(str, g_sub_menu_side_butt[g_sub_menu_selection]);
				break;
		#endif

		case MENU_VERSION:
		{	// show the version string on multiple lines - if need be
			const unsigned int slen = strlen(Version_str);
			unsigned int m = 0;
			unsigned int k = 0;
			i = 0;
			while (i < (sizeof(str) - 1) && k < slen)
			{
				const char c = Version_str[k++];
				if (c == ' ' || c == '-' || c == '_')
				{
					if (m >= 3)
					{
						str[i++] = '\n';
						m = 0;
					}
					else
						str[i++] = c;
				}
				else
				{
					str[i++] = c;
					if (++m >= 9 && k < slen && i < (sizeof(str) - 1))
					{
						if (m > 0)
						{
							m = 0;
							str[i++] = '\n';
						}
					}
				}
			}

			// add the date and time
			strcat(str, "\n \n" __DATE__ "\n" __TIME__);
			break;
		}

		case MENU_RESET:
			strcpy(str, g_sub_menu_reset[g_sub_menu_selection]);
			break;

		case MENU_FREQ_LOCK:
			switch (g_sub_menu_selection)
			{
				case FREQ_LOCK_NORMAL:
					strcpy(str, "137~174\n400~470\n+ others");
					break;
				case FREQ_LOCK_FCC:
					strcpy(str, "FCC HAM\n144~148\n420~450");
					break;
				case FREQ_LOCK_CE:
					strcpy(str, "CE HAM\n144~146\n430~440");
					break;
				case FREQ_LOCK_GB:
					strcpy(str, "GB HAM\n144~148\n430~440");
					break;
				case FREQ_LOCK_430:
					strcpy(str, "137~174\n400~430");
					break;
				case FREQ_LOCK_438:
					strcpy(str, "137~174\n400~438");
					break;
				case FREQ_LOCK_446:
					strcpy(str, "446.00625\n~\n446.19375");
					break;
				#ifdef ENABLE_TX_UNLOCK
					case FREQ_LOCK_TX_UNLOCK:
						sprintf(str, "UNLOCKED\n%u~%u", BX4819_BAND1.lower / 100000, BX4819_BAND2.upper / 100000);
						break;
				#endif
			}
			break;

		#ifdef ENABLE_F_CAL_MENU
			case MENU_F_CALI:
				{
					const uint32_t value   = 22656 + g_sub_menu_selection;
					const uint32_t xtal_Hz = (0x4f0000u + value) * 5;

					writeXtalFreqCal(g_sub_menu_selection, false);

					sprintf(str, "%d\n%u.%06u",
						g_sub_menu_selection,
						xtal_Hz / 1000000, xtal_Hz % 1000000);

					#ifdef ENABLE_TRIM_TRAILING_ZEROS
						NUMBER_trim_trailing_zeros(str);
					#endif

					strcat(str, "\nMHz");
				}
				break;
		#endif

		case MENU_BAT_CAL:
		{
			const uint16_t vol = (uint32_t)g_battery_voltage_average * g_battery_calibration[3] / g_sub_menu_selection;
			if (!g_in_sub_menu)
				sprintf(str, "%u.%02uV\n%d", vol / 100, vol % 100, g_sub_menu_selection);
			else
				sprintf(str, "%u.%02uV\n(%#4d)\n%#4d", vol / 100, vol % 100, g_battery_calibration[3], g_sub_menu_selection);
			break;
		}
	}

	if (!already_printed)
	{	// we now do multi-line text in a single string

		unsigned int y;
		unsigned int lines = 1;
		unsigned int len   = strlen(str);
		bool         small = false;

		if (len > 0)
		{
			// count number of lines
			for (i = 0; i < len; i++)
			{
				if (str[i] == '\n' && i < (len - 1))
				{	// found new line char
					lines++;
					str[i] = 0;  // null terminate the line
				}
			}

			if (lines > 3)
			{	// switch to small text
				small = true;
				if (lines > 7)
					lines = 7;
			}

			// center vertically'ish
			if (small)
				y = 3 - ((lines + 0) / 2);
			else
				y = 2 - ((lines + 0) / 2);

			// draw the text lines
			for (i = 0; i < len && lines > 0; lines--)
			{
				if (small)
					UI_PrintStringSmall(str + i, sub_menu_x1, sub_menu_x2, y);
				else
					UI_PrintString(str + i, sub_menu_x1, sub_menu_x2, y, 8);

				// look for start of next line
				while (i < len && str[i] >= 32)
					i++;

				// hop over the null term char(s)
				while (i < len && str[i] < 32)
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
			strcpy(str, "NULL");
		else
			UI_GenerateChannelStringEx(str, "CH-", g_sub_menu_selection);

//		if (g_sub_menu_selection == 0xFF || !g_eeprom.scan_list_enabled[i])
		if (g_sub_menu_selection < 0 || !g_eeprom.scan_list_enabled[i])
		{
			// channel number
			UI_PrintString(str, sub_menu_x1, sub_menu_x2, 0, 8);

			// channel name
			BOARD_fetchChannelName(str, g_sub_menu_selection);
			if (str[0] == 0)
				strcpy(str, "--");
			UI_PrintString(str, sub_menu_x1, sub_menu_x2, 2, 8);
		}
		else
		{
			// channel number
			UI_PrintString(str, sub_menu_x1, sub_menu_x2, 0, 8);

			// channel name
			memset(str, 0, sizeof(str));
			BOARD_fetchChannelName(str, g_sub_menu_selection);
			if (str[0] == 0)
				strcpy(str, "--");
			UI_PrintStringSmall(str, sub_menu_x1, sub_menu_x2, 2);

			if (IS_USER_CHANNEL(g_eeprom.scan_list_priority_ch1[i]))
			{
				sprintf(str, "PRI1:%u", g_eeprom.scan_list_priority_ch1[i] + 1);
				UI_PrintString(str, sub_menu_x1, sub_menu_x2, 3, 8);
			}

			if (IS_USER_CHANNEL(g_eeprom.scan_list_priority_ch2[i]))
			{
				sprintf(str, "PRI2:%u", g_eeprom.scan_list_priority_ch2[i] + 1);
				UI_PrintString(str, sub_menu_x1, sub_menu_x2, 5, 8);
			}
		}
	}

	if ((g_menu_cursor == MENU_RX_CTCSS || g_menu_cursor == MENU_RX_CDCSS) && g_css_scan_mode != CSS_SCAN_MODE_OFF)
		UI_PrintString("SCAN", sub_menu_x1, sub_menu_x2, 4, 8);

	if (g_menu_cursor == MENU_UP_CODE)
		if (strlen(g_eeprom.dtmf_key_up_code) > 8)
			UI_PrintString(g_eeprom.dtmf_key_up_code + 8, sub_menu_x1, sub_menu_x2, 4, 8);

	if (g_menu_cursor == MENU_DN_CODE)
		if (strlen(g_eeprom.dtmf_key_down_code) > 8)
			UI_PrintString(g_eeprom.dtmf_key_down_code + 8, sub_menu_x1, sub_menu_x2, 4, 8);

	if (g_menu_cursor == MENU_RX_CTCSS ||
	    g_menu_cursor == MENU_TX_CTCSS ||
	    g_menu_cursor == MENU_RX_CDCSS ||
	    g_menu_cursor == MENU_TX_CDCSS ||
	    g_menu_cursor == MENU_DTMF_LIST)
	{
		if (g_in_sub_menu)
		{
			unsigned int Offset;
			NUMBER_ToDigits(g_sub_menu_selection, str);
			Offset = (g_menu_cursor == MENU_DTMF_LIST) ? 2 : 3;
			UI_Displaysmall_digits(Offset, str + (8 - Offset), 105, 0, false);
		}
	}

	if ((g_menu_cursor == MENU_RESET    ||
	     g_menu_cursor == MENU_MEM_SAVE   ||
	     g_menu_cursor == MENU_MEM_NAME ||
	     g_menu_cursor == MENU_MEM_DEL) && g_ask_for_confirmation)
	{	// display confirmation
		strcpy(str, (g_ask_for_confirmation == 1) ? "SURE?" : "WAIT!");
		UI_PrintString(str, sub_menu_x1, sub_menu_x2, 5, 8);
	}

	ST7565_BlitFullScreen();
}
