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

#ifndef ENABLE_OVERLAY
	#include "ARMCM0.h"
#endif
#include "app/aircopy.h"
#include "audio.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/crc.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/uart.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

// **********************

// aircopy packet format is very simple ..
//
//  payloads ................ 0xABCD + 2 byte eeprom address + 64 byte payload + 2 byte CRC + 0xDCBA
//  1of11 req/ack additon ... 0xBCDA + 2 byte eeprom address +                   2 byte CRC + 0xCDBA

#define AIRCOPY_MAGIC_START_REQ    0xBCDA   // used to request a block resend
#define AIRCOPY_MAGIC_END_REQ      0xCDBA   // used to request a block resend

#define AIRCOPY_MAGIC_START        0xABCD   // normal start value
#define AIRCOPY_MAGIC_END          0xDCBA   // normal end   value

#define AIRCOPY_LAST_EEPROM_ADDR   0x1E00   // size of eeprom transferred

// FSK payload data length
#define AIRCOPY_DATA_PACKET_SIZE   (2 + 2 + 64 + 2 + 2)

// FSK req/ack data length .. 0xBCDA + 2 byte eeprom address + 2 byte CRC + 0xCDBA
#define AIRCOPY_REQ_PACKET_SIZE    (2 + 2 + 2 + 2)

// **********************

const unsigned int g_aircopy_block_max = 120;
unsigned int       g_aircopy_block_number;
uint8_t            g_aircopy_rx_errors_fsk_crc;
uint8_t            g_aircopy_rx_errors_magic;
uint8_t            g_aircopy_rx_errors_crc;
aircopy_state_t    g_aircopy_state;

uint16_t           g_fsk_buffer[AIRCOPY_DATA_PACKET_SIZE / 2];
unsigned int       g_fsk_write_index;
uint16_t           g_fsk_tx_timeout_10ms;

uint8_t            aircopy_send_count_down_10ms;

void AIRCOPY_init(void)
{
	// turn the backlight ON
	GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);

	RADIO_setup_registers(true);

	BK4819_SetupAircopy(AIRCOPY_DATA_PACKET_SIZE);

	BK4819_reset_fsk();

	g_aircopy_state = AIRCOPY_READY;

	g_fsk_write_index = 0;
	BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, false);  // LED off
	BK4819_start_fsk_rx(AIRCOPY_DATA_PACKET_SIZE);

	GUI_SelectNextDisplay(DISPLAY_AIRCOPY);
}

void AIRCOPY_start_fsk_tx(const int request_block_num)
{
	const unsigned int eeprom_addr = (request_block_num < 0) ? g_aircopy_block_number * 64 : (unsigned int)request_block_num * 64;
	uint16_t           fsk_reg59;
	unsigned int       k;
	unsigned int       tx_size = 0;

	// *********

	// packet start
	g_fsk_buffer[tx_size++] = (request_block_num < 0) ? AIRCOPY_MAGIC_START : AIRCOPY_MAGIC_START_REQ;

	// eeprom address
	g_fsk_buffer[tx_size++] = eeprom_addr;

	// data
	if (request_block_num < 0)
	{
		EEPROM_ReadBuffer(eeprom_addr, &g_fsk_buffer[tx_size], 64);
		tx_size += 64 / 2;
	}

	// data CRC
	g_fsk_buffer[tx_size++] = CRC_Calculate(&g_fsk_buffer[1], (request_block_num < 0) ? 2 + 64 : 2);

	// packet end
	g_fsk_buffer[tx_size++] = (request_block_num < 0) ? AIRCOPY_MAGIC_END : AIRCOPY_MAGIC_END_REQ;

	// *********

	{	// scramble the packet
		uint8_t *p = (uint8_t *)&g_fsk_buffer[1];
		for (k = 0; k < ((tx_size - 2) * 2); k++)
			*p++ ^= obfuscate_array[k % ARRAY_SIZE(obfuscate_array)];
	}

	g_fsk_tx_timeout_10ms = 1000 / 10; // 1 second timeout

	// turn the TX on
	RADIO_enableTX(true);

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
	// 0x0068 0000 0000 0110 1000
	//
	fsk_reg59 = (0u << 15) |   // 0 or 1   1 = clear TX FIFO
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

	// set the packet size
	BK4819_WriteRegister(0x5D, (((tx_size * 2) - 1) << 8));

	// clear TX fifo
	BK4819_WriteRegister(0x59, (1u << 15) | fsk_reg59);
	BK4819_WriteRegister(0x59, fsk_reg59);

	// load the packet
	for (k = 0; k < tx_size; k++)
		BK4819_WriteRegister(0x5F, g_fsk_buffer[k]);

	// enable tx interrupt(s)
	BK4819_WriteRegister(0x3F, BK4819_REG_3F_FSK_TX_FINISHED);

	// enable scramble, enable TX
	BK4819_WriteRegister(0x59, (1u << 13) | (1u << 11) | fsk_reg59);
}

