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

#include "bk4819.h"
#include "bsp/dp32g030/gpio.h"
#include "bsp/dp32g030/portcon.h"
#include "functions.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/systick.h"
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
#include "misc.h"
#ifdef ENABLE_MDC1200
	#include "mdc1200.h"
#endif

#ifndef ARRAY_SIZE
	#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

static uint16_t g_bk4819_gpio_out_state;

//const uint32_t rf_filter_transition_freq = 28000000;  // original
  const uint32_t rf_filter_transition_freq = 26500000;

BK4819_filter_bandwidth_t m_bandwidth = BK4819_FILTER_BW_NARROW;

bool g_rx_idle_mode;

__inline uint16_t scale_freq(const uint16_t freq)
{	// with rounding
//	return (((uint32_t)freq * 1032444u) + 50000u) / 100000u;
	return (((uint32_t)freq * 338311u) + (1u << 14)) >> 15;    // max freq = 12695
}

void BK4819_Init(void)
{
	g_bk4819_gpio_out_state = 0x9000;

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);

	// reset the chip
	BK4819_write_reg(0x00, (1u << 15));
	BK4819_write_reg(0x00, 0);

	BK4819_write_reg(0x37, 0x1D0F);
	BK4819_write_reg(0x36, 0x0022);

#ifdef ENABLE_AM_FIX
	BK4819_DisableAGC();
#else
	BK4819_EnableAGC();  // only do this in linear modulation modes, not FM
#endif

	BK4819_set_mic_gain(31);

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
	BK4819_write_reg(0x48,	//  0xB3A8);     // 1011 00 111010 1000
		(11u << 12) |     // ??? 0..15
		( 0u << 10) |     // AF Rx Gain-1
		(58u <<  4) |     // AF Rx Gain-2
		( 8u <<  0));     // AF DAC Gain (after Gain-1 and Gain-2)

	// squelch mode
	BK4819_write_reg(0x77, 0x88EF);     // rssi + noise + glitch .. RT-890
//	BK4819_write_reg(0x77, 0xA8EF);     // rssi + noise + glitch .. default
//	BK4819_write_reg(0x77, 0xAAEF);     // rssi + glitch
//	BK4819_write_reg(0x77, 0xCCEF);     // rssi + noise
//	BK4819_write_reg(0x77, 0xFFEF);     // rssi

	BK4819_config_sub_audible();

	const uint8_t dtmf_coeffs[] = {111, 107, 103, 98, 80, 71, 58, 44, 65, 55, 37, 23, 228, 203, 181, 159};
	for (unsigned int i = 0; i < ARRAY_SIZE(dtmf_coeffs); i++)
		BK4819_write_reg(0x09, (i << 12) | dtmf_coeffs[i]);

	BK4819_write_reg(0x1F, 0x5454);  // 0101 0100 01 01 0100
	BK4819_write_reg(0x3E, 41015);   // band selection threshold = VCO max frequency (Hz) / 96 / 640
	BK4819_write_reg(0x33, 0x9000);  // 1001 0000 0000 0000 .. GPIO
	BK4819_write_reg(0x3F, 0);       // disable interrupts

#if 0
	// RT-890

//	BK4819_write_reg(0x37, 0x1D0F);

//	DisableAGC(0);

	BK4819_write_reg(0x13, 0x03BE);
	BK4819_write_reg(0x12, 0x037B);
	BK4819_write_reg(0x11, 0x027B);
	BK4819_write_reg(0x10, 0x007A);
	BK4819_write_reg(0x14, 0x0019);
	BK4819_write_reg(0x49, 0x2A38);
	BK4819_write_reg(0x7B, 0x8420);

	BK4819_write_reg(0x1E, 0x4C58);  // ???
	BK4819_write_reg(0x2A, 0x4F18);  // ???
	BK4819_write_reg(0x53, 0xE678);  // ???
	BK4819_write_reg(0x2C, 0x5705);  // ???
	BK4819_write_reg(0x4B, 0x7102);  // AF gains
	BK4819_write_reg(0x26, 0x13A0);  // ???
#endif
}

static uint16_t BK4819_read_16(void)
{
	unsigned int i;
	uint16_t     Value = 0;

	PORTCON_PORTC_IE = (PORTCON_PORTC_IE & ~PORTCON_PORTC_IE_C2_MASK) | PORTCON_PORTC_IE_C2_BITS_ENABLE;
	GPIOC->DIR = (GPIOC->DIR & ~GPIO_DIR_2_MASK) | GPIO_DIR_2_BITS_INPUT;
	SYSTICK_Delay250ns(1);  // 4
	for (i = 0; i < 16; i++)
	{
		Value <<= 1;
		Value |= GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_Delay250ns(1);  // 4
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_Delay250ns(1);  // 4
	}
	PORTCON_PORTC_IE = (PORTCON_PORTC_IE & ~PORTCON_PORTC_IE_C2_MASK) | PORTCON_PORTC_IE_C2_BITS_DISABLE;
	GPIOC->DIR = (GPIOC->DIR & ~GPIO_DIR_2_MASK) | GPIO_DIR_2_BITS_OUTPUT;

	return Value;
}

uint16_t BK4819_read_reg(const uint8_t Register)
{
	uint16_t Value;
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	SYSTICK_Delay250ns(1);  // 4
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	BK4819_write_8(Register | 0x80);
	Value = BK4819_read_16();
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	SYSTICK_Delay250ns(1);  // 4
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
	return Value;
}

void BK4819_write_reg(const uint8_t Register, uint16_t Data)
{
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	SYSTICK_Delay250ns(1);  // 4
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	BK4819_write_8(Register);
	BK4819_write_16(Data);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCN);
	SYSTICK_Delay250ns(1);  // 4
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
}

void BK4819_write_8(uint8_t Data)
{
	unsigned int i;

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	for (i = 0; i < 8; i++)
	{
		if ((Data & 0x80) == 0)
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
		else
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
		SYSTICK_Delay250ns(1);  // 4
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_Delay250ns(1);  // 4
		Data <<= 1;
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_Delay250ns(1);  // 4
	}
}

void BK4819_write_16(uint16_t Data)
{
	unsigned int i;
	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
	for (i = 0; i < 16; i++)
	{
		if ((Data & 0x8000) == 0)
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
		else
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SDA);
		SYSTICK_Delay250ns(1);  // 4
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		Data <<= 1;
		SYSTICK_Delay250ns(1);  // 4
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_BK4819_SCL);
		SYSTICK_Delay250ns(1);  // 4
	}
}

void BK4819_set_AFC(unsigned int level)
{
	if (level > 8)
		level = 8;

	if (level == 0)
		BK4819_write_reg(0x73, BK4819_read_reg(0x73) | (1u << 4));  // disable
	else
		BK4819_write_reg(0x73, ((8u - level) << 11) | (0u << 4));
}

void BK4819_DisableAFC(void)
{
}

void BK4819_DisableAGC(void)
{
	// REG_7E
	//
	// <15> 0 AGC Fix Mode.
	// 1=Fix; 0=Auto.
	//
	// <14:12> 0b011 AGC Fix Index.
	// 011=Max, then 010,001,000,111,110,101,100(min).
	//
	// <5:3> 0b101 DC Filter Band Width for Tx (MIC In).
	// 000=Bypass DC filter;
	//
	// <2:0> 0b110 DC Filter Band Width for Rx (IF In).
	// 000=Bypass DC filter;
	//
	BK4819_write_reg(0x7E,  // 0x302E   0 011 000000 101 110
//		(1u << 15) |      // 0  AGC fix mode
		(0u << 15) |      // 0  AGC fix mode
		(3u << 12) |      // 3  AGC fix index
		(5u <<  3) |      // 5  DC Filter band width for Tx (MIC In)
		(6u <<  0));      // 6  DC Filter band width for Rx (I.F In)

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
	//         5 =  -4dB <<<
	//         4 =  -6dB
	//         3 =  -9dB
	//         2 = -14dB
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
	BK4819_write_reg(0x13, (3u << 8) | (5u << 5) | (3u << 3) | (6u << 0));  // 000000 11 101 11 110
	BK4819_write_reg(0x12, 0x037B);  // 000000 11 011 11 011
	BK4819_write_reg(0x11, 0x027B);  // 000000 10 011 11 011
	BK4819_write_reg(0x10, 0x007A);  // 000000 00 011 11 010
	BK4819_write_reg(0x14, 0x0019);  // 000000 00 000 11 001

	BK4819_write_reg(0x49, (0u << 14) | (84u << 7) | (56u << 0));  // 0x2A38 AGC thresholds

	BK4819_write_reg(0x7B, 0x8420);  // RSSI table
}

