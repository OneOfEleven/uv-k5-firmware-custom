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

#include "app/dtmf.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "board.h"
#include "bsp/dp32g030/gpio.h"
#include "bsp/dp32g030/portcon.h"
#include "bsp/dp32g030/saradc.h"
#include "bsp/dp32g030/syscon.h"
#include "driver/adc.h"
//#include "driver/backlight.h"
#ifdef ENABLE_FMRADIO
	#include "driver/bk1080.h"
#endif
#include "driver/bk4819.h"
#include "driver/crc.h"
#include "driver/eeprom.h"
#include "driver/flash.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/st7565.h"
#include "frequencies.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#if defined(ENABLE_OVERLAY)
	#include "sram-overlay.h"
#endif
#include "ui/menu.h"

static const uint32_t gDefaultFrequencyTable[] =
{
	14500000,    //
	14550000,    //
	43300000,    //
	43320000,    //
	43350000     //
};

#if defined(ENABLE_OVERLAY)
	void BOARD_FLASH_Init(void)
	{
		FLASH_Init(FLASH_READ_MODE_1_CYCLE);
		FLASH_ConfigureTrimValues();
		SYSTEM_ConfigureClocks();

		overlay_FLASH_MainClock       = 48000000;
		overlay_FLASH_ClockMultiplier = 48;

		FLASH_Init(FLASH_READ_MODE_2_CYCLE);
	}
#endif

void BOARD_GPIO_Init(void)
{
	GPIOA->DIR |= 0
		// A7 = UART1 TX default as OUTPUT from bootloader!
		// A8 = UART1 RX default as INPUT from bootloader!
		// Key pad + I2C
		| GPIO_DIR_10_BITS_OUTPUT
		// Key pad + I2C
		| GPIO_DIR_11_BITS_OUTPUT
		// Key pad + Voice chip
		| GPIO_DIR_12_BITS_OUTPUT
		// Key pad + Voice chip
		| GPIO_DIR_13_BITS_OUTPUT
		;
	GPIOA->DIR &= ~(0
		// Key pad
		| GPIO_DIR_3_MASK // INPUT
		// Key pad
		| GPIO_DIR_4_MASK // INPUT
		// Key pad
		| GPIO_DIR_5_MASK // INPUT
		// Key pad
		| GPIO_DIR_6_MASK // INPUT
		);
	GPIOB->DIR |= 0
		// Back light
		| GPIO_DIR_6_BITS_OUTPUT
		// ST7565
		| GPIO_DIR_9_BITS_OUTPUT
		// ST7565 + SWD IO
		| GPIO_DIR_11_BITS_OUTPUT
		// B14 = SWD_CLK assumed INPUT by default
		// BK1080
		| GPIO_DIR_15_BITS_OUTPUT
		;
	GPIOC->DIR |= 0
		// BK4819 SCN
		| GPIO_DIR_0_BITS_OUTPUT
		// BK4819 SCL
		| GPIO_DIR_1_BITS_OUTPUT
		// BK4819 SDA
		| GPIO_DIR_2_BITS_OUTPUT
		// Flash light
		| GPIO_DIR_3_BITS_OUTPUT
		// Speaker
		| GPIO_DIR_4_BITS_OUTPUT
		;
	GPIOC->DIR &= ~(0
		// PTT button
		| GPIO_DIR_5_MASK // INPUT
		);

	#if defined(ENABLE_FMRADIO)
		GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BK1080);
	#endif
}

