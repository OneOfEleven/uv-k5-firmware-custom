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

#include <stdio.h>   // NULL

#include "bk4819.h"
#include "bsp/dp32g030/gpio.h"
#include "bsp/dp32g030/portcon.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/systick.h"

#ifndef ARRAY_SIZE
	#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

static const uint16_t FSK_RogerTable[7] = {0xF1A2, 0x7446, 0x61A4, 0x6544, 0x4E8A, 0xE044, 0xEA84};

static uint16_t gBK4819_GpioOutState;

bool g_rx_idle_mode;

__inline uint16_t scale_freq(const uint16_t freq)
{
//	return (((uint32_t)freq * 1032444u) + 50000u) / 100000u;   // with rounding
	return (((uint32_t)freq * 1353245u) + (1u << 16)) >> 17;   // with rounding
}

void BK4819_Init(void)
{
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);

	BK4819_WriteRegister(BK4819_REG_00, 0x8000);
	BK4819_WriteRegister(BK4819_REG_00, 0x0000);

	BK4819_WriteRegister(BK4819_REG_37, 0x1D0F);
	BK4819_WriteRegister(BK4819_REG_36, 0x0022);

	BK4819_SetAGC(0);
//	BK4819_SetAGC(1);

	BK4819_WriteRegister(BK4819_REG_19, 0x1041);  // 0001 0000 0100 0001 <15> MIC AGC  1 = disable  0 = enable

	BK4819_WriteRegister(BK4819_REG_7D, 0xE940);

	// REG_48 .. RX AF level
	//
	// <15:12> 11  ???  0 to 15
	//
	// <11:10> 0 AF Rx Gain-1
	//         0 =   0dB
	//         1 =  -6dB
	//         2 = -12dB
	//         3 = -18dB
	//
	// <9:4>   60 AF Rx Gain-2  -26dB ~ 5.5dB   0.5dB/step
	//         63 = max
	//          0 = mute
	//
	// <3:0>   15 AF DAC Gain (after Gain-1 and Gain-2) approx 2dB/step
	//         15 = max
	//          0 = min
	//
	BK4819_WriteRegister(BK4819_REG_48,	//  0xB3A8);     // 1011 00 111010 1000
		(11u << 12) |     // ??? 0..15
		( 0u << 10) |     // AF Rx Gain-1
		(58u <<  4) |     // AF Rx Gain-2
		( 8u <<  0));     // AF DAC Gain (after Gain-1 and Gain-2)

#if 1
	const uint8_t dtmf_coeffs[] = {111, 107, 103, 98, 80, 71, 58, 44, 65, 55, 37, 23, 228, 203, 181, 159};
	for (unsigned int i = 0; i < ARRAY_SIZE(dtmf_coeffs); i++)
		BK4819_WriteRegister(BK4819_REG_09, (i << 12) | dtmf_coeffs[i]);
#else
	// original code
	BK4819_WriteRegister(BK4819_REG_09, 0x006F);  // 6F
	BK4819_WriteRegister(BK4819_REG_09, 0x106B);  // 6B
	BK4819_WriteRegister(BK4819_REG_09, 0x2067);  // 67
	BK4819_WriteRegister(BK4819_REG_09, 0x3062);  // 62
	BK4819_WriteRegister(BK4819_REG_09, 0x4050);  // 50
	BK4819_WriteRegister(BK4819_REG_09, 0x5047);  // 47
	BK4819_WriteRegister(BK4819_REG_09, 0x603A);  // 3A
	BK4819_WriteRegister(BK4819_REG_09, 0x702C);  // 2C
	BK4819_WriteRegister(BK4819_REG_09, 0x8041);  // 41
	BK4819_WriteRegister(BK4819_REG_09, 0x9037);  // 37
	BK4819_WriteRegister(BK4819_REG_09, 0xA025);  // 25
	BK4819_WriteRegister(BK4819_REG_09, 0xB017);  // 17
	BK4819_WriteRegister(BK4819_REG_09, 0xC0E4);  // E4
	BK4819_WriteRegister(BK4819_REG_09, 0xD0CB);  // CB
	BK4819_WriteRegister(BK4819_REG_09, 0xE0B5);  // B5
	BK4819_WriteRegister(BK4819_REG_09, 0xF09F);  // 9F
#endif

	BK4819_WriteRegister(BK4819_REG_1F, 0x5454);
	BK4819_WriteRegister(BK4819_REG_3E, 0xA037);

	gBK4819_GpioOutState = 0x9000;

	BK4819_WriteRegister(BK4819_REG_33, 0x9000);
	BK4819_WriteRegister(BK4819_REG_3F, 0);
}

static uint16_t BK4819_ReadU16(void)
{
	unsigned int i;
	uint16_t     Value;

	PORTCON_PORTC_IE = (PORTCON_PORTC_IE & ~PORTCON_PORTC_IE_C2_MASK) | PORTCON_PORTC_IE_C2_BITS_ENABLE;
	GPIOC->DIR = (GPIOC->DIR & ~GPIO_DIR_2_MASK) | GPIO_DIR_2_BITS_INPUT;
	SYSTICK_DelayUs(1);

	Value = 0;
	for (i = 0; i < 16; i++)
	{
		Value <<= 1;
		Value |= GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1);
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1);
	}
	PORTCON_PORTC_IE = (PORTCON_PORTC_IE & ~PORTCON_PORTC_IE_C2_MASK) | PORTCON_PORTC_IE_C2_BITS_DISABLE;
	GPIOC->DIR = (GPIOC->DIR & ~GPIO_DIR_2_MASK) | GPIO_DIR_2_BITS_OUTPUT;

	return Value;
}

uint16_t BK4819_ReadRegister(BK4819_REGISTER_t Register)
{
	uint16_t Value;

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);

	SYSTICK_DelayUs(1);

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	BK4819_WriteU8(Register | 0x80);
	Value = BK4819_ReadU16();
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);

	SYSTICK_DelayUs(1);

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);

	return Value;
}

void BK4819_WriteRegister(BK4819_REGISTER_t Register, uint16_t Data)
{
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);

	SYSTICK_DelayUs(1);

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	BK4819_WriteU8(Register);

	SYSTICK_DelayUs(1);

	BK4819_WriteU16(Data);

	SYSTICK_DelayUs(1);

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);

	SYSTICK_DelayUs(1);

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
}

void BK4819_WriteU8(uint8_t Data)
{
	unsigned int i;

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	for (i = 0; i < 8; i++)
	{
		if ((Data & 0x80) == 0)
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
		else
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);

		SYSTICK_DelayUs(1);
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1);

		Data <<= 1;

		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1);
	}
}

void BK4819_WriteU16(uint16_t Data)
{
	unsigned int i;

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	for (i = 0; i < 16; i++)
	{
		if ((Data & 0x8000) == 0)
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
		else
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);

		SYSTICK_DelayUs(1);
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);

		Data <<= 1;

		SYSTICK_DelayUs(1);
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1);
	}
}

