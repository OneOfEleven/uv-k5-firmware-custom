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
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

static const uint16_t Obfuscation[8] = {0x6C16, 0xE614, 0x912E, 0x400D, 0x3521, 0x40D5, 0x0313, 0x80E9};

aircopy_state_t g_aircopy_state;
uint16_t        g_air_copy_block_number;
uint16_t        g_errors_during_air_copyy;
uint8_t         g_air_copy_is_send_mode;

uint16_t        g_fsk_buffer[36];

void AIRCOPY_SendMessage(void)
{
	unsigned int i;

	g_fsk_buffer[1] = (g_air_copy_block_number & 0x3FF) << 6;

	EEPROM_ReadBuffer(g_fsk_buffer[1], &g_fsk_buffer[2], 64);

	g_fsk_buffer[34] = CRC_Calculate(&g_fsk_buffer[1], 2 + 64);

	for (i = 0; i < 34; i++)
		g_fsk_buffer[i + 1] ^= Obfuscation[i % 8];

	if (++g_air_copy_block_number >= 0x78)
		g_aircopy_state = AIRCOPY_COMPLETE;

	RADIO_SetTxParameters();

	BK4819_SendFSKData(g_fsk_buffer);
	BK4819_SetupPowerAmplifier(0, 0);
	BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1, false);

	g_air_copy_send_count_down = 30;
}

void AIRCOPY_StorePacket(void)
{
	uint16_t Status;

	if (g_fsk_wite_index < 36)
		return;

	g_fsk_wite_index = 0;
	g_update_display = true;

	Status = BK4819_ReadRegister(BK4819_REG_0B);

	BK4819_PrepareFSKReceive();

	// Doc says bit 4 should be 1 = CRC OK, 0 = CRC FAIL, but original firmware checks for FAIL.

	if ((Status & 0x0010U) == 0 && g_fsk_buffer[0] == 0xABCD && g_fsk_buffer[35] == 0xDCBA)
	{
		uint16_t     CRC;
		unsigned int i;

		for (i = 0; i < 34; i++)
			g_fsk_buffer[i + 1] ^= Obfuscation[i % 8];

		CRC = CRC_Calculate(&g_fsk_buffer[1], 2 + 64);

		if (g_fsk_buffer[34] == CRC)
		{
			const uint16_t *pData;
			uint16_t        Offset = g_fsk_buffer[1];
			if (Offset < 0x1E00)
			{
				pData = &g_fsk_buffer[2];
				for (i = 0; i < 8; i++)
				{
					EEPROM_WriteBuffer(Offset, pData);
					pData  += 4;
					Offset += 8;
				}

				if (Offset == 0x1E00)
					g_aircopy_state = AIRCOPY_COMPLETE;

				g_air_copy_block_number++;

				return;
			}
		}
	}
	
	g_errors_during_air_copyy++;
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
					g_another_voice_id             = (voice_id_t)Key;
				#endif
				g_rx_vfo->band                     = i;
				Frequency                         += 75;
				Frequency                          = FREQUENCY_FloorToStep(Frequency, g_rx_vfo->step_freq, 0);
				g_rx_vfo->freq_config_rx.frequency = Frequency;
				g_rx_vfo->freq_config_tx.frequency = Frequency;
				RADIO_ConfigureSquelchAndOutputPower(g_rx_vfo);
				g_current_vfo                      = g_rx_vfo;
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
		if (g_input_box_index == 0)
		{
			g_fsk_wite_index          = 0;
			g_air_copy_block_number   = 0;
			g_errors_during_air_copyy = 0;
			g_input_box_index         = 0;
			g_air_copy_is_send_mode   = 0;

			BK4819_PrepareFSKReceive();

			g_aircopy_state = AIRCOPY_TRANSFER;
		}
		else
			g_input_box[--g_input_box_index] = 10;

		g_request_display_screen = DISPLAY_AIRCOPY;
	}
}

static void AIRCOPY_Key_MENU(bool key_pressed, bool key_held)
{
	if (!key_held && key_pressed)
	{
		g_fsk_wite_index        = 0;
		g_air_copy_block_number = 0;
		g_input_box_index       = 0;
		g_air_copy_is_send_mode = 1;
		g_fsk_buffer[0]         = 0xABCD;
		g_fsk_buffer[1]         = 0;
		g_fsk_buffer[35]        = 0xDCBA;

		AIRCOPY_SendMessage();

		GUI_DisplayScreen();

		g_aircopy_state = AIRCOPY_TRANSFER;
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
