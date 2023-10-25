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

#include <string.h>   // NULL and memset

#include "bk4819.h"
#include "bsp/dp32g030/gpio.h"
#include "bsp/dp32g030/portcon.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "misc.h"
#ifdef ENABLE_MDC1200
	#include "mdc1200.h"
#endif

#ifndef ARRAY_SIZE
	#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

static uint16_t gBK4819_GpioOutState;

bool g_rx_idle_mode;

__inline uint16_t scale_freq(const uint16_t freq)
{
//	return (((uint32_t)freq * 1032444u) + 50000u) / 100000u;   // with rounding
	return (((uint32_t)freq * 1353245U) + (1U << 16)) >> 17;   // with rounding
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

//	BK4819_SetAGC(0);
	BK4819_SetAGC(true);     // ???

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
		(11U << 12) |     // ??? 0..15
		( 0U << 10) |     // AF Rx Gain-1
		(58U <<  4) |     // AF Rx Gain-2
		( 8U <<  0));     // AF DAC Gain (after Gain-1 and Gain-2)

#if 1
	const uint8_t dtmf_coeffs[] = {111, 107, 103, 98, 80, 71, 58, 44, 65, 55, 37, 23, 228, 203, 181, 159};
	for (uint32_t i = 0; i < sizeof(dtmf_coeffs); i++)
		BK4819_WriteRegister(BK4819_REG_09, ((i << 12) | dtmf_coeffs[i]));
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

#if 0
	// rt-890
//	BK4819_WriteRegister(0x37, 0x1D0F);

//	DisableAGC(0);
	BK4819_WriteRegister(0x13, 0x03BE);
	BK4819_WriteRegister(0x12, 0x037B);
	BK4819_WriteRegister(0x11, 0x027B);
	BK4819_WriteRegister(0x10, 0x007A);
	BK4819_WriteRegister(0x14, 0x0019);
	BK4819_WriteRegister(0x49, 0x2A38);
	BK4819_WriteRegister(0x7B, 0x8420);

	BK4819_WriteRegister(0x33, 0x1F00);
	BK4819_WriteRegister(0x35, 0x0000);
	BK4819_WriteRegister(0x1E, 0x4C58);
	BK4819_WriteRegister(0x1F, 0xA656);
//	BK4819_WriteRegister(0x3E, gCalibration.BandSelectionThreshold);
	BK4819_WriteRegister(0x3F, 0x0000);
	BK4819_WriteRegister(0x2A, 0x4F18);
	BK4819_WriteRegister(0x53, 0xE678);
	BK4819_WriteRegister(0x2C, 0x5705);
	BK4819_WriteRegister(0x4B, 0x7102);
	BK4819_WriteRegister(0x77, 0x88EF);
	BK4819_WriteRegister(0x26, 0x13A0);
#endif
}

static uint16_t BK4819_ReadU16(void)
{
	uint16_t Value;

	PORTCON_PORTC_IE = (PORTCON_PORTC_IE & ~PORTCON_PORTC_IE_C2_MASK) | PORTCON_PORTC_IE_C2_BITS_ENABLE;
	GPIOC->DIR = (GPIOC->DIR & ~GPIO_DIR_2_MASK) | GPIO_DIR_2_BITS_INPUT;
	SYSTICK_DelayUs(1U);

	Value = 0;
	for (int32_t i = 0; i < 16; i++)
	{
		Value <<= 1;
		Value |= GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1U);
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1U);
	}
	PORTCON_PORTC_IE = (PORTCON_PORTC_IE & ~PORTCON_PORTC_IE_C2_MASK) | PORTCON_PORTC_IE_C2_BITS_DISABLE;
	GPIOC->DIR = (GPIOC->DIR & ~GPIO_DIR_2_MASK) | GPIO_DIR_2_BITS_OUTPUT;

	return Value;
}

uint16_t BK4819_ReadRegister(bk4819_register_t Register)
{
	uint16_t Value;

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);

	SYSTICK_DelayUs(1U);

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	BK4819_WriteU8(Register | 0x80);
	Value = BK4819_ReadU16();
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);

	SYSTICK_DelayUs(1U);

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);

	return Value;
}

void BK4819_WriteRegister(bk4819_register_t Register, uint16_t Data)
{
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);

	SYSTICK_DelayUs(1U);

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);

	BK4819_WriteU8(Register);
	BK4819_WriteU16(Data);

	SYSTICK_DelayUs(1U);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	SYSTICK_DelayUs(1U);

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
}

void BK4819_WriteU8(uint8_t Data)
{
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	for (int32_t i = 7; i >= 0; i--)
	{
		if ((Data & (1 << i)))
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
		else
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);

		SYSTICK_DelayUs(1U);

		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1U);

		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1U);
	}
}

void BK4819_WriteU16(uint16_t Data)
{
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	for (int32_t i = 15; i >= 0; i--)
	{
		if ((Data & (1 << i)))
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
		else
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);

		SYSTICK_DelayUs(1U);

		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1U);

		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_DelayUs(1U);
	}
}