void BOARD_PORTCON_Init(void)
{
	// PORT A pin selection

	PORTCON_PORTA_SEL0 &= ~(0
		// Key pad
		| PORTCON_PORTA_SEL0_A3_MASK
		// Key pad
		| PORTCON_PORTA_SEL0_A4_MASK
		// Key pad
		| PORTCON_PORTA_SEL0_A5_MASK
		// Key pad
		| PORTCON_PORTA_SEL0_A6_MASK
		);
	PORTCON_PORTA_SEL0 |= 0
		// Key pad
		| PORTCON_PORTA_SEL0_A3_BITS_GPIOA3
		// Key pad
		| PORTCON_PORTA_SEL0_A4_BITS_GPIOA4
		// Key pad
		| PORTCON_PORTA_SEL0_A5_BITS_GPIOA5
		// Key pad
		| PORTCON_PORTA_SEL0_A6_BITS_GPIOA6
		// UART1 TX, wasn't cleared in previous step / relying on default value!
		| PORTCON_PORTA_SEL0_A7_BITS_UART1_TX
		;

	PORTCON_PORTA_SEL1 &= ~(0
		// Key pad + I2C
		| PORTCON_PORTA_SEL1_A10_MASK
		// Key pad + I2C
		| PORTCON_PORTA_SEL1_A11_MASK
		// Key pad + Voice chip
		| PORTCON_PORTA_SEL1_A12_MASK
		// Key pad + Voice chip
		| PORTCON_PORTA_SEL1_A13_MASK
		);
	PORTCON_PORTA_SEL1 |= 0
		// UART1 RX, wasn't cleared in previous step / relying on default value!
		| PORTCON_PORTA_SEL1_A8_BITS_UART1_RX
		// Battery voltage, wasn't cleared in previous step / relying on default value!
		| PORTCON_PORTA_SEL1_A9_BITS_SARADC_CH4
		// Key pad + I2C
		| PORTCON_PORTA_SEL1_A10_BITS_GPIOA10
		// Key pad + I2C
		| PORTCON_PORTA_SEL1_A11_BITS_GPIOA11
		// Key pad + Voice chip
		| PORTCON_PORTA_SEL1_A12_BITS_GPIOA12
		// Key pad + Voice chip
		| PORTCON_PORTA_SEL1_A13_BITS_GPIOA13
		// Battery Current, wasn't cleared in previous step / relying on default value!
		| PORTCON_PORTA_SEL1_A14_BITS_SARADC_CH9
		;

	// PORT B pin selection

	PORTCON_PORTB_SEL0 &= ~(0
		// Back light
		| PORTCON_PORTB_SEL0_B6_MASK
		// SPI0 SSN
		| PORTCON_PORTB_SEL0_B7_MASK
		);
	PORTCON_PORTB_SEL0 |= 0
		// Back light
		| PORTCON_PORTB_SEL0_B6_BITS_GPIOB6
		// SPI0 SSN
		| PORTCON_PORTB_SEL0_B7_BITS_SPI0_SSN
		;

	PORTCON_PORTB_SEL1 &= ~(0
		// ST7565
		| PORTCON_PORTB_SEL1_B9_MASK
		// ST7565 + SWD IO
		| PORTCON_PORTB_SEL1_B11_MASK
		// SWD CLK
		| PORTCON_PORTB_SEL1_B14_MASK
		// BK1080
		| PORTCON_PORTB_SEL1_B15_MASK
		);
	PORTCON_PORTB_SEL1 |= 0
		// SPI0 CLK, wasn't cleared in previous step / relying on default value!
		| PORTCON_PORTB_SEL1_B8_BITS_SPI0_CLK
		// ST7565
		| PORTCON_PORTB_SEL1_B9_BITS_GPIOB9
		// SPI0 MOSI, wasn't cleared in previous step / relying on default value!
		| PORTCON_PORTB_SEL1_B10_BITS_SPI0_MOSI
#if defined(ENABLE_SWD)
		// SWD IO
		| PORTCON_PORTB_SEL1_B11_BITS_SWDIO
		// SWD CLK
		| PORTCON_PORTB_SEL1_B14_BITS_SWCLK
#else
		// ST7565
		| PORTCON_PORTB_SEL1_B11_BITS_GPIOB11
#endif
		;

	// PORT C pin selection

	PORTCON_PORTC_SEL0 &= ~(0
		// BK4819 SCN
		| PORTCON_PORTC_SEL0_C0_MASK
		// BK4819 SCL
		| PORTCON_PORTC_SEL0_C1_MASK
		// BK4819 SDA
		| PORTCON_PORTC_SEL0_C2_MASK
		// Flash light
		| PORTCON_PORTC_SEL0_C3_MASK
		// Speaker
		| PORTCON_PORTC_SEL0_C4_MASK
		// PTT button
		| PORTCON_PORTC_SEL0_C5_MASK
		);

	// PORT A pin configuration

	PORTCON_PORTA_IE |= 0
		// Keypad
		| PORTCON_PORTA_IE_A3_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_IE_A4_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_IE_A5_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_IE_A6_BITS_ENABLE
		// A7 = UART1 TX disabled by default
		// UART1 RX
		| PORTCON_PORTA_IE_A8_BITS_ENABLE
		;
	PORTCON_PORTA_IE &= ~(0
		// Keypad + I2C
		| PORTCON_PORTA_IE_A10_MASK
		// Keypad + I2C
		| PORTCON_PORTA_IE_A11_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_IE_A12_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_IE_A13_MASK
		);

	PORTCON_PORTA_PU |= 0
		// Keypad
		| PORTCON_PORTA_PU_A3_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_PU_A4_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_PU_A5_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_PU_A6_BITS_ENABLE
		;
	PORTCON_PORTA_PU &= ~(0
		// Keypad + I2C
		| PORTCON_PORTA_PU_A10_MASK
		// Keypad + I2C
		| PORTCON_PORTA_PU_A11_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_PU_A12_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_PU_A13_MASK
		);

	PORTCON_PORTA_PD &= ~(0
		// Keypad
		| PORTCON_PORTA_PD_A3_MASK
		// Keypad
		| PORTCON_PORTA_PD_A4_MASK
		// Keypad
		| PORTCON_PORTA_PD_A5_MASK
		// Keypad
		| PORTCON_PORTA_PD_A6_MASK
		// Keypad + I2C
		| PORTCON_PORTA_PD_A10_MASK
		// Keypad + I2C
		| PORTCON_PORTA_PD_A11_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_PD_A12_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_PD_A13_MASK
		);

	PORTCON_PORTA_OD |= 0
		// Keypad
		| PORTCON_PORTA_OD_A3_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_OD_A4_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_OD_A5_BITS_ENABLE
		// Keypad
		| PORTCON_PORTA_OD_A6_BITS_ENABLE
		;
	PORTCON_PORTA_OD &= ~(0
		// Keypad + I2C
		| PORTCON_PORTA_OD_A10_MASK
		// Keypad + I2C
		| PORTCON_PORTA_OD_A11_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_OD_A12_MASK
		// Keypad + Voice chip
		| PORTCON_PORTA_OD_A13_MASK
		);

	// PORT B pin configuration

	PORTCON_PORTB_IE |= 0
		| PORTCON_PORTB_IE_B14_BITS_ENABLE
		;
	PORTCON_PORTB_IE &= ~(0
		// Back light
		| PORTCON_PORTB_IE_B6_MASK
		// UART1
		| PORTCON_PORTB_IE_B7_MASK
		| PORTCON_PORTB_IE_B8_MASK
		// ST7565
		| PORTCON_PORTB_IE_B9_MASK
		// SPI0 MOSI
		| PORTCON_PORTB_IE_B10_MASK
#if !defined(ENABLE_SWD)
		// ST7565
		| PORTCON_PORTB_IE_B11_MASK
#endif
		// BK1080
		| PORTCON_PORTB_IE_B15_MASK
		);

	PORTCON_PORTB_PU &= ~(0
		// Back light
		| PORTCON_PORTB_PU_B6_MASK
		// ST7565
		| PORTCON_PORTB_PU_B9_MASK
		// ST7565 + SWD IO
		| PORTCON_PORTB_PU_B11_MASK
		// SWD CLK
		| PORTCON_PORTB_PU_B14_MASK
		// BK1080
		| PORTCON_PORTB_PU_B15_MASK
		);

	PORTCON_PORTB_PD &= ~(0
		// Back light
		| PORTCON_PORTB_PD_B6_MASK
		// ST7565
		| PORTCON_PORTB_PD_B9_MASK
		// ST7565 + SWD IO
		| PORTCON_PORTB_PD_B11_MASK
		// SWD CLK
		| PORTCON_PORTB_PD_B14_MASK
		// BK1080
		| PORTCON_PORTB_PD_B15_MASK
		);

	PORTCON_PORTB_OD &= ~(0
		// Back light
		| PORTCON_PORTB_OD_B6_MASK
		// ST7565
		| PORTCON_PORTB_OD_B9_MASK
		// ST7565 + SWD IO
		| PORTCON_PORTB_OD_B11_MASK
		// BK1080
		| PORTCON_PORTB_OD_B15_MASK
		);

	PORTCON_PORTB_OD |= 0
		// SWD CLK
		| PORTCON_PORTB_OD_B14_BITS_ENABLE
		;

	// PORT C pin configuration

	PORTCON_PORTC_IE |= 0
		// PTT button
		| PORTCON_PORTC_IE_C5_BITS_ENABLE
		;
	PORTCON_PORTC_IE &= ~(0
		// BK4819 SCN
		| PORTCON_PORTC_IE_C0_MASK
		// BK4819 SCL
		| PORTCON_PORTC_IE_C1_MASK
		// BK4819 SDA
		| PORTCON_PORTC_IE_C2_MASK
		// Flash Light
		| PORTCON_PORTC_IE_C3_MASK
		// Speaker
		| PORTCON_PORTC_IE_C4_MASK
		);

	PORTCON_PORTC_PU |= 0
		// PTT button
		| PORTCON_PORTC_PU_C5_BITS_ENABLE
		;
	PORTCON_PORTC_PU &= ~(0
		// BK4819 SCN
		| PORTCON_PORTC_PU_C0_MASK
		// BK4819 SCL
		| PORTCON_PORTC_PU_C1_MASK
		// BK4819 SDA
		| PORTCON_PORTC_PU_C2_MASK
		// Flash Light
		| PORTCON_PORTC_PU_C3_MASK
		// Speaker
		| PORTCON_PORTC_PU_C4_MASK
		);

	PORTCON_PORTC_PD &= ~(0
		// BK4819 SCN
		| PORTCON_PORTC_PD_C0_MASK
		// BK4819 SCL
		| PORTCON_PORTC_PD_C1_MASK
		// BK4819 SDA
		| PORTCON_PORTC_PD_C2_MASK
		// Flash Light
		| PORTCON_PORTC_PD_C3_MASK
		// Speaker
		| PORTCON_PORTC_PD_C4_MASK
		// PTT Button
		| PORTCON_PORTC_PD_C5_MASK
		);

	PORTCON_PORTC_OD &= ~(0
		// BK4819 SCN
		| PORTCON_PORTC_OD_C0_MASK
		// BK4819 SCL
		| PORTCON_PORTC_OD_C1_MASK
		// BK4819 SDA
		| PORTCON_PORTC_OD_C2_MASK
		// Flash Light
		| PORTCON_PORTC_OD_C3_MASK
		// Speaker
		| PORTCON_PORTC_OD_C4_MASK
		);
	PORTCON_PORTC_OD |= 0
		// BK4819 SCN
		| PORTCON_PORTC_OD_C0_BITS_DISABLE
		// BK4819 SCL
		| PORTCON_PORTC_OD_C1_BITS_DISABLE
		// BK4819 SDA
		| PORTCON_PORTC_OD_C2_BITS_DISABLE
		// Flash Light
		| PORTCON_PORTC_OD_C3_BITS_DISABLE
		// Speaker
		| PORTCON_PORTC_OD_C4_BITS_DISABLE
		// PTT button
		| PORTCON_PORTC_OD_C5_BITS_ENABLE
		;
}

