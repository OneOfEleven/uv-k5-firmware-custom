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

#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "font.h"
#include "misc.h"
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

void UI_PrintString(const char *str, unsigned int x, const unsigned int end, const unsigned int line, const unsigned int width)
{
	const unsigned int length    = strlen(str);
	const unsigned int font_size = ARRAY_SIZE(g_font_big);
	uint8_t           *f_buf1    = g_frame_buffer[line + 0];
	uint8_t           *f_buf2    = g_frame_buffer[line + 1];
	unsigned int       i;

	if (end > x)
	{
		const int ofs = ((int)(end - x) - (length * width) - 1) / 2;
		if (ofs > 0 && (x + ofs) <= end)
			x += ofs;
	}

//	for (i = 0; i < length && (x + width) <= LCD_WIDTH; i++, x += width)
	for (i = 0; i < length; i++, x += width)
	{
		const int c = (int)str[i] - ' ';
		if (c >= 0 && c < (int)font_size)
		{
			memcpy(f_buf1 + x, &g_font_big[c][0], 8);
			memcpy(f_buf2 + x, &g_font_big[c][8], 7);
		}
	}
}

static void UI_print_string(
	const char        *str,
	unsigned int       x,
	const unsigned int end,
	const unsigned int line,
	const uint8_t     *font,
	const unsigned int font_size,
	const unsigned int char_width)
{
	const unsigned int char_pitch = char_width + 1;  // char width + 1 pixel space between chars
	const size_t       length     = strlen(str);
	uint8_t           *f_buf      = g_frame_buffer[line];
	unsigned int       i;

	if (end > x)
	{
		const int ofs = ((int)(end - x) - (length * char_pitch) - 1) / 2;
		if (ofs > 0 && (x + ofs) <= end)
			x += ofs;
	}

	for (i = 0; i < length && (x + char_width) <= LCD_WIDTH; i++, x += char_pitch)
//	for (i = 0; i < length; i++, x += char_pitch)
	{
		const int c = (int)str[i] - ' ';
		if (c >= 0 && c < (int)font_size)
			memcpy(f_buf + x, font + (char_width * c), char_width);
	}
}

void UI_PrintStringSmall(const char *str, const unsigned int start, const unsigned int end, const unsigned int line)
{
	UI_print_string(str, start, end, line, (const uint8_t *)g_font_small, ARRAY_SIZE(g_font_small), ARRAY_SIZE(g_font_small[0]));
}

#ifdef ENABLE_SMALL_BOLD
	void UI_PrintStringSmallBold(const char *str, const unsigned int start, const unsigned int end, const unsigned int line)
	{
		UI_print_string(str, start, end, line, (const uint8_t *)g_font_small_bold, ARRAY_SIZE(g_font_small_bold), ARRAY_SIZE(g_font_small_bold[0]));
	}
#endif

#ifdef ENABLE_SMALLEST_FONT

void PutPixel(const unsigned int x, const unsigned int y, const bool fill)
{
	if (fill)
		g_frame_buffer[y >> 3][x] |=   1u << (y & 7u);
	else
		g_frame_buffer[y >> 3][x] &= ~(1u << (y & 7u));
}

void PutPixelStatus(const unsigned int x, const unsigned int y, bool fill)
{
	if (fill)
		g_status_line[x] |=   1u << y;
	else
		g_status_line[x] &= ~(1u << y);
}

void UI_PrintStringSmallest(const void *pString, unsigned int x, const unsigned int y, const bool statusbar, const bool fill)
{
	const unsigned int char_width  = ARRAY_SIZE(g_font3x5[0]);
	const unsigned int char_height = 5;
//	const uint8_t      pixel_mask  = (1u << char_height) - 1;
	const uint8_t *p = (const uint8_t *)pString;
	int c;

	while ((c = *p++) != 0)
	{
		c -= ' ';
		if (c >= 0 && c < (int)ARRAY_SIZE(g_font3x5))
		{
			for (unsigned int xx = 0; xx < char_width; xx++)
			{
				uint8_t pixels = g_font3x5[c][xx];
				if (statusbar)
				{
					for (unsigned int yy = 0; yy <= char_height; yy++, pixels >>= 1)
						if (pixels & 1u)
							PutPixelStatus(x + xx, y + yy, fill);
				}
				else
				{
					for (unsigned int yy = 0; yy <= char_height; yy++, pixels >>= 1)
						if (pixels & 1u)
							PutPixel(x + xx, y + yy, fill);
				}
			}
		}
		x += char_width + 1;
	}
}