void BK4819_SetAGC(uint8_t Value)
{
	if (Value == 0)
	{
		// REG_10
		//
		// 0x0038 Rx AGC Gain Table[0]. (Index Max->Min is 3,2,1,0,-1)
		//
		// <15:10> ???
		//
		// <9:8>   LNA Gain Short
		//         3 =   0dB  <<<
		//         2 = -24dB       // was -11
		//         1 = -30dB       // was -16
		//         0 = -33dB       // was -19
		//
		// <7:5>   LNA Gain
		//         7 =   0dB
		//         6 =  -2dB
		//         5 =  -4dB
		//         4 =  -6dB
		//         3 =  -9dB
		//         2 = -14dB <<<
		//         1 = -19dB
		//         0 = -24dB
		//
		// <4:3>   MIXER Gain
		//         3 =   0dB <<<
		//         2 =  -3dB
		//         1 =  -6dB
		//         0 =  -8dB
		//
		// <2:0>   PGA Gain
		//         7 =   0dB
		//         6 =  -3dB <<<
		//         5 =  -6dB
		//         4 =  -9dB
		//         3 = -15dB
		//         2 = -21dB
		//         1 = -27dB
		//         0 = -33dB
		//
		BK4819_WriteRegister(BK4819_REG_13, (3u << 8) | (2u << 5) | (3u << 3) | (6u << 0));  // 000000 11 101 11 110

		BK4819_WriteRegister(BK4819_REG_12, 0x037B);  // 000000 11 011 11 011
		BK4819_WriteRegister(BK4819_REG_11, 0x027B);  // 000000 10 011 11 011
		BK4819_WriteRegister(BK4819_REG_10, 0x007A);  // 000000 00 011 11 010
		BK4819_WriteRegister(BK4819_REG_14, 0x0019);  // 000000 00 000 11 001

		BK4819_WriteRegister(BK4819_REG_49, 0x2A38);
		BK4819_WriteRegister(BK4819_REG_7B, 0x8420);
	}
	else
	if (Value == 1)
	{	// what does this do ???

		unsigned int i;

		// REG_10
		//
		// 0x0038 Rx AGC Gain Table[0]. (Index Max->Min is 3,2,1,0,-1)
		//
		// (15:10> ???
		//
		// <9:8>   LNA Gain Short
		//         3 =   0dB   << original
		//         2 = -24dB       // was -11
		//         1 = -30dB       // was -16
		//         0 = -33dB       // was -19
		//
		// <7:5>   LNA Gain
		//         7 =   0dB
		//         6 =  -2dB
		//         5 =  -4dB
		//         4 =  -6dB
		//         3 =  -9dB
		//         2 = -14dB   << original
		//         1 = -19dB
		//         0 = -24dB
		//
		// <4:3>   MIXER Gain
		//         3 =   0dB   << original
		//         2 =  -3dB
		//         1 =  -6dB
		//         0 =  -8dB
		//
		// <2:0>   PGA Gain
		//         7 =   0dB
		//         6 =  -3dB   << original
		//         5 =  -6dB
		//         4 =  -9dB
		//         3 = -15dB
		//         2 = -21dB
		//         1 = -27dB
		//         0 = -33dB
		//
		BK4819_WriteRegister(BK4819_REG_13, (3u << 8) | (2u << 5) | (3u << 3) | (6u << 0));

		BK4819_WriteRegister(BK4819_REG_12, 0x037C);  // 000000 11 011 11 100
		BK4819_WriteRegister(BK4819_REG_11, 0x027B);  // 000000 10 011 11 011
		BK4819_WriteRegister(BK4819_REG_10, 0x007A);  // 000000 00 011 11 010
		BK4819_WriteRegister(BK4819_REG_14, 0x0018);  // 000000 00 000 11 000

		BK4819_WriteRegister(BK4819_REG_49, 0x2A38);
		BK4819_WriteRegister(BK4819_REG_7B, 0x318C);

		BK4819_WriteRegister(BK4819_REG_7C, 0x595E);
		BK4819_WriteRegister(BK4819_REG_20, 0x8DEF);

		for (i = 0; i < 8; i++)
			// Bug? The bit 0x2000 below overwrites the (i << 13)
			BK4819_WriteRegister(BK4819_REG_06, ((i << 13) | 0x2500u) + 0x036u);
	}
}

void BK4819_set_GPIO_pin(BK4819_GPIO_PIN_t Pin, bool bSet)
{
	if (bSet)
		gBK4819_GpioOutState |=  (0x40u >> Pin);
	else
		gBK4819_GpioOutState &= ~(0x40u >> Pin);

	BK4819_WriteRegister(BK4819_REG_33, gBK4819_GpioOutState);
}

void BK4819_SetCDCSSCodeWord(uint32_t CodeWord)
{
	// REG_51
	//
	// <15>  0
	//       1 = Enable TxCTCSS/CDCSS
	//       0 = Disable
	//
	// <14>  0
	//       1 = GPIO0Input for CDCSS
	//       0 = Normal Mode (for BK4819 v3)
	//
	// <13>  0
	//       1 = Transmit negative CDCSS code
	//       0 = Transmit positive CDCSS code
	//
	// <12>  0 CTCSS/CDCSS mode selection
	//       1 = CTCSS
	//       0 = CDCSS
	//
	// <11>  0 CDCSS 24/23bit selection
	//       1 = 24bit
	//       0 = 23bit
	//
	// <10>  0 1050HzDetectionMode
	//       1 = 1050/4 Detect Enable, CTC1 should be set to 1050/4 Hz
	//
	// <9>   0 Auto CDCSS Bw Mode
	//       1 = Disable
	//       0 = Enable
	//
	// <8>   0 Auto CTCSS Bw Mode
	//       0 = Enable
	//       1 = Disable
	//
	// <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning
	//       0   = min
	//       127 = max

	// Enable CDCSS
	// Transmit positive CDCSS code
	// CDCSS Mode
	// CDCSS 23bit
	// Enable Auto CDCSS Bw Mode
	// Enable Auto CTCSS Bw Mode
	// CTCSS/CDCSS Tx Gain1 Tuning = 51
	//
	BK4819_WriteRegister(BK4819_REG_51,
		BK4819_REG_51_ENABLE_CxCSS         |
		BK4819_REG_51_GPIO6_PIN2_NORMAL    |
		BK4819_REG_51_TX_CDCSS_POSITIVE    |
		BK4819_REG_51_MODE_CDCSS           |
		BK4819_REG_51_CDCSS_23_BIT         |
		BK4819_REG_51_1050HZ_NO_DETECTION  |
		BK4819_REG_51_AUTO_CDCSS_BW_ENABLE |
		BK4819_REG_51_AUTO_CTCSS_BW_ENABLE |
		(51u << BK4819_REG_51_SHIFT_CxCSS_TX_GAIN1));

	// REG_07 <15:0>
	//
	// When <13> = 0 for CTC1
	// <12:0> = CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 1 for CTC2 (Tail 55Hz Rx detection)
	// <12:0> = CTC2 (should below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 2 for CDCSS 134.4Hz
	// <12:0> = CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC1 | 2775u);

	// REG_08 <15:0> <15> = 1 for CDCSS high 12bit
	//               <15> = 0 for CDCSS low  12bit
	// <11:0> = CDCSShigh/low 12bit code
	//
	BK4819_WriteRegister(BK4819_REG_08, (0u << 15) | ((CodeWord >>  0) & 0x0FFF)); // LS 12-bits
	BK4819_WriteRegister(BK4819_REG_08, (1u << 15) | ((CodeWord >> 12) & 0x0FFF)); // MS 12-bits
}

void BK4819_SetCTCSSFrequency(uint32_t FreqControlWord)
{
	// REG_51 <15>  0                                 1 = Enable TxCTCSS/CDCSS           0 = Disable
	// REG_51 <14>  0                                 1 = GPIO0Input for CDCSS           0 = Normal Mode.(for BK4819v3)
	// REG_51 <13>  0                                 1 = Transmit negative CDCSS code   0 = Transmit positive CDCSScode
	// REG_51 <12>  0 CTCSS/CDCSS mode selection      1 = CTCSS                          0 = CDCSS
	// REG_51 <11>  0 CDCSS 24/23bit selection        1 = 24bit                          0 = 23bit
	// REG_51 <10>  0 1050HzDetectionMode             1 = 1050/4 Detect Enable, CTC1 should be set to 1050/4 Hz
	// REG_51 <9>   0 Auto CDCSS Bw Mode              1 = Disable                        0 = Enable.
	// REG_51 <8>   0 Auto CTCSS Bw Mode              0 = Enable                         1 = Disable
	// REG_51 <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning     0 = min                            127 = max

	uint16_t Config;
	if (FreqControlWord == 2625)
	{	// Enables 1050Hz detection mode
		// Enable TxCTCSS
		// CTCSS Mode
		// 1050/4 Detect Enable
		// Enable Auto CDCSS Bw Mode
		// Enable Auto CTCSS Bw Mode
		// CTCSS/CDCSS Tx Gain1 Tuning = 74
		//
		Config = 0x944A;   // 1 0 0 1 0 1 0 0 0 1001010
	}
	else
	{	// Enable TxCTCSS
		// CTCSS Mode
		// Enable Auto CDCSS Bw Mode
		// Enable Auto CTCSS Bw Mode
		// CTCSS/CDCSS Tx Gain1 Tuning = 74
		//
		Config = 0x904A;   // 1 0 0 1 0 0 0 0 0 1001010
	}
	BK4819_WriteRegister(BK4819_REG_51, Config);

	// REG_07 <15:0>
	//
	// When <13> = 0 for CTC1
	// <12:0> = CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 1 for CTC2 (Tail RX detection)
	// <12:0> = CTC2 (should below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 2 for CDCSS 134.4Hz
	// <12:0> = CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC1 | (((FreqControlWord * 206488u) + 50000u) / 100000u));   // with rounding
}

