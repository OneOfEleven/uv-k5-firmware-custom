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

#define INCLUDE_AES

#include <string.h>

#if !defined(ENABLE_OVERLAY)
	#include "ARMCM0.h"
#endif
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "app/uart.h"
#include "board.h"
#include "bsp/dp32g030/dma.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/aes.h"
#include "driver/bk4819.h"
#include "driver/crc.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#if defined(ENABLE_UART)
	#include "driver/uart.h"
#endif
#include "functions.h"
#include "misc.h"
#include "settings.h"
#if defined(ENABLE_OVERLAY)
	#include "sram-overlay.h"
#endif
#include "version.h"
#include "ui/ui.h"

#define DMA_INDEX(x, y) (((x) + (y)) % sizeof(UART_DMA_Buffer))

#define EEPROM_SIZE     0x2000u  // 8192 .. BL24C64 I2C eeprom chip

// ****************************************************

typedef struct {
	uint16_t ID;
	uint16_t Size;
} __attribute__((packed)) Header_t;

typedef struct {
	uint8_t  pad[2];
	uint16_t ID;
} __attribute__((packed)) Footer_t;

typedef struct {
	Header_t Header;
	uint32_t time_stamp;
} __attribute__((packed)) cmd_0514_t;

// version
typedef struct {
	Header_t Header;
	struct {
		char     Version[16];
		uint8_t  has_custom_aes_key;
		uint8_t  password_locked;
		uint8_t  pad[2];
		uint32_t Challenge[4];
	} __attribute__((packed)) Data;
} __attribute__((packed)) reply_0514_t;

typedef struct {
	Header_t Header;
	uint16_t Offset;
	uint8_t  Size;
	uint8_t  pad;
	uint32_t time_stamp;
} __attribute__((packed)) cmd_051B_t;

typedef struct {
	Header_t Header;
	struct {
		uint16_t Offset;
		uint8_t  Size;
		uint8_t  pad;
		uint8_t  Data[128];
	} __attribute__((packed)) Data;
} __attribute__((packed)) reply_051B_t;

typedef struct {
	Header_t Header;
	uint16_t Offset;
	uint8_t  Size;
	uint8_t  allow_password;
	uint32_t time_stamp;
//	uint8_t  Data[0];      // new compiler strict warning settings doesn't allow zero-length arrays
} __attribute__((packed)) cmd_051D_t;

typedef struct {
	Header_t Header;
	struct {
		uint16_t Offset;
	} __attribute__((packed)) Data;
} __attribute__((packed)) reply_051D_t;

typedef struct {
	Header_t Header;
	struct {
		uint16_t RSSI;
		uint8_t  ExNoiseIndicator;
		uint8_t  GlitchIndicator;
	} __attribute__((packed)) Data;
} __attribute__((packed)) reply_0527_t;

typedef struct {
	Header_t Header;
	struct {
		uint16_t Voltage;
		uint16_t Current;
	} __attribute__((packed)) Data;
} __attribute__((packed)) reply_0529_t;

typedef struct {
	Header_t Header;
	uint32_t Response[4];
} __attribute__((packed)) cmd_052D_t;

typedef struct {
	Header_t Header;
	struct {
		uint8_t locked;
		uint8_t pad[3];
	} __attribute__((packed)) Data;
} __attribute__((packed)) reply_052D_t;

typedef struct {
	Header_t Header;
	uint32_t time_stamp;
} __attribute__((packed)) cmd_052F_t;

static union
{
	uint8_t Buffer[256];
	struct
	{
		Header_t Header;
		uint8_t  Data[252];
	} __attribute__((packed));
} __attribute__((packed)) UART_Command;

uint32_t time_stamp    = 0;
uint16_t write_index   = 0;
bool     is_encrypted  = true;

#ifdef INCLUDE_AES
	uint8_t  is_locked = (uint8_t)true;
	uint8_t  try_count = 0;
#endif

// ****************************************************

static void SendReply(void *preply, uint16_t Size)
{
	Header_t Header;
	Footer_t Footer;

	if (is_encrypted)
	{
		uint8_t     *pBytes = (uint8_t *)preply;
		unsigned int i;
		for (i = 0; i < Size; i++)
			pBytes[i] ^= obfuscate_array[i % 16];
	}

	Header.ID   = 0xCDAB;
	Header.Size = Size;
	UART_Send(&Header, sizeof(Header));
	UART_Send(preply, Size);

	if (is_encrypted)
	{
		Footer.pad[0] = obfuscate_array[(Size + 0) % 16] ^ 0xFF;
		Footer.pad[1] = obfuscate_array[(Size + 1) % 16] ^ 0xFF;
	}
	else
	{
		Footer.pad[0] = 0xFF;
		Footer.pad[1] = 0xFF;
	}
	Footer.ID = 0xBADC;
	UART_Send(&Footer, sizeof(Footer));
}

