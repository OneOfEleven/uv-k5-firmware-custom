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

#ifndef APP_APP_H
#define APP_APP_H

#include <stdbool.h>

#include "functions.h"
#include "frequencies.h"
#include "radio.h"

extern const uint8_t orig_lnas;
extern const uint8_t orig_lna;
extern const uint8_t orig_mixer;
extern const uint8_t orig_pga;

void     APP_end_tx(void);
void     APP_stop_scan(void);
void     APP_channel_next(const bool remember_current, const scan_state_dir_t scan_direction);
bool     APP_start_listening(void);
uint32_t APP_set_frequency_by_step(vfo_info_t *pInfo, int8_t Step);
void     APP_time_slice_10ms(void);
void     APP_time_slice_500ms(void);

#endif