void AIRCOPY_stop_fsk_tx(void)
{
	if (g_aircopy_state != AIRCOPY_TX && g_fsk_tx_timeout_10ms == 0)
		return;

	g_fsk_tx_timeout_10ms = 0;

	// disable the TX
	BK4819_SetupPowerAmplifier(0, 0);                            //
	BK4819_set_GPIO_pin(BK4819_GPIO1_PIN29_PA_ENABLE, false);    // PA off
	BK4819_set_GPIO_pin(BK4819_GPIO5_PIN1_RED, false);           // LED off

	BK4819_reset_fsk();

	if (g_aircopy_state == AIRCOPY_TX)
	{
		g_aircopy_block_number++;

		// TX pause/gap time till we start the next packet
		aircopy_send_count_down_10ms = 250 / 10;   // 250ms

		g_update_display = true;
		GUI_DisplayScreen();
	}
}

void AIRCOPY_process_fsk_tx_10ms(void)
{
	uint16_t interrupt_bits = 0;

	if (g_aircopy_state != AIRCOPY_TX && g_aircopy_state != AIRCOPY_RX)
		return;

	if (g_fsk_tx_timeout_10ms == 0)
	{	// not currently TX'ing

		if (g_aircopy_state == AIRCOPY_TX)
		{	// we're still TX transferring

			if (g_fsk_write_index > 0)
				return;        // currently RX'ing a packet

			if (aircopy_send_count_down_10ms > 0)
				if (--aircopy_send_count_down_10ms > 0)
					return;    // not yet time to TX next packet

			if (g_aircopy_block_number >= g_aircopy_block_max)
			{	// transfer is complete
				g_aircopy_state = AIRCOPY_TX_COMPLETE;
				AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);
			}
			else
			{	// start next TX packet
				AIRCOPY_start_fsk_tx(-1);
			}

			g_update_display = true;
			GUI_DisplayScreen();
		}

		return;
	}

	if (--g_fsk_tx_timeout_10ms > 0)
	{	// still TX'ing
		if ((BK4819_ReadRegister(0x0C) & (1u << 0)) == 0)
			return;
		BK4819_WriteRegister(0x02, 0);
		interrupt_bits = BK4819_ReadRegister(0x02);
		if ((interrupt_bits & BK4819_REG_02_FSK_TX_FINISHED) == 0)
			return;            // TX not yet finished
	}

	AIRCOPY_stop_fsk_tx();

	if (g_aircopy_state == AIRCOPY_RX)
	{
		g_fsk_write_index = 0;
		BK4819_start_fsk_rx(AIRCOPY_DATA_PACKET_SIZE);
	}
	else
	if (g_aircopy_state == AIRCOPY_TX)
	{
		g_fsk_write_index = 0;
		BK4819_start_fsk_rx(AIRCOPY_REQ_PACKET_SIZE);
	}
}

