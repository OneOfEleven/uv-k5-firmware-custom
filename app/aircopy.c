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

#include "app/aircopy.h"
#include "audio.h"
#include "driver/bk4819.h"
#include "driver/crc.h"
#include "driver/eeprom.h"
#include "driver/system.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

#define AIRCOPY_MAGIC_START        0xABCD
#define AIRCOPY_MAGIC_END          0xDCBA

#define AIRCOPY_LAST_EEPROM_ADDR   0x1E00

const uint8_t    g_aircopy_block_max = 120;
uint8_t          g_aircopy_block_number;
uint8_t          g_aircopy_rx_errors;
aircopy_state_t  g_aircopy_state;

uint8_t          aircopy_send_count_down_10ms;

uint16_t         g_fsk_buffer[36];
unsigned int     g_fsk_write_index;
uint16_t         g_fsk_tx_timeout_10ms;

void AIRCOPY_start_FSK_tx(const uint8_t request_packet)
{
	unsigned int   i;
	const uint16_t eeprom_addr = (uint16_t)g_aircopy_block_number * 64;

	// will be used to ask the TX/ing radio to resend a missing/corrupted packet
	(void)request_packet;
	
	// *********

	// packet start
	g_fsk_buffer[0] = AIRCOPY_MAGIC_START;

	// eeprom address
	g_fsk_buffer[1] = eeprom_addr;

	// data
	EEPROM_ReadBuffer(eeprom_addr, &g_fsk_buffer[2], 64);

	// data CRC
	g_fsk_buffer[34] = CRC_Calculate(&g_fsk_buffer[1], 2 + 64);

	// packet end
	g_fsk_buffer[35] = AIRCOPY_MAGIC_END;

	// *********
	
	{	// scramble the packet
		//for (i = 0; i < 34; i++)
			//g_fsk_buffer[1 + i] ^= Obfuscation[i % ARRAY_SIZE(Obfuscation)];

		uint8_t *p = (uint8_t *)&g_fsk_buffer[1];
		for (i = 0; i < (34 * 2); i++)
			*p++ ^= obfuscate_array[i % ARRAY_SIZE(obfuscate_array)];
	}
	
	// TX the packet
	RADIO_SetTxParameters();
	BK4819_SetupPowerAmplifier(0, g_current_vfo->p_tx->frequency); // VERY low TX power

	// turn the RED LED on
	BK4819_set_GPIO_pin(BK4819_GPIO1_PIN29_RED, true);

	// start sending the packet

	// let the TX stabilize
	SYSTEM_DelayMs(10);
	
	BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_3F_FSK_TX_FINISHED);

	BK4819_WriteRegister(BK4819_REG_59, 0x8068);
	BK4819_WriteRegister(BK4819_REG_59, 0x0068);
	
	// load the packet
	for (i = 0; i < 36; i++)
		BK4819_WriteRegister(BK4819_REG_5F, g_fsk_buffer[i]);
	
//	SYSTEM_DelayMs(20);
	
	BK4819_WriteRegister(BK4819_REG_59, 0x2868);
	
	g_fsk_tx_timeout_10ms = 1000 / 10; // 1 second timeout
}

void AIRCOPY_stop_FSK_tx(void)
{	
	if (g_aircopy_state != AIRCOPY_TX && g_fsk_tx_timeout_10ms == 0)
		return;
	
	g_fsk_tx_timeout_10ms = 0;
	
	BK4819_WriteRegister(BK4819_REG_02, 0);  // disable all interrupts
	
//	SYSTEM_DelayMs(20);

	BK4819_ResetFSK();

	// disable the TX
	BK4819_SetupPowerAmplifier(0, 0);
	BK4819_set_GPIO_pin(BK4819_GPIO5_PIN1, false);

	// turn the RED LED off
	BK4819_set_GPIO_pin(BK4819_GPIO1_PIN29_RED, false);
			
	if (++g_aircopy_block_number >= g_aircopy_block_max)
	{	// transfer is complete
		g_aircopy_state = AIRCOPY_TX_COMPLETE;
	}
	else
	{
		// TX pause/gap time till we start the next packet
		#if 0
			aircopy_send_count_down_10ms = 300 / 10;   // 300ms
		#else
			aircopy_send_count_down_10ms =  10 / 10;   // 10ms
		#endif
	}
	
	g_update_display = true;
	GUI_DisplayScreen();
}