void BK4819_EnableAGC(void)
{
	// TODO: See if this attenuates overloading
	// signals as well as boosting weak ones
	//
	// REG_7E
	//
	// <15> 0 AGC Fix Mode.
	// 1=Fix; 0=Auto.
	//
	// <14:12> 0b011 AGC Fix Index.
	// 011=Max, then 010,001,000,111,110,101,100(min).
	//
	// <5:3> 0b101 DC Filter Band Width for Tx (MIC In).
	// 000=Bypass DC filter;
	//
	// <2:0> 0b110 DC Filter Band Width for Rx (IF In).
	// 000=Bypass DC filter;

	BK4819_write_reg(0x7E,
		(0u << 15) |      // 0  AGC fix mode
		(3u << 12) |      // 3  AGC fix index
		(5u <<  3) |      // 5  DC Filter band width for Tx (MIC In)
		(6u <<  0));      // 6  DC Filter band width for Rx (I.F In)

	// TBR: fagci has this listed as two values, agc_rssi and lna_peak_rssi
	// This is why AGC appeared to do nothing as-is for Rx
	//
	// REG_62
	//
	// <15:8> 0xFF AGC RSSI
	//
	// <7:0> 0xFF LNA Peak RSSI
	//
	// TBR: Using S9+30 (173) and S9 (143) as suggested values
	BK4819_write_reg(0x62, (173u << 8) | (143u << 0));

	// AGC auto-adjusts the following LNA values, no need to set them ourselves
	//BK4819_write_reg(0x13, (3u << 8) | (5u << 5) | (3u << 3) | (6u << 0));  // 000000 11 101 11 110
	//BK4819_write_reg(0x12, 0x037B);  // 000000 11 011 11 011
	//BK4819_write_reg(0x11, 0x027B);  // 000000 10 011 11 011
	//BK4819_write_reg(0x10, 0x007A);  // 000000 00 011 11 010
	//BK4819_write_reg(0x14, 0x0019);  // 000000 00 000 11 001

	BK4819_write_reg(0x49, (0u << 14) | (84u << 7) | (56u << 0));  // 0x2A38 AGC thresholds

	BK4819_write_reg(0x7B, 0x8420);  // RSSI table

	for (unsigned int i = 0; i < 8; i++)
		BK4819_write_reg(0x06, ((i & 7u) << 13) | (0x4A << 7) | (0x36 << 0));
}

void BK4819_set_GPIO_pin(bk4819_gpio_pin_t Pin, bool bSet)
{
	if (bSet)
		g_bk4819_gpio_out_state |=  (0x40u >> Pin);
	else
		g_bk4819_gpio_out_state &= ~(0x40u >> Pin);

	BK4819_write_reg(0x33, g_bk4819_gpio_out_state);
}

void BK4819_EnableVox(uint16_t VoxEnableThreshold, uint16_t VoxDisableThreshold)
{
	// VOX Algorithm
	//if (vox_amp > VoxEnableThreshold)                VOX = 1;
	//else
	//if (vox_amp < VoxDisableThreshold) (After Delay) VOX = 0;

	if (VoxEnableThreshold  > 2047)
		VoxEnableThreshold  = 2047;
	if (VoxDisableThreshold > 2047)
		VoxDisableThreshold = 2047;

	// 0xA000 is undocumented
	BK4819_write_reg(0x46, (20u << 11) | VoxEnableThreshold); // ???, amp threshold for vox on

	BK4819_write_reg(0x79, (3u << 11) | VoxDisableThreshold); // vox det interval time, amp threshold for vox off

	// Bottom 12 bits are undocumented, 15:12 vox disable delay *128ms
	BK4819_write_reg(0x7A, (2u << 12) | 0x089A); // vox disable delay = 128*5 = 640ms
	// 0010 100010011010

	// Enable VOX
	BK4819_write_reg(0x31, BK4819_read_reg(0x31) | (1u << 2));
}

void BK4819_set_TX_deviation(const bool narrow)
{
	const uint8_t scrambler = (BK4819_read_reg(0x31) >> 1) & 1u;
	uint16_t deviation = narrow ? 900 : 1232;  // 0 ~ 4095
	if (scrambler)
		deviation -= 200;
	BK4819_write_reg(0x40, (3u << 12) | deviation);
}

void BK4819_SetFilterBandwidth(const BK4819_filter_bandwidth_t Bandwidth)
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
	// <8:6>   1 AF-TX-LPF-2 filter band width
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

	m_bandwidth = Bandwidth;

	// when received signal is weak, the RX bandwidth is reduced

	switch (Bandwidth)
	{
		default:
		case BK4819_FILTER_BW_WIDE:		// 25kHz
			val =                // 0x3028);         // 0 011 000 000 10 1 0 00
				(0u << 15) |     // 0
				(4u << 12) |     // 3 RF filter bandwidth
				(2u <<  9) |     // 0 RF filter bandwidth when signal is weak
				(3u <<  6) |     // 0 AF-TX-LPF-2 filter band width
				(2u <<  4) |     // 2 BW Mode Selection
				(1u <<  3) |     // 1
				(0u <<  2) |     // 0 Gain after FM Demodulation
				(0u <<  0);      // 0
			break;

		case BK4819_FILTER_BW_NARROW:	// 12.5kHz
			val =
				(0u << 15) |     // 0
				(4u << 12) |     // 4 RF filter bandwidth
				(2u <<  9) |     // 0 RF filter bandwidth when signal is weak
				(2u <<  6) |     // 1 AF-TX-LPF-2 filter Band Width
				(0u <<  4) |     // 0 BW Mode Selection
				(1u <<  3) |     // 1
				(0u <<  2) |     // 0 Gain after FM Demodulation
				(0u <<  0);      // 0
			break;

		case BK4819_FILTER_BW_NARROWER:	// 6.25kHz
			val =
				(0u << 15) |     // 0
				(3u << 12) |     // 3 RF filter bandwidth
				(2u <<  9) |     // 0 RF filter bandwidth when signal is weak
				(1u <<  6) |     // 1 AF-TX-LPF-2 filter Band Width
				(1u <<  4) |     // 1 BW Mode Selection
				(1u <<  3) |     // 1
				(0u <<  2) |     // 0 Gain after FM Demodulation
				(0u <<  0);      // 0
			break;
	}

	BK4819_write_reg(0x43, val);
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
	//                                                                         280MHz     gain 1 = 1  gain 2 = 0  gain 1 = 4  gain 2 = 2
	const uint8_t gain   = (frequency == 0) ? 0 : (frequency < rf_filter_transition_freq) ? (1u << 3) | (0u << 0) : (4u << 3) | (2u << 0);
	const uint8_t enable = 1;
	BK4819_write_reg(0x36, ((uint16_t)bias << 8) | ((uint16_t)enable << 7) | ((uint16_t)gain << 0));
}

void BK4819_set_rf_frequency(const uint32_t frequency, const bool trigger_update)
{
	BK4819_write_reg(0x38, (frequency >>  0) & 0xFFFF);
	BK4819_write_reg(0x39, (frequency >> 16) & 0xFFFF);

	if (trigger_update)
	{	// trigger a PLL/VCO update
		const uint16_t reg = BK4819_read_reg(0x30);
		BK4819_write_reg(0x30, reg & ~BK4819_REG_30_ENABLE_VCO_CALIB);
		BK4819_write_reg(0x30, reg);
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
	// squelch mode
//	BK4819_write_reg(0x77, 0x88EF);     // rssi + noise + glitch .. RT-890
//	BK4819_write_reg(0x77, 0xA8EF);     // rssi + noise + glitch .. default
//	BK4819_write_reg(0x77, 0xAAEF);     // rssi + glitch
//	BK4819_write_reg(0x77, 0xCCEF);     // rssi + noise
//	BK4819_write_reg(0x77, 0xFFEF);     // rssi

	// REG_70
	//
	// <15>   0 Enable TONE1
	//        1 = Enable
	//        0 = Disable
	//
	// <14:8> 0 TONE1 tuning
	//        0 ~ 127
	//
	// <7>    0 Enable TONE2
	//        1 = Enable
	//        0 = Disable
	//
	// <6:0>  0 TONE2/FSK tuning
	//        0 ~ 127
	//
	BK4819_write_reg(0x70, 0);

	// Glitch threshold for Squelch = close
	//
	// 0 ~ 255
	//
	BK4819_write_reg(0x4D, 0xA000 | squelch_close_glitch_thresh);

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
	BK4819_write_reg(0x4E,  // 01 101 11 1 00000000
	#ifndef ENABLE_FASTER_CHANNEL_SCAN
		// original
		(1u << 14) |                  // 1 ???
		(5u << 11) |                  // 5  squelch = open  delay .. 0 ~ 7
		(3u <<  9) |                  // 3  squelch = close delay .. 0 ~ 3
		squelch_open_glitch_thresh);  // 0 ~ 255
	#else
		// faster (but twitchier)
		(1u << 14) |                  // 1 ???
		(2u << 11) |                  // 5  squelch = open  delay .. 0 ~ 7
		(3u <<  9) |                  // 3  squelch = close delay .. 0 ~ 3
		squelch_open_glitch_thresh);  // 0 ~ 255
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
	BK4819_write_reg(0x4F, ((uint16_t)squelch_close_noise_thresh << 8) | squelch_open_noise_thresh);

	// REG_78
	//
	// <15:8> 72 RSSI threshold for Squelch = open    0.5dB/step
	//
	// <7:0>  70 RSSI threshold for Squelch = close   0.5dB/step
	//
	BK4819_write_reg(0x78, ((uint16_t)squelch_open_rssi_thresh   << 8) | squelch_close_rssi_thresh);

	BK4819_SetAF(BK4819_AF_MUTE);

	BK4819_RX_TurnOn();
}

void BK4819_SetAF(BK4819_af_type_t AF)
{
	BK4819_write_reg(0x47, 0);
//	BK4819_write_reg(0x47, 0x6040 | (AF << 8));   // 0110 0000 0100 0000
	BK4819_write_reg(0x47, (1u << 14) | (1u << 13) | ((AF & 15u) << 8) | (1u << 6));
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
	BK4819_write_reg(0x37, 0x1F0F);  // 0001 1111 0000 1111

	BK4819_write_reg(0x30, 0);
	BK4819_write_reg(0x30,
		BK4819_REG_30_ENABLE_VCO_CALIB |
//		BK4819_REG_30_ENABLE_UNKNOWN   |
		BK4819_REG_30_ENABLE_RX_LINK   |
		BK4819_REG_30_ENABLE_AF_DAC    |
		BK4819_REG_30_ENABLE_DISC_MODE |
		BK4819_REG_30_ENABLE_PLL_VCO   |
//		BK4819_REG_30_ENABLE_PA_GAIN   |
//		BK4819_REG_30_ENABLE_MIC_ADC   |
//		BK4819_REG_30_ENABLE_TX_DSP    |
		BK4819_REG_30_ENABLE_RX_DSP    |
	0);
}