void BOARD_ADC_Init(void)
{
	ADC_Config_t Config;

	Config.CLK_SEL            = SYSCON_CLK_SEL_W_SARADC_SMPL_VALUE_DIV2;
	Config.CH_SEL             = ADC_CH4 | ADC_CH9;
	Config.AVG                = SARADC_CFG_AVG_VALUE_8_SAMPLE;
	Config.CONT               = SARADC_CFG_CONT_VALUE_SINGLE;
	Config.MEM_MODE           = SARADC_CFG_MEM_MODE_VALUE_CHANNEL;
	Config.SMPL_CLK           = SARADC_CFG_SMPL_CLK_VALUE_INTERNAL;
	Config.SMPL_WIN           = SARADC_CFG_SMPL_WIN_VALUE_15_CYCLE;
	Config.SMPL_SETUP         = SARADC_CFG_SMPL_SETUP_VALUE_1_CYCLE;
	Config.ADC_TRIG           = SARADC_CFG_ADC_TRIG_VALUE_CPU;
	Config.CALIB_KD_VALID     = SARADC_CALIB_KD_VALID_VALUE_YES;
	Config.CALIB_OFFSET_VALID = SARADC_CALIB_OFFSET_VALID_VALUE_YES;
	Config.DMA_EN             = SARADC_CFG_DMA_EN_VALUE_DISABLE;
	Config.IE_CHx_EOC         = SARADC_IE_CHx_EOC_VALUE_NONE;
	Config.IE_FIFO_FULL       = SARADC_IE_FIFO_FULL_VALUE_DISABLE;
	Config.IE_FIFO_HFULL      = SARADC_IE_FIFO_HFULL_VALUE_DISABLE;

	ADC_Configure(&Config);
	ADC_Enable();
	ADC_SoftReset();
}

