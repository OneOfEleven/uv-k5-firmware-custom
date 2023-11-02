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

#ifndef DRIVER_BK1080_H
#define DRIVER_BK1080_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/bk1080-regs.h"

extern uint16_t BK1080_freq_lower;
extern uint16_t BK1080_freq_upper;
extern uint16_t BK1080_freq_base;
extern int16_t  BK1080_freq_offset;

void     BK1080_Init(const uint16_t frequency, const bool initialise);
uint16_t BK1080_ReadRegister(BK1080_register_t Register);
void     BK1080_WriteRegister(BK1080_register_t Register, uint16_t Value);
void     BK1080_Mute(const bool Mute);
void     BK1080_SetFrequency(uint16_t Frequency);
int16_t  BK1080_get_freq_offset(const uint16_t Frequency);

#endif

