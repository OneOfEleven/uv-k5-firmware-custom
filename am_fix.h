
/* Copyright 2023 OneOfEleven
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

#ifndef AM_FIXH

#include <stdint.h>
#include <stdbool.h>

extern const uint8_t orig_lna_short;
extern const uint8_t orig_lna;
extern const uint8_t orig_mixer;
extern const uint8_t orig_pga;

#ifdef ENABLE_AM_FIX
	extern int16_t rssi_db_gain_diff[2];

	void AM_fix_init(void);
	void AM_fix_reset(const int vfo);
	void AM_fix_adjust_frontEnd_10ms(const int vfo);
	#ifdef ENABLE_AM_FIX_SHOW_DATA
		void AM_fix_print_data(const int vfo, char *s);
	#endif
		
#endif

#endif
