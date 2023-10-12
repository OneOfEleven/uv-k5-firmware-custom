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

#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "audio.h"
#include "bsp/dp32g030/gpio.h"
#ifdef ENABLE_FMRADIO
	#include "driver/bk1080.h"
#endif
#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/ui.h"

#ifdef ENABLE_VOICE

	static const uint8_t VoiceClipLengthChinese[58] =
	{
		0x32, 0x32, 0x32, 0x37, 0x37, 0x32, 0x32, 0x32,
		0x32, 0x37, 0x37, 0x32, 0x64, 0x64, 0x64, 0x64,
		0x64, 0x69, 0x64, 0x69, 0x5A, 0x5F, 0x5F, 0x64,
		0x64, 0x69, 0x64, 0x64, 0x69, 0x69, 0x69, 0x64,
		0x64, 0x6E, 0x69, 0x5F, 0x64, 0x64, 0x64, 0x69,
		0x69, 0x69, 0x64, 0x69, 0x64, 0x64, 0x55, 0x5F,
		0x5A, 0x4B, 0x4B, 0x46, 0x46, 0x69, 0x64, 0x6E,
		0x5A, 0x64,
	};

	static const uint8_t VoiceClipLengthEnglish[76] =
	{
		0x50, 0x32, 0x2D, 0x2D, 0x2D, 0x37, 0x37, 0x37,
		0x32, 0x32, 0x3C, 0x37, 0x46, 0x46, 0x4B, 0x82,
		0x82, 0x6E, 0x82, 0x46, 0x96, 0x64, 0x46, 0x6E,
		0x78, 0x6E, 0x87, 0x64, 0x96, 0x96, 0x46, 0x9B,
		0x91, 0x82, 0x82, 0x73, 0x78, 0x64, 0x82, 0x6E,
		0x78, 0x82, 0x87, 0x6E, 0x55, 0x78, 0x64, 0x69,
		0x9B, 0x5A, 0x50, 0x3C, 0x32, 0x55, 0x64, 0x64,
		0x50, 0x46, 0x46, 0x46, 0x4B, 0x4B, 0x50, 0x50,
		0x55, 0x4B, 0x4B, 0x32, 0x32, 0x32, 0x32, 0x37,
		0x41, 0x32, 0x3C, 0x37,
	};

	voice_id_t        g_voice_id[8];
	uint8_t           g_voice_read_index;
	uint8_t           g_voice_write_index;
	volatile uint16_t g_count_down_to_play_next_voice_10ms;
	volatile bool     g_flag_play_queued_voice;
	voice_id_t        g_another_voice_id = VOICE_ID_INVALID;

#endif

beep_type_t g_beep_to_play = BEEP_NONE;