void BK4819_set_rf_filter_path(const uint32_t Frequency)
{
	if (Frequency == 0 || Frequency == 0xFFFFFFFF)
	{	// OFF
		BK4819_set_GPIO_pin(BK4819_GPIO4_PIN32_VHF_LNA, false);
		BK4819_set_GPIO_pin(BK4819_GPIO3_PIN31_UHF_LNA, false);
	}
	else
	if (Frequency < rf_filter_transition_freq)
	{	// VHF
		BK4819_set_GPIO_pin(BK4819_GPIO4_PIN32_VHF_LNA, true);
		BK4819_set_GPIO_pin(BK4819_GPIO3_PIN31_UHF_LNA, false);
	}
	else
//	if (Frequency >= rf_filter_transition_freq)
	{	// UHF
		BK4819_set_GPIO_pin(BK4819_GPIO4_PIN32_VHF_LNA, false);
		BK4819_set_GPIO_pin(BK4819_GPIO3_PIN31_UHF_LNA, true);
	}
}

void BK4819_set_scrambler(const int index)
{
	const uint16_t Value = BK4819_read_reg(0x31);
	if (index <= 0)
	{	// disable
		BK4819_write_reg(0x31, Value & ~(1u << 1));
	}
	else
	{	// enable
		uint16_t freq = 2600 + ((index - 1) * 50);       // 2600Hz ++ 50 Hz steps
		if (freq > 12000)
			freq = 12000;

		BK4819_write_reg(0x31, Value | (1u << 1));   // enable
		BK4819_write_reg(0x71, scale_freq(freq));
	}
}

bool BK4819_CompanderEnabled(void)
{
	return (BK4819_read_reg(0x31) & (1u << 3)) ? true : false;
}

void BK4819_SetCompander(const unsigned int mode)
{
	// mode 0 .. OFF
	// mode 1 .. TX
	// mode 2 .. RX
	// mode 3 .. TX and RX

	const uint16_t r31 = BK4819_read_reg(0x31);

	if (mode == 0)
	{	// disable
		BK4819_write_reg(0x31, r31 & ~(1u << 3));
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
	const uint16_t compress_ratio = (mode == 1 || mode >= 3) ? 2 : 0;  // 2:1
	BK4819_write_reg(0x29, // (BK4819_read_reg(0x29) & ~(3u << 14)) | (compress_ratio << 14));
		(compress_ratio << 14) |
		(86u            <<  7) |   // compress 0dB
		(64u            <<  0));   // compress noise dB

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
	const uint16_t expand_ratio = (mode >= 2) ? 1 : 0;   // 1:2
	BK4819_write_reg(0x28, // (BK4819_read_reg(0x28) & ~(3u << 14)) | (expand_ratio << 14));
		(expand_ratio << 14) |
		(86u          <<  7) |   // expander 0dB
		(56u          <<  0));   // expander noise dB

	// enable
	BK4819_write_reg(0x31, r31 | (1u << 3));
}

void BK4819_DisableVox(void)
{
	const uint16_t Value = BK4819_read_reg(0x31);
	BK4819_write_reg(0x31, Value & 0xFFFB);
}

void BK4819_DisableDTMF(void)
{
	BK4819_write_reg(0x24, 0);
}

void BK4819_EnableDTMF(void)
{
	// no idea what this does
	BK4819_write_reg(0x21, 0x06D8);        // 0000 0110 1101 1000

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
	BK4819_write_reg(0x24,                      // 1 00011000 1 1 1 1110
		(1u        << BK4819_REG_24_SHIFT_UNKNOWN_15) |
		(threshold << BK4819_REG_24_SHIFT_THRESHOLD)  |      // 0 ~ 255
		(1u        << BK4819_REG_24_SHIFT_UNKNOWN_6)  |
		              BK4819_REG_24_ENABLE            |
		              BK4819_REG_24_SELECT_DTMF       |
		(15u       << BK4819_REG_24_SHIFT_MAX_SYMBOLS));     // 0 ~ 15
}

void BK4819_start_tone(const uint16_t frequency, const unsigned int level, const bool tx, const bool tx_mute)
{
	SYSTEM_DelayMs(1);

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

	SYSTEM_DelayMs(1);

	// mute TX
	BK4819_write_reg(0x50, (1u << 15) | 0x3B20);

	BK4819_write_reg(0x70, BK4819_REG_70_ENABLE_TONE1 | ((level & 0x7f) << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));

	BK4819_write_reg(0x30, 0);

	if (!tx)
	{
		BK4819_write_reg(0x30,
//			BK4819_REG_30_ENABLE_VCO_CALIB |
//			BK4819_REG_30_ENABLE_UNKNOWN   |
//			BK4819_REG_30_ENABLE_RX_LINK   |
			BK4819_REG_30_ENABLE_AF_DAC    |
			BK4819_REG_30_ENABLE_DISC_MODE |
//			BK4819_REG_30_ENABLE_PLL_VCO   |
//			BK4819_REG_30_ENABLE_PA_GAIN   |
//			BK4819_REG_30_ENABLE_MIC_ADC   |
			BK4819_REG_30_ENABLE_TX_DSP    |
//			BK4819_REG_30_ENABLE_RX_DSP    |
		0);
	}
	else
	{
		BK4819_write_reg(0x30,
			BK4819_REG_30_ENABLE_VCO_CALIB |
			BK4819_REG_30_ENABLE_UNKNOWN   |
//			BK4819_REG_30_ENABLE_RX_LINK   |
			BK4819_REG_30_ENABLE_AF_DAC    |
			BK4819_REG_30_ENABLE_DISC_MODE |
			BK4819_REG_30_ENABLE_PLL_VCO   |
			BK4819_REG_30_ENABLE_PA_GAIN   |
//			BK4819_REG_30_ENABLE_MIC_ADC   |
			BK4819_REG_30_ENABLE_TX_DSP    |
//			BK4819_REG_30_ENABLE_RX_DSP    |
		0);
	}

	BK4819_write_reg(0x71, scale_freq(frequency));

	SYSTEM_DelayMs(1);

//	BK4819_SetAF(tx ? BK4819_AF_BEEP : BK4819_AF_TONE);
	BK4819_SetAF(BK4819_AF_TONE);  // RX
//	BK4819_SetAF(BK4819_AF_BEEP);  // TX

	if (!tx_mute)
		BK4819_write_reg(0x50, 0x3B20);   // 0011 1011 0010 0000

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

	SYSTEM_DelayMs(1);
}

void BK4819_stop_tones(const bool tx)
{
	SYSTEM_DelayMs(1);

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

	SYSTEM_DelayMs(1);

	BK4819_SetAF(BK4819_AF_MUTE);

//	BK4819_EnterTxMute();

	SYSTEM_DelayMs(1);

	BK4819_write_reg(0x70, 0);

	BK4819_write_reg(0x30, 0);
	if (!tx)
	{
		BK4819_write_reg(0x30,
			BK4819_REG_30_ENABLE_VCO_CALIB |
//			BK4819_REG_30_ENABLE_UNKNOWN   |
			BK4819_REG_30_ENABLE_RX_LINK   |
			BK4819_REG_30_ENABLE_AF_DAC    |
			BK4819_REG_30_ENABLE_DISC_MODE |
			BK4819_REG_30_ENABLE_PLL_VCO   |
//			BK4819_REG_30_ENABLE_PA_GAIN   |
//			BK4819_REG_30_ENABLE_MIC_ADC   |
//			BK4819_REG_30_ENABLE_TX_DSP    |
			BK4819_REG_30_ENABLE_RX_DSP    |
		0);
	}
	else
	{
		BK4819_write_reg(0x30,
			BK4819_REG_30_ENABLE_VCO_CALIB |
			BK4819_REG_30_ENABLE_UNKNOWN   |
//			BK4819_REG_30_ENABLE_RX_LINK   |
			BK4819_REG_30_ENABLE_AF_DAC    |
			BK4819_REG_30_ENABLE_DISC_MODE |
			BK4819_REG_30_ENABLE_PLL_VCO   |
			BK4819_REG_30_ENABLE_PA_GAIN   |
			BK4819_REG_30_ENABLE_MIC_ADC   |
			BK4819_REG_30_ENABLE_TX_DSP    |
//			BK4819_REG_30_ENABLE_RX_DSP    |
		0);
	}

	SYSTEM_DelayMs(1);

	BK4819_ExitTxMute();

	SYSTEM_DelayMs(1);
}

void BK4819_PlayRoger(const unsigned int type)
{
	uint32_t tone1_Hz;
	uint32_t tone2_Hz;

	switch (type)
	{
		case 1:
			tone1_Hz = 1540;
			tone2_Hz = 1310;
			break;
		case 2:
			tone1_Hz = 500;
			tone2_Hz = 700;
			break;
		default:
			return;
	}

	const uint16_t prev_af = BK4819_read_reg(0x47);

	BK4819_start_tone(tone1_Hz, 10, true, false);
	SYSTEM_DelayMs(150);
	BK4819_write_reg(0x71, scale_freq(tone2_Hz));
	SYSTEM_DelayMs(150);
	BK4819_stop_tones(true);

	BK4819_write_reg(0x47, prev_af);
}

void BK4819_EnterTxMute(void)
{
	BK4819_write_reg(0x50, 0xBB20);
}

void BK4819_ExitTxMute(void)
{
	BK4819_write_reg(0x50, 0x3B20);   // 0011 1011 0010 0000
}

void BK4819_Sleep(void)
{
	BK4819_write_reg(0x30, 0);
	BK4819_write_reg(0x37, 0x1D00);  // 0 0 0111 0 1 0000 0 0 0 0
}

void BK4819_setTxAudio(const unsigned int mode)
{
	switch (mode)
	{
		case 0:
			break;
		case 1:
			BK4819_write_reg(0x53,0xE678); // ???
			BK4819_write_reg(0x4B,0x7102); // enable TX audio AGC
			BK4819_write_reg(0x27,0x7430); // ???
//			BK4819_write_reg(0x29,0xAB2A);
			break;
		case 2:
			BK4819_write_reg(0x4B,0x7120); // disable TX audio AGC
			BK4819_write_reg(0x27,0xC430); // ???
//			BK4819_write_reg(0x29,0xAB20);
			break;
	}
}

void BK4819_set_mic_gain(unsigned int level)
{
	if (level > 31)
		level = 31;

	// mic gain 0.5dB/step 0 to 31
	BK4819_write_reg(0x7D, 0xE940 | level);

//	BK4819_write_reg(0x19, 0x1041);  // 0001 0000 0100 0001 <15> MIC AGC  1 = disable  0 = enable  .. doesn't work
//	BK4819_write_reg(0x19, BK4819_read_reg(0x19) & ~(1u << 15));  // enable mic AGC

//	BK4819_setTxAudio(1);
}

void BK4819_TurnsOffTones_TurnsOnRX(void)
{
	BK4819_write_reg(0x70, 0);
	BK4819_SetAF(BK4819_AF_MUTE);

	BK4819_ExitTxMute();

	BK4819_write_reg(0x30, 0);
	BK4819_write_reg(0x30,
		BK4819_REG_30_ENABLE_VCO_CALIB |
//		BK4819_REG_30_ENABLE_UNKNOWN   |
		BK4819_REG_30_ENABLE_RX_LINK   |
		BK4819_REG_30_ENABLE_AF_DAC    |
		BK4819_REG_30_ENABLE_DISC_MODE |
		BK4819_REG_30_ENABLE_PLL_VCO   |
//		BK4819_REG_30_ENABLE_PA_GAIN   |
//		BK4819_REG_30_ENABLE_MIC_ADC   |
//		BK4819_REG_30_ENABLE_TX_DSP    |
		BK4819_REG_30_ENABLE_RX_DSP    |
	0);
}

void BK4819_Idle(void)
{
	BK4819_write_reg(0x30, 0);
}
/*
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
	BK4819_write_reg(0x7E, // 0x302E);   // 0 011 000000 101 110
		(0u << 15) |      // 0  AGC fix mode
		(3u << 12) |      // 3  AGC fix index
		(5u <<  3) |      // 5  DC Filter band width for Tx (MIC In)
		(6u <<  0));      // 6  DC Filter band width for Rx (I.F In)
}
*/
void BK4819_PrepareTransmit(void)
{
//	BK4819_ExitBypass();
	BK4819_ExitTxMute();

	BK4819_config_sub_audible();

	BK4819_write_reg(0x30, 0);
	BK4819_write_reg(0x30,
		BK4819_REG_30_ENABLE_VCO_CALIB |
		BK4819_REG_30_ENABLE_UNKNOWN   |
//		BK4819_REG_30_ENABLE_RX_LINK   |
//		BK4819_REG_30_ENABLE_AF_DAC    |
		BK4819_REG_30_ENABLE_DISC_MODE |
		BK4819_REG_30_ENABLE_PLL_VCO   |
		BK4819_REG_30_ENABLE_PA_GAIN   |
		BK4819_REG_30_ENABLE_MIC_ADC   |
		BK4819_REG_30_ENABLE_TX_DSP    |
//		BK4819_REG_30_ENABLE_RX_DSP    |
	0);
}

void BK4819_Conditional_RX_TurnOn(void)
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

	BK4819_write_reg(0x70,
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
	BK4819_write_reg(0x70, 0);
	BK4819_DisableDTMF();

	BK4819_write_reg(0x30,
		BK4819_REG_30_ENABLE_VCO_CALIB |
		BK4819_REG_30_ENABLE_UNKNOWN   |
//		BK4819_REG_30_ENABLE_RX_LINK   |
//		BK4819_REG_30_ENABLE_AF_DAC    |
		BK4819_REG_30_ENABLE_DISC_MODE |
		BK4819_REG_30_ENABLE_PLL_VCO   |
		BK4819_REG_30_ENABLE_PA_GAIN   |
		BK4819_REG_30_ENABLE_MIC_ADC   |
		BK4819_REG_30_ENABLE_TX_DSP    |
//		BK4819_REG_30_ENABLE_RX_DSP    |
	0);

	if (!bKeep)
		BK4819_ExitTxMute();
}

void BK4819_EnableTXLink(void)
{
	BK4819_write_reg(0x30,
		BK4819_REG_30_ENABLE_VCO_CALIB |
		BK4819_REG_30_ENABLE_UNKNOWN   |
//		BK4819_REG_30_ENABLE_RX_LINK   |
		BK4819_REG_30_ENABLE_AF_DAC    |
		BK4819_REG_30_ENABLE_DISC_MODE |
		BK4819_REG_30_ENABLE_PLL_VCO   |
		BK4819_REG_30_ENABLE_PA_GAIN   |
//		BK4819_REG_30_ENABLE_MIC_ADC   |
		BK4819_REG_30_ENABLE_TX_DSP    |
//		BK4819_REG_30_ENABLE_RX_DSP    |
	0);
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
    	BK4819_write_reg(0x71, scale_freq(tones[0][index]));
    	BK4819_write_reg(0x72, scale_freq(tones[1][index]));
    }
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
//	BK4819_write_reg(0x70, BK4819_REG_70_MASK_ENABLE_TONE1 | (96u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));
	BK4819_write_reg(0x70, BK4819_REG_70_MASK_ENABLE_TONE1 | (28u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));

	BK4819_write_reg(0x71, scale_freq(Frequency));

	BK4819_SetAF(bLocalLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);

	BK4819_EnableTXLink();

	SYSTEM_DelayMs(50);

	BK4819_ExitTxMute();
}