void BK4819_SetAGC(bool Enable)
{
	if (Enable)
	{
		// what does this do ???

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
		BK4819_WriteRegister(BK4819_REG_13, (3U << 8) | (2U << 5) | (3U << 3) | (6U << 0));

		BK4819_WriteRegister(BK4819_REG_12, 0x037C);  // 000000 11 011 11 100
		BK4819_WriteRegister(BK4819_REG_11, 0x027B);  // 000000 10 011 11 011
		BK4819_WriteRegister(BK4819_REG_10, 0x007A);  // 000000 00 011 11 010
		BK4819_WriteRegister(BK4819_REG_14, 0x0018);  // 000000 00 000 11 000

		BK4819_WriteRegister(BK4819_REG_49, 0x2A38);
		BK4819_WriteRegister(BK4819_REG_7B, 0x318C);

		BK4819_WriteRegister(BK4819_REG_7C, 0x595E);
		BK4819_WriteRegister(BK4819_REG_20, 0x8DEF);

		for (int32_t i = 0; i < 8; i++)
			// Bug? The bit 0x2000 below overwrites the (i << 13)
			BK4819_WriteRegister(BK4819_REG_06, ((i << 13) | 0x2500U) + 0x036U);
	}
	else
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
		BK4819_WriteRegister(BK4819_REG_13, (3U << 8) | (2U << 5) | (3U << 3) | (6U << 0));  // 000000 11 101 11 110

		BK4819_WriteRegister(BK4819_REG_12, 0x037B);  // 000000 11 011 11 011
		BK4819_WriteRegister(BK4819_REG_11, 0x027B);  // 000000 10 011 11 011
		BK4819_WriteRegister(BK4819_REG_10, 0x007A);  // 000000 00 011 11 010
		BK4819_WriteRegister(BK4819_REG_14, 0x0019);  // 000000 00 000 11 001

		BK4819_WriteRegister(BK4819_REG_49, 0x2A38);
		BK4819_WriteRegister(BK4819_REG_7B, 0x8420);
	}
}

void BK4819_set_GPIO_pin(bk4819_gpio_pin_t Pin, bool bSet)
{
	if (bSet)
		gBK4819_GpioOutState |=  (0x40U >> Pin);
	else
		gBK4819_GpioOutState &= ~(0x40U >> Pin);

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
		(51U << BK4819_REG_51_SHIFT_CxCSS_TX_GAIN1));

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
	BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC1 | 2775U);

	// REG_08 <15:0> <15> = 1 for CDCSS high 12bit
	//               <15> = 0 for CDCSS low  12bit
	// <11:0> = CDCSShigh/low 12bit code
	//
	BK4819_WriteRegister(BK4819_REG_08, (0U << 15) | ((CodeWord >>  0) & 0x0FFF)); // LS 12-bits
	BK4819_WriteRegister(BK4819_REG_08, (1U << 15) | ((CodeWord >> 12) & 0x0FFF)); // MS 12-bits
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

	if (FreqControlWord == 2625U)
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
	BK4819_WriteRegister(BK4819_REG_07, BK4819_REG_07_MODE_CTC1 | (((FreqControlWord * 206488U) + 50000U) / 100000U));   // with rounding
}

// freq_10Hz is CTCSS Hz * 10
void BK4819_SetTailDetection(uint32_t freq_10Hz)
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
	BK4819_WriteRegister(BK4819_REG_31, REG_31_Value | (1U << 2));    // VOX Enable
}

void BK4819_SetFilterBandwidth(BK4819_filter_bandwidth_t Bandwidth, bool weak_no_different)
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
					(0U << 15) |     //  0
					(4U << 12) |     // *3 RF filter bandwidth
					(4U <<  9) |     // *0 RF filter bandwidth when signal is weak
					(6U <<  6) |     // *0 AFTxLPF2 filter Band Width
					(2U <<  4) |     //  2 BW Mode Selection
					(1U <<  3) |     //  1
					(0U <<  2) |     //  0 Gain after FM Demodulation
					(0U <<  0);      //  0
			}
			else
			{	// with weak RX signals the RX bandwidth is reduced
				val =                // 0x3028);         // 0 011 000 000 10 1 0 00
					(0U << 15) |     //  0
					(4U << 12) |     // *3 RF filter bandwidth
					(2U <<  9) |     // *0 RF filter bandwidth when signal is weak
					(6U <<  6) |     // *0 AFTxLPF2 filter Band Width
					(2U <<  4) |     //  2 BW Mode Selection
					(1U <<  3) |     //  1
					(0U <<  2) |     //  0 Gain after FM Demodulation
					(0U <<  0);      //  0
			}
			break;

		case BK4819_FILTER_BW_NARROW:	// 12.5kHz
			if (weak_no_different)
			{
				val =
					(0U << 15) |     //  0
					(4U << 12) |     // *4 RF filter bandwidth
					(4U <<  9) |     // *0 RF filter bandwidth when signal is weak
					(0U <<  6) |     // *1 AFTxLPF2 filter Band Width
					(0U <<  4) |     //  0 BW Mode Selection
					(1U <<  3) |     //  1
					(0U <<  2) |     //  0 Gain after FM Demodulation
					(0U <<  0);      //  0
			}
			else
			{
				val =                // 0x4048);        // 0 100 000 001 00 1 0 00
					(0U << 15) |     //  0
					(4U << 12) |     // *4 RF filter bandwidth
					(2U <<  9) |     // *0 RF filter bandwidth when signal is weak
					(0U <<  6) |     // *1 AFTxLPF2 filter Band Width
					(0U <<  4) |     //  0 BW Mode Selection
					(1U <<  3) |     //  1
					(0U <<  2) |     //  0 Gain after FM Demodulation
					(0U <<  0);      //  0
			}
			break;

		case BK4819_FILTER_BW_NARROWER:	// 6.25kHz
			if (weak_no_different)
			{
				val =
					(0U << 15) |     //  0
					(3U << 12) |     //  3 RF filter bandwidth
					(3U <<  9) |     // *0 RF filter bandwidth when signal is weak
					(1U <<  6) |     //  1 AFTxLPF2 filter Band Width
					(1U <<  4) |     //  1 BW Mode Selection
					(1U <<  3) |     //  1
					(0U <<  2) |     //  0 Gain after FM Demodulation
					(0U <<  0);      //  0
			}
			else
			{
				val =
					(0U << 15) |     //  0
					(3U << 12) |     //  3 RF filter bandwidth
					(0U <<  9) |     //  0 RF filter bandwidth when signal is weak
					(1U <<  6) |     //  1 AFTxLPF2 filter Band Width
					(1U <<  4) |     //  1 BW Mode Selection
					(1U <<  3) |     //  1
					(0U <<  2) |     //  1 Gain after FM Demodulation
					(0U <<  0);      //  0
			}
			break;
	}

	BK4819_WriteRegister(BK4819_REG_43, val);
}

