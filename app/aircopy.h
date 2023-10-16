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

extern const unsigned int g_aircopy_block_max;
extern unsigned int       g_aircopy_block_number;
extern uint8_t            g_aircopy_rx_errors_fsk_crc;
extern uint8_t            g_aircopy_rx_errors_magic;
extern uint8_t            g_aircopy_rx_errors_crc;
extern aircopy_state_t    g_aircopy_state;
extern uint16_t           g_fsk_buffer[36];
extern unsigned int       g_fsk_write_index;
extern uint16_t           g_fsk_tx_timeout_10ms;

void AIRCOPY_init(void);
void AIRCOPY_process_fsk_tx_10ms(void);
void AIRCOPY_process_fsk_rx_10ms(void);
void AIRCOPY_process_key(key_code_t key, bool key_pressed, bool key_held);

#endif
