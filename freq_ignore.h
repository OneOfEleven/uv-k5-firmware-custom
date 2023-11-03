/* Copyright 2023 One of Eleven
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

#ifndef FREQ_IGNORE_H
#define FREQ_IGNORE_H

#include <stdint.h>

#ifdef ENABLE_SCAN_IGNORE_LIST
	void FI_clear_freq_ignored(void);
	int  FI_freq_ignored(const uint32_t frequency);
	void FI_add_freq_ignored(const uint32_t frequency);
	void FI_sub_freq_ignored(const uint32_t frequency);
#endif

#endif

