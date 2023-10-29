/* Copyright 2023 Manuel Jinger
 * Copyright 2023 Dual Tachyon
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

#ifndef DRIVER_KEYBOARD_H
#define DRIVER_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

enum key_code_e {
	KEY_0 = 0,  // DTMF 0
	KEY_1,      // DTMF 1
	KEY_2,      // DTMF 2
	KEY_3,      // DTMF 3
	KEY_4,      // DTMF 4
	KEY_5,      // DTMF 5
	KEY_6,      // DTMF 6
	KEY_7,      // DTMF 7
	KEY_8,      // DTMF 8
	KEY_9,      // DTMF 9
	KEY_MENU,   // DTMF A
	KEY_UP,     // DTMF B
	KEY_DOWN,   // DTMF C
	KEY_EXIT,   // DTMF D
	KEY_STAR,   // DTMF *
	KEY_F,      // DTMF #
	KEY_PTT,    //
	KEY_SIDE2,  //
	KEY_SIDE1,  //
	KEY_INVALID //
};
typedef enum key_code_e key_code_t;

extern uint8_t    g_ptt_debounce;
extern uint8_t    g_key_debounce_press;
extern uint8_t    g_key_debounce_repeat;
extern key_code_t g_key_prev;
extern key_code_t g_key_pressed;
extern bool       g_key_held;
extern bool       g_fkey_pressed;
extern bool       g_ptt_is_pressed;

extern bool       g_ptt_was_released;
extern bool       g_ptt_was_pressed;
extern uint8_t    g_keypad_locked;

key_code_t KEYBOARD_Poll(void);

#endif

