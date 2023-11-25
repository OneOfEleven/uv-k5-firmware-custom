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

#include "bsp/dp32g030/gpio.h"
#include "bsp/dp32g030/portcon.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/systick.h"

void I2C_Start(void)
{
	GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
	SYSTICK_Delay250ns(4);
	GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
	SYSTICK_Delay250ns(4);
	GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
	SYSTICK_Delay250ns(4);
	GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
	SYSTICK_Delay250ns(4);
}

void I2C_Stop(void)
{
	GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
	SYSTICK_Delay250ns(4);
	GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
	SYSTICK_Delay250ns(4);
	GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
	SYSTICK_Delay250ns(4);
	GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
	SYSTICK_Delay250ns(4);
}

uint8_t I2C_Read(const bool end, const bool fast)
{
	const unsigned int delay = fast ? 2 : 4;
	int                i     = 8;
	uint8_t            data  = 0;

	PORTCON_PORTA_IE |= PORTCON_PORTA_IE_A11_BITS_ENABLE;
	PORTCON_PORTA_OD &= ~PORTCON_PORTA_OD_A11_MASK;
	GPIOA->DIR &= ~GPIO_DIR_11_MASK;

	while (--i >= 0)
	{
		GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
		SYSTICK_Delay250ns(delay);
		GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
		SYSTICK_Delay250ns(delay);
		data = (data << 1) | (GPIO_CheckBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA) ? 1u : 0u);
		GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
		SYSTICK_Delay250ns(delay);
	}

	PORTCON_PORTA_IE &= ~PORTCON_PORTA_IE_A11_MASK;
	PORTCON_PORTA_OD |= PORTCON_PORTA_OD_A11_BITS_ENABLE;
	GPIOA->DIR |= GPIO_DIR_11_BITS_OUTPUT;
	GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
	SYSTICK_Delay250ns(delay);
	if (end)
		GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
	else
		GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
	SYSTICK_Delay250ns(delay);
	GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
	SYSTICK_Delay250ns(delay);
	GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
	SYSTICK_Delay250ns(delay);

	return data;
}

int I2C_Write(uint8_t data) //, const bool fast)
{
	const unsigned int delay = 4; // fast ? 2 : 4;
	int i   = 8;
	int ret = -1;

	GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
	SYSTICK_Delay250ns(delay);
	while (--i >= 0)
	{
		if ((data & 0x80) == 0)
			GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
		else
			GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
		data <<= 1;
		SYSTICK_Delay250ns(delay);
		GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
		SYSTICK_Delay250ns(delay);
		GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
		SYSTICK_Delay250ns(delay);
	}

	PORTCON_PORTA_IE |= PORTCON_PORTA_IE_A11_BITS_ENABLE;
	PORTCON_PORTA_OD &= ~PORTCON_PORTA_OD_A11_MASK;
	GPIOA->DIR &= ~GPIO_DIR_11_MASK;
	GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
	SYSTICK_Delay250ns(delay);
	GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
	SYSTICK_Delay250ns(delay);

	for (i = 0; i < 255; i++)
	{
		if (GPIO_CheckBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA) == 0)
		{
			ret = 0;
			break;
		}
	}

	GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
	SYSTICK_Delay250ns(delay);
	PORTCON_PORTA_IE &= ~PORTCON_PORTA_IE_A11_MASK;
	PORTCON_PORTA_OD |= PORTCON_PORTA_OD_A11_BITS_ENABLE;
	GPIOA->DIR |= GPIO_DIR_11_BITS_OUTPUT;
	GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);

	return ret;
}

int I2C_ReadBuffer(void *pBuffer, const unsigned int Size, const bool fast)
{
	uint8_t *pData = (uint8_t *)pBuffer;
	unsigned int i;

	if (Size == 1)
	{
		*pData = I2C_Read(true, fast);
		return 1;
	}

	for (i = 0; i < (Size - 1); i++)
		pData[i] = I2C_Read(false, fast);
	pData[i++] = I2C_Read(true, fast);

	return Size;
}

int I2C_WriteBuffer(const void *pBuffer, const unsigned int Size)
{
	const uint8_t *pData = (const uint8_t *)pBuffer;
	unsigned int i;

	for (i = 0; i < Size; i++)
		if (I2C_Write(*pData++) < 0)
			return -1;

	return 0;
}