// freq_10Hz is CTCSS Hz * 10
void BK4819_SetTailDetection(const uint32_t freq_10Hz)
{
	// REG_07 <15:0>
	//
	// When <13> = 0 for CTC1
	// <12:0> = CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 1 for CTC2 (Tail RX detection)
	// <12:0> = CTC2 (should below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 2 for CDCSS 134.4Hz
	// <12:0> = CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC2 | ((253910 + (freq_10Hz / 2)) / freq_10Hz));  // with rounding
}

void BK4819_EnableVox(uint16_t VoxEnableThreshold, uint16_t VoxDisableThreshold)
{
	//VOX Algorithm
	//if (voxamp>VoxEnableThreshold)                VOX = 1;
	//else
	//if (voxamp<VoxDisableThreshold) (After Delay) VOX = 0;

	const uint16_t REG_31_Value = BK4819_ReadRegister(BK4819_REG_31);

	// 0xA000 is undocumented?
	BK4819_WriteRegister(BK4819_REG_46, 0xA000 | (VoxEnableThreshold & 0x07FF));

	// 0x1800 is undocumented?
	BK4819_WriteRegister(BK4819_REG_79, 0x1800 | (VoxDisableThreshold & 0x07FF));

	// Bottom 12 bits are undocumented, 15:12 vox disable delay *128ms
	BK4819_WriteRegister(BK4819_REG_7A, 0x289A); // vox disable delay = 128*5 = 640ms

	// Enable VOX
	BK4819_WriteRegister(BK4819_REG_31, REG_31_Value | (1u << 2));    // VOX Enable
}

void BK4819_SetFilterBandwidth(const BK4819_filter_bandwidth_t Bandwidth, const bool weak_no_different)
{
	// REG_43
	// <15>    0 ???
	//
	// <14:12> 4 RF filter bandwidth
	//         0 = 1.7  kHz
	//         1 = 2.0  kHz
	//         2 = 2.5  kHz
	//         3 = 3.0  kHz
	//         4 = 3.75 kHz
	//         5 = 4.0  kHz
	//         6 = 4.25 kHz
	//         7 = 4.5  kHz
	// if <5> == 1, RF filter bandwidth * 2
	//
	// <11:9>  0 RF filter bandwidth when signal is weak
	//         0 = 1.7  kHz
	//         1 = 2.0  kHz
	//         2 = 2.5  kHz
	//         3 = 3.0  kHz
	//         4 = 3.75 kHz
	//         5 = 4.0  kHz
	//         6 = 4.25 kHz
	//         7 = 4.5  kHz
	// if <5> == 1, RF filter bandwidth * 2
	//
	// <8:6>   1 AFTxLPF2 filter Band Width
	//         1 = 2.5  kHz (for 12.5k channel space)
	//         2 = 2.75 kHz
	//         0 = 3.0  kHz (for 25k   channel space)
	//         3 = 3.5  kHz
	//         4 = 4.5  kHz
	//         5 = 4.25 kHz
	//         6 = 4.0  kHz
	//         7 = 3.75 kHz
	//
	// <5:4>   0 BW Mode Selection
	//         0 = 12.5k
	//         1 =  6.25k
	//         2 = 25k/20k
	//
	// <3>     1 ???
	//
	// <2>     0 Gain after FM Demodulation
	//         0 = 0dB
	//         1 = 6dB
	//
	// <1:0>   0 ???

	uint16_t val;

	switch (Bandwidth)
	{
		default:
		case BK4819_FILTER_BW_WIDE:	// 25kHz
			if (weak_no_different)
			{	// make the RX bandwidth the same with weak signals
				val =
					(0u << 15) |     //  0
					(4u << 12) |     // *3 RF filter bandwidth
					(4u <<  9) |     // *0 RF filter bandwidth when signal is weak
					(6u <<  6) |     // *0 AFTxLPF2 filter Band Width
					(2u <<  4) |     //  2 BW Mode Selection
					(1u <<  3) |     //  1
					(0u <<  2) |     //  0 Gain after FM Demodulation
					(0u <<  0);      //  0
			}
			else
			{	// with weak RX signals the RX bandwidth is reduced
				val =                // 0x3028);         // 0 011 000 000 10 1 0 00
					(0u << 15) |     //  0
					(4u << 12) |     // *3 RF filter bandwidth
					(2u <<  9) |     // *0 RF filter bandwidth when signal is weak
					(6u <<  6) |     // *0 AFTxLPF2 filter Band Width
					(2u <<  4) |     //  2 BW Mode Selection
					(1u <<  3) |     //  1
					(0u <<  2) |     //  0 Gain after FM Demodulation
					(0u <<  0);      //  0
			}
			break;

		case BK4819_FILTER_BW_NARROW:	// 12.5kHz
			if (weak_no_different)
			{
				val =
					(0u << 15) |     //  0
					(4u << 12) |     // *4 RF filter bandwidth
					(4u <<  9) |     // *0 RF filter bandwidth when signal is weak
					(0u <<  6) |     // *1 AFTxLPF2 filter Band Width
					(0u <<  4) |     //  0 BW Mode Selection
					(1u <<  3) |     //  1
					(0u <<  2) |     //  0 Gain after FM Demodulation
					(0u <<  0);      //  0
			}
			else
			{
				val =                // 0x4048);        // 0 100 000 001 00 1 0 00
					(0u << 15) |     //  0
					(4u << 12) |     // *4 RF filter bandwidth
					(2u <<  9) |     // *0 RF filter bandwidth when signal is weak
					(0u <<  6) |     // *1 AFTxLPF2 filter Band Width
					(0u <<  4) |     //  0 BW Mode Selection
					(1u <<  3) |     //  1
					(0u <<  2) |     //  0 Gain after FM Demodulation
					(0u <<  0);      //  0
			}
			break;

		case BK4819_FILTER_BW_NARROWER:	// 6.25kHz
			if (weak_no_different)
			{
				val =
					(0u << 15) |     //  0
					(3u << 12) |     //  3 RF filter bandwidth
					(3u <<  9) |     // *0 RF filter bandwidth when signal is weak
					(1u <<  6) |     //  1 AFTxLPF2 filter Band Width
					(1u <<  4) |     //  1 BW Mode Selection
					(1u <<  3) |     //  1
					(0u <<  2) |     //  0 Gain after FM Demodulation
					(0u <<  0);      //  0
			}
			else
			{
				val =
					(0u << 15) |     //  0
					(3u << 12) |     //  3 RF filter bandwidth
					(0u <<  9) |     //  0 RF filter bandwidth when signal is weak
					(1u <<  6) |     //  1 AFTxLPF2 filter Band Width
					(1u <<  4) |     //  1 BW Mode Selection
					(1u <<  3) |     //  1
					(0u <<  2) |     //  1 Gain after FM Demodulation
					(0u <<  0);      //  0
			}
			break;
	}

	BK4819_WriteRegister(BK4819_REG_43, val);
}

void BK4819_SetupPowerAmplifier(const uint8_t bias, const uint32_t frequency)
{
	// REG_36 <15:8> 0 PA Bias output 0 ~ 3.2V
	//               255 = 3.2V
	//                 0 = 0V
	//
	// REG_36 <7>    0
	//               1 = Enable PA-CTL output
	//               0 = Disable (Output 0 V)
	//
	// REG_36 <5:3>  7 PA gain 1 tuning
	//               7 = max
	//               0 = min
	//
	// REG_36 <2:0>  7 PA gain 2 tuning
	//               7 = max
	//               0 = min
	//
	//                                  280MHz       gain 1 = 1  gain 2 = 0  gain 1 = 4  gain 2 = 2
	const uint8_t gain   = (frequency < 28000000) ? (1u << 3) | (0u << 0) : (4u << 3) | (2u << 0);
	const uint8_t enable = 1;
	BK4819_WriteRegister(BK4819_REG_36, ((uint16_t)bias << 8) | ((uint16_t)enable << 7) | ((uint16_t)gain << 0));
}

