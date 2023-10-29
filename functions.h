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

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <stdint.h>

enum function_type_e
{
	FUNCTION_FOREGROUND = 0,  // idle, scanning
	FUNCTION_TRANSMIT,        // transmitting
//	FUNCTION_MONITOR,         // receiving with squelch forced open
	FUNCTION_NEW_RECEIVE,     // signal just received
	FUNCTION_RECEIVE,         // receive mode
	FUNCTION_POWER_SAVE,      // sleeping
	FUNCTION_PANADAPTER       // bandscope mode (panadpter/spectrum) .. not yet implemented
};
typedef enum function_type_e function_type_t;

extern function_type_t g_current_function;

void FUNCTION_Init(void);
void FUNCTION_Select(function_type_t Function);

#endif

