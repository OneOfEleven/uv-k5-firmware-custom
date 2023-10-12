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

#ifdef ENABLE_AIRCOPY

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

extern aircopy_state_t g_aircopy_state;
extern uint16_t        g_air_copy_block_number;
extern uint16_t        g_errors_during_air_copy;
extern uint16_t        g_fsk_buffer[36];

void AIRCOPY_SendMessage(void);
void AIRCOPY_StorePacket(void);
void AIRCOPY_ProcessKeys(key_code_t Key, bool bKeyPressed, bool bKeyHeld);

#endif

#endif

