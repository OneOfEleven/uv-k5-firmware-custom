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

#include <string.h>

#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "font.h"
#include "ui/helper.h"
#include "ui/inputbox.h"

#ifndef ARRAY_SIZE
	#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))
#endif

void UI_GenerateChannelString(char *pString, const uint8_t Channel, const char separating_char)
{
	unsigned int i;

	if (pString == NULL)
		return;
	
	if (g_input_box_index == 0)
	{
		sprintf(pString, "CH%c%02u", separating_char, Channel + 1);
		return;
	}

	pString[0] = 'C';
	pString[1] = 'H';
	pString[2] = separating_char;
	for (i = 0; i < 2; i++)
		pString[i + 3] = (g_input_box[i] == 10) ? '_' : g_input_box[i] + '0';
}

void UI_GenerateChannelStringEx(char *pString, const char *prefix, const uint8_t ChannelNumber)
{
	if (pString == NULL)
		return;
	
	if (g_input_box_index > 0)
	{
		unsigned int i;
		for (i = 0; i < 3; i++)
			pString[i] = (g_input_box[i] == 10) ? '-' : g_input_box[i] + '0';
		return;
	}

	pString[0] = 0;
	
	if (prefix)
		strcpy(pString, prefix);
	
	if (ChannelNumber == 0xFF)
		strcpy(pString, "NULL");
	else
		sprintf(pString + strlen(prefix), "%03u", ChannelNumber + 1);
}

void UI_PrintString(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t Width)
{
	size_t i;
	size_t Length = strlen(pString);

	if (End > Start)
		Start += (((End - Start) - (Length * Width)) + 1) / 2;

	for (i = 0; i < Length; i++)
	{
		if (pString[i] >= ' ' && pString[i] < 127)
		{
			const unsigned int index = pString[i] - ' ';
			const unsigned int ofs   = (unsigned int)Start + (i * Width);
			memmove(g_frame_buffer[Line + 0] + ofs, &g_font_big[index][0], 8);
			memmove(g_frame_buffer[Line + 1] + ofs, &g_font_big[index][8], 7);
		}
	}
}

void UI_PrintStringSmall(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
	const size_t Length = strlen(pString);
	size_t       i;

	if (End > Start)
		Start += (((End - Start) - (Length * 7)) + 1) / 2;

	const unsigned int char_width   = ARRAY_SIZE(g_font_small[0]);
	const unsigned int char_spacing = char_width + 1;
	uint8_t            *pFb         = g_frame_buffer[Line] + Start;
	for (i = 0; i < Length; i++)
	{
		if (pString[i] >= 32)
		{
			const unsigned int index = (unsigned int)pString[i] - 32;
			if (index < ARRAY_SIZE(g_font_small))
				memmove(pFb + (i * char_spacing) + 1, &g_font_small[index], char_width);
		}
	}
}

