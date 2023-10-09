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

#include "ui/inputbox.h"

char    g_input_box[8];
uint8_t g_input_box_index;

void INPUTBOX_Append(const key_code_t Digit)
{
	if (g_input_box_index >= sizeof(g_input_box))
		return;

	if (g_input_box_index == 0)
		memset(g_input_box, 10, sizeof(g_input_box));

	if (Digit >= KEY_0 && Digit != KEY_INVALID)
		g_input_box[g_input_box_index++] = (char)(Digit - KEY_0);
}

