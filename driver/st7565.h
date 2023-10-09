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

#ifndef DRIVER_ST7565_H
#define DRIVER_ST7565_H

#include <stdbool.h>
#include <stdint.h>

#define LCD_WIDTH       128
#define LCD_HEIGHT       64

extern uint8_t g_status_line[128];
extern uint8_t g_frame_buffer[7][128];

void ST7565_DrawLine(const unsigned int Column, const unsigned int Line, const unsigned int Size, const uint8_t *pBitmap);
void ST7565_BlitFullScreen(void);
void ST7565_BlitStatusLine(void);
void ST7565_FillScreen(uint8_t Value);
void ST7565_Init(const bool full);
void ST7565_HardwareReset(void);
void ST7565_SelectColumnAndLine(uint8_t Column, uint8_t Line);
void ST7565_WriteByte(uint8_t Value);

#endif

