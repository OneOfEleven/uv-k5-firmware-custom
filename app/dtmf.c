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
#include <stdio.h>   // NULL

#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "app/search.h"
#include "bsp/dp32g030/gpio.h"
#include "audio.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "dtmf.h"
#include "external/printf/printf.h"
#include "misc.h"
#include "settings.h"
#include "ui/ui.h"

char               g_dtmf_string[15];
				   
char               g_dtmf_input_box[15];
uint8_t            g_dtmf_input_box_index;
bool               g_dtmf_input_mode;
uint8_t            g_dtmf_prev_index;
				   
char               g_dtmf_rx[17];
uint8_t            g_dtmf_rx_index;
uint8_t            g_dtmf_rx_timeout;
bool               g_dtmf_rx_pending;
				   
char               g_dtmf_rx_live[20];
uint8_t            g_dtmf_rx_live_timeout;

bool               g_dtmf_is_contact_valid;
char               g_dtmf_id[4];
char               g_dtmf_caller[4];
char               g_dtmf_callee[4];
dtmf_state_t       g_dtmf_state;
uint8_t            g_dtmf_decode_ring_count_down_500ms;
uint8_t            g_dtmf_chosen_contact;
uint8_t            g_dtmf_auto_reset_time_500ms;
dtmf_call_state_t  g_dtmf_call_state;
dtmf_reply_state_t g_dtmf_reply_state;
dtmf_call_mode_t   g_dtmf_call_mode;
bool               g_dtmf_is_tx;
uint8_t            g_dtmf_tx_stop_count_down_500ms;
bool               g_dtmf_IsGroupCall;

void DTMF_clear_RX(void)
{
	g_dtmf_rx_timeout = 0;
	g_dtmf_rx_index   = 0;
	g_dtmf_rx_pending = false;
	memset(g_dtmf_rx, 0, sizeof(g_dtmf_rx));
}

bool DTMF_ValidateCodes(char *pCode, const unsigned int size)
{
	unsigned int i;

	if (pCode[0] == 0xFF || pCode[0] == 0)
		return false;

	for (i = 0; i < size; i++)
	{
		if (pCode[i] == 0xFF || pCode[i] == 0)
		{
			pCode[i] = 0;
			break;
		}

		if ((pCode[i] < '0' || pCode[i] > '9') && (pCode[i] < 'A' || pCode[i] > 'D') && pCode[i] != '*' && pCode[i] != '#')
			return false;
	}

	return true;
}

bool DTMF_GetContact(const int Index, char *pContact)
{
	int i = -1;
	if (Index >= 0 && Index < MAX_DTMF_CONTACTS && pContact != NULL)
	{
		EEPROM_ReadBuffer(0x1C00 + (Index * 16), pContact, 16);
		i = (int)pContact[0] - ' ';
	}
	return (i < 0 || i >= 95) ? false : true;
}

bool DTMF_FindContact(const char *pContact, char *pResult)
{
	char         Contact[16];
	unsigned int i;

	for (i = 0; i < MAX_DTMF_CONTACTS; i++)
	{
		unsigned int j;

		if (!DTMF_GetContact(i, Contact))
			return false;

		for (j = 0; j < 3; j++)
			if (pContact[j] != Contact[j + 8])
				break;

		if (j == 3)
		{
			memcpy(pResult, Contact, 8);
			pResult[8] = 0;
			return true;
		}
	}

	return false;
}

char DTMF_GetCharacter(const unsigned int code)
{
	switch (code)
	{
		case KEY_0:    return '0';
		case KEY_1:    return '1';
		case KEY_2:    return '2';
		case KEY_3:    return '3';
		case KEY_4:    return '4';
		case KEY_5:    return '5';
		case KEY_6:    return '6';
		case KEY_7:    return '7';
		case KEY_8:    return '8';
		case KEY_9:    return '9';
		case KEY_MENU: return 'A';
		case KEY_UP:   return 'B';
		case KEY_DOWN: return 'C';
		case KEY_EXIT: return 'D';
		case KEY_STAR: return '*';
		case KEY_F:    return '#';
		default:       return 0xff;
	}
}

bool DTMF_CompareMessage(const char *pMsg, const char *pTemplate, const unsigned int size, const bool bCheckGroup)
{
	unsigned int i;
	for (i = 0; i < size; i++)
	{
		if (pMsg[i] != pTemplate[i])
		{
			if (!bCheckGroup || pMsg[i] != g_eeprom.dtmf_group_call_code)
				return false;
			g_dtmf_IsGroupCall = true;
		}
	}

	return true;
}

dtmf_call_mode_t DTMF_CheckGroupCall(const char *pMsg, const unsigned int size)
{
	unsigned int i;
	for (i = 0; i < size; i++)
		if (pMsg[i] == g_eeprom.dtmf_group_call_code)
			break;

	return (i < size) ? DTMF_CALL_MODE_GROUP : DTMF_CALL_MODE_NOT_GROUP;
}