void BK4819_SetFrequency(uint32_t Frequency)
{
	BK4819_WriteRegister(BK4819_REG_38, (Frequency >>  0) & 0xFFFF);
	BK4819_WriteRegister(BK4819_REG_39, (Frequency >> 16) & 0xFFFF);
}

void BK4819_SetupSquelch(
		uint8_t squelch_open_rssi_thresh,
		uint8_t squelch_close_rssi_thresh,
		uint8_t squelch_open_noise_thresh,
		uint8_t squelch_close_noise_thresh,
		uint8_t squelch_close_glitch_thresh,
		uint8_t squelch_open_glitch_thresh)
{
	// REG_70
	//
	// <15>   0 Enable TONE1
	//        1 = Enable
	//        0 = Disable
	//
	// <14:8> 0 TONE1 tuning gain
	//        0 ~ 127
	//
	// <7>    0 Enable TONE2
	//        1 = Enable
	//        0 = Disable
	//
	// <6:0>  0 TONE2/FSK tuning gain
	//        0 ~ 127
	//
	BK4819_WriteRegister(BK4819_REG_70, 0);

	// Glitch threshold for Squelch = close
	//
	// 0 ~ 255
	//
	BK4819_WriteRegister(BK4819_REG_4D, 0xA000 | squelch_close_glitch_thresh);

	// REG_4E
	//
	// <15:14> 1 ???
	//
	// <13:11> 5 Squelch = open  Delay Setting
	//         0 ~ 7
	//
	// <10:9>  7 Squelch = close Delay Setting
	//         0 ~ 3
	//
	// <8>     0 ???
	//
	// <7:0>   8 Glitch threshold for Squelch = open
	//         0 ~ 255
	//
	BK4819_WriteRegister(BK4819_REG_4E,  // 01 101 11 1 00000000
	#ifndef ENABLE_FASTER_CHANNEL_SCAN
		// original (*)
		(1u << 14) |                  //  1 ???
		(3u << 11) |                  // *5  squelch = open  delay .. 0 ~ 7
		(2u <<  9) |                  // *3  squelch = close delay .. 0 ~ 3
		squelch_open_glitch_thresh);     //  0 ~ 255
	#else
		// faster (but twitchier)
		(1u << 14) |                  //  1 ???
		(2u << 11) |                  // *5  squelch = open  delay .. 0 ~ 7
		(1u <<  9) |                  // *3  squelch = close delay .. 0 ~ 3
		squelch_open_glitch_thresh);     //  0 ~ 255
	#endif

	// REG_4F
	//
	// <14:8> 47 Ex-noise threshold for Squelch = close
	//        0 ~ 127
	//
	// <7>    ???
	//
	// <6:0>  46 Ex-noise threshold for Squelch = open
	//        0 ~ 127
	//
	BK4819_WriteRegister(BK4819_REG_4F, ((uint16_t)squelch_close_noise_thresh << 8) | squelch_open_noise_thresh);

	// REG_78
	//
	// <15:8> 72 RSSI threshold for Squelch = open    0.5dB/step
	//
	// <7:0>  70 RSSI threshold for Squelch = close   0.5dB/step
	//
	BK4819_WriteRegister(BK4819_REG_78, ((uint16_t)squelch_open_rssi_thresh   << 8) | squelch_close_rssi_thresh);

	BK4819_SetAF(BK4819_AF_MUTE);

	BK4819_RX_TurnOn();
}

void BK4819_SetAF(BK4819_af_type_t AF)
{
	// AF Output Inverse Mode = Inverse
	// Undocumented bits 0x2040
	//
//	BK4819_WriteRegister(BK4819_REG_47, 0x6040 | (AF << 8));
	BK4819_WriteRegister(BK4819_REG_47, (6u << 12) | (AF << 8) | (1u << 6));
}

void BK4819_RX_TurnOn(void)
{
	// DSP Voltage Setting = 1
	// ANA LDO = 2.7v
	// VCO LDO = 2.7v
	// RF LDO  = 2.7v
	// PLL LDO = 2.7v
	// ANA LDO bypass
	// VCO LDO bypass
	// RF LDO  bypass
	// PLL LDO bypass
	// Reserved bit is 1 instead of 0
	// Enable  DSP
	// Enable  XTAL
	// Enable  Band Gap
	//
	BK4819_WriteRegister(BK4819_REG_37, 0x1F0F);  // 0001 1111 0000 1111

	// Turn off everything
	BK4819_WriteRegister(BK4819_REG_30, 0);

	// Enable  VCO Calibration
	// Enable  RX Link
	// Enable  AF DAC
	// Enable  PLL/VCO
	// Disable PA Gain
	// Disable MIC ADC
	// Disable TX DSP
	// Enable  RX DSP
	//
	BK4819_WriteRegister(BK4819_REG_30, 0xbff1); // 1 0 1111 1 1 1111 0 0 0 1
}

void BK4819_PickRXFilterPathBasedOnFrequency(uint32_t Frequency)
{
	if (Frequency < 28000000)
	{
		BK4819_set_GPIO_pin(BK4819_GPIO2_PIN30, true);
		BK4819_set_GPIO_pin(BK4819_GPIO3_PIN31, false);
	}
	else
	if (Frequency == 0xFFFFFFFF)
	{
		BK4819_set_GPIO_pin(BK4819_GPIO2_PIN30, false);
		BK4819_set_GPIO_pin(BK4819_GPIO3_PIN31, false);
	}
	else
	{
		BK4819_set_GPIO_pin(BK4819_GPIO2_PIN30, false);
		BK4819_set_GPIO_pin(BK4819_GPIO3_PIN31, true);
	}
}

void BK4819_DisableScramble(void)
{
	const uint16_t Value = BK4819_ReadRegister(BK4819_REG_31);
	BK4819_WriteRegister(BK4819_REG_31, Value & ~(1u << 1));
}

void BK4819_EnableScramble(uint8_t Type)
{
	const uint16_t Value = BK4819_ReadRegister(BK4819_REG_31);
	BK4819_WriteRegister(BK4819_REG_31, Value | (1u << 1));

	BK4819_WriteRegister(BK4819_REG_71, 0x68DC + (Type * 1032));   // 0110 1000 1101 1100
}

bool BK4819_CompanderEnabled(void)
{
	return (BK4819_ReadRegister(BK4819_REG_31) & (1u << 3)) ? true : false;
}

void BK4819_SetCompander(const unsigned int mode)
{
	// mode 0 .. OFF
	// mode 1 .. TX
	// mode 2 .. RX
	// mode 3 .. TX and RX

	const uint16_t r31 = BK4819_ReadRegister(BK4819_REG_31);

	if (mode == 0)
	{	// disable
		BK4819_WriteRegister(BK4819_REG_31, r31 & ~(1u << 3));
		return;
	}

	// REG_29
	//
	// <15:14> 10 Compress (AF Tx) Ratio
	//         00 = Disable
	//         01 = 1.333:1
	//         10 = 2:1
	//         11 = 4:1
	//
	// <13:7>  86 Compress (AF Tx) 0 dB point (dB)
	//
	// <6:0>   64 Compress (AF Tx) noise point (dB)
	//
	const uint16_t compress_ratio    = (mode == 1 || mode >= 3) ? 2 : 0;  // 2:1
	const uint16_t compress_0dB      = 86;
	const uint16_t compress_noise_dB = 64;
//	AB40  10 1010110 1000000
	BK4819_WriteRegister(BK4819_REG_29, // (BK4819_ReadRegister(BK4819_REG_29) & ~(3u << 14)) | (compress_ratio << 14));
		(compress_ratio    << 14) |
		(compress_0dB      <<  7) |
		(compress_noise_dB <<  0));

	// REG_28
	//
	// <15:14> 01 Expander (AF Rx) Ratio
	//         00 = Disable
	//         01 = 1:2
	//         10 = 1:3
	//         11 = 1:4
	//
	// <13:7>  86 Expander (AF Rx) 0 dB point (dB)
	//
	// <6:0>   56 Expander (AF Rx) noise point (dB)
	//
	const uint16_t expand_ratio    = (mode >= 2) ? 1 : 0;   // 1:2
	const uint16_t expand_0dB      = 86;
	const uint16_t expand_noise_dB = 56;
//	6B38  01 1010110 0111000
	BK4819_WriteRegister(BK4819_REG_28, // (BK4819_ReadRegister(BK4819_REG_28) & ~(3u << 14)) | (expand_ratio << 14));
		(expand_ratio    << 14) |
		(expand_0dB      <<  7) |
		(expand_noise_dB <<  0));

	// enable
	BK4819_WriteRegister(BK4819_REG_31, r31 | (1u << 3));
}

