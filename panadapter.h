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

#ifndef PANADAPTER_H
#define PANADAPTER_H

#include <stdint.h>
#include <stdbool.h>

#include "driver/st7565.h"

// number of bins either side of the VFO RX frequency
#define PANADAPTER_BINS   ((LCD_WIDTH / 2) - 1)

#define PANADAPTER_MAX_STEP    2500
#define PANADAPTER_MIN_STEP    625

extern bool     g_panadapter_enabled;
extern uint32_t g_panadapter_peak_freq;
extern int      g_panadapter_vfo_mode;
extern uint8_t  g_panadapter_rssi[PANADAPTER_BINS + 1 + PANADAPTER_BINS];
extern uint8_t  g_panadapter_max_rssi;
extern uint8_t  g_panadapter_min_rssi;

bool PAN_scanning(void);
void PAN_process_10ms(void);

#endif