void AIRCOPY_process_fsk_rx_10ms(void)
{
	const unsigned int block_size   = 64;
	const unsigned int write_size   = 8;
	const unsigned int req_ack_size = 4;
	uint16_t           interrupt_bits;
	uint16_t           status;
	uint16_t           crc1;
	uint16_t           crc2;
	uint16_t           eeprom_addr;
	uint16_t          *data;
	unsigned int       block_num;
	bool               req_ack_packet = false;
	unsigned int       i;

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
	status = BK4819_ReadRegister(0x59);

	if (status & (1u << 11) || g_fsk_tx_timeout_10ms > 0)
		return;   // FSK TX is busy

	if ((status & (1u << 12)) == 0)
	{	// FSK RX is disabled, enable it
		g_fsk_write_index = 0;
		BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, false);  // LED off
		BK4819_start_fsk_rx((g_aircopy_state == AIRCOPY_TX) ? AIRCOPY_REQ_PACKET_SIZE : AIRCOPY_DATA_PACKET_SIZE);
	}

	status = BK4819_ReadRegister(0x0C);
	if ((status & (1u << 0)) == 0)
		return;                                                // no flagged interrupts

	// read the interrupt flags
	BK4819_WriteRegister(0x02, 0);                    // clear them
	interrupt_bits = BK4819_ReadRegister(0x02);

	if (interrupt_bits & BK4819_REG_02_FSK_RX_SYNC)
		BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, true);   // LED on

	if (interrupt_bits & BK4819_REG_02_FSK_RX_FINISHED)
		BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, false);  // LED off

	if ((interrupt_bits & BK4819_REG_02_FSK_FIFO_ALMOST_FULL) == 0)
		return;

	BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, true);       // LED on

	// fetch RX'ed data
	for (i = 0; i < 4; i++)
	{
		const uint16_t word = BK4819_ReadRegister(0x5F);
		if (g_fsk_write_index < ARRAY_SIZE(g_fsk_buffer))
			g_fsk_buffer[g_fsk_write_index++] = word;
	}

	// REG_0B read only
	//
	// <15:12> ???
	//
	// <11:8>  DTMF/5-tone code received
	//
	// <7>     FSK RX sync negative has been found
	//
	// <6>     FSK RX sync positive has been found
	//
	// <5>     ???
	//
	// <4>     FSK RX CRC indicator
	//         1 = CRC pass
	//         0 = CRC fail
	//
	// <3:0>   ???
	//
	status = BK4819_ReadRegister(0x0B);

	// check to see if it's a REQ/ACK packet
	if (g_fsk_write_index == req_ack_size)
		req_ack_packet = (g_fsk_buffer[0] == AIRCOPY_MAGIC_START_REQ && g_fsk_buffer[g_fsk_write_index - 1] == AIRCOPY_MAGIC_END_REQ);

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
//		UART_printf("aircopy rx %04X %u\r\n", interrupt_bits, g_fsk_write_index);
	#endif

	if (g_fsk_write_index < ARRAY_SIZE(g_fsk_buffer) && !req_ack_packet)
		return;        // not yet a complete packet

	// restart the RX
	BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, false);     // LED off
	BK4819_start_fsk_rx((g_aircopy_state == AIRCOPY_TX) ? AIRCOPY_REQ_PACKET_SIZE : AIRCOPY_DATA_PACKET_SIZE);

	g_update_display = true;

	// doc says bit 4 should be 1 = CRC OK, 0 = CRC FAIL, but original firmware checks for FAIL
	if ((status & (1u << 4)) != 0)
	{
		g_aircopy_rx_errors_fsk_crc++;

		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
			UART_printf("aircopy status %04X\r\n", status);
		#endif

		g_fsk_write_index = 0;
		return;
	}

	{	// unscramble the packet
		uint8_t *p = (uint8_t *)&g_fsk_buffer[1];
		for (i = 0; i < ((g_fsk_write_index - 2) * 2); i++)
			*p++ ^= obfuscate_array[i % ARRAY_SIZE(obfuscate_array)];
	}

	// compute the CRC
	crc1 = CRC_Calculate(&g_fsk_buffer[1], (g_fsk_write_index - 3) * 2);
	// fetch the CRC
	crc2 = g_fsk_buffer[g_fsk_write_index - 2];

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		// show the entire packet
		UART_SendText("aircopy");
		for (i = 0; i < g_fsk_write_index; i++)
			UART_printf(" %04X", g_fsk_buffer[i]);
		UART_printf(" - %04X\r\n", status);
	#endif

	// check the CRC
	if (crc2 != crc1)
	{	// invalid CRC
		g_aircopy_rx_errors_crc++;

		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
			UART_printf("aircopy invalid CRC %04X %04X\r\n", crc2, crc1);
		#endif

		if (g_aircopy_state == AIRCOPY_RX)
			goto send_req;

		g_fsk_write_index = 0;
		return;
	}

	eeprom_addr =  g_fsk_buffer[1];
	data        = &g_fsk_buffer[2];

	block_num = eeprom_addr / block_size;

	if (req_ack_packet)
	{	// it's a req/ack packet

		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
			UART_printf("aircopy RX req %04X %04X\r\n", block_num * 64, g_aircopy_block_number * 64);
		#endif

		if (g_aircopy_state == AIRCOPY_TX)
		{	// we are the TX'ing radio
			if (block_num >= g_aircopy_block_max)
			{	// they have all the blocks .. transfer is complete
				g_aircopy_block_number = g_aircopy_block_max;
				g_aircopy_state        = AIRCOPY_TX_COMPLETE;
				AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);
			}
			else
			{	// send them the block they want
				g_aircopy_block_number       = block_num;  // go to the block number they want
				aircopy_send_count_down_10ms = 0;          // TX asap
			}
		}

		g_fsk_write_index = 0;
		return;
	}

	if (g_aircopy_state != AIRCOPY_RX)
	{	// not in RX mode .. ignore it
		g_fsk_write_index = 0;
		return;
	}

	if (g_fsk_buffer[0] != AIRCOPY_MAGIC_START || g_fsk_buffer[g_fsk_write_index - 1] != AIRCOPY_MAGIC_END)
	{	// invalid magics
		g_aircopy_rx_errors_magic++;
		goto send_req;
	}

	if (eeprom_addr != (block_num * block_size))
	{	// eeprom address not block aligned .. ignore it
		goto send_req;
	}

	if (block_num != g_aircopy_block_number)
	{	// not the block number we're expecting .. request the correct one
		goto send_req;
	}

	if ((eeprom_addr + block_size) > AIRCOPY_LAST_EEPROM_ADDR)
	{	// ignore it
		goto send_req;
	}

	// clear the error counts
	g_aircopy_rx_errors_fsk_crc = 0;
	g_aircopy_rx_errors_magic   = 0;
	g_aircopy_rx_errors_crc     = 0;

	// eeprom block appears valid .. write it directly to eeprom

	for (i = 0; i < (block_size / write_size); i++)
	{
		if (eeprom_addr == 0x0E98)
		{	// power-on password .. wipe it
			//#ifndef ENABLE_PWRON_PASSWORD
				memset(data, 0xff, 4);
			//#endif
		}
		else
		if (eeprom_addr == 0x0F30 || eeprom_addr == 0x0F38)
		{	// AES key .. wipe it
			//#ifdef ENABLE_RESET_AES_KEY
				memset(data, 0xff, 8);
			//#endif
		}
		else
		if (eeprom_addr == 0x0F40)
		{	// killed flag, wipe it
			data[2] = 0;
		}

		EEPROM_WriteBuffer8(eeprom_addr, data);   // 8 bytes at a time

		data        += write_size / sizeof(data[0]);
		eeprom_addr += write_size;
	}

	g_aircopy_block_number = block_num + 1;
	g_fsk_write_index      = 0;

	if (eeprom_addr >= AIRCOPY_LAST_EEPROM_ADDR)
	{	// transfer is complete
		g_aircopy_state  = AIRCOPY_RX_COMPLETE;
		AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);

		#ifdef ENABLE_AIRCOPY_RX_REBOOT
			#if defined(ENABLE_OVERLAY)
				overlay_FLASH_RebootToBootloader();
			#else
				NVIC_SystemReset();
			#endif
		#endif
	}
	
	return;
	
