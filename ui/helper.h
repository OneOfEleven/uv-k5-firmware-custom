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

void UI_GenerateChannelString(char *pString, const uint8_t Channel, const char separating_char);
void UI_GenerateChannelStringEx(char *pString, const char *prefix, const uint8_t ChannelNumber);
void UI_PrintString(const char *str, unsigned int start, const unsigned int end, const unsigned int line, const unsigned int width);
void UI_PrintStringSmall(const char *str, const unsigned int start, const unsigned int end, const unsigned int line);
#ifdef ENABLE_SMALL_BOLD
	void UI_PrintStringSmallBold(const char *str, const unsigned int start, const unsigned int end, const unsigned int line);
#endif
#ifdef ENABLE_SMALLEST_FONT
	void UI_PrintStringSmallest(const void *pString, unsigned int x, const unsigned int y, const bool statusbar, const bool fill);
#endif
void UI_PrintStringSmallBuffer(const char *pString, uint8_t *buffer);
void UI_DisplayFrequencyBig(const char *pDigits, uint8_t X, uint8_t Y, bool bDisplayLeadingZero, bool flag, unsigned int length);
void UI_DisplayFrequency(const char *pDigits, uint8_t X, uint8_t Y, bool bDisplayLeadingZero, unsigned int length);
void UI_DisplayFrequencySmall(const char *pDigits, uint8_t X, uint8_t Y, bool bDisplayLeadingZero);
void UI_Displaysmall_digits(const uint8_t size, const char *str, const uint8_t x, const uint8_t y, const bool display_leading_zeros);

#endif