void BK4819_disable_sub_audible(void)
{
	BK4819_write_reg(0x51, 0);
}

void BK4819_config_sub_audible(void)
{
//	#ifdef ENABLE_CTCSS_TAIL_PHASE_SHIFT
//		BK4819_gen_tail(2);    // 180 deg
//	#else
//		BK4819_gen_tail(4);
		BK4819_write_reg(0x52, (0u << 15) | (0u << 13) | (0u << 12) | (10u << 6) | (15u << 0)); // 0x028F);  // 0 00 0 001010 001111
//	#endif
}

// freq_10Hz is CTCSS Hz * 10
void BK4819_set_tail_detection(const uint32_t freq_10Hz)
{
	// REG_07 <15:0>
	//
	// <15)     ???
	//
	// <14:13>  0 = CTC-1, 1 = CTC-2 (tail detection), 2 = CDCSS 134.4Hz
	//
	// if <14:13> == 0
	// <12:0> CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// if <14:13> == 1
	// <12:0> CTC2 (should below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// if <14:13> == 2
	// <12:0> CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	#ifdef ENABLE_CTCSS_TAIL_PHASE_SHIFT
		BK4819_write_reg(0x07, (0u << 13) | (((freq_10Hz * 206488u) + 50000u) / 100000u));
	#else
		BK4819_write_reg(0x07, (1u << 13) | ((253910 + (freq_10Hz / 2)) / freq_10Hz));  // with rounding
	#endif
}

void BK4819_gen_tail(const unsigned int tail)
{
	// REG_52
	//
	// <15>    degree shift CTCSS or 134.4Hz tail CDCSS
	//         0 = normal
	//         1 = enable
	//
	// <14:13> CTCSS tail mode selection (only valid when <15> == 1)
	//         0 = for 134.4Hz CTCSS Tail when CDCSS mode
	//         1 = CTCSS0 120° phase shift
	//         2 = CTCSS0 180° phase shift
	//         3 = CTCSS0 240° phase shift
	//
	// <12>    CTCSS Detection Threshold Mode
	//         1 = ~0.1%
	//         0 =  0.1Hz
	//
	// <11:6>  10 CTCSS found detect threshold 0 ~ 63
	//
	// <5:0>   15 CTCSS lost  detect threshold 0 ~ 63

	uint16_t tail_phase_shift            = 1;
	uint16_t ctcss_tail_mode_selection   = 0;
	uint16_t ctcss_detect_threshold_mode = 0;
	#if 0
		// original QS setting
		uint16_t ctcss_found_threshold       = 10;
		uint16_t ctcss_lost_threshold        = 15;
	#else
		// increase it to help reduce false detections when doing CTCSS/CDCSS searching
		uint16_t ctcss_found_threshold       = 15;
		uint16_t ctcss_lost_threshold        = 23;
	#endif

	switch (tail)
	{
		case 0: // 134.4Hz CTCSS Tail
//			ctcss_tail_mode_selection = 0;
			break;
		case 1: // 120° phase shift
			ctcss_tail_mode_selection = 1;
			break;
		case 2: // 180° phase shift        1 10 0 001010 001111
			ctcss_tail_mode_selection = 2;
			break;
		case 3: // 240° phase shift
			ctcss_tail_mode_selection = 3;
			break;
		default:
		case 4: // 55Hz tone freq
			tail_phase_shift      = 0;
			ctcss_found_threshold = 17;
			ctcss_lost_threshold  = 47;
			break;
	}

	BK4819_write_reg(0x52,
		(tail_phase_shift            << 15) |
		(ctcss_tail_mode_selection   << 13) |
		(ctcss_detect_threshold_mode << 12) |
		(ctcss_found_threshold       <<  6) |
		(ctcss_lost_threshold        <<  0));
}