send_req:
	g_fsk_write_index = 0;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		UART_printf("aircopy TX req %04X %04X\r\n", g_aircopy_block_number * 64, block_num * 64);
	#endif

	// this packet takes 150ms start to finish
	AIRCOPY_start_fsk_tx(g_aircopy_block_number);
	g_fsk_tx_timeout_10ms = 200 / 5;             // allow up to 200ms for the TX to complete
	while (g_fsk_tx_timeout_10ms-- > 0)
	{
		SYSTEM_DelayMs(5);
		if (BK4819_ReadRegister(0x0C) & (1u << 0))
		{	// we have interrupt flags
			BK4819_WriteRegister(0x02, 0);
			const uint16_t interrupt_bits = BK4819_ReadRegister(0x02);
			if (interrupt_bits & BK4819_REG_02_FSK_TX_FINISHED)
				g_fsk_tx_timeout_10ms = 0;       // TX is complete
		}
	}
	AIRCOPY_stop_fsk_tx();

	BK4819_start_fsk_rx(AIRCOPY_DATA_PACKET_SIZE);
}

static void AIRCOPY_Key_DIGITS(key_code_t Key, bool key_pressed, bool key_held)
{
	if (g_aircopy_state == AIRCOPY_RX || g_aircopy_state == AIRCOPY_TX)
		return;

	if (g_aircopy_state != AIRCOPY_READY)
	{
		g_aircopy_state = AIRCOPY_READY;

		AIRCOPY_stop_fsk_tx();

		g_update_display = true;
		GUI_DisplayScreen();
	}

	if (!key_held && key_pressed)
	{
		uint32_t      Frequency;
		unsigned int  i;

		INPUTBOX_append(Key);

		g_request_display_screen = DISPLAY_AIRCOPY;

		if (g_input_box_index < 6)
		{
			#ifdef ENABLE_VOICE
				g_another_voice_id = (voice_id_t)Key;
			#endif
			return;
		}

		g_input_box_index = 0;

		NUMBER_Get(g_input_box, &Frequency);

		for (i = 0; i < ARRAY_SIZE(FREQ_BAND_TABLE); i++)
		{
			if (Frequency >= FREQ_BAND_TABLE[i].lower && Frequency < FREQ_BAND_TABLE[i].upper)
			{
				#ifdef ENABLE_VOICE
					g_another_voice_id = (voice_id_t)Key;
				#endif

				g_rx_vfo->band = i;

				// round the frequency to nearest step size
				Frequency = ((Frequency + (g_rx_vfo->step_freq / 2)) / g_rx_vfo->step_freq) * g_rx_vfo->step_freq;

				g_aircopy_freq = Frequency;
				#ifdef ENABLE_AIRCOPY_REMEMBER_FREQ
					SETTINGS_save();   // remeber the frequency for the next time
				#endif

				g_rx_vfo->freq_config_rx.frequency = Frequency;
				g_rx_vfo->freq_config_tx.frequency = Frequency;
				RADIO_ConfigureSquelchAndOutputPower(g_rx_vfo);

				g_current_vfo = g_rx_vfo;

				AIRCOPY_init();
				return;
			}
		}

		g_request_display_screen = DISPLAY_AIRCOPY;
	}
}