static void SendVersion(void)
{
	reply_0514_t reply;

	unsigned int slen = strlen(Version_str);
	if (slen > (sizeof(reply.Data.Version) - 1))
		slen = sizeof(reply.Data.Version) - 1;
	
	memset(&reply, 0, sizeof(reply));
	reply.Header.ID               = 0x0515;
	reply.Header.Size             = sizeof(reply.Data);
	memcpy(reply.Data.Version, Version_str, slen);
	reply.Data.has_custom_aes_key = g_has_custom_aes_key;
	reply.Data.password_locked    = g_password_locked;
	reply.Data.Challenge[0]       = g_challenge[0];
	reply.Data.Challenge[1]       = g_challenge[1];
	reply.Data.Challenge[2]       = g_challenge[2];
	reply.Data.Challenge[3]       = g_challenge[3];

	SendReply(&reply, sizeof(reply));
}

#ifdef INCLUDE_AES

static bool IsBadChallenge(const uint32_t *pKey, const uint32_t *pIn, const uint32_t *pResponse)
{
	unsigned int i;
	uint32_t     IV[4] = {0, 0, 0, 0};

	AES_Encrypt(pKey, IV, pIn, IV, true);

	for (i = 0; i < 4; i++)
		if (IV[i] != pResponse[i])
			return true;

	return false;
}

#endif

// version
static void cmd_0514(const uint8_t *pBuffer)
{
	const cmd_0514_t *pCmd = (const cmd_0514_t *)pBuffer;

	time_stamp = pCmd->time_stamp;

	g_serial_config_tick_500ms = serial_config_tick_500ms;

	// show message
	g_request_display_screen = DISPLAY_MAIN;
	g_update_display         = true;

	SendVersion();
}

// read eeprom
static void cmd_051B(const uint8_t *pBuffer)
{
	const cmd_051B_t *pCmd = (const cmd_051B_t *)pBuffer;
	unsigned int      addr = pCmd->Offset;
	unsigned int      size = pCmd->Size;
//	bool              locked = false;
	reply_051B_t      reply;

//	if (pCmd->time_stamp != time_stamp)
//		return;

	g_serial_config_tick_500ms = serial_config_tick_500ms;

	if (addr >= EEPROM_SIZE)
		return;

	if (size > sizeof(reply.Data.Data))
		size = sizeof(reply.Data.Data);
	if (size > (EEPROM_SIZE - addr))
		size =  EEPROM_SIZE - addr;

	if (size == 0)
		return;

	memset(&reply, 0, sizeof(reply));
	reply.Header.ID   = 0x051C;
	reply.Header.Size = size + 4;
	reply.Data.Offset = addr;
	reply.Data.Size   = size;

//	if (g_has_custom_aes_key)
//		locked = is_locked;

//	if (!locked)
		EEPROM_ReadBuffer(addr, reply.Data.Data, size);

	SendReply(&reply, size + 8);
}

// write eeprom
static void cmd_051D(const uint8_t *pBuffer)
{
	const unsigned int write_size    = 8;
	const cmd_051D_t  *pCmd          = (const cmd_051D_t *)pBuffer;
	const unsigned int addr          = pCmd->Offset;
	unsigned int       size          = pCmd->Size;
#ifdef INCLUDE_AES
	bool               reload_eeprom = false;
	bool               locked        = g_has_custom_aes_key ? is_locked : g_has_custom_aes_key;
#endif
	reply_051D_t       reply;

//	if (pCmd->time_stamp != time_stamp)
//		return;

	g_serial_config_tick_500ms = serial_config_tick_500ms;

	if (addr >= EEPROM_SIZE)
		return;

	if (size > (EEPROM_SIZE - addr))
		size =  EEPROM_SIZE - addr;

	if (size == 0)
		return;

	memset(&reply, 0, sizeof(reply));
	reply.Header.ID   = 0x051E;
	reply.Header.Size = size;
	reply.Data.Offset = addr;

#ifdef INCLUDE_AES
	if (!locked)
#endif
	{
		unsigned int i;

		for (i = 0; i < (size / write_size); i++)
		{
			const unsigned int k = i * write_size;
			const unsigned int Offset = addr + k;
			uint8_t *data = (uint8_t *)pCmd + sizeof(cmd_051D_t) + k;

			if ((Offset + write_size) > EEPROM_SIZE)
				break;

			#ifdef INCLUDE_AES
				if (Offset >= 0x0F30 && Offset < 0x0F40)     // AES key
					if (!is_locked)
						reload_eeprom = true;
			#else
				if (Offset == 0x0F30)
					memset(data, 0xff, 8);   // wipe the AES key
			#endif

			//#ifndef ENABLE_KILL_REVIVE
				if (Offset == 0x0F40)
				{	// killed flag is here
					data[2] = false;	// remove it
				}
			//#endif

			#ifdef ENABLE_PWRON_PASSWORD
				if ((Offset < 0x0E98 || Offset >= 0x0E9C) || !g_password_locked || pCmd->allow_password)
					EEPROM_WriteBuffer8(Offset, data);
			#else
				if (Offset == 0x0E98)
					memset(data, 0xff, 4);   // wipe the password 
				EEPROM_WriteBuffer8(Offset, data);
			#endif
		}

		#ifdef INCLUDE_AES
			if (reload_eeprom)
				BOARD_eeprom_load();
		#endif
	}

	SendReply(&reply, sizeof(reply));
}