void BK4819_set_CDCSS_code(const uint32_t control_word)
{
	BK4819_write_reg(0x51,
		( 1u << 15) |     // TX CTCSS/CDCSS               1 = enable    0 = disable
		( 0u << 14) |     // GPIO input for CDCSS         0 = normal (for BK4819v3)
		( 0u << 13) |     // TX CDCSS code                1 = negative  0 = positive
		( 0u << 12) |     // CTCSS/CDCSS mode selection   1 = CTCSS     0 = CDCSS
		( 0u << 11) |     // CDCSS 24/23bit selection     1 = 24bit     0 = 23bit
		( 0u << 10) |     // 1050Hz detection mode        1 = enable, CTC1 should be set to 1050/4 Hz
		( 1u <<  9) |     // Auto CDCSS BW Mode           1 = disable   0 = enable
		( 1u <<  8) |     // Auto CTCSS BW Mode           1 = disable   0 = enable
		( 0u <<  7) |     // ???
		(51u <<  0));     // CTCSS/CDCSS TX gain 1        0 ~ 127

	// REG_07 <15:0>
	//
	// <15)     ???
	//
	// <14:13>  0 = CTC-1, 1 = CTC-2 (tail detection), 2 = CDCSS 134.4Hz
	//
	//    <14:13> == 0
	// <12:0> CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	//    <14:13> == 1
	// <12:0> CTC2 (should below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	//    <14:13> == 2
	// <12:0> CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	BK4819_write_reg(0x07, (0u << 13) | 2775u);

	// REG_08 <15:0> <15> = 1 for CDCSS high 12bit
	//               <15> = 0 for CDCSS low  12bit
	// <11:0> = CDCSShigh/low 12bit code
	//
	BK4819_write_reg(0x08, (0u << 15) | ((control_word >>  0) & 0x0FFF)); // LS 12-bits
	BK4819_write_reg(0x08, (1u << 15) | ((control_word >> 12) & 0x0FFF)); // MS 12-bits
}

void BK4819_set_CTCSS_freq(const uint32_t control_word)
{
	if (control_word == 0)
	{	// NOAA 1050Hz tone stuff
		BK4819_write_reg(0x51,
			( 1u << 15) |     // TX CTCSS/CDCSS               1 = enable    0 = disable
			( 0u << 14) |     // GPIO input for CDCSS         0 = normal (for BK4819v3)
			( 0u << 13) |     // TX CDCSS code                1 = negative  0 = positive
			( 1u << 12) |     // CTCSS/CDCSS mode selection   1 = CTCSS     0 = CDCSS
			( 0u << 11) |     // CDCSS 24/23bit selection     1 = 24bit     0 = 23bit
			( 1u << 10) |     // 1050Hz detection mode        1 = enable, CTC1 should be set to 1050/4 Hz
			( 0u <<  9) |     // Auto CDCSS BW Mode           1 = disable   0 = enable
			( 0u <<  8) |     // Auto CTCSS BW Mode           1 = disable   0 = enable
			( 0u <<  7) |     // ???
			(74u <<  0));     // CTCSS/CDCSS TX gain 1        0 ~ 127
	}
	else
	{	// normal CTCSS
		BK4819_write_reg(0x51,
			( 1u << 15) |     // TX CTCSS/CDCSS               1 = enable    0 = disable
			( 0u << 14) |     // GPIO input for CDCSS         0 = normal (for BK4819v3)
			( 0u << 13) |     // TX CDCSS code                1 = negative  0 = positive
			( 1u << 12) |     // CTCSS/CDCSS mode selection   1 = CTCSS     0 = CDCSS
			( 0u << 11) |     // CDCSS 24/23bit selection     1 = 24bit     0 = 23bit
			( 0u << 10) |     // 1050Hz detection mode        1 = enable, CTC1 should be set to 1050/4 Hz
			( 0u <<  9) |     // Auto CDCSS BW Mode           1 = disable   0 = enable
			( 0u <<  8) |     // Auto CTCSS BW Mode           1 = disable   0 = enable
			( 0u <<  7) |     // ???
			(74u <<  0));     // CTCSS/CDCSS TX gain 1        0 ~ 127
	}

	// REG_07 <15:0>
	//
	// <15)     0
	//
	// <14:13>  0 = CTC-1, 1 = CTC-2 (tail detection), 2 = CDCSS 134.4Hz
	//
	// if <14:13> == 0
	// <12:0> CTC1 frequency control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// if <14:13> == 1
	// <12:0> CTC2 (should be below 100Hz) frequency control word =
	//                          25391 / freq(Hz) for XTAL 13M/26M or
	//                          25000 / freq(Hz) for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	// if <14:13> == 2
	// <12:0> CDCSS baud rate frequency (134.4Hz) control word =
	//                          freq(Hz) * 20.64888 for XTAL 13M/26M or
	//                          freq(Hz) * 20.97152 for XTAL 12.8M/19.2M/25.6M/38.4M
	//
	#ifdef ENABLE_CTCSS_TAIL_PHASE_SHIFT
		BK4819_write_reg(0x07, (0u << 13) | (((control_word * 206488u) + 50000u) / 100000u));
	#else
		BK4819_write_reg(0x07, (1u << 13) | ((253910 + (control_word / 2)) / control_word));
	#endif
}

void BK4819_enable_CDCSS_tail(void)
{
	BK4819_gen_tail(0);                  // CTC134

	BK4819_write_reg(0x51,           // 0x804A);  // 1 0 0 0 0 0 0 0 0 1001010
		( 1u << 15) |     // TX CTCSS/CDCSS               1 = enable    0 = disable
		( 0u << 14) |     // GPIO0 input for CDCSS        0 = normal (for BK4819v3)
		( 0u << 13) |     // TX CDCSS code                1 = negative  0 = positive
		( 0u << 12) |     // CTCSS/CDCSS mode selection   1 = CTCSS     0 = CDCSS
		( 0u << 11) |     // CDCSS 24/23bit selection     1 = 24bit     0 = 23bit
		( 0u << 10) |     // 1050Hz detection mode        1 = enable, CTC1 should be set to 1050/4 Hz
		( 0u <<  9) |     // Auto CDCSS BW Mode           1 = disable   0 = enable
		( 0u <<  8) |     // Auto CTCSS BW Mode           1 = disable   0 = enable
		( 0u <<  7) |     // ???
		(74u <<  0));     // CTCSS/CDCSS TX gain 1        0 ~ 127
}

void BK4819_enable_CTCSS_tail(void)
{
	#ifdef ENABLE_CTCSS_TAIL_PHASE_SHIFT
		//BK4819_gen_tail(1);     // 120° phase shift
		  BK4819_gen_tail(2);     // 180° phase shift
		//BK4819_gen_tail(3);     // 240° phase shift
	#else
		BK4819_gen_tail(4);       // 55Hz tone freq
	#endif

	BK4819_write_reg(0x51,           // 0x804A);  // 1 0 0 0 0 0 0 0 0 1001010
		( 1u << 15) |     // TX CTCSS/CDCSS               1 = enable    0 = disable
		( 0u << 14) |     // GPIO0 input for CDCSS        0 = normal (for BK4819v3)
		( 0u << 13) |     // TX CDCSS code                1 = negative  0 = positive
		( 1u << 12) |     // CTCSS/CDCSS mode selection   1 = CTCSS     0 = CDCSS
		( 0u << 11) |     // CDCSS 24/23bit selection     1 = 24bit     0 = 23bit
		( 0u << 10) |     // 1050Hz detection mode        1 = enable, CTC1 should be set to 1050/4 Hz
		( 0u <<  9) |     // Auto CDCSS BW Mode           1 = disable   0 = enable
		( 0u <<  8) |     // Auto CTCSS BW Mode           1 = disable   0 = enable
		( 0u <<  7) |     // ???
		(74u <<  0));     // CTCSS/CDCSS TX gain 1        0 ~ 127
}

uint16_t BK4819_GetRSSI(void)
{
	return BK4819_read_reg(0x67) & 0x01FF;
}

uint8_t BK4819_GetGlitchIndicator(void)
{
	return BK4819_read_reg(0x63) & 0x00FF;
}

uint8_t BK4819_GetExNoiceIndicator(void)
{
	return BK4819_read_reg(0x65) & 0x007F;
}

uint16_t BK4819_GetVoiceAmplitudeOut(void)
{
	return BK4819_read_reg(0x64);
}