void DTMF_clear_input_box(void)
{
	memset(g_dtmf_input_box, 0, sizeof(g_dtmf_input_box));
	g_dtmf_input_box_index = 0;
	g_dtmf_input_mode      = false;
}

void DTMF_Append(const char code)
{
	if (g_dtmf_input_box_index == 0)
	{
		memset(g_dtmf_input_box, '-', sizeof(g_dtmf_input_box) - 1);
		g_dtmf_input_box[sizeof(g_dtmf_input_box) - 1] = 0;
	}

	if (g_dtmf_input_box_index < (sizeof(g_dtmf_input_box) - 1))
		g_dtmf_input_box[g_dtmf_input_box_index++] = code;
}

void DTMF_HandleRequest(void)
{	// proccess the RX'ed DTMF characters

	char         String[21];
	unsigned int Offset;

	if (!g_dtmf_rx_pending)
		return;   // nothing new received

	if (g_scan_state_dir != SCAN_STATE_DIR_OFF || g_css_scan_mode != CSS_SCAN_MODE_OFF)
	{	// we're busy scanning
		DTMF_clear_RX();
		return;
	}

	#ifdef ENABLE_KILL_REVIVE
		if (!g_rx_vfo->dtmf_decoding_enable && !g_setting_radio_disabled)
	#else
		if (!g_rx_vfo->dtmf_decoding_enable)
	#endif
	{	// D-DCD is disabled or we're enabled
		DTMF_clear_RX();
		return;
	}

	g_dtmf_rx_pending = false;

	#ifdef ENABLE_KILL_REVIVE
		if (g_dtmf_rx_index >= 9)
		{	// look for the RADIO DISABLE code
	
			sprintf(String, "%s%c%s", g_eeprom.ani_dtmf_id, g_eeprom.dtmf_separate_code, g_eeprom.kill_code);
	
			Offset = g_dtmf_rx_index - strlen(String);
	
			if (DTMF_CompareMessage(g_dtmf_rx + Offset, String, strlen(String), true))
			{	// bugger
	
				if (g_eeprom.permit_remote_kill)
				{
					g_setting_radio_disabled = true;      // :(
	
					DTMF_clear_RX();
	
					SETTINGS_save();
	
					g_dtmf_reply_state = DTMF_REPLY_AB;
	
					#ifdef ENABLE_FMRADIO
						if (g_fm_radio_mode)
						{
							FM_TurnOff();
							GUI_SelectNextDisplay(DISPLAY_MAIN);
						}
					#endif
				}
				else
				{
					g_dtmf_reply_state = DTMF_REPLY_NONE;
				}
	
				g_dtmf_call_state = DTMF_CALL_STATE_NONE;
	
				g_update_display  = true;
				g_update_status   = true;
				return;
			}
		}

		if (g_dtmf_rx_index >= 9)
		{	// look for the REVIVE code
	
			sprintf(String, "%s%c%s", g_eeprom.ani_dtmf_id, g_eeprom.dtmf_separate_code, g_eeprom.revive_code);
	
			Offset = g_dtmf_rx_index - strlen(String);
	
			if (DTMF_CompareMessage(g_dtmf_rx + Offset, String, strlen(String), true))
			{	// shit, we're back !
	
				g_setting_radio_disabled  = false;
	
				DTMF_clear_RX();
	
				SETTINGS_save();
	
				g_dtmf_reply_state = DTMF_REPLY_AB;
				g_dtmf_call_state  = DTMF_CALL_STATE_NONE;
	
				g_update_display   = true;
				g_update_status    = true;
				return;
			}
		}
	#endif

	if (g_dtmf_rx_index >= 2)
	{	// look for ACK reply

		strcpy(String, "AB");

		Offset = g_dtmf_rx_index - strlen(String);

		if (DTMF_CompareMessage(g_dtmf_rx + Offset, String, strlen(String), true))
		{	// ends with "AB"

			if (g_dtmf_reply_state != DTMF_REPLY_NONE)          // 1of11
//			if (g_dtmf_call_state == DTMF_CALL_STATE_CALL_OUT)  // 1of11
			{
				g_dtmf_state = DTMF_STATE_TX_SUCC;
				DTMF_clear_RX();
				g_update_display = true;
				return;
			}
		}
	}

	if (g_dtmf_call_state == DTMF_CALL_STATE_CALL_OUT &&
	    g_dtmf_call_mode  == DTMF_CALL_MODE_NOT_GROUP &&
	    g_dtmf_rx_index >= 9)
	{	// waiting for a reply

		sprintf(String, "%s%c%s", g_dtmf_string, g_eeprom.dtmf_separate_code, "AAAAA");

		Offset = g_dtmf_rx_index - strlen(String);

		if (DTMF_CompareMessage(g_dtmf_rx + Offset, String, strlen(String), false))
		{	// we got a response
			g_dtmf_state    = DTMF_STATE_CALL_OUT_RSP;
			DTMF_clear_RX();
			g_update_display = true;
		}
	}

	#ifdef ENABLE_KILL_REVIVE
		if (g_setting_radio_disabled)
			return;        // we've been disabled
	#endif

	if (g_dtmf_rx_index >= 7)
	{	// see if we're being called

		g_dtmf_IsGroupCall = false;

		sprintf(String, "%s%c", g_eeprom.ani_dtmf_id, g_eeprom.dtmf_separate_code);

		Offset = g_dtmf_rx_index - strlen(String) - 3;

		if (DTMF_CompareMessage(g_dtmf_rx + Offset, String, strlen(String), true))
		{	// it's for us !

			g_dtmf_call_state = DTMF_CALL_STATE_RECEIVED;

			memset(g_dtmf_callee, 0, sizeof(g_dtmf_callee));
			memset(g_dtmf_caller, 0, sizeof(g_dtmf_caller));
			memcpy(g_dtmf_callee, g_dtmf_rx + Offset + 0, 3);
			memcpy(g_dtmf_caller, g_dtmf_rx + Offset + 4, 3);

			DTMF_clear_RX();

			g_update_display = true;

			#pragma GCC diagnostic push
			#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

			switch (g_eeprom.dtmf_decode_response)
			{
				case DTMF_DEC_RESPONSE_BOTH:
					g_dtmf_decode_ring_count_down_500ms = dtmf_decode_ring_countdown_500ms;
				case DTMF_DEC_RESPONSE_REPLY:
					g_dtmf_reply_state = DTMF_REPLY_AAAAA;
					break;
				case DTMF_DEC_RESPONSE_RING:
					g_dtmf_decode_ring_count_down_500ms = dtmf_decode_ring_countdown_500ms;
					break;
				default:
				case DTMF_DEC_RESPONSE_NONE:
					g_dtmf_decode_ring_count_down_500ms = 0;
					g_dtmf_reply_state = DTMF_REPLY_NONE;
					break;
			}

			#pragma GCC diagnostic pop

			if (g_dtmf_IsGroupCall)
				g_dtmf_reply_state = DTMF_REPLY_NONE;
		}
	}
}

