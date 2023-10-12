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

#ifndef APP_AIRCOPY_H
#define APP_AIRCOPY_H

#include "driver/keyboard.h"

enum aircopy_state_e
{
	AIRCOPY_READY = 0,
	AIRCOPY_RX,
	AIRCOPY_TX,
	AIRCOPY_RX_COMPLETE,
	AIRCOPY_TX_COMPLETE
};
typedef enum aircopy_state_e aircopy_state_t;

extern const uint8_t    g_aircopy_block_max;
extern uint8_t          g_aircopy_block_number;
extern uint8_t          g_aircopy_rx_errors;
extern aircopy_state_t  g_aircopy_state;
extern uint16_t         g_aircopy_fsk_buffer[36];
extern uint8_t          g_aircopy_send_count_down_10ms;
extern unsigned int     g_aircopy_fsk_write_index;

void AIRCOPY_SendMessage(void);
void AIRCOPY_StorePacket(void);
void AIRCOPY_ProcessKeys(key_code_t key, bool key_pressed, bool key_held);

#endif
