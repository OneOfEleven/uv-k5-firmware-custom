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

#ifndef DRIVER_BACKLIGHT_H
#define DRIVER_BACKLIGHT_H

#include <stdint.h>
#include <stdbool.h>

#define BACKLIGHT_MAX_BRIGHTNESS  100

extern uint16_t g_backlight_tick_10ms;
extern bool     g_backlight_on;

void     BACKLIGHT_init(void);
uint16_t BACKLIGHT_ticks(void);
void     BACKLIGHT_set_brightness(unsigned int brightness);
void     BACKLIGHT_turn_on(const uint16_t min_ticks);
void     BACKLIGHT_turn_off(void);

#endif