void BK4819_SetupPowerAmplifier(uint8_t bias, uint32_t frequency)
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
	//                                                         280MHz     gain 1 = 1  gain 2 = 0  gain 1 = 4  gain 2 = 2
	const uint8_t gain   = (frequency == 0U) ? 0U : (frequency < 28000000U) ? (1U << 3) | (0U << 0) : (4U << 3) | (2U << 0);
	const uint8_t enable = 1U;
	BK4819_WriteRegister(BK4819_REG_36, ((uint16_t)bias << 8) | ((uint16_t)enable << 7) | ((uint16_t)gain << 0));
}

void BK4819_set_rf_frequency(uint32_t frequency, bool trigger_update)
{
	BK4819_WriteRegister(BK4819_REG_38, (frequency >>  0) & 0xFFFF);
	BK4819_WriteRegister(BK4819_REG_39, (frequency >> 16) & 0xFFFF);

	if (trigger_update)
	{
		// <15>    0 VCO Calibration    1 = enable   0 = disable
		// <14>    ???
		// <13:10> 0 RX Link           15 = enable   0 = disable
		// <9>     0 AF DAC             1 = enable   0 = disable
		// <8>     0 DISC Mode          1 = enable   0 = disable
		// <7:4>   0 PLL/VCO           15 = enable   0 = disable
		// <3>     0 PA Gain            1 = enable   0 = disable
		// <2>     0 MIC ADC            1 = enable   0 = disable
		// <1>     0 TX DSP             1 = enable   0 = disable
		// <0>     0 RX DSP             1 = enable   0 = disable
		//
		// trigger a PLL/VCO update
		//
		const uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
//		BK4819_WriteRegister(BK4819_REG_30, reg & ~(1u << 15) & (15u << 4));
		BK4819_WriteRegister(BK4819_REG_30, 0x0200);
		BK4819_WriteRegister(BK4819_REG_30, reg);
	}
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
	BK4819_WriteRegister(BK4819_REG_70, 0U);

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
//	#ifndef ENABLE_FASTER_CHANNEL_SCAN
		// original (*)
		(1U << 14) |                  // 1 ???
		(5U << 11) |                  // 5  squelch = open  delay .. 0 ~ 7
		(6U <<  9) |                  // *3  squelch = close delay .. 0 ~ 3
		squelch_open_glitch_thresh);  // 0 ~ 255
//	#else
		// faster (but twitchier)
//		(1u << 14) |                  //  1 ???
//		(2u << 11) |                  // *5  squelch = open  delay .. 0 ~ 7
//		(1u <<  9) |                  // *3  squelch = close delay .. 0 ~ 3
//		squelch_open_glitch_thresh);  //  0 ~ 255
//	#endif

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
	BK4819_WriteRegister(BK4819_REG_47, (6U << 12) | (AF << 8) | (1U << 6));
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
	BK4819_WriteRegister(BK4819_REG_30, 0U);

	// Enable  VCO Calibration
	// Enable  RX Link
	// Enable  AF DAC
	// Enable  PLL/VCO
	// Disable PA Gain
	// Disable MIC ADC
	// Disable TX DSP
	// Enable  RX DSP
	//
	BK4819_WriteRegister(BK4819_REG_30, 0xBFF1); // 1 0 1111 1 1 1111 0 0 0 1
}

void BK4819_set_rf_filter_path(uint32_t Frequency)
{
	if (Frequency < 28000000U)
	{	// VHF
		BK4819_set_GPIO_pin(BK4819_GPIO4_PIN32_VHF_LNA, true);
		BK4819_set_GPIO_pin(BK4819_GPIO3_PIN31_UHF_LNA, false);
	}
	else
	if (Frequency == 0xFFFFFFFF)
	{	// OFF
		BK4819_set_GPIO_pin(BK4819_GPIO4_PIN32_VHF_LNA, false);
		BK4819_set_GPIO_pin(BK4819_GPIO3_PIN31_UHF_LNA, false);
	}
	else
	{	// UHF
		BK4819_set_GPIO_pin(BK4819_GPIO4_PIN32_VHF_LNA, false);
		BK4819_set_GPIO_pin(BK4819_GPIO3_PIN31_UHF_LNA, true);
	}
}

void BK4819_DisableScramble(void)
{
	const uint16_t Value = BK4819_ReadRegister(BK4819_REG_31);
	BK4819_WriteRegister(BK4819_REG_31, Value & ~(1U << 1));
}

void BK4819_EnableScramble(uint8_t Type)
{
	const uint16_t Value = BK4819_ReadRegister(BK4819_REG_31);
	BK4819_WriteRegister(BK4819_REG_31, Value | (1U << 1));

	BK4819_WriteRegister(BK4819_REG_71, 0x68DC + (Type * 1032U));   // 0110 1000 1101 1100
}

bool BK4819_CompanderEnabled(void)
{
	return ((BK4819_ReadRegister(BK4819_REG_31) & (1U << 3)) ? true : false);
}