void BK4819_DisableVox(void)
{
	const uint16_t Value = BK4819_ReadRegister(BK4819_REG_31);
	BK4819_WriteRegister(BK4819_REG_31, Value & 0xFFFB);
}

void BK4819_DisableDTMF(void)
{
	BK4819_WriteRegister(BK4819_REG_24, 0);
}

void BK4819_EnableDTMF(void)
{
	// no idea what this does
	BK4819_WriteRegister(BK4819_REG_21, 0x06D8);        // 0000 0110 1101 1000

	// REG_24
	//
	// <15>   1  ???
	//
	// <14:7> 24 Threshold
	//
	// <6>    1  ???
	//
	// <5>    0  DTMF/SelCall enable
	//        1 = Enable
	//        0 = Disable
	//
	// <4>    1  DTMF or SelCall detection mode
	//        1 = for DTMF
	//        0 = for SelCall
	//
	// <3:0>  14 Max symbol number for SelCall detection
	//
//	const uint16_t threshold = 24;    // default, but doesn't decode non-QS radios
	const uint16_t threshold = 130;   // but 128 ~ 247 does
//	const uint16_t threshold =  8;    // 0 ~ 63 ? .. doesn't work with A and B's :(
	BK4819_WriteRegister(BK4819_REG_24,                      // 1 00011000 1 1 1 1110
		(1u        << BK4819_REG_24_SHIFT_UNKNOWN_15) |
		(threshold << BK4819_REG_24_SHIFT_THRESHOLD)  |      // 0 ~ 255
		(1u        << BK4819_REG_24_SHIFT_UNKNOWN_6)  |
		              BK4819_REG_24_ENABLE            |
		              BK4819_REG_24_SELECT_DTMF       |
//		(14u       << BK4819_REG_24_SHIFT_MAX_SYMBOLS));     // 0 ~ 15
		(15u       << BK4819_REG_24_SHIFT_MAX_SYMBOLS));     // 0 ~ 15
}

void BK4819_PlayTone(uint16_t Frequency, bool bTuningGainSwitch)
{
	uint16_t ToneConfig;

	BK4819_EnterTxMute();
	BK4819_SetAF(BK4819_AF_BEEP);

	if (bTuningGainSwitch == 0)
		ToneConfig = BK4819_REG_70_ENABLE_TONE1 | (96u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN);
	else
		ToneConfig = BK4819_REG_70_ENABLE_TONE1 | (28u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN);
	BK4819_WriteRegister(BK4819_REG_70, ToneConfig);

	BK4819_WriteRegister(BK4819_REG_30, 0);
	BK4819_WriteRegister(BK4819_REG_30, BK4819_REG_30_ENABLE_AF_DAC | BK4819_REG_30_ENABLE_DISC_MODE | BK4819_REG_30_ENABLE_TX_DSP);

	BK4819_WriteRegister(BK4819_REG_71, scale_freq(Frequency));
}

void BK4819_PlaySingleTone(const unsigned int tone_Hz, const unsigned int delay, const unsigned int level, const bool play_speaker)
{
	BK4819_EnterTxMute();

	if (play_speaker)
	{
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
		BK4819_SetAF(BK4819_AF_BEEP);
	}
	else
		BK4819_SetAF(BK4819_AF_MUTE);

	// level 0 ~ 127
//	BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_ENABLE_TONE1 | (96u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));
//	BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_ENABLE_TONE1 | (28u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));
	BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_ENABLE_TONE1 | ((level & 0x7f) << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));

	BK4819_EnableTXLink();
	SYSTEM_DelayMs(50);

	BK4819_WriteRegister(BK4819_REG_71, scale_freq(tone_Hz));

	BK4819_ExitTxMute();
	SYSTEM_DelayMs(delay);
	BK4819_EnterTxMute();

	if (play_speaker)
	{
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
		BK4819_SetAF(BK4819_AF_MUTE);
//		BK4819_SetAF(g_rx_vfo->am_mode ? BK4819_AF_AM : BK4819_AF_FM);
	}

	BK4819_WriteRegister(BK4819_REG_70, 0x0000);
	BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
	BK4819_ExitTxMute();
}

void BK4819_EnterTxMute(void)
{
	BK4819_WriteRegister(BK4819_REG_50, 0xBB20);
}

void BK4819_ExitTxMute(void)
{
	BK4819_WriteRegister(BK4819_REG_50, 0x3B20);
}

void BK4819_Sleep(void)
{
	BK4819_WriteRegister(BK4819_REG_30, 0);
	BK4819_WriteRegister(BK4819_REG_37, 0x1D00);
}

void BK4819_TurnsOffTones_TurnsOnRX(void)
{
	BK4819_WriteRegister(BK4819_REG_70, 0);
	BK4819_SetAF(BK4819_AF_MUTE);

	BK4819_ExitTxMute();

	BK4819_WriteRegister(BK4819_REG_30, 0);
	BK4819_WriteRegister(BK4819_REG_30,
		BK4819_REG_30_ENABLE_VCO_CALIB |
		BK4819_REG_30_ENABLE_RX_LINK   |
		BK4819_REG_30_ENABLE_AF_DAC    |
		BK4819_REG_30_ENABLE_DISC_MODE |
		BK4819_REG_30_ENABLE_PLL_VCO   |
		BK4819_REG_30_ENABLE_RX_DSP);
}

#ifdef ENABLE_AIRCOPY
	void BK4819_SetupAircopy(void)
	{
		BK4819_WriteRegister(BK4819_REG_70, 0x00E0);    // Enable Tone2, tuning gain 48
		BK4819_WriteRegister(BK4819_REG_72, 0x3065);    // Tone2 baudrate 1200
		
		BK4819_WriteRegister(BK4819_REG_58, 0x00C1);    // FSK Enable
		                                                // FSK 1.2K RX Bandwidth
														// Preamble 0xAA or 0x55
														// RX Gain 0
														// RX Mode
		                                                // (FSK1.2K, FSK2.4K Rx and NOAA SAME Rx)
														// TX Mode FSK 1.2K
													    // FSK 2.4K Tx
														
		BK4819_WriteRegister(BK4819_REG_5C, 0x5665);    // Enable CRC among other things we don't know yet
		BK4819_WriteRegister(BK4819_REG_5D, 0x4700);    // FSK Data Length 72 Bytes (0xABCD + 2 byte length + 64 byte payload + 2 byte CRC + 0xDCBA)
	}
#endif

void BK4819_ResetFSK(void)
{
	BK4819_WriteRegister(BK4819_REG_3F, 0x0000);        // Disable interrupts
	BK4819_WriteRegister(BK4819_REG_59, 0x0068);        // Sync length 4 bytes, 7 byte preamble

	SYSTEM_DelayMs(30);

	BK4819_Idle();
}

void BK4819_Idle(void)
{
	BK4819_WriteRegister(BK4819_REG_30, 0x0000);
}

void BK4819_ExitBypass(void)
{
	BK4819_SetAF(BK4819_AF_MUTE);

	// REG_7E
	//
	// <15>    0 AGC fix mode
	//         1 = fix
	//         0 = auto
	//
	// <14:12> 3 AGC fix index
	//         3 ( 3) = max
	//         2 ( 2)
	//         1 ( 1)
	//         0 ( 0)
	//         7 (-1)
	//         6 (-2)
	//         5 (-3)
	//         4 (-4) = min
	//
	// <11:6>  0 ???
	//
	// <5:3>   5 DC filter band width for Tx (MIC In)
	//         0 ~ 7
	//         0 = bypass DC filter
	//
	// <2:0>   6 DC filter band width for Rx (I.F In)
	//         0 ~ 7
	//         0 = bypass DC filter
	//
	BK4819_WriteRegister(BK4819_REG_7E, // 0x302E);   // 0 011 000000 101 110
		(0u << 15) |      // 0  AGC fix mode
		(3u << 12) |      // 3  AGC fix index
		(5u <<  3) |      // 5  DC Filter band width for Tx (MIC In)
		(6u <<  0));      // 6  DC Filter band width for Rx (I.F In)
}