void AUDIO_PlayBeep(beep_type_t Beep)
{
	uint16_t ToneConfig;
	uint16_t ToneFrequency;
	uint16_t Duration;

	if (Beep != BEEP_880HZ_60MS_TRIPLE_BEEP &&
	    Beep != BEEP_500HZ_60MS_DOUBLE_BEEP &&
	    Beep != BEEP_440HZ_500MS &&
	    Beep != BEEP_880HZ_200MS &&
	    Beep != BEEP_880HZ_500MS &&
	   !g_eeprom.beep_control)
		return;

	#ifdef ENABLE_AIRCOPY
		if (g_screen_to_display == DISPLAY_AIRCOPY)
			return;
	#endif

	if (g_current_function == FUNCTION_RECEIVE)
		return;

	if (g_current_function == FUNCTION_MONITOR)
		return;

	ToneConfig = BK4819_ReadRegister(BK4819_REG_71);

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

	if (g_current_function == FUNCTION_POWER_SAVE && g_rx_idle_mode)
		BK4819_RX_TurnOn();

	#ifdef ENABLE_FMRADIO
		#ifdef MUTE_AUDIO_FOR_VOICE
			if (g_fm_radio_mode)
				BK1080_Mute(true);
		#endif
	#endif

	SYSTEM_DelayMs(20);

	switch (Beep)
	{
		default:
		case BEEP_NONE:
			ToneFrequency = 220;
			break;
		case BEEP_1KHZ_60MS_OPTIONAL:
			ToneFrequency = 1000;
			break;
		case BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL:
		case BEEP_500HZ_60MS_DOUBLE_BEEP:
			ToneFrequency = 500;
			break;
		case BEEP_440HZ_40MS_OPTIONAL:
		case BEEP_440HZ_500MS:
			ToneFrequency = 440;
			break;
		case BEEP_880HZ_40MS_OPTIONAL:
		case BEEP_880HZ_60MS_TRIPLE_BEEP:
		case BEEP_880HZ_200MS:
		case BEEP_880HZ_500MS:
			ToneFrequency = 880;
			break;
	}

	BK4819_PlayTone(ToneFrequency, true);

	SYSTEM_DelayMs(2);

	GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

	SYSTEM_DelayMs(60);

	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

	switch (Beep)
	{
		case BEEP_880HZ_60MS_TRIPLE_BEEP:
			BK4819_ExitTxMute();
			SYSTEM_DelayMs(60);
			BK4819_EnterTxMute();
			SYSTEM_DelayMs(20);

		case BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL:
		case BEEP_500HZ_60MS_DOUBLE_BEEP:
			BK4819_ExitTxMute();
			SYSTEM_DelayMs(60);
			BK4819_EnterTxMute();
			SYSTEM_DelayMs(20);

		case BEEP_1KHZ_60MS_OPTIONAL:
			BK4819_ExitTxMute();
			Duration = 60;
			break;

		case BEEP_880HZ_40MS_OPTIONAL:
		case BEEP_440HZ_40MS_OPTIONAL:
			BK4819_ExitTxMute();
			Duration = 40;
			break;

		case BEEP_880HZ_200MS:
			BK4819_ExitTxMute();
			Duration = 200;
			break;

		case BEEP_440HZ_500MS:
		case BEEP_880HZ_500MS:
		default:
			BK4819_ExitTxMute();
			Duration = 500;
			break;
	}

	#pragma GCC diagnostic pop

	SYSTEM_DelayMs(Duration);
	BK4819_EnterTxMute();
	SYSTEM_DelayMs(20);

	GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

	#ifdef ENABLE_VOX
		g_vox_resume_count_down = 80;
	#endif

	SYSTEM_DelayMs(5);
	BK4819_TurnsOffTones_TurnsOnRX();
	SYSTEM_DelayMs(5);
	BK4819_WriteRegister(BK4819_REG_71, ToneConfig);

	if (g_enable_speaker)
		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

	#ifdef ENABLE_FMRADIO
		if (g_fm_radio_mode)
			BK1080_Mute(false);
	#endif

	if (g_current_function == FUNCTION_POWER_SAVE && g_rx_idle_mode)
		BK4819_Sleep();
}