#endif

void UI_PrintStringSmallBuffer(const char *pString, uint8_t *buffer)
{
	const unsigned int char_width   = ARRAY_SIZE(g_font_small[0]);
	const unsigned int char_spacing = char_width + 1;
	unsigned int       i;
	for (i = 0; i < strlen(pString); i++)
	{
		const int c = (int)pString[i] - ' ';
		if (c >= 0 && c < (int)ARRAY_SIZE(g_font_small))
			memcpy(buffer + (i * char_spacing) + 1, &g_font_small[c], char_width);
	}
}

void UI_DisplayFrequencyBig(const char *pDigits, uint8_t X, uint8_t Y, bool bDisplayLeadingZero, bool flag, unsigned int length)
{
	const unsigned int char_width  = 13;
	uint8_t           *pFb0        = g_frame_buffer[Y] + X;
	uint8_t           *pFb1        = pFb0 + LCD_WIDTH;
	bool               bCanDisplay = false;
	unsigned int       i           = 0;

	// MHz
	while (i < 3)
	{
		const unsigned int Digit = pDigits[i++];
		if (bDisplayLeadingZero || bCanDisplay || Digit > 0)
		{
			bCanDisplay = true;
			memcpy(pFb0, g_font_big_digits[Digit],              char_width);
			memcpy(pFb1, g_font_big_digits[Digit] + char_width, char_width);
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

#ifdef ENABLE_TRIM_TRAILING_ZEROS
	if (length == 6)
	{
		if (pDigits[length + 1] == 0 && pDigits[length + 2] == 0)
		{
			if (pDigits[length - 1] == 0)
			{
				length--;
				if (pDigits[length - 1] == 0)
					length--;
			}
		}
	}
#endif

	// fractions
	while (i < length)
	{
		const unsigned int Digit = pDigits[i++];
		memcpy(pFb0, g_font_big_digits[Digit],              char_width);
		memcpy(pFb1, g_font_big_digits[Digit] + char_width, char_width);
		pFb0 += char_width;
		pFb1 += char_width;
	}
}

void UI_DisplayFrequency(const char *pDigits, uint8_t X, uint8_t Y, bool bDisplayLeadingZero, unsigned int length)
{
	char         str[10];
	bool         bCanDisplay = false;
	unsigned int i           = 0;
	unsigned int k           = 0;

	// MHz
	while (i < 3)
	{
		const unsigned int Digit = pDigits[i++];
		if (bDisplayLeadingZero || bCanDisplay || Digit > 0)
		{
			bCanDisplay = true;
			str[k++] = (Digit < 10) ? '0' + Digit : '_';
		}
	}

	// decimal point
	str[k++] = '.';

	// fractions
	while (i < length)
	{
		const unsigned int Digit = pDigits[i++];
		str[k++] = (Digit < 10) ? '0' + Digit : '_';
	}

	str[k] = '\0';

#ifdef ENABLE_TRIM_TRAILING_ZEROS
//	NUMBER_trim_trailing_zeros(str);
#endif

	UI_PrintString(str, X, 0, Y, 8);
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
				memcpy(pFb + 1, g_font_small_digits[c], char_width);
			#else
				const unsigned int index = (c < 10) ? '0' - 32 + c : '-' - 32;
				memcpy(pFb + 1, g_font_small[index], char_width);
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
			memcpy(pFb + 1, g_font_small_digits[c], char_width);
		#else
			const unsigned int index = (c < 10) ? '0' - 32 + c : '-' - 32;
			memcpy(pFb + 1, g_font_small[index], char_width);
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
				memcpy(g_frame_buffer[y] + xx, g_font_small_digits[c], char_width);
			#else
				const unsigned int index = (c < 10) ? '0' - 32 + c : '-' - 32;
				memcpy(g_frame_buffer[y] + xx + 1, g_font_small[index], char_width);
			#endif
			xx += spacing;
		}
	}
}