void BK4819_SetCompander(uint32_t mode)
{
	// mode 0 .. OFF
	// mode 1 .. TX
	// mode 2 .. RX
	// mode 3 .. TX and RX

	const uint16_t r31 = BK4819_ReadRegister(BK4819_REG_31);

	if (mode == 0U)
	{	// disable
		BK4819_WriteRegister(BK4819_REG_31, r31 & ~(1U << 3));
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
	const uint16_t compress_ratio = (mode == 1U || mode >= 3U) ? 2U : 0U;  // 2:1
	BK4819_WriteRegister(BK4819_REG_29, // (BK4819_ReadRegister(BK4819_REG_29) & ~(3u << 14)) | (compress_ratio << 14));
		(compress_ratio << 14) |
		(86U            <<  7) |   // compress 0dB
		(64U            <<  0));   // compress noise dB

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
	const uint16_t expand_ratio = (mode >= 2U) ? 1U : 0U;   // 1:2
	BK4819_WriteRegister(BK4819_REG_28, // (BK4819_ReadRegister(BK4819_REG_28) & ~(3u << 14)) | (expand_ratio << 14));
		(expand_ratio << 14) |
		(86U          <<  7) |   // expander 0dB
		(56U          <<  0));   // expander noise dB

	// enable
	BK4819_WriteRegister(BK4819_REG_31, r31 | (1U << 3));
}

void BK4819_DisableVox(void)
{
	const uint16_t Value = BK4819_ReadRegister(BK4819_REG_31);
	BK4819_WriteRegister(BK4819_REG_31, Value & 0xFFFB);
}

void BK4819_DisableDTMF(void)
{
	BK4819_WriteRegister(BK4819_REG_24, 0U);
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
	const uint16_t threshold = 130U;   // but 128 ~ 247 does
//	const uint16_t threshold =  8;    // 0 ~ 63 ? .. doesn't work with A and B's :(
	BK4819_WriteRegister(BK4819_REG_24,                      // 1 00011000 1 1 1 1110
		(1U        << BK4819_REG_24_SHIFT_UNKNOWN_15) |
		(threshold << BK4819_REG_24_SHIFT_THRESHOLD)  |      // 0 ~ 255
		(1U        << BK4819_REG_24_SHIFT_UNKNOWN_6)  |
		              BK4819_REG_24_ENABLE            |
		              BK4819_REG_24_SELECT_DTMF       |
		(15U       << BK4819_REG_24_SHIFT_MAX_SYMBOLS));     // 0 ~ 15
}

void BK4819_StartTone1(uint16_t frequency, uint32_t level, bool set_dac)
{
//	BK4819_SetAF(BK4819_AF_MUTE);
	BK4819_SetAF(BK4819_AF_BEEP);

	BK4819_EnterTxMute();

	BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_ENABLE_TONE1 | ((level & 0x7f) << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));

	if (set_dac)
	{
		BK4819_WriteRegister(BK4819_REG_30, 0U);
		//BK4819_WriteRegister(BK4819_REG_30, BK4819_REG_30_ENABLE_AF_DAC | BK4819_REG_30_ENABLE_DISC_MODE | BK4819_REG_30_ENABLE_TX_DSP);
		BK4819_EnableTXLink();
	}

	BK4819_WriteRegister(BK4819_REG_71, scale_freq(frequency));
	BK4819_ExitTxMute();
	
//	SYSTEM_DelayMs(2);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);	// enable speaker
	SYSTEM_DelayMs(2U);
}

void BK4819_StopTones(void)
{
//	if (!g_speaker_enabled)
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

	BK4819_EnterTxMute();
	BK4819_WriteRegister(BK4819_REG_70, 0U);
	BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);  // 1100 0001 1111 1110
	BK4819_ExitTxMute();
}

void BK4819_PlayTone(uint32_t tone_Hz, uint32_t delay, uint32_t level)
{
	const uint16_t prev_af = BK4819_ReadRegister(BK4819_REG_47);
	BK4819_StartTone1(tone_Hz, level, true);
	SYSTEM_DelayMs(((delay > 2U) ? (delay - 2U) : delay));
	BK4819_StopTones();
	BK4819_WriteRegister(BK4819_REG_47, prev_af);
}

void BK4819_PlayRoger(void)
{
	#if 0
		const uint32_t tone1_Hz = 500U;
		const uint32_t tone2_Hz = 700U;
	#else
		// motorola
		const uint32_t tone1_Hz = 1540U;
		const uint32_t tone2_Hz = 1310U;
	#endif

	const uint16_t prev_af = BK4819_ReadRegister(BK4819_REG_47);

	BK4819_StartTone1(tone1_Hz, 96U, true);
	SYSTEM_DelayMs(80U - 2U);
	BK4819_StartTone1(tone2_Hz, 96U, false);
	SYSTEM_DelayMs(80U);
	BK4819_StopTones();

	BK4819_WriteRegister(BK4819_REG_47, prev_af);
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
	BK4819_WriteRegister(BK4819_REG_30, 0U);
	BK4819_WriteRegister(BK4819_REG_37, 0x1D00);
}

void BK4819_TurnsOffTones_TurnsOnRX(void)
{
	BK4819_WriteRegister(BK4819_REG_70, 0U);
	BK4819_SetAF(BK4819_AF_MUTE);

	BK4819_ExitTxMute();

	BK4819_WriteRegister(BK4819_REG_30, 0U);
	BK4819_WriteRegister(BK4819_REG_30,
		BK4819_REG_30_ENABLE_VCO_CALIB |
		BK4819_REG_30_ENABLE_RX_LINK   |
		BK4819_REG_30_ENABLE_AF_DAC    |
		BK4819_REG_30_ENABLE_DISC_MODE |
		BK4819_REG_30_ENABLE_PLL_VCO   |
		BK4819_REG_30_ENABLE_RX_DSP);
}