void BOARD_ADC_GetBatteryInfo(uint16_t *pVoltage, uint16_t *pCurrent)
{
	ADC_Start();
	while (!ADC_CheckEndOfConversion(ADC_CH9)) {}
	*pVoltage = ADC_GetValue(ADC_CH4);
	*pCurrent = ADC_GetValue(ADC_CH9);
}

void BOARD_Init(void)
{
	BOARD_PORTCON_Init();
	BOARD_GPIO_Init();
	BOARD_ADC_Init();
	ST7565_Init(true);
	#ifdef ENABLE_FMRADIO
		BK1080_Init(0, false);
	#endif
	CRC_Init();
}

void BOARD_EEPROM_load(void)
{
	unsigned int i;
	uint8_t      Data[16];

	memset(Data, 0, sizeof(Data));

	// 0E70..0E77
	EEPROM_ReadBuffer(0x0E70, Data, 8);
	g_eeprom.chan_1_call          = IS_USER_CHANNEL(Data[0]) ? Data[0] : USER_CHANNEL_FIRST;
	g_eeprom.squelch_level        = (Data[1] < 10) ? Data[1] : 1;
	g_eeprom.tx_timeout_timer     = (Data[2] < 11) ? Data[2] : 1;
	#ifdef ENABLE_NOAA
		g_eeprom.noaa_auto_scan   = (Data[3] <  2) ? Data[3] : false;
	#endif
	g_eeprom.key_lock             = (Data[4] <  2) ? Data[4] : false;
	#ifdef ENABLE_VOX
		g_eeprom.vox_switch       = (Data[5] <  2) ? Data[5] : false;
		g_eeprom.vox_level        = (Data[6] < 10) ? Data[6] : 1;
	#endif
	g_eeprom.mic_sensitivity      = (Data[7] <  5) ? Data[7] : 4;

	// 0E78..0E7F
	EEPROM_ReadBuffer(0x0E78, Data, 8);
	g_setting_contrast             = (Data[0] > 45) ? 31 : (Data[0] < 26) ? 31 : Data[0];
	g_eeprom.channel_display_mode  = (Data[1] < 4) ? Data[1] : MDF_FREQUENCY;    // 4 instead of 3 - extra display mode
	g_eeprom.cross_vfo_rx_tx       = (Data[2] < 3) ? Data[2] : CROSS_BAND_OFF;
	g_eeprom.battery_save          = (Data[3] < 5) ? Data[3] : 4;
	g_eeprom.dual_watch            = (Data[4] < 3) ? Data[4] : DUAL_WATCH_CHAN_A;
	g_eeprom.backlight             = (Data[5] < ARRAY_SIZE(g_sub_menu_backlight)) ? Data[5] : 3;
	g_eeprom.tail_note_elimination = (Data[6] < 2) ? Data[6] : false;
	g_eeprom.vfo_open              = (Data[7] < 2) ? Data[7] : true;

	// 0E80..0E87
	EEPROM_ReadBuffer(0x0E80, Data, 8);
	g_eeprom.screen_channel[0]   = IS_VALID_CHANNEL(Data[0]) ? Data[0] : (FREQ_CHANNEL_FIRST + BAND6_400MHz);
	g_eeprom.screen_channel[1]   = IS_VALID_CHANNEL(Data[3]) ? Data[3] : (FREQ_CHANNEL_FIRST + BAND6_400MHz);
	g_eeprom.user_channel[0]     = IS_USER_CHANNEL(Data[1])  ? Data[1] : USER_CHANNEL_FIRST;
	g_eeprom.user_channel[1]     = IS_USER_CHANNEL(Data[4])  ? Data[4] : USER_CHANNEL_FIRST;
	g_eeprom.freq_channel[0]     = IS_FREQ_CHANNEL(Data[2])  ? Data[2] : (FREQ_CHANNEL_FIRST + BAND6_400MHz);
	g_eeprom.freq_channel[1]     = IS_FREQ_CHANNEL(Data[5])  ? Data[5] : (FREQ_CHANNEL_FIRST + BAND6_400MHz);
	#ifdef ENABLE_NOAA
		g_eeprom.noaa_channel[0] = IS_NOAA_CHANNEL(Data[6])  ? Data[6] : NOAA_CHANNEL_FIRST;
		g_eeprom.noaa_channel[1] = IS_NOAA_CHANNEL(Data[7])  ? Data[7] : NOAA_CHANNEL_FIRST;
	#endif

#ifdef ENABLE_FMRADIO
	{	// 0E88..0E8F
		struct
		{
			uint16_t SelectedFrequency;
			uint8_t  SelectedChannel;
			uint8_t  IsMrMode;
			uint8_t  Padding[8];
		} __attribute__((packed)) FM;

		EEPROM_ReadBuffer(0x0E88, &FM, 8);
		g_eeprom.fm_lower_limit = 760;
		g_eeprom.fm_upper_limit = 1080;
		if (FM.SelectedFrequency < g_eeprom.fm_lower_limit || FM.SelectedFrequency > g_eeprom.fm_upper_limit)
			g_eeprom.fm_selected_frequency = 960;
		else
			g_eeprom.fm_selected_frequency = FM.SelectedFrequency;

		g_eeprom.fm_selected_channel = FM.SelectedChannel;
		g_eeprom.fm_is_channel_mode        = (FM.IsMrMode < 2) ? FM.IsMrMode : false;
	}

	// 0E40..0E67
	EEPROM_ReadBuffer(0x0E40, g_fm_channels, sizeof(g_fm_channels));
	FM_ConfigureChannelState();
#endif

	// 0E90..0E97
	EEPROM_ReadBuffer(0x0E90, Data, 8);
	g_eeprom.beep_control            = (Data[0] < 2)              ? Data[0] : true;
	g_eeprom.key1_short_press_action = (Data[1] < ACTION_OPT_LEN) ? Data[1] : ACTION_OPT_MONITOR;
	g_eeprom.key1_long_press_action  = (Data[2] < ACTION_OPT_LEN) ? Data[2] : ACTION_OPT_FLASHLIGHT;
	g_eeprom.key2_short_press_action = (Data[3] < ACTION_OPT_LEN) ? Data[3] : ACTION_OPT_SCAN;
	g_eeprom.key2_long_press_action  = (Data[4] < ACTION_OPT_LEN) ? Data[4] : ACTION_OPT_NONE;
	g_eeprom.scan_resume_mode        = (Data[5] < 3)              ? Data[5] : SCAN_RESUME_CO;
	g_eeprom.auto_keypad_lock        = (Data[6] < 2)              ? Data[6] : false;
	g_eeprom.pwr_on_display_mode     = (Data[7] < 4)              ? Data[7] : PWR_ON_DISPLAY_MODE_VOLTAGE;

	// 0E98..0E9F
	EEPROM_ReadBuffer(0x0E98, Data, 8);
	memmove(&g_eeprom.power_on_password, Data, 4);

	// 0EA0..0EA7
	#ifdef ENABLE_VOICE
		EEPROM_ReadBuffer(0x0EA0, Data, 8);
		g_eeprom.voice_prompt = (Data[0] < 3) ? Data[0] : VOICE_PROMPT_ENGLISH;
	#endif

	{	// 0EA8..0EAF
		struct {
			uint8_t  alarm_mode;
			uint8_t  roger_mode;
			uint8_t  repeater_tail_tone_elimination;
			uint8_t  tx_vfo;
			uint32_t air_copy_freq;
		} __attribute__((packed)) array;

		EEPROM_ReadBuffer(0x0EA8, &array, sizeof(array));

		#ifdef ENABLE_ALARM
			g_eeprom.alarm_mode                 = (array.alarm_mode < 2) ? array.alarm_mode : true;
		#endif
		g_eeprom.roger_mode                     = (array.roger_mode < 3) ? array.roger_mode : ROGER_MODE_OFF;
		g_eeprom.repeater_tail_tone_elimination = (array.repeater_tail_tone_elimination < 11) ? array.repeater_tail_tone_elimination : 0;
		g_eeprom.tx_vfo                         = (array.tx_vfo < 2) ? array.tx_vfo : 0;
		#ifdef ENABLE_AIRCOPY_FREQ
		{
			unsigned int i;
			for (i = 0; i < ARRAY_SIZE(FREQ_BAND_TABLE); i++)
			{
				if (array.air_copy_freq >= FREQ_BAND_TABLE[i].lower && array.air_copy_freq < FREQ_BAND_TABLE[i].upper)
				{
					g_aircopy_freq = array.air_copy_freq;
					break;
				}
			}	
		}
		#endif
	}
	
	// 0ED0..0ED7
	EEPROM_ReadBuffer(0x0ED0, Data, 8);
	g_eeprom.dtmf_side_tone               = (Data[0] < 2) ? Data[0] : true;
	g_eeprom.dtmf_separate_code           = DTMF_ValidateCodes((char *)(Data + 1), 1) ? Data[1] : '*';
	g_eeprom.dtmf_group_call_code         = DTMF_ValidateCodes((char *)(Data + 2), 1) ? Data[2] : '#';
	g_eeprom.dtmf_decode_response         = (Data[3] < 4) ? Data[3] : DTMF_DEC_RESPONSE_RING;
	g_eeprom.dtmf_auto_reset_time         = (Data[4] <= DTMF_HOLD_MAX) ? Data[4] : (Data[4] >= DTMF_HOLD_MIN) ? Data[4] : DTMF_HOLD_MAX;
	g_eeprom.dtmf_preload_time            = (Data[5] < 101) ? Data[5] * 10 : 200;
	g_eeprom.dtmf_first_code_persist_time = (Data[6] < 101) ? Data[6] * 10 : 70;
	g_eeprom.dtmf_hash_code_persist_time  = (Data[7] < 101) ? Data[7] * 10 : 70;

	// 0ED8..0EDF
	EEPROM_ReadBuffer(0x0ED8, Data, 8);
	g_eeprom.dtmf_code_persist_time  = (Data[0] < 101) ? Data[0] * 10 : 70;
	g_eeprom.dtmf_code_interval_time = (Data[1] < 101) ? Data[1] * 10 : 70;
	g_eeprom.permit_remote_kill      = (Data[2] <   2) ? Data[2] : false;

	// 0EE0..0EE7
	EEPROM_ReadBuffer(0x0EE0, Data, 8);
	if (DTMF_ValidateCodes((char *)Data, 8))
		memmove(g_eeprom.ani_dtmf_id, Data, 8);
	else
	{
		memset(g_eeprom.ani_dtmf_id, 0, sizeof(g_eeprom.ani_dtmf_id));
		strcpy(g_eeprom.ani_dtmf_id, "123");
	}

	// 0EE8..0EEF
	EEPROM_ReadBuffer(0x0EE8, Data, 8);
	if (DTMF_ValidateCodes((char *)Data, 8))
		memmove(g_eeprom.kill_code, Data, 8);
	else
	{
		memset(g_eeprom.kill_code, 0, sizeof(g_eeprom.kill_code));
		strcpy(g_eeprom.kill_code, "ABCD9");
	}

	// 0EF0..0EF7
	EEPROM_ReadBuffer(0x0EF0, Data, 8);
	if (DTMF_ValidateCodes((char *)Data, 8))
		memmove(g_eeprom.revive_code, Data, 8);
	else
	{
		memset(g_eeprom.revive_code, 0, sizeof(g_eeprom.revive_code));
		strcpy(g_eeprom.revive_code, "9DCBA");
	}

	// 0EF8..0F07
	EEPROM_ReadBuffer(0x0EF8, Data, 16);
	if (DTMF_ValidateCodes((char *)Data, 16))
		memmove(g_eeprom.dtmf_up_code, Data, 16);
	else
	{
		memset(g_eeprom.dtmf_up_code, 0, sizeof(g_eeprom.dtmf_up_code));
		strcpy(g_eeprom.dtmf_up_code, "12345");
	}

	// 0F08..0F17
	EEPROM_ReadBuffer(0x0F08, Data, 16);
	if (DTMF_ValidateCodes((char *)Data, 16))
		memmove(g_eeprom.dtmf_down_code, Data, 16);
	else
	{
		memset(g_eeprom.dtmf_down_code, 0, sizeof(g_eeprom.dtmf_down_code));
		strcpy(g_eeprom.dtmf_down_code, "54321");
	}

	// 0F18..0F1F
	EEPROM_ReadBuffer(0x0F18, Data, 8);
//	g_eeprom.scan_list_default = (Data[0] < 2) ? Data[0] : false;
	g_eeprom.scan_list_default = (Data[0] < 3) ? Data[0] : false;  // we now have 'all' channel scan option
	for (i = 0; i < 2; i++)
	{
		const unsigned int j = 1 + (i * 3);
		g_eeprom.scan_list_enabled[i]     = (Data[j + 0] < 2) ? Data[j] : false;
		g_eeprom.scan_list_priority_ch1[i] =  Data[j + 1];
		g_eeprom.scan_list_priority_ch2[i] =  Data[j + 2];
	}

	// 0F40..0F47
	EEPROM_ReadBuffer(0x0F40, Data, 8);
	g_setting_freq_lock          = (Data[0] < 6) ? Data[0] : F_LOCK_OFF;
	g_setting_350_tx_enable      = (Data[1] < 2) ? Data[1] : false;  // was true
	g_setting_killed             = (Data[2] < 2) ? Data[2] : false;
	g_setting_200_tx_enable      = (Data[3] < 2) ? Data[3] : false;
	g_setting_500_tx_enable      = (Data[4] < 2) ? Data[4] : false;
	g_setting_350_enable         = (Data[5] < 2) ? Data[5] : true;
	g_setting_scramble_enable    = (Data[6] < 2) ? Data[6] : true;
	g_setting_tx_enable          = (Data[7] & (1u << 0)) ? true : false;
	g_setting_live_dtmf_decoder  = (Data[7] & (1u << 1)) ? true : false;
	g_setting_battery_text       = (((Data[7] >> 2) & 3u) <= 2) ? (Data[7] >> 2) & 3 : 2;
	#ifdef ENABLE_AUDIO_BAR
		g_setting_mic_bar        = (Data[7] & (1u << 4)) ? true : false;
	#endif
	#ifdef ENABLE_AM_FIX
		g_setting_am_fix         = (Data[7] & (1u << 5)) ? true : false;
	#endif
	g_setting_backlight_on_tx_rx = (Data[7] >> 6) & 3u;

	if (!g_eeprom.vfo_open)
	{
		g_eeprom.screen_channel[0] = g_eeprom.user_channel[0];
		g_eeprom.screen_channel[1] = g_eeprom.user_channel[1];
	}

	// 0D60..0E27
	EEPROM_ReadBuffer(0x0D60, g_user_channel_attributes, sizeof(g_user_channel_attributes));

	// *****************************
	
	// 0F30..0F3F .. AES key
	EEPROM_ReadBuffer(0x0F30, g_custom_aes_key, sizeof(g_custom_aes_key));
	g_has_custom_aes_key = false;
	for (i = 0; i < ARRAY_SIZE(g_custom_aes_key); i++)
	{
		if (g_custom_aes_key[i] != 0xFFFFFFFFu)
		{
			g_has_custom_aes_key = true;
			break;
		}
	}
	
#if ENABLE_RESET_AES_KEY
	// a fix to wipe the darned AES key
	if (g_has_custom_aes_key)
	{	// ugh :( .. wipe it
		uint8_t *p_aes = (uint8_t*)&g_custom_aes_key;
		memset(p_aes, 0xff, sizeof(g_custom_aes_key));
		for (i = 0; i < sizeof(g_custom_aes_key); i += 8)
			EEPROM_WriteBuffer(0x0F30 + i, &p_aes[i]);
		g_has_custom_aes_key = false;
	}
#endif
}

