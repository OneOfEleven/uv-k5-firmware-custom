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

#ifndef UI_UI_H
#define UI_UI_H

#include <stdbool.h>
#include <stdint.h>

void UI_GenerateChannelString(char *pString, const uint8_t Channel);
void UI_GenerateChannelStringEx(char *pString, const bool bShowPrefix, const uint8_t ChannelNumber);
void UI_PrintString(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t Width);
void UI_PrintStringSmall(const char *pString, uint8_t Start, uint8_t End, uint8_t Line);
#ifdef ENABLE_SMALL_BOLD
	void UI_PrintStringSmallBold(const char *pString, uint8_t Start, uint8_t End, uint8_t Line);
#endif
void UI_PrintStringSmallBuffer(const char *pString, uint8_t *buffer);
void UI_DisplayFrequency(const char *pDigits, uint8_t X, uint8_t Y, bool bDisplayLeadingZero, bool flag);
void UI_DisplayFrequencySmall(const char *pDigits, uint8_t X, uint8_t Y, bool bDisplayLeadingZero);
void UI_Displaysmall_digits(const uint8_t size, const char *str, const uint8_t x, const uint8_t y, const bool display_leading_zeros);

#endif