void BK4819_Idle(void)
{
	BK4819_WriteRegister(BK4819_REG_30, 0U);
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
		(0U << 15) |      // 0  AGC fix mode
		(3U << 12) |      // 3  AGC fix index
		(5U <<  3) |      // 5  DC Filter band width for Tx (MIC In)
		(6U <<  0));      // 6  DC Filter band width for Rx (I.F In)
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
		BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_RX_ENABLE, true);
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
	BK4819_WriteRegister(BK4819_REG_70, 0U);
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
	uint32_t index = ((Code >= 65) ? (Code - 55) : ((Code <= 35) ? 15 :((Code <= 42) ? 14 : (Code - '0'))));
	const uint16_t tones[2][16] =
	{
			{ // tone1
					941,  // '0'
					679,  // '1'
					697,  // '2'
					679,  // '3'
					770,  // '4'
					770,  // '5'
					770,  // '6'
					852,  // '7'
					852,  // '8'
					852,  // '9'
					679,  // 'A'
					770,  // 'B'
					852,  // 'C'
					941,  // 'D'
					941,  // '*'
					941,  // '#'
			},
			{ // tone2
					1336, // '0'
					1209, // '1'
					1336, // '2'
					1477, // '3'
					1209, // '4'
					1336, // '5'
					1477, // '6'
					1209, // '7'
					1336, // '8'
					1477, // '9'
					1633, // 'A'
					1633, // 'B'
					1633, // 'C'
					1633, // 'D'
					1209, // '*'
					1477  // '#'
			}
	};

    if (index < 16)
    {
    	BK4819_WriteRegister(BK4819_REG_71, (((uint32_t)tones[0][index] * 103244U) + 5000U) / 10000U);   // with rounding
    	BK4819_WriteRegister(BK4819_REG_72, (((uint32_t)tones[1][index] * 103244U) + 5000U) / 10000U);   // with rounding
    }
}

void BK4819_PlayDTMFString(const char *pString, bool bDelayFirst, uint16_t FirstCodePersistTime, uint16_t HashCodePersistTime, uint16_t CodePersistTime, uint16_t CodeInternalTime)
{
	if (pString == NULL)
		return;

	for (int32_t i = 0; pString[i]; i++)
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
	BK4819_WriteRegister(BK4819_REG_70, BK4819_REG_70_MASK_ENABLE_TONE1 | (28U << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));

	BK4819_WriteRegister(BK4819_REG_71, scale_freq(Frequency));

	BK4819_SetAF(bLocalLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);

	BK4819_EnableTXLink();

	SYSTEM_DelayMs(50U);

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
	BK4819_GenTail(0U);     // CTC134
	BK4819_WriteRegister(BK4819_REG_51, 0x804A);
}

void BK4819_EnableCTCSS(void)
{
	#ifdef ENABLE_CTCSS_TAIL_PHASE_SHIFT
		//BK4819_GenTail(1);     // 120° phase shift
		BK4819_GenTail(2U);       // 180° phase shift
		//BK4819_GenTail(3);     // 240° phase shift
	#else
		BK4819_GenTail(4U);       // 55Hz tone freq
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
	return (BK4819_ReadRegister(BK4819_REG_67) & 0x01FF);
}

uint8_t  BK4819_GetGlitchIndicator(void)
{
	return (BK4819_ReadRegister(BK4819_REG_63) & 0x00FF);
}

uint8_t  BK4819_GetExNoiceIndicator(void)
{
	return (BK4819_ReadRegister(BK4819_REG_65) & 0x007F);
}

uint16_t BK4819_GetVoiceAmplitudeOut(void)
{
	return BK4819_ReadRegister(BK4819_REG_64);
}

uint8_t BK4819_GetAfTxRx(void)
{
	return (BK4819_ReadRegister(BK4819_REG_6F) & 0x003F);
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
	const bool     finished  = (((high >> 15) & 1U) == 0);
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

	if (((High >> 15) & 1U) == 0)
	{	// CDCSS
		Low         = BK4819_ReadRegister(BK4819_REG_6A);
		*pCdcssFreq = ((uint32_t)(High & 0xFFF) << 12) | (Low & 0xFFF);

		return BK4819_CSS_RESULT_CDCSS;
	}

	Low = BK4819_ReadRegister(BK4819_REG_68);
	if (((Low >> 15) & 1U) == 0)
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
		(  0U << 14) |          // 0 frequency scan Time
		(290U <<  1) |          // ???
		(  0U <<  0));          // 0 frequency scan enable
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
		(  0U << 14) |          // 0 frequency scan time
		(290U <<  1) |          // ???
		(  1U <<  0));          // 1 frequency scan enable
}

void BK4819_SetScanFrequency(uint32_t Frequency)
{
	BK4819_set_rf_frequency(Frequency, false);

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
	BK4819_WriteRegister(BK4819_REG_30, 0U);
}

void BK4819_StopScan(void)
{
	BK4819_DisableFrequencyScan();
	BK4819_Disable();
}

uint8_t BK4819_GetDTMF_5TONE_Code(void)
{
	return ((BK4819_ReadRegister(BK4819_REG_0B) >> 8) & 0x0F);
}

uint8_t BK4819_get_CDCSS_code_type(void)
{
	return ((BK4819_ReadRegister(BK4819_REG_0C) >> 14) & 3U);
}

uint8_t BK4819_GetCTCShift(void)
{
	return ((BK4819_ReadRegister(BK4819_REG_0C) >> 12) & 3U);
}

uint8_t BK4819_GetCTCType(void)
{
	return ((BK4819_ReadRegister(BK4819_REG_0C) >> 10) & 3U);
}