static void AIRCOPY_Key_EXIT(bool key_pressed, bool key_held)
{
	if (!key_pressed)
		return;

	if (g_aircopy_state != AIRCOPY_READY)
	{
		if (!key_held)
		{
			// turn the green LED off
			BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, false);

			g_input_box_index = 0;
			g_aircopy_state   = AIRCOPY_READY;

			AIRCOPY_stop_fsk_tx();

			AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);

			AIRCOPY_init();

			g_update_display  = true;
			GUI_DisplayScreen();
		}
	}
	else
	if (key_held)
	{
		if (g_input_box_index > 0)
		{	// cancel the frequency input
			g_input_box_index = 0;
			g_update_display  = true;
			GUI_DisplayScreen();
		}
	}
	else
	if (g_input_box_index > 0)
	{	// entering a new frequency to use
		g_input_box[--g_input_box_index] = 10;
		GUI_DisplayScreen();
	}
	else
	{	// enter RX mode

		// turn the green LED off
		BK4819_set_GPIO_pin(BK4819_GPIO6_PIN2_GREEN, false);

		g_input_box_index = 0;

		AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);

		AIRCOPY_init();

		g_fsk_write_index           = 0;
		g_aircopy_block_number      = 0;
		g_aircopy_rx_errors_fsk_crc = 0;
		g_aircopy_rx_errors_magic   = 0;
		g_aircopy_rx_errors_crc     = 0;
		g_aircopy_state             = AIRCOPY_RX;

		BK4819_start_fsk_rx(AIRCOPY_DATA_PACKET_SIZE);

		g_update_display = true;
		GUI_DisplayScreen();
	}
}

static void AIRCOPY_Key_MENU(bool key_pressed, bool key_held)
{
	(void)key_held;

	if (g_aircopy_state == AIRCOPY_RX || g_aircopy_state == AIRCOPY_TX)
		return;   // busy

	if (key_pressed && !key_held)
	{	// key released

		// enter TX mode

		g_input_box_index = 0;

		AUDIO_PlayBeep(BEEP_880HZ_60MS_TRIPLE_BEEP);

		AIRCOPY_init();

		g_fsk_write_index            = 0;
		g_aircopy_block_number       = 0;
		g_aircopy_rx_errors_fsk_crc  = 0;
		g_aircopy_rx_errors_magic    = 0;
		g_aircopy_rx_errors_crc      = 0;
		g_fsk_tx_timeout_10ms        = 0;
		aircopy_send_count_down_10ms = 0;
		g_aircopy_state              = AIRCOPY_TX;

		g_update_display = true;
		GUI_DisplayScreen();
	}
}

void AIRCOPY_process_key(key_code_t Key, bool key_pressed, bool key_held)
{
	switch (Key)
	{
		case KEY_0:
		case KEY_1:
		case KEY_2:
		case KEY_3:
		case KEY_4:
		case KEY_5:
		case KEY_6:
		case KEY_7:
		case KEY_8:
		case KEY_9:
			AIRCOPY_Key_DIGITS(Key, key_pressed, key_held);
			break;
		case KEY_MENU:
			AIRCOPY_Key_MENU(key_pressed, key_held);
			break;
		case KEY_EXIT:
			AIRCOPY_Key_EXIT(key_pressed, key_held);
			break;
		default:
			break;
	}
}
