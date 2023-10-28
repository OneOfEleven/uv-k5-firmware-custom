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

#ifndef DTMF_H
#define DTMF_H

#include <stdbool.h>
#include <stdint.h>

#define    MAX_DTMF_CONTACTS   16

enum {  // seconds
	DTMF_HOLD_MIN =  5,
	DTMF_HOLD_MAX = 60
};

enum dtmf_state_e {
	DTMF_STATE_0 = 0,
	DTMF_STATE_TX_SUCC,
	DTMF_STATE_CALL_OUT_RSP
};
typedef enum dtmf_state_e dtmf_state_t;

enum dtmf_call_state_e {
	DTMF_CALL_STATE_NONE = 0,
	DTMF_CALL_STATE_CALL_OUT,
	DTMF_CALL_STATE_RECEIVED,
	DTMF_CALL_STATE_RECEIVED_STAY
};
typedef enum dtmf_call_state_e dtmf_call_state_t;

enum dtmf_decode_response_e {
	DTMF_DEC_RESPONSE_NONE = 0,
	DTMF_DEC_RESPONSE_RING,
	DTMF_DEC_RESPONSE_REPLY,
	DTMF_DEC_RESPONSE_BOTH
};
typedef enum dtmf_decode_response_e dtmf_decode_response_t;

enum dtmf_reply_state_e {
	DTMF_REPLY_NONE = 0,
	DTMF_REPLY_ANI,
	DTMF_REPLY_AB,
	DTMF_REPLY_AAAAA
};
typedef enum dtmf_reply_state_e dtmf_reply_state_t;

enum dtmf_call_mode_e {
	DTMF_CALL_MODE_NOT_GROUP = 0,
	DTMF_CALL_MODE_GROUP,
	DTMF_CALL_MODE_DTMF
};
typedef enum dtmf_call_mode_e dtmf_call_mode_t;

extern char               g_dtmf_string[15];

extern char               g_dtmf_input_box[15];
extern uint8_t            g_dtmf_input_box_index;
extern bool               g_dtmf_input_mode;
extern uint8_t            g_dtmf_prev_index;

extern char               g_dtmf_rx[17];
extern uint8_t            g_dtmf_rx_index;
extern uint8_t            g_dtmf_rx_timeout;
extern bool               g_dtmf_rx_pending;

extern char               g_dtmf_rx_live[20];
extern uint8_t            g_dtmf_rx_live_timeout;

extern bool               g_dtmf_is_contact_valid;
extern char               g_dtmf_id[4];
extern char               g_dtmf_caller[4];
extern char               g_dtmf_callee[4];
extern dtmf_state_t       g_dtmf_state;
extern uint8_t            g_dtmf_decode_ring_tick_500ms;
extern uint8_t            g_dtmf_chosen_contact;
extern uint8_t            g_dtmf_auto_reset_time_500ms;
extern dtmf_call_state_t  g_dtmf_call_state;
extern dtmf_reply_state_t g_dtmf_reply_state;
extern dtmf_call_mode_t   g_dtmf_call_mode;
extern bool               g_dtmf_is_tx;
extern uint8_t            g_dtmf_tx_stop_tick_500ms;

void DTMF_clear_RX(void);
bool DTMF_ValidateCodes(char *pCode, const unsigned int size);
bool DTMF_GetContact(const int Index, char *pContact);
bool DTMF_FindContact(const char *pContact, char *pResult);
char DTMF_GetCharacter(const unsigned int code);
bool DTMF_CompareMessage(const char *pDTMF, const char *pTemplate, const unsigned int size, const bool flag);
dtmf_call_mode_t DTMF_CheckGroupCall(const char *pDTMF, const unsigned int size);
void DTMF_clear_input_box(void);
void DTMF_Append(const char vode);
void DTMF_HandleRequest(void);
bool DTMF_Reply(void);

#endif
