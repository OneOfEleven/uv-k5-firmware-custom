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

#include "app/aircopy.h"
#include "audio.h"
#include "driver/bk4819.h"
#include "driver/crc.h"
#include "driver/eeprom.h"
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

static const uint16_t Obfuscation[] = {
	0x6C16, 0xE614, 0x912E, 0x400D, 0x3521, 0x40D5, 0x0313, 0x80E9
};

const uint8_t    g_aircopy_block_max = 120;
uint8_t          g_aircopy_block_number;
uint8_t          g_aircopy_rx_errors;
aircopy_state_t  g_aircopy_state;
uint16_t         g_aircopy_fsk_buffer[36];
uint8_t          g_aircopy_send_count_down_10ms;
unsigned int     g_aircopy_fsk_write_index;

void AIRCOPY_SendMessage(void)
{
	unsigned int   i;
	const uint16_t eeprom_addr = (uint16_t)g_aircopy_block_number * 64;
	
	// *********

	// packet start
	g_aircopy_fsk_buffer[0] = AIRCOPY_MAGIC_START;

	// eeprom address
	g_aircopy_fsk_buffer[1] = eeprom_addr;

	// data
	EEPROM_ReadBuffer(eeprom_addr, &g_aircopy_fsk_buffer[2], 64);

	// data CRC
	g_aircopy_fsk_buffer[34] = CRC_Calculate(&g_aircopy_fsk_buffer[1], 2 + 64);

	// packet end
	g_aircopy_fsk_buffer[35] = AIRCOPY_MAGIC_END;

	// *********
	
	// scramble the packet
	for (i = 0; i < 34; i++)
		g_aircopy_fsk_buffer[1 + i] ^= Obfuscation[i % ARRAY_SIZE(Obfuscation)];

	RADIO_SetTxParameters();

	BK4819_SendFSKData(g_aircopy_fsk_buffer);
	BK4819_SetupPowerAmplifier(0, 0);
	BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1, false);

	if (++g_aircopy_block_number >= g_aircopy_block_max)
	{
		g_aircopy_state  = AIRCOPY_TX_COMPLETE;
		g_update_display = true;
	}

	// TX pause/gap time
	#if 0
		g_aircopy_send_count_down_10ms = 300 / 10;   // 300ms
	#else
		g_aircopy_send_count_down_10ms = 30 / 10;   // 30ms
	#endif
}

void AIRCOPY_StorePacket(void)
{
	uint16_t Status;

	if (g_aircopy_fsk_write_index < ARRAY_SIZE(g_aircopy_fsk_buffer))
		return;

	g_aircopy_fsk_write_index = 0;
	g_update_display          = true;

	Status = BK4819_ReadRegister(BK4819_REG_0B);

	BK4819_PrepareFSKReceive();

	// Doc says bit 4 should be 1 = CRC OK, 0 = CRC FAIL, but original firmware checks for FAIL

	if ((Status & (1u << 4)) == 0 && g_aircopy_fsk_buffer[0] == AIRCOPY_MAGIC_START && g_aircopy_fsk_buffer[35] == AIRCOPY_MAGIC_END)
	{
		uint16_t     CRC;
		unsigned int i;

		for (i = 0; i < 34; i++)
			g_aircopy_fsk_buffer[1 + i] ^= Obfuscation[i % ARRAY_SIZE(Obfuscation)];

		CRC = CRC_Calculate(&g_aircopy_fsk_buffer[1], 2 + 64);

		if (g_aircopy_fsk_buffer[34] == CRC)
		{
			uint16_t eeprom_addr = g_aircopy_fsk_buffer[1];

			if (eeprom_addr < AIRCOPY_LAST_EEPROM_ADDR)
			{
				const uint16_t *pData = &g_aircopy_fsk_buffer[2];

				for (i = 0; i < 8; i++)
				{
					EEPROM_WriteBuffer(eeprom_addr, pData);
					pData       += 4;
					eeprom_addr += 8;
				}

				//g_aircopy_block_number++;
				g_aircopy_block_number = eeprom_addr / 64;

				if (eeprom_addr >= AIRCOPY_LAST_EEPROM_ADDR)
				{	// reached end of eeprom config area
			
					g_aircopy_state  = AIRCOPY_RX_COMPLETE;
					g_update_display = true;
				}

				return;
			}
		}
	}

	g_aircopy_rx_errors++;
}

static void AIRCOPY_Key_DIGITS(key_code_t Key, bool key_pressed, bool key_held)
{
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
	if (!key_held && key_pressed)
	{
		if (g_input_box_index > 0)
		{	// entering a new frequency to use
			g_input_box[--g_input_box_index] = 10;
		}
		else
		{	// enter RX mode

			g_aircopy_state  = AIRCOPY_RX;
			g_update_display = true;
			GUI_DisplayScreen();

			g_aircopy_fsk_write_index = 0;
			g_aircopy_block_number    = 0;
			g_aircopy_rx_errors       = 0;
			g_input_box_index         = 0;

			BK4819_PrepareFSKReceive();
		}

		g_request_display_screen = DISPLAY_AIRCOPY;
	}
}

static void AIRCOPY_Key_MENU(bool key_pressed, bool key_held)
{
	if (!key_held && key_pressed)
	{	// enter TX mode

		g_aircopy_state  = AIRCOPY_TX;
		g_update_display = true;
		GUI_DisplayScreen();

		g_input_box_index = 0;

		g_aircopy_fsk_write_index = 0;
		g_aircopy_block_number    = 0;
		g_aircopy_fsk_buffer[0]   = AIRCOPY_MAGIC_START;
		g_aircopy_fsk_buffer[1]   = 0;      // block number
		g_aircopy_fsk_buffer[35]  = AIRCOPY_MAGIC_END;

		AIRCOPY_SendMessage();
	}
}

void AIRCOPY_ProcessKeys(key_code_t Key, bool key_pressed, bool key_held)
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