#ifdef ENABLE_AIRCOPY
	void BK4819_SetupAircopy(uint32_t packet_size)
	{
		if (packet_size == 0U)
			return;

		// REG_70
		//
		// <15>   0 TONE-1
		//        1 = enable
		//        0 = disable
		//
		// <14:8> 0 TONE-1 gain
		//
		// <7>    0 TONE-2
		//        1 = enable
		//        0 = disable
		//
		// <6:0>  0 TONE-2 / FSK gain
		//        0 ~ 127
		//
		// enable tone-2, set gain
		//
		BK4819_WriteRegister(BK4819_REG_70,   // 0 0000000 1 1100000
			( 0U << 15) |
			( 0U <<  8) |
			( 1U <<  7) |
//			(96u <<  0));
			(127U <<  0));  // best waveform

		// REG_72
		//
		// <15:0> 0x2854 TONE2/FSK frequency control word
		//        = freq(Hz) * 10.32444 for XTAL 13M / 26M or
		//        = freq(Hz) * 10.48576 for XTAL 12.8M / 19.2M / 25.6M / 38.4M
		//
		// tone-2 = 1200Hz
		//
		BK4819_WriteRegister(BK4819_REG_72, ((1200U * 103244U) + 5000U) / 10000U);   // with rounding

		// aircopy is done in direct FM mode
		//
		BK4819_WriteRegister(BK4819_REG_58, // 0x00C1);   // 000 000 00 11 00 000 1
			(0U << 13) |		// 1 FSK TX mode selection
								//   0 = FSK 1.2K and FSK 2.4K TX .. no tones, direct FM
								//   1 = FFSK 1200 / 1800 TX
								//   2 = ???
								//   3 = FFSK 1200 / 2400 TX
								//   4 = ???
								//   5 = NOAA SAME TX
								//   6 = ???
								//   7 = ???
								//
			(0U << 10) |		// 0 FSK RX mode selection
								//   0 = FSK 1.2K, FSK 2.4K RX and NOAA SAME RX .. no tones, direct FM
								//   1 = ???
								//   2 = ???
								//   3 = ???
								//   4 = FFSK 1200 / 2400 RX
								//   5 = ???
								//   6 = ???
								//   7 = FFSK 1200 / 1800 RX
								//
			(0U << 8) |			// 0 FSK RX gain
								//   0 ~ 3
								//
			(3U << 6) |			// 0 ???
								//   0 ~ 3
								//
			(0U << 4) |			// 0 FSK preamble type selection
								//   0 = 0xAA or 0x55 due to the MSB of FSK sync byte 0
								//   1 = ???
								//   2 = 0x55
								//   3 = 0xAA
								//
			(0U << 1) |			// 1 FSK RX bandwidth setting
								//   0 = FSK 1.2K .. no tones, direct FM
								//   1 = FFSK 1200 / 1800
								//   2 = NOAA SAME RX
								//   3 = ???
								//   4 = FSK 2.4K and FFSK 1200 / 2400
								//   5 = ???
								//   6 = ???
								//   7 = ???
								//
			(1U << 0));			// 1 FSK enable
								//   0 = disable
								//   1 = enable

		// REG_5C
		//
		// <15:7> ???
		//
		// <6>    1 CRC option enable
		//        0 = disable
		//        1 = enable
		//
		// <5:0>  ???
		//
		// Enable CRC among other things we don't know yet
		//
		BK4819_WriteRegister(BK4819_REG_5C, 0x5665);   // 010101100 1 100101

		// REG_5D
		//
		// <15:8> 15 FSK data length (byte) Low 8 bits (total 11 bits for BK4819v3)
		//        15 means 16 bytes in length
		//
		// <7:5>  0 FSK data
		//
		// <4:0>  0 ???
		//
		BK4819_WriteRegister(BK4819_REG_5D, ((packet_size - 1U) << 8));
	}
#endif

void BK4819_reset_fsk(void)
{
	BK4819_WriteRegister(BK4819_REG_3F, 0U);             // disable interrupts

	BK4819_WriteRegister(BK4819_REG_59, // 0x0068);     // 0 0 0 0 0 0 0 0 0110 1 000
		(0U << 15) |   // 0 or 1   1 = clear TX FIFO
		(0U << 14) |   // 0 or 1   1 = clear RX FIFO
		(0U << 13) |   // 0 or 1   1 = scramble
		(0U << 12) |   // 0 or 1   1 = enable RX
		(0U << 11) |   // 0 or 1   1 = enable TX
		(0U << 10) |   // 0 or 1   1 = invert data when RX
		(0U <<  9) |   // 0 or 1   1 = invert data when TX
		(0U <<  8) |   // 0 or 1   ???
		(6U <<  4) |   // 0 ~ 15   preamble Length Selection
		(1U <<  3) |   // 0 or 1   sync length selection
		(0U <<  0));   // 0 ~ 7    ???

	BK4819_Idle();
}