#ifdef ENABLE_SMALL_BOLD
	void UI_PrintStringSmallBold(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
	{
		const size_t Length = strlen(pString);
		size_t       i;
	
		if (End > Start)
			Start += (((End - Start) - (Length * 7)) + 1) / 2;
	
		const unsigned int char_width   = ARRAY_SIZE(g_font_small_bold[0]);
		const unsigned int char_spacing = char_width + 1;
		uint8_t            *pFb         = g_frame_buffer[Line] + Start;
		for (i = 0; i < Length; i++)
		{
			if (pString[i] >= 32)
			{
				const unsigned int index = (unsigned int)pString[i] - 32;
				if (index < ARRAY_SIZE(g_font_small_bold))
					memmove(pFb + (i * char_spacing) + 1, &g_font_small_bold[index], char_width);
			}
		}
	}
#endif

void UI_PrintStringSmallBuffer(const char *pString, uint8_t *buffer)
{
	size_t i;
	const unsigned int char_width   = ARRAY_SIZE(g_font_small[0]);
	const unsigned int char_spacing = char_width + 1;
	for (i = 0; i < strlen(pString); i++)
	{
		if (pString[i] >= 32)
		{
			const unsigned int index = (unsigned int)pString[i] - 32;
			if (index < ARRAY_SIZE(g_font_small))
				memmove(buffer + (i * char_spacing) + 1, &g_font_small[index], char_width);
		}
	}
}

void UI_DisplayFrequency(const char *pDigits, uint8_t X, uint8_t Y, bool bDisplayLeadingZero, bool flag)
{
	const unsigned int char_width  = 13;
	uint8_t           *pFb0        = g_frame_buffer[Y] + X;
	uint8_t           *pFb1        = pFb0 + 128;
	bool               bCanDisplay = false;
	unsigned int       i           = 0;
	
	// MHz
	while (i < 3)
	{
		const unsigned int Digit = pDigits[i++];
		if (bDisplayLeadingZero || bCanDisplay || Digit > 0)
		{
			bCanDisplay = true;
			memmove(pFb0, g_font_big_digits[Digit],              char_width);
			memmove(pFb1, g_font_big_digits[Digit] + char_width, char_width);
		}
		else
		if (flag)
		{
			pFb0 -= 6;
			pFb1 -= 6;
		}
		pFb0 += char_width;
		pFb1 += char_width;
	}

	// decimal point
	*pFb1 = 0x60; pFb0++; pFb1++;
	*pFb1 = 0x60; pFb0++; pFb1++;
	*pFb1 = 0x60; pFb0++; pFb1++;
	
	// kHz
	while (i < 6)
	{
		const unsigned int Digit = pDigits[i++];
		memmove(pFb0, g_font_big_digits[Digit],              char_width);
		memmove(pFb1, g_font_big_digits[Digit] + char_width, char_width);
		pFb0 += char_width;
		pFb1 += char_width;
	}
}

void UI_DisplayFrequencySmall(const char *pDigits, uint8_t X, uint8_t Y, bool bDisplayLeadingZero)
{
	const unsigned int char_width  = ARRAY_SIZE(g_font_small[0]);
	const unsigned int spacing     = 1 + char_width;
	uint8_t           *pFb         = g_frame_buffer[Y] + X;
	bool               bCanDisplay = false;
	unsigned int       i           = 0;

	// MHz
	while (i < 3)
	{
		const unsigned int c = pDigits[i++];
		if (bDisplayLeadingZero || bCanDisplay || c > 0)
		{
			#if 0
				memmove(pFb + 1, g_font_small_digits[c], char_width);
			#else
				const unsigned int index = (c < 10) ? '0' - 32 + c : '-' - 32;
				memmove(pFb + 1, g_font_small[index], char_width);
			#endif
			pFb += spacing;
			bCanDisplay = true;
		}
	}

	// decimal point
	pFb++;
	pFb++;
	*pFb++ = 0x60;
	*pFb++ = 0x60;
	pFb++;

	// kHz
	while (i < 8)
	{
		const unsigned int c = pDigits[i++];
		#if 0
			memmove(pFb + 1, g_font_small_digits[c], char_width);
		#else
			const unsigned int index = (c < 10) ? '0' - 32 + c : '-' - 32;
			memmove(pFb + 1, g_font_small[index], char_width);
		#endif
		pFb += spacing;
	}
}

void UI_Displaysmall_digits(const uint8_t size, const char *str, const uint8_t x, const uint8_t y, const bool display_leading_zeros)
{
	const unsigned int char_width  = ARRAY_SIZE(g_font_small[0]);
	const unsigned int spacing     = 1 + char_width;
	bool               display     = display_leading_zeros;
	unsigned int       xx;
	unsigned int       i;
	for (i = 0, xx = x; i < size; i++)
	{
		const unsigned int c = (unsigned int)str[i];
		if (c > 0)
			display = true;    // non '0'
		if (display && c < 11)
		{
			#if 0
				memmove(g_frame_buffer[y] + xx, g_font_small_digits[c], char_width);
			#else
				const unsigned int index = (c < 10) ? '0' - 32 + c : '-' - 32;
				memmove(g_frame_buffer[y] + xx + 1, g_font_small[index], char_width);
			#endif
			xx += spacing;
		}
	}
}