void AIRCOPY_process_FSK_tx_10ms(void)
{
	if (g_aircopy_state != AIRCOPY_TX)
		return;

	if (g_fsk_tx_timeout_10ms == 0)
	{	// not currently TX'ing
		if (g_aircopy_block_number < g_aircopy_block_max)
		{	// not yet finished the complete transfer
			if (aircopy_send_count_down_10ms > 0)
			{	// waiting till it's time to TX next packet
				if (--aircopy_send_count_down_10ms == 0)
				{	// start next packet
					AIRCOPY_start_FSK_tx(0xff);

					g_update_display = true;
					GUI_DisplayScreen();
				}
			}
		}
		return;
	}

	if (--g_fsk_tx_timeout_10ms > 0)
	{	// still TX'ing
		if ((BK4819_ReadRegister(BK4819_REG_0C) & (1u << 0)) == 0)
			return; /// TX not yet finished
	}

	AIRCOPY_stop_FSK_tx();
}

void AIRCOPY_process_FSK_rx_10ms(const uint16_t interrupt_status_bits)
{
	unsigned int i;
	uint16_t     Status;

	if (g_aircopy_state != AIRCOPY_RX)
		return;

	if (interrupt_status_bits & BK4819_REG_02_FSK_RX_SYNC)
	{
		// turn the green LED on
//		BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_GREEN, true);
	}

	if (interrupt_status_bits & BK4819_REG_02_FSK_RX_FINISHED)
	{
		// turn the green LED off
//		BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_GREEN, false);
	}
	
	if (interrupt_status_bits & BK4819_REG_02_FSK_FIFO_ALMOST_FULL)
	{
		for (i = 0; i < 4; i++)
			g_fsk_buffer[g_fsk_write_index++] = BK4819_ReadRegister(BK4819_REG_5F);

		if (g_fsk_write_index < ARRAY_SIZE(g_fsk_buffer))
		{
			// turn the green LED on
			BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_GREEN, true);
			return;
		}

		// turn the green LED off
		BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_GREEN, false);

		g_fsk_write_index = 0;
		g_update_display  = true;
	
		Status = BK4819_ReadRegister(BK4819_REG_0B);
	
		BK4819_PrepareFSKReceive();
	
		// Doc says bit 4 should be 1 = CRC OK, 0 = CRC FAIL, but original firmware checks for FAIL
	
		if ((Status & (1u << 4)) == 0 &&
		    g_fsk_buffer[0] == AIRCOPY_MAGIC_START &&
		    g_fsk_buffer[35] == AIRCOPY_MAGIC_END)
		{
			unsigned int i;
			uint16_t     CRC;
	
			{	// unscramble the packet
				uint8_t *p = (uint8_t *)&g_fsk_buffer[1];
				for (i = 0; i < (34 * 2); i++)
					*p++ ^= obfuscate_array[i % ARRAY_SIZE(obfuscate_array)];
			}
			
			CRC = CRC_Calculate(&g_fsk_buffer[1], 2 + 64);
	
			if (g_fsk_buffer[34] == CRC)
			{	// CRC is valid
				uint16_t eeprom_addr = g_fsk_buffer[1];
	
				if (eeprom_addr == 0)
				{	// start again
					g_aircopy_block_number = 0;
					g_aircopy_rx_errors    = 0;
				}
				
				if ((eeprom_addr + 64) <= AIRCOPY_LAST_EEPROM_ADDR)
				{	// eeprom block is valid .. write it directly to eeprom
			
					uint16_t *pData = &g_fsk_buffer[2];
					for (i = 0; i < 8; i++)
					{
						if (eeprom_addr == 0x0E98)
						{	// power-on password .. wipe it
							#ifndef ENABLE_PWRON_PASSWORD
								pData[0] = 0xffff;
								pData[1] = 0xffff;
							#endif
						}
						else
						if (eeprom_addr == 0x0F30)
						{	// AES key .. wipe it
							#ifdef ENABLE_RESET_AES_KEY
								pData[0] = 0xffff;
								pData[1] = 0xffff;
								pData[2] = 0xffff;
								pData[3] = 0xffff;
							#endif
						}
							
						EEPROM_WriteBuffer(eeprom_addr, pData);   // 8 bytes at a time
						pData       += 4;
						eeprom_addr += 8;
					}
	
					g_aircopy_block_number = eeprom_addr / 64;
	
					if (eeprom_addr >= AIRCOPY_LAST_EEPROM_ADDR)
					{	// reached end of eeprom config area
						g_aircopy_state  = AIRCOPY_RX_COMPLETE;
				
						// turn the green LED off
						BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_GREEN, false);
						
						g_update_display = true;
					}
	
					memset(g_fsk_buffer, 0, sizeof(g_fsk_buffer));
					return;
				}
			}
		}
	
		g_aircopy_rx_errors++;
	}
}