void BOARD_EEPROM_LoadMoreSettings(void)
{
//	uint8_t Mic;

	EEPROM_ReadBuffer(0x1EC0, g_eeprom_1EC0_0, 8);
	memmove(g_eeprom_1EC0_1, g_eeprom_1EC0_0, 8);
	memmove(g_eeprom_1EC0_2, g_eeprom_1EC0_0, 8);
	memmove(g_eeprom_1EC0_3, g_eeprom_1EC0_0, 8);

	// 8 * 16-bit values
	EEPROM_ReadBuffer(0x1EC0, g_eeprom_rssi_calib[0], 8);
	EEPROM_ReadBuffer(0x1EC8, g_eeprom_rssi_calib[1], 8);

	EEPROM_ReadBuffer(0x1F40, g_battery_calibration, 12);
	if (g_battery_calibration[0] >= 5000)
	{
		g_battery_calibration[0] = 1900;
		g_battery_calibration[1] = 2000;
	}
	g_battery_calibration[5] = 2300;

	#ifdef ENABLE_VOX
		EEPROM_ReadBuffer(0x1F50 + (g_eeprom.vox_level * 2), &g_eeprom.vox1_threshold, 2);
		EEPROM_ReadBuffer(0x1F68 + (g_eeprom.vox_level * 2), &g_eeprom.vox0_threshold, 2);
	#endif

	//EEPROM_ReadBuffer(0x1F80 + g_eeprom.mic_sensitivity, &Mic, 1);
	//g_eeprom.mic_sensitivity_tuning = (Mic < 32) ? Mic : 15;
	g_eeprom.mic_sensitivity_tuning = g_mic_gain_dB_2[g_eeprom.mic_sensitivity];

	{
		struct
		{
			int16_t  BK4819_XtalFreqLow;
			uint16_t EEPROM_1F8A;
			uint16_t EEPROM_1F8C;
			uint8_t  volume_gain;
			uint8_t  dac_gain;
		} __attribute__((packed)) Misc;

		// radio 1 .. 04 00 46 00 50 00 2C 0E
		// radio 2 .. 05 00 46 00 50 00 2C 0E
		EEPROM_ReadBuffer(0x1F88, &Misc, 8);

		g_eeprom.BK4819_xtal_freq_low = (Misc.BK4819_XtalFreqLow >= -1000 && Misc.BK4819_XtalFreqLow <= 1000) ? Misc.BK4819_XtalFreqLow : 0;
		g_eeprom_1F8A                 = Misc.EEPROM_1F8A & 0x01FF;
		g_eeprom_1F8C                 = Misc.EEPROM_1F8C & 0x01FF;
		g_eeprom.volume_gain          = (Misc.volume_gain < 64) ? Misc.volume_gain : 58;
		g_eeprom.dac_gain             = (Misc.dac_gain    < 16) ? Misc.dac_gain    : 8;

		BK4819_WriteRegister(BK4819_REG_3B, 22656 + g_eeprom.BK4819_xtal_freq_low);
//		BK4819_WriteRegister(BK4819_REG_3C, g_eeprom.BK4819_XTAL_FREQ_HIGH);
	}
}