void BK4819_start_fsk_rx(uint32_t packet_size)
{
	uint16_t fsk_reg59;

	BK4819_reset_fsk();

	BK4819_WriteRegister(BK4819_REG_02, 0U);    // clear interrupt flags

	// set the packet size
	BK4819_WriteRegister(BK4819_REG_5D, ((packet_size - 1U) << 8));

	BK4819_RX_TurnOn();

//	BK4819_WriteRegister(BK4819_REG_3F,                             BK4819_REG_3F_FSK_RX_FINISHED | BK4819_REG_3F_FSK_FIFO_ALMOST_FULL);
	BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_3F_FSK_RX_SYNC | BK4819_REG_3F_FSK_RX_FINISHED | BK4819_REG_3F_FSK_FIFO_ALMOST_FULL);

	// REG_59
	//
	// <15>  0 TX FIFO
	//       1 = clear
	//
	// <14>  0 RX FIFO
	//       1 = clear
	//
	// <13>  0 FSK Scramble
	//       1 = Enable
	//
	// <12>  0 FSK RX
	//       1 = Enable
	//
	// <11>  0 FSK TX
	//       1 = Enable
	//
	// <10>  0 FSK data when RX
	//       1 = Invert
	//
	// <9>   0 FSK data when TX
	//       1 = Invert
	//
	// <8>   0 ???
	//
	// <7:4> 0 FSK preamble length selection
	//       0  =  1 byte
	//       1  =  2 bytes
	//       2  =  3 bytes
	//       15 = 16 bytes
	//
	// <3>   0 FSK sync length selection
	//       0 = 2 bytes (FSK Sync Byte 0, 1)
	//       1 = 4 bytes (FSK Sync Byte 0, 1, 2, 3)
	//
	// <2:0> 0 ???
	//
	fsk_reg59 = (0U << 15) |   // 0 or 1   1 = clear TX FIFO
	            (0U << 14) |   // 0 or 1   1 = clear RX FIFO
	            (0U << 13) |   // 0 or 1   1 = scramble
				(0U << 12) |   // 0 or 1   1 = enable RX
				(0U << 11) |   // 0 or 1   1 = enable TX
				(0U << 10) |   // 0 or 1   1 = invert data when RX
				(0U <<  9) |   // 0 or 1   1 = invert data when TX
				(0U <<  8) |   // 0 or 1   ???
//				(6U <<  4) |   // 0 ~ 15   preamble Length Selection
				(4U <<  4) |   // 0 ~ 15   preamble Length Selection .. 1of11 .. a little shorter than the TX length
				(1U <<  3) |   // 0 or 1   sync length selection
				(0U <<  0);    // 0 ~ 7    ???

	BK4819_WriteRegister(BK4819_REG_59, (1U << 14) | fsk_reg59);               // clear RX fifo
	BK4819_WriteRegister(BK4819_REG_59, (1U << 13) | (1U << 12) | fsk_reg59);  // enable scrambler, enable RX
}

#ifdef ENABLE_MDC1200