void BK4819_PrepareTransmit(void)
{
	BK4819_ExitBypass();
	BK4819_ExitTxMute();
	BK4819_TxOn_Beep();
}

void BK4819_TxOn_Beep(void)
{
	BK4819_WriteRegister(BK4819_REG_37, 0x1D0F);
	BK4819_WriteRegister(BK4819_REG_52, 0x028F);
	BK4819_WriteRegister(BK4819_REG_30, 0x0000);
	BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
}

void BK4819_ExitSubAu(void)
{
	// REG_51
	//
	// <15>  0
	//       1 = Enable TxCTCSS/CDCSS
	//       0 = Disable
	//
	// <14>  0
	//       1 = GPIO0Input for CDCSS
	//       0 = Normal Mode (for BK4819 v3)
	//
	// <13>  0
	//       1 = Transmit negative CDCSS code
	//       0 = Transmit positive CDCSS code
	//
	// <12>  0 CTCSS/CDCSS mode selection
	//       1 = CTCSS
	//       0 = CDCSS
	//
	// <11>  0 CDCSS 24/23bit selection
	//       1 = 24bit
	//       0 = 23bit
	//
	// <10>  0 1050HzDetectionMode
	//       1 = 1050/4 Detect Enable, CTC1 should be set to 1050/4 Hz
	//
	// <9>   0 Auto CDCSS Bw Mode
	//       1 = Disable
	//       0 = Enable
	//
	// <8>   0 Auto CTCSS Bw Mode
	//       0 = Enable
	//       1 = Disable
	//
	// <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning
	//       0   = min
	//       127 = max
	//
	BK4819_WriteRegister(BK4819_REG_51, 0x0000);
}

void BK4819_Conditional_RX_TurnOn_and_GPIO6_Enable(void)
{
	if (g_rx_idle_mode)
	{
		BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2, true);
		BK4819_RX_TurnOn();
	}
}

void BK4819_EnterDTMF_TX(bool bLocalLoopback)
{
	BK4819_EnableDTMF();
	BK4819_EnterTxMute();
	BK4819_SetAF(bLocalLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);

	BK4819_WriteRegister(BK4819_REG_70,
		BK4819_REG_70_MASK_ENABLE_TONE1                |
		(83u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN) |
		BK4819_REG_70_MASK_ENABLE_TONE2                |
		(83u << BK4819_REG_70_SHIFT_TONE2_TUNING_GAIN));

	BK4819_EnableTXLink();
}

void BK4819_ExitDTMF_TX(bool bKeep)
{
	BK4819_EnterTxMute();
	BK4819_SetAF(BK4819_AF_MUTE);
	BK4819_WriteRegister(BK4819_REG_70, 0x0000);
	BK4819_DisableDTMF();
	BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
	if (!bKeep)
		BK4819_ExitTxMute();
}

void BK4819_EnableTXLink(void)
{
	BK4819_WriteRegister(BK4819_REG_30,
		BK4819_REG_30_ENABLE_VCO_CALIB |
		BK4819_REG_30_ENABLE_UNKNOWN   |
		BK4819_REG_30_DISABLE_RX_LINK  |
		BK4819_REG_30_ENABLE_AF_DAC    |
		BK4819_REG_30_ENABLE_DISC_MODE |
		BK4819_REG_30_ENABLE_PLL_VCO   |
		BK4819_REG_30_ENABLE_PA_GAIN   |
		BK4819_REG_30_DISABLE_MIC_ADC  |
		BK4819_REG_30_ENABLE_TX_DSP    |
		BK4819_REG_30_DISABLE_RX_DSP);
}

void BK4819_PlayDTMF(char Code)
{
	uint16_t tone1 = 0;
	uint16_t tone2 = 0;

	switch (Code)                                       //  Hz   Hz
	{	                                                //
		case '0': tone1 = 9715; tone2 = 13793; break;   //  941  1336
		case '1': tone1 = 7196; tone2 = 12482; break;   //  679  1209
		case '2': tone1 = 7196; tone2 = 13793; break;   //  697  1336
		case '3': tone1 = 7196; tone2 = 15249; break;   //  679  1477
		case '4': tone1 = 7950; tone2 = 12482; break;   //  770  1209
		case '5': tone1 = 7950; tone2 = 13793; break;   //  770  1336
		case '6': tone1 = 7950; tone2 = 15249; break;   //  770  1477
		case '7': tone1 = 8796; tone2 = 12482; break;   //  852  1209
		case '8': tone1 = 8796; tone2 = 13793; break;   //  852  1336
		case '9': tone1 = 8796; tone2 = 15249; break;   //  852  1477
		case 'A': tone1 = 7196; tone2 = 16860; break;   //  679  1633
		case 'B': tone1 = 7950; tone2 = 16860; break;   //  770  1633
		case 'C': tone1 = 8796; tone2 = 16860; break;   //  852  1633
		case 'D': tone1 = 9715; tone2 = 16860; break;   //  941  1633
		case '*': tone1 = 9715; tone2 = 12482; break;   //  941  1209
		case '#': tone1 = 9715; tone2 = 15249; break;   //  941  1477
	}

	if (tone1 > 0)
		BK4819_WriteRegister(BK4819_REG_71, tone1);
	if (tone2 > 0)
		BK4819_WriteRegister(BK4819_REG_72, tone2);
}

void BK4819_PlayDTMFString(const char *pString, bool bDelayFirst, uint16_t FirstCodePersistTime, uint16_t HashCodePersistTime, uint16_t CodePersistTime, uint16_t CodeInternalTime)
{
	unsigned int i;

	if (pString == NULL)
		return;

	for (i = 0; pString[i]; i++)
	{
		uint16_t Delay;
		BK4819_PlayDTMF(pString[i]);
		BK4819_ExitTxMute();
		if (bDelayFirst && i == 0)
			Delay = FirstCodePersistTime;
		else
		if (pString[i] == '*' || pString[i] == '#')
			Delay = HashCodePersistTime;
		else
			Delay = CodePersistTime;
		SYSTEM_DelayMs(Delay);
		BK4819_EnterTxMute();
		SYSTEM_DelayMs(CodeInternalTime);
	}
}

void BK4819_TransmitTone(bool bLocalLoopback, uint32_t Frequency)
{
	BK4819_EnterTxMute();

	// REG_70
	//
	// <15>   0 Enable TONE1
	//        1 = Enable
	//        0 = Disable
	//
	// <14:8> 0 TONE1 tuning gain
	//        0 ~ 127
	//
	// <7>    0 Enable TONE2
	//        1 = Enable
	//        0 = Disable
	//
	// <6:0>  0 TONE2/FSK amplitude
	//        0 ~ 127
	//
	// set the tone amplitude
	//
//	BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_MASK_ENABLE_TONE1 | (96u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));
	BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_MASK_ENABLE_TONE1 | (28u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));

	BK4819_WriteRegister(BK4819_REG_71, scale_freq(Frequency));

	BK4819_SetAF(bLocalLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);

	BK4819_EnableTXLink();

	SYSTEM_DelayMs(50);

	BK4819_ExitTxMute();
}

void BK4819_GenTail(uint8_t Tail)
{
	// REG_52
	//
	// <15>    0 Enable 120/180/240 degree shift CTCSS or 134.4Hz Tail when CDCSS mode
	//         0 = Normal
	//         1 = Enable
	//
	// <14:13> 0 CTCSS tail mode selection (only valid when REG_52 <15> = 1)
	//         00 = for 134.4Hz CTCSS Tail when CDCSS mode
	//         01 = CTCSS0 120° phase shift
	//         10 = CTCSS0 180° phase shift
	//         11 = CTCSS0 240° phase shift
	//
	// <12>    0 CTCSSDetectionThreshold Mode
	//         1 = ~0.1%
	//         0 =  0.1 Hz
	//
	// <11:6>  0x0A CTCSS found detect threshold
	//
	// <5:0>   0x0F CTCSS lost  detect threshold

	// REG_07 <15:0>
	//
	// When <13> = 0 for CTC1
	// <12:0> = CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 1 for CTC2 (Tail 55Hz Rx detection)
	// <12:0> = CTC2 (should below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// When <13> = 2 for CDCSS 134.4Hz
	// <12:0> = CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz)*20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M

	switch (Tail)
	{
		case 0: // 134.4Hz CTCSS Tail
			BK4819_WriteRegister(BK4819_REG_52, 0x828F);   // 1 00 0 001010 001111
			break;
		case 1: // 120° phase shift
			BK4819_WriteRegister(BK4819_REG_52, 0xA28F);   // 1 01 0 001010 001111
			break;
		case 2: // 180° phase shift
			BK4819_WriteRegister(BK4819_REG_52, 0xC28F);   // 1 10 0 001010 001111
			break;
		case 3: // 240° phase shift
			BK4819_WriteRegister(BK4819_REG_52, 0xE28F);   // 1 11 0 001010 001111
			break;
		case 4: // 55Hz tone freq
			BK4819_WriteRegister(BK4819_REG_07, 0x046f);   // 0 00 0 010001 101111
			break;
	}
}