#ifdef ENABLE_VOICE

	void AUDIO_PlayVoice(uint8_t VoiceID)
	{
		unsigned int i;

		if (g_eeprom.voice_prompt == VOICE_PROMPT_OFF)
			return;
		
		GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_VOICE_0);
		SYSTEM_DelayMs(20);
		GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_VOICE_0);

		for (i = 0; i < 8; i++)
		{
			if ((VoiceID & 0x80U) == 0)
				GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_VOICE_1);
			else
				GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_VOICE_1);
			SYSTICK_DelayUs(1000);
			GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_VOICE_0);
			SYSTICK_DelayUs(1200);
			GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_VOICE_0);
			VoiceID <<= 1;
			SYSTICK_DelayUs(200);
		}
	}

	void AUDIO_PlaySingleVoice(bool flag)
	{
		uint8_t Delay;
		uint8_t VoiceID = g_voice_id[0];

		if (g_eeprom.voice_prompt == VOICE_PROMPT_OFF || g_voice_write_index == 0)
			goto Bailout;

		if (g_eeprom.voice_prompt == VOICE_PROMPT_CHINESE)
		{	// Chinese
			if (VoiceID >= ARRAY_SIZE(VoiceClipLengthChinese))
				goto Bailout;

			Delay    = VoiceClipLengthChinese[VoiceID];
			VoiceID += VOICE_ID_CHI_BASE;
		}
		else
		{	// English
			if (VoiceID >= ARRAY_SIZE(VoiceClipLengthEnglish))
				goto Bailout;

			Delay    = VoiceClipLengthEnglish[VoiceID];
			VoiceID += VOICE_ID_ENG_BASE;
		}

		#ifdef MUTE_AUDIO_FOR_VOICE
			if (g_current_function == FUNCTION_RECEIVE || g_current_function == FUNCTION_MONITOR)
				BK4819_SetAF(BK4819_AF_MUTE);
		#endif
			
		#ifdef ENABLE_FMRADIO
			#ifdef MUTE_AUDIO_FOR_VOICE
				if (g_fm_radio_mode)
					BK1080_Mute(true);
			#endif
		#endif

		GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

		#ifdef ENABLE_VOX
			g_vox_resume_count_down = 2000;
		#endif

		SYSTEM_DelayMs(5);
		
		AUDIO_PlayVoice(VoiceID);

		if (g_voice_write_index == 1)
			Delay += 3;

		if (flag)
		{
			SYSTEM_DelayMs(Delay * 10);

			if (g_current_function == FUNCTION_RECEIVE || g_current_function == FUNCTION_MONITOR)
				BK4819_SetAF(g_rx_vfo->am_mode ? BK4819_AF_AM : BK4819_AF_FM);

			#ifdef ENABLE_FMRADIO
				if (g_fm_radio_mode)
					BK1080_Mute(false);
			#endif

			if (!g_enable_speaker)
				GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

			g_voice_write_index    = 0;
			g_voice_read_index     = 0;

			#ifdef ENABLE_VOX
				g_vox_resume_count_down = 80;
			#endif

			return;
		}

		g_voice_read_index                   = 1;
		g_count_down_to_play_next_voice_10ms = Delay;
		g_flag_play_queued_voice             = false;

		return;

	Bailout:
		g_voice_read_index  = 0;
		g_voice_write_index = 0;
	}

	void AUDIO_SetVoiceID(uint8_t Index, voice_id_t VoiceID)
	{
		if (g_eeprom.voice_prompt == VOICE_PROMPT_OFF || Index == 0)
		{
			g_voice_write_index = 0;
			g_voice_read_index  = 0;
		}

		if (g_eeprom.voice_prompt != VOICE_PROMPT_OFF && Index < ARRAY_SIZE(g_voice_id))
		{
			g_voice_id[Index] = VoiceID;
			g_voice_write_index++;
		}
	}

	uint8_t AUDIO_SetDigitVoice(uint8_t Index, uint16_t Value)
	{
		uint16_t Remainder;
		uint8_t  Result;
		uint8_t  Count;

		if (g_eeprom.voice_prompt == VOICE_PROMPT_OFF || Index == 0)
		{
			g_voice_write_index = 0;
			g_voice_read_index  = 0;
		}

		if (g_eeprom.voice_prompt == VOICE_PROMPT_OFF)
			return 0;

		Count     = 0;
		Result    = Value / 1000U;
		Remainder = Value % 1000U;
		if (Remainder < 100U)
		{
			if (Remainder < 10U)
				goto Skip;
		}
		else
		{
			Result = Remainder / 100U;
			g_voice_id[g_voice_write_index++] = (voice_id_t)Result;
			Count++;
			Remainder -= Result * 100U;
		}
		Result = Remainder / 10U;
		g_voice_id[g_voice_write_index++] = (voice_id_t)Result;
		Count++;
		Remainder -= Result * 10U;

	Skip:
		g_voice_id[g_voice_write_index++] = (voice_id_t)Remainder;

		return Count + 1U;
	}

	void AUDIO_PlayQueuedVoice(void)
	{
		uint8_t VoiceID;
		uint8_t Delay;
		bool    Skip = false;

		if (g_eeprom.voice_prompt == VOICE_PROMPT_OFF)
		{
			g_voice_write_index = 0;
			g_voice_read_index  = 0;
			return;
		}

		if (g_voice_read_index != g_voice_write_index && g_eeprom.voice_prompt != VOICE_PROMPT_OFF)
		{
			VoiceID = g_voice_id[g_voice_read_index];
			if (g_eeprom.voice_prompt == VOICE_PROMPT_CHINESE)
			{
				if (VoiceID < ARRAY_SIZE(VoiceClipLengthChinese))
				{
					Delay = VoiceClipLengthChinese[VoiceID];
					VoiceID += VOICE_ID_CHI_BASE;
				}
				else
					Skip = true;
			}
			else
			{
				if (VoiceID < ARRAY_SIZE(VoiceClipLengthEnglish))
				{
					Delay = VoiceClipLengthEnglish[VoiceID];
					VoiceID += VOICE_ID_ENG_BASE;
				}
				else
					Skip = true;
			}

			g_voice_read_index++;

			if (!Skip)
			{
				if (g_voice_read_index == g_voice_write_index)
					Delay += 3;

				AUDIO_PlayVoice(VoiceID);

				g_count_down_to_play_next_voice_10ms = Delay;
				g_flag_play_queued_voice           = false;

				#ifdef ENABLE_VOX
					g_vox_resume_count_down = 2000;
				#endif

				return;
			}
		}

		// ***********************
		// unmute the radios audio
		
		if (g_current_function == FUNCTION_RECEIVE || g_current_function == FUNCTION_MONITOR)
			BK4819_SetAF(g_rx_vfo->am_mode ? BK4819_AF_AM : BK4819_AF_FM);
		
		#ifdef ENABLE_FMRADIO
			if (g_fm_radio_mode)
				BK1080_Mute(false);
		#endif

		if (!g_enable_speaker)
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);

		// **********************
		
		#ifdef ENABLE_VOX
			g_vox_resume_count_down = 80;
		#endif

		g_voice_write_index    = 0;
		g_voice_read_index     = 0;
	}

#endif
