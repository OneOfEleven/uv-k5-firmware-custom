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

#ifndef DCS_H
#define DCS_H

#include <stdint.h>

enum dcs_code_type_e
{
	CODE_TYPE_NONE = 0,
	CODE_TYPE_CONTINUOUS_TONE,
	CODE_TYPE_DIGITAL,
	CODE_TYPE_REVERSE_DIGITAL
};

typedef enum dcs_code_type_e dcs_code_type_t;

enum {
	CDCSS_POSITIVE_CODE = 1U,
	CDCSS_NEGATIVE_CODE = 2U,
};

extern const uint16_t CTCSS_TONE_LIST[50];
extern const uint16_t DCS_CODE_LIST[104];

uint32_t DCS_GetGolayCodeWord(dcs_code_type_t code_type, uint8_t Option);
uint8_t DCS_GetCdcssCode(uint32_t Code);
uint8_t DCS_GetCtcssCode(int Code);

#endif
