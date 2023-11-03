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

#include <string.h>     // NULL and memcmp

#include "driver/eeprom.h"
#include "driver/i2c.h"
#include "driver/system.h"

void EEPROM_ReadBuffer(const uint16_t address, void *p_buffer, const unsigned int size)
{
	if ((address + size) > 0x2000 || size == 0)
		return;

	I2C_Start();
	I2C_Write(0xA0);
	I2C_Write((address >> 8) & 0xFF);
	I2C_Write((address >> 0) & 0xFF);

	I2C_Start();
	I2C_Write(0xA1);
//	I2C_ReadBuffer(p_buffer, size, false);
	I2C_ReadBuffer(p_buffer, size, true);   // faster read
	I2C_Stop();
}

void EEPROM_WriteBuffer8(const uint16_t address, const void *p_buffer)
{
	if (p_buffer == NULL || (address + 8) > 0x2000)
		return;

#if 0
	// normal way

	I2C_Start();
	I2C_Write(0xA0);
	I2C_Write((address >> 8) & 0xFF);
	I2C_Write((address >> 0) & 0xFF);
	I2C_WriteBuffer(p_buffer, 8);
	I2C_Stop();

	// give the EEPROM time to burn the data in (apparently takes 1.5ms ~ 5ms)
	SYSTEM_DelayMs(6);

#else
	// eeprom wear reduction
	// only write the data if it's different to what's already there

	uint8_t buffer[8];

	EEPROM_ReadBuffer(address, buffer, 8);

	if (memcmp(p_buffer, buffer, 8) != 0)
	{
		I2C_Start();
		I2C_Write(0xA0);
		I2C_Write((address >> 8) & 0xFF);
		I2C_Write((address >> 0) & 0xFF);
		I2C_WriteBuffer(p_buffer, 8);
		I2C_Stop();

		// give the EEPROM time to burn the data in (apparently takes 1.5ms ~ 5ms)
		SYSTEM_DelayMs(6);
	}

#endif
}