static void AIRCOPY_Key_DIGITS(key_code_t Key, bool key_pressed, bool key_held)
{
	if (g_aircopy_state == AIRCOPY_RX || g_aircopy_state == AIRCOPY_TX)
		return;

	if (g_aircopy_state != AIRCOPY_READY)
	{
		AIRCOPY_stop_FSK_tx();
		
		g_aircopy_state  = AIRCOPY_READY;
		g_update_display = true;
		GUI_DisplayScreen();
	}

	if (!key_held && key_pressed)
	{
		uint32_t      Frequency;
		unsigned int  i;

		INPUTBOX_Append(Key);
		
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
				#ifdef ENABLE_AIRCOPY_FREQ
					SETTINGS_SaveSettings();   // remeber the frequency for the next time
				#endif
		
				g_rx_vfo->freq_config_rx.frequency = Frequency;
				g_rx_vfo->freq_config_tx.frequency = Frequency;
				RADIO_ConfigureSquelchAndOutputPower(g_rx_vfo);
		
				g_current_vfo = g_rx_vfo;
		
				RADIO_SetupRegisters(true);
				BK4819_SetupAircopy();
				BK4819_ResetFSK();
		
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
			BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_GREEN, false);

			AIRCOPY_stop_FSK_tx();
			g_input_box_index = 0;
			g_aircopy_state   = AIRCOPY_READY;
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
		BK4819_set_GPIO_pin(BK4819_GPIO0_PIN28_GREEN, false);

		g_input_box_index = 0;

		g_aircopy_state  = AIRCOPY_RX;
		g_update_display = true;
		GUI_DisplayScreen();

		g_fsk_write_index      = 0;
		g_aircopy_block_number = 0;
		g_aircopy_rx_errors    = 0;
		memset(g_fsk_buffer, 0, sizeof(g_fsk_buffer));
		
		BK4819_PrepareFSKReceive();
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
	
		g_aircopy_state  = AIRCOPY_TX;
		g_update_display = true;
		GUI_DisplayScreen();

		g_input_box_index = 0;

		g_fsk_write_index      = 0;
		g_aircopy_block_number = 0;
		g_aircopy_rx_errors    = 0;

		g_fsk_tx_timeout_10ms        = 0;
		aircopy_send_count_down_10ms = 30 / 10;   // 30ms
	}
}

void AIRCOPY_ProcessKey(key_code_t Key, bool key_pressed, bool key_held)
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