uint32_t BOARD_fetchChannelFrequency(const int channel)
{
	struct
	{
		uint32_t frequency;
		uint32_t offset;
	} __attribute__((packed)) info;

	EEPROM_ReadBuffer(channel * 16, &info, sizeof(info));

	return info.frequency;
}

void BOARD_fetchChannelName(char *s, const int channel)
{
	int i;

	if (s == NULL)
		return;

	memset(s, 0, 11);  // 's' had better be large enough !

	if (channel < 0)
		return;

	if (!RADIO_CheckValidChannel(channel, false, 0))
		return;


	EEPROM_ReadBuffer(0x0F50 + (channel * 16), s + 0, 8);
	EEPROM_ReadBuffer(0x0F58 + (channel * 16), s + 8, 2);

	for (i = 0; i < 10; i++)
		if (s[i] < 32 || s[i] > 127)
			break;                // invalid char

	s[i--] = 0;                   // null term

	while (i >= 0 && s[i] == 32)  // trim trailing spaces
		s[i--] = 0;               // null term
}

void BOARD_FactoryReset(bool bIsAll)
{
	uint16_t i;
	uint8_t  Template[8];

	memset(Template, 0xFF, sizeof(Template));

	for (i = 0x0C80; i < 0x1E00; i += 8)
	{
		if (
			!(i >= 0x0EE0 && i < 0x0F18) &&         // ANI ID + DTMF codes
			!(i >= 0x0F30 && i < 0x0F50) &&         // AES KEY + F LOCK + Scramble Enable
			!(i >= 0x1C00 && i < 0x1E00) &&         // DTMF contacts
			!(i >= 0x0EB0 && i < 0x0ED0) &&         // Welcome strings
			!(i >= 0x0EA0 && i < 0x0EA8) &&         // Voice Prompt
			(bIsAll ||
			(
				!(i >= 0x0D60 && i < 0x0E28) &&     // MR Channel Attributes
				!(i >= 0x0F18 && i < 0x0F30) &&     // Scan List
				!(i >= 0x0F50 && i < 0x1C00) &&     // MR Channel Names
				!(i >= 0x0E40 && i < 0x0E70) &&     // FM Channels
				!(i >= 0x0E88 && i < 0x0E90)        // FM settings
				))
			)
		{
			EEPROM_WriteBuffer(i, Template);
		}
	}

	if (bIsAll)
	{
		RADIO_InitInfo(g_rx_vfo, FREQ_CHANNEL_FIRST + BAND6_400MHz, 43350000);

		// set the first few memory channels
		for (i = 0; i < ARRAY_SIZE(gDefaultFrequencyTable); i++)
		{
			const uint32_t Frequency           = gDefaultFrequencyTable[i];
			g_rx_vfo->freq_config_rx.frequency = Frequency;
			g_rx_vfo->freq_config_tx.frequency = Frequency;
			g_rx_vfo->band                     = FREQUENCY_GetBand(Frequency);
			SETTINGS_SaveChannel(USER_CHANNEL_FIRST + i, 0, g_rx_vfo, 2);
		}
	}
}
