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

#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>
#include <stdint.h>

extern uint16_t          g_battery_calibration[6];
extern uint16_t          g_usb_current_voltage;
extern uint16_t          g_usb_current;
extern uint16_t          g_battery_voltages[4];
extern uint16_t          g_battery_voltage_average;
extern uint8_t           g_battery_display_level;
extern bool              g_charging_with_type_c;
extern bool              g_low_battery;
extern bool              g_low_battery_blink;
extern uint16_t          g_battery_check_counter;
extern volatile uint16_t g_power_save_tick_10ms;

unsigned int BATTERY_VoltsToPercent(const unsigned int voltage_10mV);
void         BATTERY_GetReadings(const bool bDisplayBatteryLevel);

#endif