void BK4819_EnableCDCSS(void)
{
	BK4819_GenTail(0);     // CTC134
	BK4819_WriteRegister(BK4819_REG_51, 0x804A);
}

void BK4819_EnableCTCSS(void)
{
	#ifdef ENABLE_CTCSS_TAIL_PHASE_SHIFT
		//BK4819_GenTail(1);     // 120° phase shift
		BK4819_GenTail(2);       // 180° phase shift
		//BK4819_GenTail(3);     // 240° phase shift
	#else
		BK4819_GenTail(4);       // 55Hz tone freq
	#endif

	// REG_51
	//
	// <15>  0
	//       1 = Enable TxCTCSS/CDCSS
	//       0 = Disable
	//
	// <14>  0
	//       1 = GPIO0Input for CDCSS
	//       0 = Normal Mode (for BK4819 v3)
	//
	// <13>  0
	//       1 = Transmit negative CDCSS code
	//       0 = Transmit positive CDCSS code
	//
	// <12>  0 CTCSS/CDCSS mode selection
	//       1 = CTCSS
	//       0 = CDCSS
	//
	// <11>  0 CDCSS 24/23bit selection
	//       1 = 24bit
	//       0 = 23bit
	//
	// <10>  0 1050HzDetectionMode
	//       1 = 1050/4 Detect Enable, CTC1 should be set to 1050/4 Hz
	//
	// <9>   0 Auto CDCSS Bw Mode
	//       1 = Disable
	//       0 = Enable
	//
	// <8>   0 Auto CTCSS Bw Mode
	//       0 = Enable
	//       1 = Disable
	//
	// <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning
	//       0   = min
	//       127 = max

	BK4819_WriteRegister(BK4819_REG_51, 0x904A); // 1 0 0 1 0 0 0 0 0 1001010
}

uint16_t BK4819_GetRSSI(void)
{
	return BK4819_ReadRegister(BK4819_REG_67) & 0x01FF;
}

uint8_t  BK4819_GetGlitchIndicator(void)
{
	return BK4819_ReadRegister(BK4819_REG_63) & 0x00FF;
}

uint8_t  BK4819_GetExNoiceIndicator(void)
{
	return BK4819_ReadRegister(BK4819_REG_65) & 0x007F;
}

uint16_t BK4819_GetVoiceAmplitudeOut(void)
{
	return BK4819_ReadRegister(BK4819_REG_64);
}

uint8_t BK4819_GetAfTxRx(void)
{
	return BK4819_ReadRegister(BK4819_REG_6F) & 0x003F;
}

bool BK4819_GetFrequencyScanResult(uint32_t *pFrequency)
{
	// **********
	// REG_0D  read only
	//
	// <15>    frequency scan indicator
	//         1 = busy
	//         0 = finished
	//
	// <14:11> ???
	//
	// <10:0>  frequency scan high 16 bits
	//
	// **********
	// REG_0E  read only
	//
	// <15:0>  frequency scan low 16 bits
	//
	// **********
	// (REG_0D <10:0> << 16) | (REG_0E <15:0>) .. unit is 10Hz
	//
	const uint16_t high      = BK4819_ReadRegister(BK4819_REG_0D);
	const uint16_t low       = BK4819_ReadRegister(BK4819_REG_0E);
	const bool     finished  = ((high >> 15) & 1u) == 0;
	*pFrequency              = ((uint32_t)(high & 0x07FF) << 16) | low;
	return finished;
}

BK4819_CSS_scan_result_t BK4819_GetCxCSSScanResult(uint32_t *pCdcssFreq, uint16_t *pCtcssFreq)
{
	// **********
	// REG_68 read only
	//
	// <15>   CTCSS scan indicator
	//        1 = busy
	//        0 = found
	//
	// <12:0> CTCSS frequency (Hz)
	//        div by 20.64888 ... 13M / 26M XTAL
	//        div by 20.97152 ... 12.8M / 19.2M / 25.6M / 38.4M XTAL
	//
	// **********
	// REG_69 read only
	//
	// <15>	  CDCSS scan indicator
	//        1 = busy
	//        0 = found
	//
	// <14>   23 or 24 bit CDCSS Indicator (BK4819v3)
	//        1 = 24 bit
	//        0 = 23 bit
	//
	// <11:0> CDCSS High 12 bits
	//
	// **********
	// REG_6A read only
	//
	// <11:0> CDCSS Low 12 bits
	//
	//
	const uint16_t High = BK4819_ReadRegister(BK4819_REG_69);
	uint16_t       Low;

	if (((High >> 15) & 1u) == 0)
	{	// CDCSS
		Low         = BK4819_ReadRegister(BK4819_REG_6A);
		*pCdcssFreq = ((uint32_t)(High & 0xFFF) << 12) | (Low & 0xFFF);
		return BK4819_CSS_RESULT_CDCSS;
	}

	Low = BK4819_ReadRegister(BK4819_REG_68);
	if (((Low >> 15) & 1u) == 0)
	{	// CTCSS
		*pCtcssFreq = ((uint32_t)(Low & 0x1FFF) * 4843) / 10000;
		return BK4819_CSS_RESULT_CTCSS;
	}

	return BK4819_CSS_RESULT_NOT_FOUND;
}

void BK4819_DisableFrequencyScan(void)
{
	// REG_32
	//
	// <15:14> 0 frequency scan time
	//         0 = 0.2 sec
	//         1 = 0.4 sec
	//         2 = 0.8 sec
	//         3 = 1.6 sec
	//
	// <13:1>  ???
	//
	// <0>     0 frequency scan enable
	//         1 = enable
	//         0 = disable
	//
	BK4819_WriteRegister(BK4819_REG_32, // 0x0244);    // 00 0000100100010 0
		(  0u << 14) |          // 0 frequency scan Time
		(290u <<  1) |          // ???
		(  0u <<  0));          // 0 frequency scan enable
}

void BK4819_EnableFrequencyScan(void)
{
	// REG_32
	//
	// <15:14> 0 frequency scan time
	//         0 = 0.2 sec
	//         1 = 0.4 sec
	//         2 = 0.8 sec
	//         3 = 1.6 sec
	//
	// <13:1>  ???
	//
	// <0>     0 frequency scan enable
	//         1 = enable
	//         0 = disable
	//
	BK4819_WriteRegister(BK4819_REG_32, // 0x0245);   // 00 0000100100010 1
		(  0u << 14) |          // 0 frequency scan time
		(290u <<  1) |          // ???
		(  1u <<  0));          // 1 frequency scan enable
}