uint8_t BK4819_GetAfTxRx(void)
{
	return BK4819_read_reg(0x6F) & 0x003F;
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
	const uint16_t high      = BK4819_read_reg(0x0D);
	const uint16_t low       = BK4819_read_reg(0x0E);
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
	const uint16_t High = BK4819_read_reg(0x69);
	uint16_t       Low;

	if (((High >> 15) & 1u) == 0)
	{	// CDCSS
		Low         = BK4819_read_reg(0x6A);
		*pCdcssFreq = ((uint32_t)(High & 0xFFF) << 12) | (Low & 0xFFF);
		return BK4819_CSS_RESULT_CDCSS;
	}

	Low = BK4819_read_reg(0x68);
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
	BK4819_write_reg(0x32, // 0x0244);    // 00 0000100100010 0
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
	BK4819_write_reg(0x32, // 0x0245);   // 00 0000100100010 1
		(  0u << 14) |          // 0 frequency scan time
		(290u <<  1) |          // ???
		(  1u <<  0));          // 1 frequency scan enable
}

void BK4819_set_scan_frequency(uint32_t Frequency)
{
	BK4819_set_rf_frequency(Frequency, false);

	BK4819_write_reg(0x51,
		(0u << 15) |     // TX CTCSS/CDCSS               1 = enable    0 = disable
		(0u << 14) |     // GPIO input for CDCSS         0 = normal (for BK4819v3)
		(0u << 13) |     // TX CDCSS code                1 = negative  0 = positive
		(0u << 12) |     // CTCSS/CDCSS mode selection   1 = CTCSS     0 = CDCSS
		(0u << 11) |     // CDCSS 24/23bit selection     1 = 24bit     0 = 23bit
		(0u << 10) |     // 1050Hz detection mode        1 = enable, CTC1 should be set to 1050/4 Hz
		(1u <<  9) |     // Auto CDCSS BW Mode           1 = disable   0 = enable
		(1u <<  8) |     // Auto CTCSS BW Mode           1 = disable   0 = enable
		(0u <<  7) |     // ???
		(0u <<  0));     // CTCSS/CDCSS TX gain 1        0 ~ 127

	BK4819_RX_TurnOn();
}

void BK4819_StopScan(void)
{
	BK4819_DisableFrequencyScan();
	BK4819_write_reg(0x30, 0);
}

uint8_t BK4819_GetDTMF_5TONE_Code(void)
{
	return (BK4819_read_reg(0x0B) >> 8) & 0x0F;
}

uint8_t BK4819_get_CDCSS_code_type(void)
{
	return (BK4819_read_reg(0x0C) >> 14) & 3u;
}

uint8_t BK4819_GetCTCShift(void)
{
	return (BK4819_read_reg(0x0C) >> 12) & 3u;
}

uint8_t BK4819_GetCTCType(void)
{
	return (BK4819_read_reg(0x0C) >> 10) & 3u;
}

void BK4819_reset_fsk(void)
{
	const uint16_t fsk_reg59 =
		(0u << 15) |   // 0 or 1   1 = clear TX FIFO
		(0u << 14) |   // 0 or 1   1 = clear RX FIFO
		(0u << 13) |   // 0 or 1   1 = scramble
		(0u << 12) |   // 0 or 1   1 = enable RX
		(0u << 11) |   // 0 or 1   1 = enable TX
		(0u << 10) |   // 0 or 1   1 = invert data when RX
		(0u <<  9) |   // 0 or 1   1 = invert data when TX
		(0u <<  8) |   // 0 or 1   ???
		(6u <<  4) |   // 0 ~ 15   preamble Length Selection
		(1u <<  3) |   // 0 or 1   sync length selection
		(0u <<  0);    // 0 ~ 7    ???

	BK4819_write_reg(0x3F, 0);  // disable interrupts

	BK4819_write_reg(0x59, (1u << 15) | (1u << 14) | fsk_reg59); // clear FIFO's
	BK4819_write_reg(0x59, fsk_reg59);

	BK4819_write_reg(0x30, 0);
}

#ifdef ENABLE_AIRCOPY
	void BK4819_SetupAircopy(const unsigned int packet_size)
	{
		if (packet_size == 0)
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
		BK4819_write_reg(0x70,   // 0 0000000 1 1100000
			( 0u << 15) |
			( 0u <<  8) |
			( 1u <<  7) |
//			(96u <<  0));
			(127u <<  0));  // best waveform

		// REG_72
		//
		// <15:0> 0x2854 TONE2/FSK frequency control word
		//        = freq(Hz) * 10.32444 for XTAL 13M / 26M or
		//        = freq(Hz) * 10.48576 for XTAL 12.8M / 19.2M / 25.6M / 38.4M
		//
		// tone-2 = 1200Hz
		//
		BK4819_write_reg(0x72, scale_freq(1200));

		// aircopy is done in direct FM mode
		//
		BK4819_write_reg(0x58, // 0x00C1);   // 000 000 00 11 00 000 1
			(0u << 13) |		// 1 FSK TX mode selection
								//   0 = FSK 1.2K and FSK 2.4K TX .. no tones, direct FM
								//   1 = FFSK 1200 / 1800 TX
								//   2 = ???
								//   3 = FFSK 1200 / 2400 TX
								//   4 = ???
								//   5 = NOAA SAME TX
								//   6 = ???
								//   7 = ???
								//
			(0u << 10) |		// 0 FSK RX mode selection
								//   0 = FSK 1.2K, FSK 2.4K RX and NOAA SAME RX .. no tones, direct FM
								//   1 = ???
								//   2 = ???
								//   3 = ???
								//   4 = FFSK 1200 / 2400 RX
								//   5 = ???
								//   6 = ???
								//   7 = FFSK 1200 / 1800 RX
								//
			(0u << 8) |			// 0 FSK RX gain
								//   0 ~ 3
								//
			(3u << 6) |			// 0 ???
								//   0 ~ 3
								//
			(0u << 4) |			// 0 FSK preamble type selection
								//   0 = 0xAA or 0x55 due to the MSB of FSK sync byte 0
								//   1 = ???
								//   2 = 0x55
								//   3 = 0xAA
								//
			(0u << 1) |			// 1 FSK RX bandwidth setting
								//   0 = FSK 1.2K .. no tones, direct FM
								//   1 = FFSK 1200 / 1800
								//   2 = NOAA SAME RX
								//   3 = ???
								//   4 = FSK 2.4K and FFSK 1200 / 2400
								//   5 = ???
								//   6 = ???
								//   7 = ???
								//
			(1u << 0));			// 1 FSK enable
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
		BK4819_write_reg(0x5C, 0x5665);   // 010101100 1 100101

		// REG_5D
		//
		// <15:8> 15 FSK data length (byte) Low 8 bits (total 11 bits for BK4819v3)
		//        15 means 16 bytes in length
		//
		// <7:5>  0 FSK data
		//
		// <4:0>  0 ???
		//
		BK4819_write_reg(0x5D, ((packet_size - 1) << 8));
	}

	void BK4819_start_aircopy_fsk_rx(const unsigned int packet_size)
	{
		uint16_t fsk_reg59;

		BK4819_reset_fsk();

		BK4819_write_reg(0x02, 0);    // clear interrupt flags

		// set the almost full threshold
		BK4819_write_reg(0x5E, (64u << 3) | (1u << 0));  // 0 ~ 127, 0 ~ 7

		// set the packet size
		BK4819_write_reg(0x5D, ((packet_size - 1) << 8));

		BK4819_RX_TurnOn();

	//	BK4819_write_reg(0x3F,                             BK4819_REG_3F_FSK_RX_FINISHED | BK4819_REG_3F_FSK_FIFO_ALMOST_FULL);
		BK4819_write_reg(0x3F, BK4819_REG_3F_FSK_RX_SYNC | BK4819_REG_3F_FSK_RX_FINISHED | BK4819_REG_3F_FSK_FIFO_ALMOST_FULL);

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
		fsk_reg59 = (0u << 15) |   // 0 or 1   1 = clear TX FIFO
					(0u << 14) |   // 0 or 1   1 = clear RX FIFO
					(0u << 13) |   // 0 or 1   1 = scramble
					(0u << 12) |   // 0 or 1   1 = enable RX
					(0u << 11) |   // 0 or 1   1 = enable TX
					(0u << 10) |   // 0 or 1   1 = invert data when RX
					(0u <<  9) |   // 0 or 1   1 = invert data when TX
					(0u <<  8) |   // 0 or 1   ???
	//				(6u <<  4) |   // 0 ~ 15   preamble Length Selection
					(4u <<  4) |   // 0 ~ 15   preamble Length Selection .. 1of11 .. a little shorter than the TX length
					(1u <<  3) |   // 0 or 1   sync length selection
					(0u <<  0);    // 0 ~ 7    ???

		BK4819_write_reg(0x59, (1u << 15) | (1u << 14) | fsk_reg59);  // clear FIFO's
		BK4819_write_reg(0x59, (1u << 13) | (1u << 12) | fsk_reg59);  // enable scrambler, enable RX
	}
#endif

#ifdef ENABLE_MDC1200
	void BK4819_enable_mdc1200_rx(const bool enable)
	{
		// REG_70
		//
		// <15>    0 TONE-1
		//         1 = enable
		//         0 = disable
		//
		// <14:8>  0 TONE-1 gain
		//
		// <7>     0 TONE-2
		//         1 = enable
		//         0 = disable
		//
		// <6:0>   0 TONE-2 / FSK gain
		//         0 ~ 127
		//
		// enable tone-2, set gain

		// REG_72
		//
		// <15:0>  0x2854 TONE-2 / FSK frequency control word
		//         = freq(Hz) * 10.32444 for XTAL 13M / 26M or
		//         = freq(Hz) * 10.48576 for XTAL 12.8M / 19.2M / 25.6M / 38.4M
		//
		// tone-2 = 1200Hz

		// REG_58
		//
		// <15:13> 1 FSK TX mode selection
		//         0 = FSK 1.2K and FSK 2.4K TX .. no tones, direct FM
		//         1 = FFSK 1200 / 1800 TX
		//         2 = ???
		//         3 = FFSK 1200 / 2400 TX
		//         4 = ???
		//         5 = NOAA SAME TX
		//         6 = ???
		//         7 = ???
		//
		// <12:10> 0 FSK RX mode selection
		//         0 = FSK 1.2K, FSK 2.4K RX and NOAA SAME RX .. no tones, direct FM
		//         1 = ???
		//         2 = ???
		//         3 = ???
		//         4 = FFSK 1200 / 2400 RX
		//         5 = ???
		//         6 = ???
		//         7 = FFSK 1200 / 1800 RX
		//
		// <9:8>   0 FSK RX gain
		//         0 ~ 3
		//
		// <7:6>   0 ???
		//         0 ~ 3
		//
		// <5:4>   0 FSK preamble type selection
		//         0 = 0xAA or 0x55 due to the MSB of FSK sync byte 0
		//         1 = ???
		//         2 = 0x55
		//         3 = 0xAA
		//
		// <3:1>   1 FSK RX bandwidth setting
		//         0 = FSK 1.2K .. no tones, direct FM
		//         1 = FFSK 1200 / 1800
		//         2 = NOAA SAME RX
		//         3 = ???
		//         4 = FSK 2.4K and FFSK 1200 / 2400
		//         5 = ???
		//         6 = ???
		//         7 = ???
		//
		// <0>     1 FSK enable
		//         0 = disable
		//         1 = enable

		// REG_5C
		//
		// <15:7>  ???
		//
		// <6>     1 CRC option enable
		//         0 = disable
		//         1 = enable
		//
		// <5:0>   ???
		//
		// disable CRC

		// REG_5D
		//
		// set the packet size

		if (enable)
		{
			const uint16_t fsk_reg59 =
				(0u << 15) |   // 1 = clear TX FIFO
				(0u << 14) |   // 1 = clear RX FIFO
				(0u << 13) |   // 1 = scramble
				(0u << 12) |   // 1 = enable RX
				(0u << 11) |   // 1 = enable TX
				(0u << 10) |   // 1 = invert data when RX
				(0u <<  9) |   // 1 = invert data when TX
				(0u <<  8) |   // ???
				(0u <<  4) |   // 0 ~ 15 preamble length selection .. mdc1200 does not send bit reversals :(
				(1u <<  3) |   // 0/1 sync length selection
				(0u <<  0);    // 0 ~ 7  ???

			BK4819_write_reg(0x70,
				( 0u << 15) |    // 0
				( 0u <<  8) |    // 0
				( 1u <<  7) |    // 1
				(96u <<  0));    // 96

			BK4819_write_reg(0x72, scale_freq(1200));

			BK4819_write_reg(0x58,
				(1u << 13) |		// 1 FSK TX mode selection
									//   0 = FSK 1.2K and FSK 2.4K TX .. no tones, direct FM
									//   1 = FFSK 1200 / 1800 TX
									//   2 = ???
									//   3 = FFSK 1200 / 2400 TX
									//   4 = ???
									//   5 = NOAA SAME TX
									//   6 = ???
									//   7 = ???
									//
				(7u << 10) |		// 0 FSK RX mode selection
									//   0 = FSK 1.2K, FSK 2.4K RX and NOAA SAME RX .. no tones, direct FM
									//   1 = ???
									//   2 = ???
									//   3 = ???
									//   4 = FFSK 1200 / 2400 RX
									//   5 = ???
									//   6 = ???
									//   7 = FFSK 1200 / 1800 RX
									//
				(3u << 8) |			// 0 FSK RX gain
									//   0 ~ 3
									//
				(0u << 6) |			// 0 ???
									//   0 ~ 3
									//
				(0u << 4) |			// 0 FSK preamble type selection
									//   0 = 0xAA or 0x55 due to the MSB of FSK sync byte 0
									//   1 = ???
									//   2 = 0x55
									//   3 = 0xAA
									//
				(1u << 1) |			// 1 FSK RX bandwidth setting
									//   0 = FSK 1.2K .. no tones, direct FM
									//   1 = FFSK 1200 / 1800
									//   2 = NOAA SAME RX
									//   3 = ???
									//   4 = FSK 2.4K and FFSK 1200 / 2400
									//   5 = ???
									//   6 = ???
									//   7 = ???
									//
				(1u << 0));			// 1 FSK enable
									//   0 = disable
									//   1 = enable

			// REG_5A .. bytes 0 & 1 sync pattern
			//
			// <15:8> sync byte 0
			// < 7:0> sync byte 1
//			BK4819_write_reg(0x5A, ((uint16_t)mdc1200_sync_suc_xor[0] << 8) | (mdc1200_sync_suc_xor[1] << 0));
			BK4819_write_reg(0x5A, ((uint16_t)mdc1200_sync_suc_xor[1] << 8) | (mdc1200_sync_suc_xor[2] << 0));

			// REG_5B .. bytes 2 & 3 sync pattern
			//
			// <15:8> sync byte 2
			// < 7:0> sync byte 3
//			BK4819_write_reg(0x5B, ((uint16_t)mdc1200_sync_suc_xor[2] << 8) | (mdc1200_sync_suc_xor[3] << 0));
			BK4819_write_reg(0x5B, ((uint16_t)mdc1200_sync_suc_xor[3] << 8) | (mdc1200_sync_suc_xor[4] << 0));

			// disable CRC
			BK4819_write_reg(0x5C, 0x5625);   // 01010110 0 0 100101
//			BK4819_write_reg(0x5C, 0xAA30);   // 10101010 0 0 110000

			// set the almost full threshold
			BK4819_write_reg(0x5E, (64u << 3) | (1u << 0));  // 0 ~ 127, 0 ~ 7

			{	// packet size .. sync + 14 bytes - size of a single mdc1200 packet
//				uint16_t size = 1 + (MDC1200_FEC_K * 2);
				uint16_t size = 0 + (MDC1200_FEC_K * 2);
//				size -= (fsk_reg59 & (1u << 3)) ? 4 : 2;
				size = ((size + 1) / 2) * 2;             // round up to even, else FSK RX doesn't work
				BK4819_write_reg(0x5D, ((size - 1) << 8));
			}

			// clear FIFO's then enable RX
			BK4819_write_reg(0x59, (1u << 15) | (1u << 14) | fsk_reg59);
			BK4819_write_reg(0x59, (1u << 12) | fsk_reg59);

			// clear interrupt flags
			BK4819_write_reg(0x02, 0);

//			BK4819_RX_TurnOn();

			// enable interrupts
//			BK4819_write_reg(0x3F, BK4819_read_reg(0x3F) | BK4819_REG_3F_FSK_RX_SYNC | BK4819_REG_3F_FSK_RX_FINISHED | BK4819_REG_3F_FSK_FIFO_ALMOST_FULL);
		}
		else
		{
			BK4819_write_reg(0x70, 0);
			BK4819_write_reg(0x58, 0);
		}
	}

	void BK4819_send_MDC1200(const uint8_t op, const uint8_t arg, const uint16_t id, const bool long_preamble)
	{
		uint16_t fsk_reg59;
		uint8_t  packet[42];

		// create the MDC1200 packet
		const unsigned int size = MDC1200_encode_single_packet(packet, op, arg, id);

		//BK4819_ExitTxMute();
		BK4819_write_reg(0x50, 0x3B20);  // 0011 1011 0010 0000

		BK4819_write_reg(0x30,
			BK4819_REG_30_ENABLE_VCO_CALIB |
			BK4819_REG_30_ENABLE_UNKNOWN   |
//			BK4819_REG_30_ENABLE_RX_LINK   |
			BK4819_REG_30_ENABLE_AF_DAC    |
			BK4819_REG_30_ENABLE_DISC_MODE |
			BK4819_REG_30_ENABLE_PLL_VCO   |
			BK4819_REG_30_ENABLE_PA_GAIN   |
//			BK4819_REG_30_ENABLE_MIC_ADC   |
			BK4819_REG_30_ENABLE_TX_DSP    |
//			BK4819_REG_30_ENABLE_RX_DSP    |
		0);

		#if 1
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
			BK4819_SetAF(BK4819_AF_MUTE);
		#else
			// let the user hear the FSK being sent
			BK4819_SetAF(BK4819_AF_BEEP);
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);
		#endif
//		SYSTEM_DelayMs(2);

		// REG_51
		//
		// <15>  TxCTCSS/CDCSS   0 = disable 1 = Enable
		//
		// turn off CTCSS/CDCSS during FFSK
		const uint16_t css_val = BK4819_read_reg(0x51);
		BK4819_write_reg(0x51, 0);

		// set the FM deviation level
		const uint16_t dev_val = BK4819_read_reg(0x40);
		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//			UART_printf("tx dev %04X\r\n", dev_val);
		#endif
		{
			uint16_t deviation = 850;
			switch (m_bandwidth)
			{
				case BK4819_FILTER_BW_WIDE:     deviation = 1050; break;
				case BK4819_FILTER_BW_NARROW:   deviation =  850; break;
				case BK4819_FILTER_BW_NARROWER: deviation =  750; break;
			}
			//BK4819_write_reg(0x40, (3u << 12) | (deviation & 0xfff));
			BK4819_write_reg(0x40, (dev_val & 0xf000) | (deviation & 0xfff));
		}

		// REG_2B   0
		//
		// <15> 1 Enable CTCSS/CDCSS DC cancellation after FM Demodulation   1 = enable 0 = disable
		// <14> 1 Enable AF DC cancellation after FM Demodulation            1 = enable 0 = disable
		// <10> 0 AF RX HPF 300Hz filter     0 = enable 1 = disable
		// <9>  0 AF RX LPF 3kHz filter      0 = enable 1 = disable
		// <8>  0 AF RX de-emphasis filter   0 = enable 1 = disable
		// <2>  0 AF TX HPF 300Hz filter     0 = enable 1 = disable
		// <1>  0 AF TX LPF filter           0 = enable 1 = disable
		// <0>  0 AF TX pre-emphasis filter  0 = enable 1 = disable
		//
		// disable the 300Hz HPF and FM pre-emphasis filter
		//
		const uint16_t filt_val = BK4819_read_reg(0x2B);
		BK4819_write_reg(0x2B, (1u << 2) | (1u << 0));

		// *******************************************
		// setup the FFSK modem as best we can for MDC1200

		// MDC1200 uses 1200/1800 Hz FSK tone frequencies 1200 bits/s
		//
		BK4819_write_reg(0x58, // 0x37C3);   // 001 101 11 11 00 001 1
			(1u << 13) |		// 1 FSK TX mode selection
								//   0 = FSK 1.2K and FSK 2.4K TX .. no tones, direct FM
								//   1 = FFSK 1200/1800 TX
								//   2 = ???
								//   3 = FFSK 1200/2400 TX
								//   4 = ???
								//   5 = NOAA SAME TX
								//   6 = ???
								//   7 = ???
								//
			(7u << 10) |		// 0 FSK RX mode selection
								//   0 = FSK 1.2K, FSK 2.4K RX and NOAA SAME RX .. no tones, direct FM
								//   1 = ???
								//   2 = ???
								//   3 = ???
								//   4 = FFSK 1200/2400 RX
								//   5 = ???
								//   6 = ???
								//   7 = FFSK 1200/1800 RX
								//
			(0u << 8) |			// 0 FSK RX gain
								//   0 ~ 3
								//
			(0u << 6) |			// 0 ???
								//   0 ~ 3
								//
			(0u << 4) |			// 0 FSK preamble type selection
								//   0 = 0xAA or 0x55 due to the MSB of FSK sync byte 0
								//   1 = ???
								//   2 = 0x55
								//   3 = 0xAA
								//
			(1u << 1) |			// 1 FSK RX bandwidth setting
								//   0 = FSK 1.2K .. no tones, direct FM
								//   1 = FFSK 1200/1800
								//   2 = NOAA SAME RX
								//   3 = ???
								//   4 = FSK 2.4K and FFSK 1200/2400
								//   5 = ???
								//   6 = ???
								//   7 = ???
								//
			(1u << 0));			// 1 FSK enable
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
		BK4819_write_reg(0x72, scale_freq(1200));

		// REG_70
		//
		// <15>   0 TONE-1
		//        1 = enable
		//        0 = disable
		//
		// <14:8> 0 TONE-1 tuning
		//
		// <7>    0 TONE-2
		//        1 = enable
		//        0 = disable
		//
		// <6:0>  0 TONE-2 / FSK tuning
		//        0 ~ 127
		//
		// enable tone-2, set gain
		//
		BK4819_write_reg(0x70,   // 0 0000000 1 1100000
			( 0u << 15) |    // 0
			( 0u <<  8) |    // 0
			( 1u <<  7) |    // 1
			(96u <<  0));    // 96
//			(127u <<  0));

		// REG_59
		//
		// <15>  0 TX FIFO             1 = clear
		// <14>  0 RX FIFO             1 = clear
		// <13>  0 FSK Scramble        1 = Enable
		// <12>  0 FSK RX              1 = Enable
		// <11>  0 FSK TX              1 = Enable
		// <10>  0 FSK data when RX    1 = Invert
		// <9>   0 FSK data when TX    1 = Invert
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
		fsk_reg59 = (0u << 15) |   // 0/1     1 = clear TX FIFO
					(0u << 14) |   // 0/1     1 = clear RX FIFO
					(0u << 13) |   // 0/1     1 = scramble
					(0u << 12) |   // 0/1     1 = enable RX
					(0u << 11) |   // 0/1     1 = enable TX
					(0u << 10) |   // 0/1     1 = invert data when RX
					(0u <<  9) |   // 0/1     1 = invert data when TX
					(0u <<  8) |   // 0/1     ???
					(0u <<  4) |   // 0 ~ 15  preamble length .. bit toggling
					(1u <<  3) |   // 0/1     sync length
					(0u <<  0);    // 0 ~ 7   ???
		fsk_reg59 |= long_preamble ? 15u << 4 : 3u << 4; 
			
		// Set packet length (not including pre-amble and sync bytes that we can't seem to disable)
		BK4819_write_reg(0x5D, ((size - 1) << 8));

		// REG_5A
		//
		// <15:8> 0x55 FSK Sync Byte 0 (Sync Byte 0 first, then 1,2,3)
		// <7:0>  0x55 FSK Sync Byte 1
		//
		BK4819_write_reg(0x5A, 0x0000);                   // bytes 1 & 2

		// REG_5B
		//
		// <15:8> 0x55 FSK Sync Byte 2 (Sync Byte 0 first, then 1,2,3)
		// <7:0>  0xAA FSK Sync Byte 3
		//
		BK4819_write_reg(0x5B, 0x0000);                   // bytes 2 & 3

		// CRC setting (plus other stuff we don't know what)
		//
		// REG_5C
		//
		// <15:7> ???
		//
		// <6>    1 CRC option enable    0 = disable  1 = enable
		//
		// <5:0>  ???
		//
		// disable CRC
		//
		// NB, this also affects TX pre-amble in some way
		//
		BK4819_write_reg(0x5C, 0x5625);   // 010101100 0 100101
//		BK4819_write_reg(0x5C, 0xAA30);   // 101010100 0 110000
//		BK4819_write_reg(0x5C, 0x0030);   // 000000000 0 110000

		BK4819_write_reg(0x59, (1u << 15) | (1u << 14) | fsk_reg59);   // clear FIFO's
		BK4819_write_reg(0x59, fsk_reg59);                             // release the FIFO reset

		{	// load the entire packet data into the TX FIFO buffer
			unsigned int i;
			const uint16_t *p = (const uint16_t *)packet;
			for (i = 0; i < (size / sizeof(p[0])); i++)
				BK4819_write_reg(0x5F, p[i]);  // load 16-bits at a time
		}

		// enable tx interrupt
		BK4819_write_reg(0x3F, BK4819_REG_3F_FSK_TX_FINISHED);

		// enable FSK TX
		BK4819_write_reg(0x59, (1u << 11) | fsk_reg59);

		{	// packet time is ..
			// 173ms for PTT ID, acks, emergency
			// 266ms for call alert and sel-calls

			// allow up to 310ms for the TX to complete
			// if it takes any longer then somethings gone wrong, we shut the TX down
			unsigned int timeout = 300 / 4;

			while (timeout-- > 0)
			{
				SYSTEM_DelayMs(4);
				if (BK4819_read_reg(0x0C) & (1u << 0))
				{	// we have interrupt flags
					BK4819_write_reg(0x02, 0);
					if (BK4819_read_reg(0x02) & BK4819_REG_02_FSK_TX_FINISHED)
						timeout = 0;       // TX is complete
				}
			}
		}

		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_SPEAKER);

		// disable FSK
		BK4819_write_reg(0x59, fsk_reg59);

		BK4819_write_reg(0x3F, 0);   // disable interrupts
		BK4819_write_reg(0x70, 0);
		BK4819_write_reg(0x58, 0);

		// restore FM deviation level
		BK4819_write_reg(0x40, dev_val);

		// restore TX/RX filtering
		BK4819_write_reg(0x2B, filt_val);

		// restore the CTCSS/CDCSS setting
		BK4819_write_reg(0x51, css_val);

		//BK4819_EnterTxMute();
		BK4819_write_reg(0x50, 0xBB20); // 1011 1011 0010 0000

		//BK4819_SetAF(BK4819_AF_MUTE);
		BK4819_write_reg(0x47, (1u << 14) | (1u << 13) | (BK4819_AF_MUTE << 8) | (1u << 6));

		BK4819_write_reg(0x30,
			BK4819_REG_30_ENABLE_VCO_CALIB |
			BK4819_REG_30_ENABLE_UNKNOWN   |
//			BK4819_REG_30_ENABLE_RX_LINK   |
//			BK4819_REG_30_ENABLE_AF_DAC    |
			BK4819_REG_30_ENABLE_DISC_MODE |
			BK4819_REG_30_ENABLE_PLL_VCO   |
			BK4819_REG_30_ENABLE_PA_GAIN   |
			BK4819_REG_30_ENABLE_MIC_ADC   |
			BK4819_REG_30_ENABLE_TX_DSP    |
//			BK4819_REG_30_ENABLE_RX_DSP    |
		0);

		//BK4819_ExitTxMute();
		BK4819_write_reg(0x50, 0x3B20);  // 0011 1011 0010 0000
	}