// read RSSI
static void cmd_0527(void)
{
	reply_0527_t reply;

	memset(&reply, 0, sizeof(reply));
	reply.Header.ID             = 0x0528;
	reply.Header.Size           = sizeof(reply.Data);
	reply.Data.RSSI             = BK4819_ReadRegister(0x67) & 0x01FF;
	reply.Data.ExNoiseIndicator = BK4819_ReadRegister(0x65) & 0x007F;
	reply.Data.GlitchIndicator  = BK4819_ReadRegister(0x63);

	SendReply(&reply, sizeof(reply));
}

// read ADC
static void cmd_0529(void)
{
	uint16_t voltage;
	uint16_t current;
	reply_0529_t reply;
	memset(&reply, 0, sizeof(reply));
	reply.Header.ID   = 0x52A;
	reply.Header.Size = sizeof(reply.Data);
	// Original doesn't actually send current!
	BOARD_ADC_GetBatteryInfo(&voltage, &current);
	reply.Data.Voltage = voltage;
	reply.Data.Current = current;
	SendReply(&reply, sizeof(reply));
}

#ifdef INCLUDE_AES

static void cmd_052D(const uint8_t *pBuffer)
{
	cmd_052D_t  *pCmd   = (cmd_052D_t *)pBuffer;
	bool         locked = g_has_custom_aes_key;
	uint32_t     response[4];
	reply_052D_t reply;

	g_serial_config_tick_500ms = serial_config_tick_500ms;

	if (!locked)
	{
		memcpy((void *)&response, &pCmd->Response, sizeof(response));    // overcome strict compiler warning settings
		locked = IsBadChallenge(g_custom_aes_key, g_challenge, response);
	}

	if (!locked)
	{
		memcpy((void *)&response, &pCmd->Response, sizeof(response));    // overcome strict compiler warning settings
		locked = IsBadChallenge(g_default_aes_key, g_challenge, response);
		if (locked)
			try_count++;
	}

	if (try_count < 3)
	{
		if (!locked)
			try_count = 0;
	}
	else
	{
		try_count = 3;
		locked    = true;
	}

	is_locked = locked;

	memset(&reply, 0, sizeof(reply));
	reply.Header.ID   = 0x052E;
	reply.Header.Size = sizeof(reply.Data);
	reply.Data.locked = is_locked;

	SendReply(&reply, sizeof(reply));
}

#endif

static void cmd_052F(const uint8_t *pBuffer)
{
	const cmd_052F_t *pCmd = (const cmd_052F_t *)pBuffer;

	g_eeprom.dual_watch                       = DUAL_WATCH_OFF;
	g_eeprom.cross_vfo_rx_tx                  = CROSS_BAND_OFF;
	g_eeprom.rx_vfo                           = 0;
	g_eeprom.dtmf_side_tone                   = false;
	g_eeprom.vfo_info[0].frequency_reverse    = false;
	g_eeprom.vfo_info[0].p_rx                  = &g_eeprom.vfo_info[0].freq_config_rx;
	g_eeprom.vfo_info[0].p_tx                  = &g_eeprom.vfo_info[0].freq_config_tx;
	g_eeprom.vfo_info[0].tx_offset_freq_dir   = TX_OFFSET_FREQ_DIR_OFF;
	g_eeprom.vfo_info[0].dtmf_ptt_id_tx_mode  = PTT_ID_OFF;
	g_eeprom.vfo_info[0].dtmf_decoding_enable = false;

	g_serial_config_tick_500ms = serial_config_tick_500ms;

	#ifdef ENABLE_NOAA
		g_is_noaa_mode = false;
	#endif

	if (g_current_function == FUNCTION_POWER_SAVE)
		FUNCTION_Select(FUNCTION_FOREGROUND);

	time_stamp = pCmd->time_stamp;

	// show message
	g_request_display_screen = DISPLAY_MAIN;
	g_update_display = true;

	SendVersion();
}