void BK4819_PlayRogerMDC1200(void)
{
	uint16_t fsk_reg59;
	uint8_t  packet[42];

	const uint8_t  op  = MDC1200_OP_CODE_POST_ID;
	const uint8_t  arg = 0x80;
	const uint16_t id  = 0xB183;

	// create the MDC1200 packet
	const uint32_t size = MDC1200_encode_single_packet(packet, op, arg, id);

	BK4819_SetAF(BK4819_AF_MUTE);
//	BK4819_SetAF(BK4819_AF_BEEP);

	BK4819_EnableTXLink();
	SYSTEM_DelayMs(10U);

	// MDC1200 uses 1200/1800 Hz FSK tone frequencies 1200 bits/s 
	//
	BK4819_WriteRegister(BK4819_REG_58, // 0x37C3);   // 001 101 11 11 00 001 1
		(1U << 13) |		// 1 FSK TX mode selection
							//   0 = FSK 1.2K and FSK 2.4K TX .. no tones, direct FM
							//   1 = FFSK 1200/1800 TX
							//   2 = ???
							//   3 = FFSK 1200/2400 TX
							//   4 = ???
							//   5 = NOAA SAME TX
							//   6 = ???
							//   7 = ???
							//
		(7U << 10) |		// 0 FSK RX mode selection
							//   0 = FSK 1.2K, FSK 2.4K RX and NOAA SAME RX .. no tones, direct FM
							//   1 = ???
							//   2 = ???
							//   3 = ???
							//   4 = FFSK 1200/2400 RX
							//   5 = ???
							//   6 = ???
							//   7 = FFSK 1200/1800 RX
							//
		(0U << 8) |			// 0 FSK RX gain
							//   0 ~ 3
							//
		(0U << 6) |			// 0 ???
							//   0 ~ 3
							//
		(0U << 4) |			// 0 FSK preamble type selection
							//   0 = 0xAA or 0x55 due to the MSB of FSK sync byte 0
							//   1 = ???
							//   2 = 0x55
							//   3 = 0xAA
							//
		(1U << 1) |			// 1 FSK RX bandwidth setting
							//   0 = FSK 1.2K .. no tones, direct FM
							//   1 = FFSK 1200/1800
							//   2 = NOAA SAME RX
							//   3 = ???
							//   4 = FSK 2.4K and FFSK 1200/2400
							//   5 = ???
							//   6 = ???
							//   7 = ???
							//
		(1U << 0));			// 1 FSK enable
							//   0 = disable
							//   1 = enable

	// REG_72
	//
	// <15:0> 0x2854 TONE-2 / FSK frequency control word
	//        = freq(Hz) * 10.32444 for XTAL 13M / 26M or
	//        = freq(Hz) * 10.48576 for XTAL 12.8M / 19.2M / 25.6M / 38.4M
	//
	// tone-2 = 1200Hz
	//
	BK4819_WriteRegister(BK4819_REG_72, ((1200U * 103244U) + 5000U) / 10000U);   // with rounding

	// REG_70
	//
	// <15>   0 TONE-1
	//        1 = enable
	//        0 = disable
	//
	// <14:8> 0 TONE-1 gain
	//
	// <7>    0 TONE-2
	//        1 = enable
	//        0 = disable
	//
	// <6:0>  0 TONE-2 / FSK gain
	//        0 ~ 127
	//
	// enable tone-2, set gain
	//
	BK4819_WriteRegister(BK4819_REG_70,   // 0 0000000 1 1100000
		( 0U << 15) |    // 0
		( 0U <<  8) |    // 0
		( 1U <<  7) |    // 1
//		(96u <<  0));    // 96
		(127U <<  0));    // produces the best undistorted waveform, this is not gain but affects filtering

	// REG_59
	//
	// <15>  0 TX FIFO
	//       1 = clear
	//
	// <14>  0 RX FIFO
	//       1 = clear
	//
	// <13>  0 FSK Scramble
	//       1 = Enable
	//
	// <12>  0 FSK RX
	//       1 = Enable
	//
	// <11>  0 FSK TX
	//       1 = Enable
	//
	// <10>  0 FSK data when RX
	//       1 = Invert
	//
	// <9>   0 FSK data when TX
	//       1 = Invert
	//
	// <8>   0 ???
	//
	// <7:4> 0 FSK preamble length selection
	//       0  =  1 byte
	//       1  =  2 bytes
	//       2  =  3 bytes
	//       15 = 16 bytes
	//
	// <3>   0 FSK sync length selection
	//       0 = 2 bytes (FSK Sync Byte 0, 1)
	//       1 = 4 bytes (FSK Sync Byte 0, 1, 2, 3)
	//
	// <2:0> 0 ???
	//
	fsk_reg59 = (0U << 15) |   // 0 ~ 1   1 = clear TX FIFO
	            (0U << 14) |   // 0 ~ 1   1 = clear RX FIFO
	            (0U << 13) |   // 0 ~ 1   1 = scramble
				(0U << 12) |   // 0 ~ 1   1 = enable RX
				(0U << 11) |   // 0 ~ 1   1 = enable TX
				(0U << 10) |   // 0 ~ 1   1 = invert data when RX
				(0U <<  9) |   // 0 ~ 1   1 = invert data when TX
				(0U <<  8) |   // 0 ~ 1   ???
				(0U <<  4) |   // 0 ~ 15  preamble length
				(0U <<  3) |   // 0 ~ 1       sync length
				(0U <<  0);    // 0 ~ 7   ???

	// Set entire packet length (not including the pre-amble and sync bytes we can't seem to disable)
	BK4819_WriteRegister(BK4819_REG_5D, ((size - 1U) << 8));

	BK4819_WriteRegister(BK4819_REG_59, (1U << 15) | fsk_reg59);   // clear TX fifo by setting the FIFO reset bit
	BK4819_WriteRegister(BK4819_REG_59, (0U << 15) | fsk_reg59);   // release the reset bit

	// REG_5A
	//
	// <15:8> 0x55 FSK Sync Byte 0 (Sync Byte 0 first, then 1,2,3)
	// <7:0>  0x55 FSK Sync Byte 1
	//
	BK4819_WriteRegister(BK4819_REG_5A, 0x0000);                   // bytes 1 & 2

	// REG_5B
	//
	// <15:8> 0x55 FSK Sync Byte 2 (Sync Byte 0 first, then 1,2,3)
	// <7:0>  0xAA FSK Sync Byte 3
	//
	BK4819_WriteRegister(BK4819_REG_5B, 0x0000);                   // bytes 2 & 3 (not used)

	// CRC setting (plus other stuff we don't know what)
	//
	// REG_5C
	//
	// <15:7> ???
	//
	// <6>    1 CRC option enable
	//        0 = disable
	//        1 = enable
	//
	// <5:0>  ???
	//
	// disable CRC
	//
//	BK4819_WriteRegister(BK4819_REG_5C, 0xAA30);   // 101010100 0 110000
	BK4819_WriteRegister(BK4819_REG_5C, 0U);        // setting to '0' doesn't make any difference !

	{	// load the entire packet data into the TX FIFO buffer
		const uint16_t *p = (const uint16_t *)packet;
		for (uint32_t i = 0; i < (size / sizeof(p[0])); i++)
			BK4819_WriteRegister(BK4819_REG_5F, p[i]);  // load 16-bits at a time
	}

	// enable tx interrupt
	BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_3F_FSK_TX_FINISHED);

	// enable TX
	BK4819_WriteRegister(BK4819_REG_59, (1U << 11) | fsk_reg59);

	{	// packet time is ..
		// 173ms for PTT ID, acks, emergency
		// 266ms for call alert and sel-calls

		// allow up to 350ms for the TX to complete
		// if it takes any longer then somethings gone wrong, we shut the TX down
		uint32_t timeout = 350U / 5U;

		while (timeout-- > 0)
		{
			SYSTEM_DelayMs(5U);
			if (BK4819_ReadRegister(BK4819_REG_0C) & (1U << 0))
			{	// we have interrupt flags
				BK4819_WriteRegister(BK4819_REG_02, 0U);
				if (BK4819_ReadRegister(BK4819_REG_02) & BK4819_REG_02_FSK_TX_FINISHED)
					timeout = 0U;       // TX is complete
			}
		}
	}
	
	// disable TX
	BK4819_WriteRegister(BK4819_REG_59, fsk_reg59);

	BK4819_WriteRegister(BK4819_REG_3F, 0U);   // disable interrupts
	BK4819_WriteRegister(BK4819_REG_70, 0U);
	BK4819_WriteRegister(BK4819_REG_58, 0U);
}

#endif

void BK4819_Enable_AfDac_DiscMode_TxDsp(void)
{
	BK4819_WriteRegister(BK4819_REG_30, 0x0000);
	BK4819_WriteRegister(BK4819_REG_30, 0x0302);
}

void BK4819_GetVoxAmp(uint16_t *pResult)
{
	*pResult = (BK4819_ReadRegister(BK4819_REG_64) & 0x7FFF);
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
	BK4819_WriteRegister(BK4819_REG_70, 0xD3D3);  // 1101 0011 1101 0011
	BK4819_EnableTXLink();
	SYSTEM_DelayMs(50U);
	BK4819_PlayDTMF(Code);
	BK4819_ExitTxMute();
}