bool DTMF_Reply(void)
{
	const uint16_t Delay   = (g_eeprom.dtmf_preload_time < 150) ? 150 : g_eeprom.dtmf_preload_time;
	const char    *pString = NULL;
	char           String[23];

	switch (g_dtmf_reply_state)
	{
		case DTMF_REPLY_ANI:
			if (g_dtmf_call_mode == DTMF_CALL_MODE_DTMF)
			{
				pString = g_dtmf_string;
			}
			else
			{	// append our ID code onto the end of the DTMF code to send
				sprintf(String, "%s%c%s", g_dtmf_string, g_eeprom.dtmf_separate_code, g_eeprom.ani_dtmf_id);
				pString = String;
			}
			break;

		case DTMF_REPLY_AB:
			pString = "AB";
			break;

		case DTMF_REPLY_AAAAA:
			sprintf(String, "%s%c%s", g_eeprom.ani_dtmf_id, g_eeprom.dtmf_separate_code, "AAAAA");
			pString = String;
			break;

		default:
		case DTMF_REPLY_NONE:
			if (g_dtmf_call_state != DTMF_CALL_STATE_NONE           ||
			    g_current_vfo->dtmf_ptt_id_tx_mode == PTT_ID_APOLLO ||
			    g_current_vfo->dtmf_ptt_id_tx_mode == PTT_ID_OFF    ||
			    g_current_vfo->dtmf_ptt_id_tx_mode == PTT_ID_TX_DOWN)
			{
				g_dtmf_reply_state = DTMF_REPLY_NONE;
				return false;
			}

			// send TX-UP DTMF
			pString = g_eeprom.dtmf_key_up_code;
			break;
	}

	g_dtmf_reply_state = DTMF_REPLY_NONE;

	if (pString == NULL)
		return false;

	if (g_eeprom.dtmf_side_tone)
	{	// the user will also hear the transmitted tones
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
		g_speaker_enabled = true;
	}

	SYSTEM_DelayMs(Delay);

	BK4819_EnterDTMF_TX(g_eeprom.dtmf_side_tone);

	BK4819_PlayDTMFString(
		pString,
		1,
		g_eeprom.dtmf_first_code_persist_time,
		g_eeprom.dtmf_hash_code_persist_time,
		g_eeprom.dtmf_code_persist_time,
		g_eeprom.dtmf_code_interval_time);

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
	g_speaker_enabled = false;

	BK4819_ExitDTMF_TX(false);
	
	return true;
}
