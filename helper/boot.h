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

#ifndef HELPER_BOOT_H
#define HELPER_BOOT_H

#include <stdint.h>
#include "driver/keyboard.h"

enum boot_mode_e
{
	BOOT_MODE_NORMAL = 0,    // normal boot
	BOOT_MODE_UNHIDE_HIDDEN,        // unhide the hidden menu items
	#ifdef ENABLE_AIRCOPY
		BOOT_MODE_AIRCOPY    // do an air-copy
	#endif
};
typedef enum boot_mode_e boot_mode_t;

boot_mode_t BOOT_GetMode(void);
void BOOT_ProcessMode(boot_mode_t Mode);

#endif