bool UART_IsCommandAvailable(void)
{
	uint16_t Index;
	uint16_t TailIndex;
	uint16_t Size;
	uint16_t CRC;
	uint16_t CommandLength;
	uint16_t DmaLength = DMA_CH0->ST & 0xFFFU;

	while (1)
	{
		if (write_index == DmaLength)
			return false;

		while (write_index != DmaLength && UART_DMA_Buffer[write_index] != 0xABU)
			write_index = DMA_INDEX(write_index, 1);

		if (write_index == DmaLength)
			return false;

		if (write_index < DmaLength)
			CommandLength = DmaLength - write_index;
		else
			CommandLength = (DmaLength + sizeof(UART_DMA_Buffer)) - write_index;

		if (CommandLength < 8)
			return 0;

		if (UART_DMA_Buffer[DMA_INDEX(write_index, 1)] == 0xCD)
			break;

		write_index = DMA_INDEX(write_index, 1);
	}

	Index = DMA_INDEX(write_index, 2);
	Size  = (UART_DMA_Buffer[DMA_INDEX(Index, 1)] << 8) | UART_DMA_Buffer[Index];

	if ((Size + 8u) > sizeof(UART_DMA_Buffer))
	{
		write_index = DmaLength;
		return false;
	}

	if (CommandLength < (Size + 8))
		return false;

	Index     = DMA_INDEX(Index, 2);
	TailIndex = DMA_INDEX(Index, Size + 2);

	if (UART_DMA_Buffer[TailIndex] != 0xDC || UART_DMA_Buffer[DMA_INDEX(TailIndex, 1)] != 0xBA)
	{
		write_index = DmaLength;
		return false;
	}

	if (TailIndex < Index)
	{
		const uint16_t ChunkSize = sizeof(UART_DMA_Buffer) - Index;
		memcpy(UART_Command.Buffer, UART_DMA_Buffer + Index, ChunkSize);
		memcpy(UART_Command.Buffer + ChunkSize, UART_DMA_Buffer, TailIndex);
	}
	else
		memcpy(UART_Command.Buffer, UART_DMA_Buffer + Index, TailIndex - Index);

	TailIndex = DMA_INDEX(TailIndex, 2);
	if (TailIndex < write_index)
	{
		memset(UART_DMA_Buffer + write_index, 0, sizeof(UART_DMA_Buffer) - write_index);
		memset(UART_DMA_Buffer, 0, TailIndex);
	}
	else
		memset(UART_DMA_Buffer + write_index, 0, TailIndex - write_index);

	write_index = TailIndex;

	if (UART_Command.Header.ID == 0x0514)
		is_encrypted = false;

	if (UART_Command.Header.ID == 0x6902)
		is_encrypted = true;

	if (is_encrypted)
	{
		unsigned int i;
		for (i = 0; i < (Size + 2u); i++)
			UART_Command.Buffer[i] ^= obfuscate_array[i % 16];
	}

	CRC = UART_Command.Buffer[Size] | (UART_Command.Buffer[Size + 1] << 8);

	return (CRC_Calculate(UART_Command.Buffer, Size) != CRC) ? false : true;
}

void UART_HandleCommand(void)
{
	switch (UART_Command.Header.ID)
	{
		case 0x0514:    // version
			cmd_0514(UART_Command.Buffer);
			break;

		case 0x051B:    // read eeprom
			cmd_051B(UART_Command.Buffer);
			break;

		case 0x051D:    // write eeprom
			cmd_051D(UART_Command.Buffer);
			break;

		case 0x051F:	// Not implementing non-authentic command
			break;

		case 0x0521:	// Not implementing non-authentic command
			break;

		case 0x0527:    // read RSSI
			cmd_0527();
			break;

		case 0x0529:    // read ADC
			cmd_0529();
			break;
			
#ifdef INCLUDE_AES
		case 0x052D:    //
			cmd_052D(UART_Command.Buffer);
			break;
#endif

		case 0x052F:    //
			cmd_052F(UART_Command.Buffer);
			break;

		case 0x05DD:    // reboot
			#if defined(ENABLE_OVERLAY)
				overlay_FLASH_RebootToBootloader();
			#else
				NVIC_SystemReset();
			#endif
			break;
	}
}