#endif

void BK4819_Enable_AfDac_DiscMode_TxDsp(void)
{
	BK4819_write_reg(0x30, 0);
	BK4819_write_reg(0x30,
//		BK4819_REG_30_ENABLE_VCO_CALIB |
//		BK4819_REG_30_ENABLE_UNKNOWN   |
//		BK4819_REG_30_ENABLE_RX_LINK   |
		BK4819_REG_30_ENABLE_AF_DAC    |
		BK4819_REG_30_ENABLE_DISC_MODE |
//		BK4819_REG_30_ENABLE_PLL_VCO   |
//		BK4819_REG_30_ENABLE_PA_GAIN   |
//		BK4819_REG_30_ENABLE_MIC_ADC   |
		BK4819_REG_30_ENABLE_TX_DSP    |
//		BK4819_REG_30_ENABLE_RX_DSP    |
	0);
}

void BK4819_GetVoxAmp(uint16_t *pResult)
{
	*pResult = BK4819_read_reg(0x64) & 0x7FFF;
}

void BK4819_SetScrambleFrequencyControlWord(uint32_t Frequency)
{
	BK4819_write_reg(0x71, scale_freq(Frequency));
}

void BK4819_PlayDTMFEx(bool bLocalLoopback, char Code)
{
	BK4819_EnableDTMF();
	BK4819_EnterTxMute();
	BK4819_SetAF(bLocalLoopback ? BK4819_AF_BEEP : BK4819_AF_MUTE);
	BK4819_write_reg(0x70, 0xD3D3);  // 1101 0011 1101 0011
	BK4819_EnableTXLink();
	SYSTEM_DelayMs(50);
	BK4819_PlayDTMF(Code);
	BK4819_ExitTxMute();
}