void BK4819_SetScanFrequency(uint32_t Frequency)
{
	BK4819_SetFrequency(Frequency);

	// REG_51
	//
	// <15>  0
	//       1 = Enable TxCTCSS/CDCSS
	//       0 = Disable
	//
	// <14>  0
	//       1 = GPIO-0 input for CDCSS
	//       0 = Normal Mode (for BK4819 v3)
	//
	// <13>  0
	//       1 = Transmit negative CDCSS code
	//       0 = Transmit positive CDCSS code
	//
	// <12>  0 CTCSS/CDCSS mode selection
	//       1 = CTCSS
	//       0 = CDCSS
	//
	// <11>  0 CDCSS 24/23bit selection
	//       1 = 24bit
	//       0 = 23bit
	//
	// <10>  0 1050Hz detection mode
	//       1 = 1050/4 detect enable, CTC1 should be set to 1050/4 Hz
	//
	// <9>   0 Auto CDCSS Bw Mode
	//       1 = Disable
	//       0 = Enable
	//
	// <8>   0 Auto CTCSS Bw Mode
	//       0 = Enable
	//       1 = Disable
	//
	// <6:0> 0 CTCSS/CDCSS Tx Gain1 Tuning
	//       0   = min
	//       127 = max
	//
	BK4819_WriteRegister(BK4819_REG_51,
		BK4819_REG_51_DISABLE_CxCSS         |
		BK4819_REG_51_GPIO6_PIN2_NORMAL     |
		BK4819_REG_51_TX_CDCSS_POSITIVE     |
		BK4819_REG_51_MODE_CDCSS            |
		BK4819_REG_51_CDCSS_23_BIT          |
		BK4819_REG_51_1050HZ_NO_DETECTION   |
		BK4819_REG_51_AUTO_CDCSS_BW_DISABLE |
		BK4819_REG_51_AUTO_CTCSS_BW_DISABLE);

	BK4819_RX_TurnOn();
}

void BK4819_Disable(void)
{
	BK4819_WriteRegister(BK4819_REG_30, 0);
}

void BK4819_StopScan(void)
{
	BK4819_DisableFrequencyScan();
	BK4819_Disable();
}

uint8_t BK4819_GetDTMF_5TONE_Code(void)
{
	return (BK4819_ReadRegister(BK4819_REG_0B) >> 8) & 0x0F;
}

uint8_t BK4819_get_CDCSS_code_type(void)
{
	return (BK4819_ReadRegister(BK4819_REG_0C) >> 14) & 3u;
}

uint8_t BK4819_GetCTCShift(void)
{
	return (BK4819_ReadRegister(BK4819_REG_0C) >> 12) & 3u;
}

uint8_t BK4819_GetCTCType(void)
{
	return (BK4819_ReadRegister(BK4819_REG_0C) >> 10) & 3u;
}
/*
void BK4819_SendFSKData(uint16_t *pData)
{
	unsigned int i;
	uint8_t Timeout = 1000 / 5; // 1 second

	SYSTEM_DelayMs(20);

	BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_3F_FSK_TX_FINISHED);
	BK4819_WriteRegister(BK4819_REG_59, 0x8068);
	BK4819_WriteRegister(BK4819_REG_59, 0x0068);

	// load the packet
	for (i = 0; i < 36; i++)
		BK4819_WriteRegister(BK4819_REG_5F, pData[i]);

	SYSTEM_DelayMs(20);

	// start sending
	BK4819_WriteRegister(BK4819_REG_59, 0x2868);

	// wait till TX is done ?
	while (Timeout-- && (BK4819_ReadRegister(BK4819_REG_0C) & (1u << 0)) == 0)
		SYSTEM_DelayMs(5);

	BK4819_WriteRegister(BK4819_REG_02, 0);  // disable all interrupts

	SYSTEM_DelayMs(20);

	BK4819_ResetFSK();
}
*/
void BK4819_PrepareFSKReceive(void)
{
	BK4819_ResetFSK();

	BK4819_WriteRegister(BK4819_REG_02, 0);
	BK4819_WriteRegister(BK4819_REG_3F, 0);

	BK4819_RX_TurnOn();
	
	BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_3F_FSK_RX_SYNC | BK4819_REG_3F_FSK_RX_FINISHED | BK4819_REG_3F_FSK_FIFO_ALMOST_FULL);
//	BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_3F_FSK_RX_FINISHED | BK4819_REG_3F_FSK_FIFO_ALMOST_FULL);

	// Clear RX FIFO
	// FSK Preamble Length 7 bytes
	// FSK SyncLength Selection
	BK4819_WriteRegister(BK4819_REG_59, 0x4068);

	// Enable FSK Scramble
	// Enable FSK RX
	// FSK Preamble Length 7 bytes
	// FSK SyncLength Selection
	BK4819_WriteRegister(BK4819_REG_59, 0x3068);
}

void BK4819_PlayRoger(void)
{
	#if 0
		const uint32_t tone1_Hz = 500;
		const uint32_t tone2_Hz = 700;
	#else
		// motorola type
		const uint32_t tone1_Hz = 1540;
		const uint32_t tone2_Hz = 1310;
	#endif

	BK4819_EnterTxMute();
	BK4819_SetAF(BK4819_AF_MUTE);

//	BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_ENABLE_TONE1 | (96u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));
	BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_ENABLE_TONE1 | (28u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));

	BK4819_EnableTXLink();
	SYSTEM_DelayMs(50);

	BK4819_WriteRegister(BK4819_REG_71, scale_freq(tone1_Hz));

	BK4819_ExitTxMute();
	SYSTEM_DelayMs(80);
	BK4819_EnterTxMute();

	BK4819_WriteRegister(BK4819_REG_71, scale_freq(tone2_Hz));

	BK4819_ExitTxMute();
	SYSTEM_DelayMs(80);
	BK4819_EnterTxMute();

	BK4819_WriteRegister(BK4819_REG_70, 0x0000);
	BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);   // 1 1 0000 0 1 1111 1 1 1 0
}

void BK4819_PlayRogerMDC(void)
{
	unsigned int i;

	BK4819_SetAF(BK4819_AF_MUTE);

	BK4819_WriteRegister(BK4819_REG_58, 0x37C3);   // FSK Enable,
	                                               // RX Bandwidth FFSK 1200/1800
	                                               // 0xAA or 0x55 Preamble
	                                               // 11 RX Gain,
	                                               // 101 RX Mode
	                                               // TX FFSK 1200/1800
	BK4819_WriteRegister(BK4819_REG_72, 0x3065);   // Set Tone-2 to 1200Hz
	BK4819_WriteRegister(BK4819_REG_70, 0x00E0);   // Enable Tone-2 and Set Tone2 Gain
	BK4819_WriteRegister(BK4819_REG_5D, 0x0D00);   // Set FSK data length to 13 bytes
	BK4819_WriteRegister(BK4819_REG_59, 0x8068);   // 4 byte sync length, 6 byte preamble, clear TX FIFO
	BK4819_WriteRegister(BK4819_REG_59, 0x0068);   // Same, but clear TX FIFO is now unset (clearing done)
	BK4819_WriteRegister(BK4819_REG_5A, 0x5555);   // First two sync bytes
	BK4819_WriteRegister(BK4819_REG_5B, 0x55AA);   // End of sync bytes. Total 4 bytes: 555555aa
	BK4819_WriteRegister(BK4819_REG_5C, 0xAA30);   // Disable CRC

	// Send the data from the roger table
	for (i = 0; i < 7; i++)
		BK4819_WriteRegister(BK4819_REG_5F, FSK_RogerTable[i]);

	SYSTEM_DelayMs(20);

	// 4 sync bytes, 6 byte preamble, Enable FSK TX
	BK4819_WriteRegister(BK4819_REG_59, 0x0868);

	SYSTEM_DelayMs(180);

	// Stop FSK TX, reset Tone-2, disable FSK
	BK4819_WriteRegister(BK4819_REG_59, 0x0068);
	BK4819_WriteRegister(BK4819_REG_70, 0x0000);
	BK4819_WriteRegister(BK4819_REG_58, 0x0000);
}

void BK4819_Enable_AfDac_DiscMode_TxDsp(void)
{
	BK4819_WriteRegister(BK4819_REG_30, 0x0000);
	BK4819_WriteRegister(BK4819_REG_30, 0x0302);
}

void BK4819_GetVoxAmp(uint16_t *pResult)
{
	*pResult = BK4819_ReadRegister(BK4819_REG_64) & 0x7FFF;
}

void BK4819_SetScrambleFrequencyControlWord(uint32_t Frequency)
{
	BK4819_WriteRegister(BK4819_REG_71, scale_freq(Frequency));
}

void BK4819_PlayDTMFEx(bool bLocalLoopback, char Code)
{
	BK4819_EnableDTMF();
	BK4819_EnterTxMute();

	BK4819_SetAF(bLocalLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);
	BK4819_WriteRegister(BK4819_REG_70, 0xD3D3);

	BK4819_EnableTXLink();

	SYSTEM_DelayMs(50);

	BK4819_PlayDTMF(Code);

	BK4819_ExitTxMute();
}
