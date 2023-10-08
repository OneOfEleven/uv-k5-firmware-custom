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

#include "app/action.h"
#include "app/app.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "app/generic.h"
#include "app/main.h"
#include "app/scanner.h"
#include "audio.h"
#include "board.h"
#include "driver/bk4819.h"
#include "dtmf.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"
#ifdef ENABLE_SPECTRUM
//	#include "app/spectrum.h"
#endif

void toggle_chan_scanlist(void)
{	// toggle the selected channels scanlist setting

	if (gScreenToDisplay == DISPLAY_SCANNER || !IS_USER_CHANNEL(gTxVfo->channel_save))
		return;

	if (gTxVfo->scanlist_1_participation)
	{
		if (gTxVfo->scanlist_2_participation)
			gTxVfo->scanlist_1_participation = 0;
		else
			gTxVfo->scanlist_2_participation = 1;
	}
	else
	{
		if (gTxVfo->scanlist_2_participation)
			gTxVfo->scanlist_2_participation = 0;
		else
			gTxVfo->scanlist_1_participation = 1;
	}

	SETTINGS_UpdateChannel(gTxVfo->channel_save, gTxVfo, true);

	gVfoConfigureMode = VFO_CONFIGURE;
	gFlagResetVfos    = true;
}

static void processFKeyFunction(const key_code_t Key, const bool beep)
{
	uint8_t Band;
	uint8_t Vfo = g_eeprom.tx_vfo;

	if (gScreenToDisplay == DISPLAY_MENU)
	{
//		if (beep)
			gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}
	
//	if (beep)
		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

	switch (Key)
	{
		case KEY_0:
			#ifdef ENABLE_FMRADIO
				ACTION_FM();
			#else


				// TODO: make use of this function key


			#endif
			break;

		case KEY_1:
			if (!IS_FREQ_CHANNEL(gTxVfo->channel_save))
			{
				g_was_f_key_pressed = false;
				gUpdateStatus   = true;
				gBeepToPlay     = BEEP_1KHZ_60MS_OPTIONAL;
				return;
			}

			Band = gTxVfo->band + 1;
			if (gSetting_350EN || Band != BAND5_350MHz)
			{
				if (Band > BAND7_470MHz)
					Band = BAND1_50MHz;
			}
			else
				Band = BAND6_400MHz;
			gTxVfo->band = Band;

			g_eeprom.screen_channel[Vfo] = FREQ_CHANNEL_FIRST + Band;
			g_eeprom.freq_channel[Vfo]   = FREQ_CHANNEL_FIRST + Band;

			gRequestSaveVFO            = true;
			gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;

			gRequestDisplayScreen      = DISPLAY_MAIN;

			if (beep)
				gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

			break;

		case KEY_2:
			if (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_CHAN_A)
				g_eeprom.cross_vfo_rx_tx = CROSS_BAND_CHAN_B;
			else
			if (g_eeprom.cross_vfo_rx_tx == CROSS_BAND_CHAN_B)
				g_eeprom.cross_vfo_rx_tx = CROSS_BAND_CHAN_A;
			else
			if (g_eeprom.dual_watch == DUAL_WATCH_CHAN_A)
				g_eeprom.dual_watch = DUAL_WATCH_CHAN_B;
			else
			if (g_eeprom.dual_watch == DUAL_WATCH_CHAN_B)
				g_eeprom.dual_watch = DUAL_WATCH_CHAN_A;
			else
				g_eeprom.tx_vfo = (Vfo + 1) & 1u;

			gRequestSaveSettings  = 1;
			gFlagReconfigureVfos  = true;

			gRequestDisplayScreen = DISPLAY_MAIN;

			if (beep)
				gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

			break;

		case KEY_3:
			#ifdef ENABLE_NOAA
				if (g_eeprom.vfo_open && IS_NOT_NOAA_CHANNEL(gTxVfo->channel_save))
			#else
				if (g_eeprom.vfo_open)
			#endif
			{
				uint8_t Channel;

				if (IS_USER_CHANNEL(gTxVfo->channel_save))
				{	// swap to frequency mode
					g_eeprom.screen_channel[Vfo] = g_eeprom.freq_channel[g_eeprom.tx_vfo];
					#ifdef ENABLE_VOICE
						g_another_voice_id        = VOICE_ID_FREQUENCY_MODE;
					#endif
					gRequestSaveVFO            = true;
					gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;
					break;
				}

				Channel = RADIO_FindNextChannel(g_eeprom.user_channel[g_eeprom.tx_vfo], 1, false, 0);
				if (Channel != 0xFF)
				{	// swap to channel mode
					g_eeprom.screen_channel[Vfo] = Channel;
					#ifdef ENABLE_VOICE
						AUDIO_SetVoiceID(0, VOICE_ID_CHANNEL_MODE);
						AUDIO_SetDigitVoice(1, Channel + 1);
						g_another_voice_id = (voice_id_t)0xFE;
					#endif
					gRequestSaveVFO     = true;
					gVfoConfigureMode   = VFO_CONFIGURE_RELOAD;
					break;
				}
			}

			if (beep)
				gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;

			break;

		case KEY_4:
			g_was_f_key_pressed          = false;
			gFlagStartScan           = true;
			gScanSingleFrequency     = false;
			gBackup_cross_vfo_rx_tx = g_eeprom.cross_vfo_rx_tx;
			g_eeprom.cross_vfo_rx_tx = CROSS_BAND_OFF;
			gUpdateStatus            = true;

//			if (beep)
//				gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

			break;

		case KEY_5:
			#ifdef ENABLE_NOAA

				if (IS_NOT_NOAA_CHANNEL(gTxVfo->channel_save))
				{
					g_eeprom.screen_channel[Vfo] = g_eeprom.noaa_channel[g_eeprom.tx_vfo];
				}
				else
				{
					g_eeprom.screen_channel[Vfo] = g_eeprom.freq_channel[g_eeprom.tx_vfo];
					#ifdef ENABLE_VOICE
						g_another_voice_id = VOICE_ID_FREQUENCY_MODE;
					#endif
				}
				gRequestSaveVFO   = true;
				gVfoConfigureMode = VFO_CONFIGURE_RELOAD;

			#else
				#ifdef ENABLE_VOX
					toggle_chan_scanlist();
				#endif
			#endif

			break;

		case KEY_6:
			ACTION_Power();
			break;

		case KEY_7:
			#ifdef ENABLE_VOX
				ACTION_Vox();
			#else
				toggle_chan_scanlist();
			#endif
			break;

		case KEY_8:
			gTxVfo->frequency_reverse = gTxVfo->frequency_reverse == false;
			gRequestSaveChannel = 1;
			break;

		case KEY_9:
			if (RADIO_CheckValidChannel(g_eeprom.chan_1_call, false, 0))
			{
				g_eeprom.user_channel[Vfo]     = g_eeprom.chan_1_call;
				g_eeprom.screen_channel[Vfo] = g_eeprom.chan_1_call;
				#ifdef ENABLE_VOICE
					AUDIO_SetVoiceID(0, VOICE_ID_CHANNEL_MODE);
					AUDIO_SetDigitVoice(1, g_eeprom.chan_1_call + 1);
					g_another_voice_id        = (voice_id_t)0xFE;
				#endif
				gRequestSaveVFO            = true;
				gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;
				break;
			}

			if (beep)
				gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			break;

		default:
			gUpdateStatus   = true;
			g_was_f_key_pressed = false;

			if (beep)
				gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
			break;
	}
}

static void MAIN_Key_DIGITS(key_code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	if (bKeyHeld)
	{	// key held down

		if (bKeyPressed)
		{
			if (gScreenToDisplay == DISPLAY_MAIN)
			{
				if (gInputBoxIndex > 0)
				{	// delete any inputted chars
					gInputBoxIndex        = 0;
					gRequestDisplayScreen = DISPLAY_MAIN;
				}

				g_was_f_key_pressed = false;
				gUpdateStatus   = true;

				processFKeyFunction(Key, false);
			}
		}

		return;
	}

	if (bKeyPressed)
	{	// key is pressed
		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;  // beep when key is pressed
		return;                                 // don't use the key till it's released
	}

	if (!g_was_f_key_pressed)
	{	// F-key wasn't pressed

		const uint8_t Vfo = g_eeprom.tx_vfo;

		gKeyInputCountdown = key_input_timeout_500ms;

		INPUTBOX_Append(Key);

		gRequestDisplayScreen = DISPLAY_MAIN;

		if (IS_USER_CHANNEL(gTxVfo->channel_save))
		{	// user is entering channel number

			uint16_t Channel;

			if (gInputBoxIndex != 3)
			{
				#ifdef ENABLE_VOICE
					g_another_voice_id   = (voice_id_t)Key;
				#endif
				gRequestDisplayScreen = DISPLAY_MAIN;
				return;
			}

			gInputBoxIndex = 0;

			Channel = ((gInputBox[0] * 100) + (gInputBox[1] * 10) + gInputBox[2]) - 1;

			if (!RADIO_CheckValidChannel(Channel, false, 0))
			{
				gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
				return;
			}

			#ifdef ENABLE_VOICE
				g_another_voice_id        = (voice_id_t)Key;
			#endif

			g_eeprom.user_channel[Vfo]     = (uint8_t)Channel;
			g_eeprom.screen_channel[Vfo] = (uint8_t)Channel;
			gRequestSaveVFO            = true;
			gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;

			return;
		}

//		#ifdef ENABLE_NOAA
//			if (IS_NOT_NOAA_CHANNEL(gTxVfo->channel_save))
//		#endif
		if (IS_FREQ_CHANNEL(gTxVfo->channel_save))
		{	// user is entering a frequency

			uint32_t Frequency;

			if (gInputBoxIndex < 6)
			{
				#ifdef ENABLE_VOICE
					g_another_voice_id = (voice_id_t)Key;
				#endif

				return;
			}

			gInputBoxIndex = 0;

			NUMBER_Get(gInputBox, &Frequency);

			// clamp the frequency entered to some valid value
			if (Frequency < frequencyBandTable[0].lower)
			{
				Frequency = frequencyBandTable[0].lower;
			}
			else
			if (Frequency >= BX4819_band1.upper && Frequency < BX4819_band2.lower)
			{
				const uint32_t center = (BX4819_band1.upper + BX4819_band2.lower) / 2;
				Frequency = (Frequency < center) ? BX4819_band1.upper : BX4819_band2.lower;
			}
			else
			if (Frequency > frequencyBandTable[ARRAY_SIZE(frequencyBandTable) - 1].upper)
			{
				Frequency = frequencyBandTable[ARRAY_SIZE(frequencyBandTable) - 1].upper;
			}

			{
				const FREQUENCY_Band_t band = FREQUENCY_GetBand(Frequency);

				#ifdef ENABLE_VOICE
					g_another_voice_id = (voice_id_t)Key;
				#endif

				if (gTxVfo->band != band)
				{
					gTxVfo->band               = band;
					g_eeprom.screen_channel[Vfo] = band + FREQ_CHANNEL_FIRST;
					g_eeprom.freq_channel[Vfo]   = band + FREQ_CHANNEL_FIRST;

					SETTINGS_SaveVfoIndices();

					RADIO_ConfigureChannel(Vfo, VFO_CONFIGURE_RELOAD);
				}

//				Frequency += 75;                        // is this meant to be rounding ?
				Frequency += gTxVfo->step_freq / 2; // no idea, but this is

				Frequency = FREQUENCY_FloorToStep(Frequency, gTxVfo->step_freq, frequencyBandTable[gTxVfo->band].lower);

				if (Frequency >= BX4819_band1.upper && Frequency < BX4819_band2.lower)
				{	// clamp the frequency to the limit
					const uint32_t center = (BX4819_band1.upper + BX4819_band2.lower) / 2;
					Frequency = (Frequency < center) ? BX4819_band1.upper - gTxVfo->step_freq : BX4819_band2.lower;
				}

				gTxVfo->freq_config_rx.frequency = Frequency;

				gRequestSaveChannel = 1;
				return;
			}

		}
		#ifdef ENABLE_NOAA
			else
			if (IS_NOAA_CHANNEL(gTxVfo->channel_save))
			{	// user is entering NOAA channel

				uint8_t Channel;

				if (gInputBoxIndex != 2)
				{
					#ifdef ENABLE_VOICE
						g_another_voice_id   = (voice_id_t)Key;
					#endif
					gRequestDisplayScreen = DISPLAY_MAIN;
					return;
				}

				gInputBoxIndex = 0;

				Channel = (gInputBox[0] * 10) + gInputBox[1];
				if (Channel >= 1 && Channel <= ARRAY_SIZE(NoaaFrequencyTable))
				{
					Channel                   += NOAA_CHANNEL_FIRST;
					#ifdef ENABLE_VOICE
						g_another_voice_id        = (voice_id_t)Key;
					#endif
					g_eeprom.noaa_channel[Vfo]   = Channel;
					g_eeprom.screen_channel[Vfo] = Channel;
					gRequestSaveVFO            = true;
					gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;
					return;
				}
			}
		#endif

		gRequestDisplayScreen = DISPLAY_MAIN;
		gBeepToPlay           = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	g_was_f_key_pressed = false;
	gUpdateStatus   = true;

	processFKeyFunction(Key, true);
}

static void MAIN_Key_EXIT(bool bKeyPressed, bool bKeyHeld)
{
	if (!bKeyHeld && bKeyPressed)
	{	// exit key pressed

		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

		if (gDTMF_CallState != DTMF_CALL_STATE_NONE && gCurrentFunction != FUNCTION_TRANSMIT)
		{	// clear CALL mode being displayed
			gDTMF_CallState = DTMF_CALL_STATE_NONE;
			gUpdateDisplay  = true;
			return;
		}

		#ifdef ENABLE_FMRADIO
			if (!gFmRadioMode)
		#endif
		{
			if (gScanStateDir == SCAN_OFF)
			{
				if (gInputBoxIndex == 0)
					return;
				gInputBox[--gInputBoxIndex] = 10;

				gKeyInputCountdown = key_input_timeout_500ms;

				#ifdef ENABLE_VOICE
					if (gInputBoxIndex == 0)
						g_another_voice_id = VOICE_ID_CANCEL;
				#endif
			}
			else
			{
				SCANNER_Stop();

				#ifdef ENABLE_VOICE
					g_another_voice_id = VOICE_ID_SCANNING_STOP;
				#endif
			}

			gRequestDisplayScreen = DISPLAY_MAIN;
			return;
		}

		#ifdef ENABLE_FMRADIO
			ACTION_FM();
		#endif

		return;
	}

	if (bKeyHeld && bKeyPressed)
	{	// exit key held down

		if (gInputBoxIndex > 0 || gDTMF_InputBox_Index > 0 || gDTMF_InputMode)
		{	// cancel key input mode (channel/frequency entry)
			gDTMF_InputMode       = false;
			gDTMF_InputBox_Index  = 0;
			memset(gDTMF_String, 0, sizeof(gDTMF_String));
			gInputBoxIndex        = 0;
			gRequestDisplayScreen = DISPLAY_MAIN;
			gBeepToPlay           = BEEP_1KHZ_60MS_OPTIONAL;
		}
	}
}

static void MAIN_Key_MENU(const bool bKeyPressed, const bool bKeyHeld)
{
	if (bKeyPressed && !bKeyHeld)
		// menu key pressed
		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

	if (bKeyHeld)
	{	// menu key held down (long press)

		if (bKeyPressed)
		{	// long press MENU key

			g_was_f_key_pressed = false;

			if (gScreenToDisplay == DISPLAY_MAIN)
			{
				if (gInputBoxIndex > 0)
				{	// delete any inputted chars
					gInputBoxIndex        = 0;
					gRequestDisplayScreen = DISPLAY_MAIN;
				}

				g_was_f_key_pressed = false;
				gUpdateStatus   = true;

				#ifdef ENABLE_COPY_CHAN_TO_VFO

					if (g_eeprom.vfo_open && gCssScanMode == CSS_SCAN_MODE_OFF)
					{

						if (gScanStateDir != SCAN_OFF)
						{
							if (gCurrentFunction != FUNCTION_INCOMING ||
							    gRxReceptionMode == RX_MODE_NONE      ||
								gScanPauseDelayIn_10ms == 0)
							{	// scan is running (not paused)
								return;
							}
						}
						
						const unsigned int vfo = get_rx_VFO();

						if (IS_USER_CHANNEL(g_eeprom.screen_channel[vfo]))
						{	// copy channel to VFO, then swap to the VFO
							
							const unsigned int channel = FREQ_CHANNEL_FIRST + g_eeprom.VfoInfo[vfo].band;

							g_eeprom.screen_channel[vfo] = channel;
							g_eeprom.VfoInfo[vfo].channel_save = channel;
							g_eeprom.tx_vfo = vfo;

							RADIO_SelectVfos();
							RADIO_ApplyOffset(gRxVfo);
							RADIO_ConfigureSquelchAndOutputPower(gRxVfo);
							RADIO_SetupRegisters(true);

							//SETTINGS_SaveChannel(channel, g_eeprom.rx_vfo, gRxVfo, 1);

							gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

							gUpdateStatus  = true;
							gUpdateDisplay = true;
						}
					}
					else
					{
						gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
					}

				#endif
			}
		}

		return;
	}

	if (!bKeyPressed && !gDTMF_InputMode)
	{	// menu key released
		const bool bFlag = (gInputBoxIndex == 0);
		gInputBoxIndex   = 0;

		if (bFlag)
		{
			gFlagRefreshSetting = true;
			gRequestDisplayScreen = DISPLAY_MENU;
			#ifdef ENABLE_VOICE
				g_another_voice_id   = VOICE_ID_MENU;
			#endif
		}
		else
		{
			gRequestDisplayScreen = DISPLAY_MAIN;
		}
	}
}

static void MAIN_Key_STAR(bool bKeyPressed, bool bKeyHeld)
{
	if (gCurrentFunction == FUNCTION_TRANSMIT)
		return;
	
	if (gInputBoxIndex > 0)
	{	// entering a frequency or DTMF string
		if (!bKeyHeld && bKeyPressed)
			gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	if (bKeyHeld && !g_was_f_key_pressed)
	{	// long press .. toggle scanning
		if (!bKeyPressed)
			return; // released

		ACTION_Scan(false);

		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
		return;
	}

	if (bKeyPressed)
	{	// just pressed
//		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
		gBeepToPlay = BEEP_880HZ_40MS_OPTIONAL;
		return;
	}
	
	// just released
	
	if (!g_was_f_key_pressed)
	{	// pressed without the F-key

		#ifdef ENABLE_NOAA
			if (gScanStateDir == SCAN_OFF && IS_NOT_NOAA_CHANNEL(gTxVfo->channel_save))
		#else
			if (gScanStateDir == SCAN_OFF)
		#endif
		{	// start entering a DTMF string

			memmove(gDTMF_InputBox, gDTMF_String, MIN(sizeof(gDTMF_InputBox), sizeof(gDTMF_String) - 1));
			gDTMF_InputBox_Index  = 0;
			gDTMF_InputMode       = true;

			gKeyInputCountdown    = key_input_timeout_500ms;

			gRequestDisplayScreen = DISPLAY_MAIN;
		}
	}
	else
	{	// with the F-key
		g_was_f_key_pressed = false;

		#ifdef ENABLE_NOAA
			if (IS_NOAA_CHANNEL(gTxVfo->channel_save))
			{
				gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
				return;
			}				
		#endif

		// scan the CTCSS/DCS code
		gFlagStartScan           = true;
		gScanSingleFrequency     = true;
		gBackup_cross_vfo_rx_tx = g_eeprom.cross_vfo_rx_tx;
		g_eeprom.cross_vfo_rx_tx = CROSS_BAND_OFF;
	}
	
	gPttWasReleased = true;

	gUpdateStatus   = true;
}

static void MAIN_Key_UP_DOWN(bool bKeyPressed, bool bKeyHeld, int8_t Direction)
{
	uint8_t Channel = g_eeprom.screen_channel[g_eeprom.tx_vfo];

	if (bKeyHeld || !bKeyPressed)
	{	// long press

		if (gInputBoxIndex > 0)
			return;

		if (!bKeyPressed)
		{
			if (!bKeyHeld)
				return;

			if (IS_FREQ_CHANNEL(Channel))
				return;

			#ifdef ENABLE_VOICE
				AUDIO_SetDigitVoice(0, gTxVfo->channel_save + 1);
				g_another_voice_id = (voice_id_t)0xFE;
			#endif

			return;
		}
	}
	else
	{
		if (gInputBoxIndex > 0)
		{
			gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			return;
		}

		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
	}

	if (gScanStateDir == SCAN_OFF)
	{
		#ifdef ENABLE_NOAA
			if (IS_NOT_NOAA_CHANNEL(Channel))
		#endif
		{
			uint8_t Next;

			if (IS_FREQ_CHANNEL(Channel))
			{	// step/down in frequency
				const uint32_t frequency = APP_SetFrequencyByStep(gTxVfo, Direction);

				if (RX_freq_check(frequency) < 0)
				{	// frequency not allowed
					gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
					return;
				}

				gTxVfo->freq_config_rx.frequency = frequency;

				gRequestSaveChannel = 1;
				return;
			}

			Next = RADIO_FindNextChannel(Channel + Direction, Direction, false, 0);
			if (Next == 0xFF)
				return;

			if (Channel == Next)
				return;

			g_eeprom.user_channel[g_eeprom.tx_vfo]     = Next;
			g_eeprom.screen_channel[g_eeprom.tx_vfo] = Next;

			if (!bKeyHeld)
			{
				#ifdef ENABLE_VOICE
					AUDIO_SetDigitVoice(0, Next + 1);
					g_another_voice_id = (voice_id_t)0xFE;
				#endif
			}
		}
		#ifdef ENABLE_NOAA
			else
			{
				Channel = NOAA_CHANNEL_FIRST + NUMBER_AddWithWraparound(g_eeprom.screen_channel[g_eeprom.tx_vfo] - NOAA_CHANNEL_FIRST, Direction, 0, 9);
				g_eeprom.noaa_channel[g_eeprom.tx_vfo]   = Channel;
				g_eeprom.screen_channel[g_eeprom.tx_vfo] = Channel;
			}
		#endif

		gRequestSaveVFO   = true;
		gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
		return;
	}

	// jump to the next channel
	CHANNEL_Next(false, Direction);
	gScanPauseDelayIn_10ms = 1;
	gScheduleScanListen    = false;

	gPttWasReleased = true;
}

void MAIN_ProcessKeys(key_code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	#ifdef ENABLE_FMRADIO
		if (gFmRadioMode && Key != KEY_PTT && Key != KEY_EXIT)
		{
			if (!bKeyHeld && bKeyPressed)
				gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			return;
		}
	#endif

	if (gDTMF_InputMode && bKeyPressed && !bKeyHeld)
	{
		const char Character = DTMF_GetCharacter(Key);
		if (Character != 0xFF)
		{	// add key to DTMF string
			DTMF_Append(Character);
			gKeyInputCountdown    = key_input_timeout_500ms;
			gRequestDisplayScreen = DISPLAY_MAIN;
			gPttWasReleased       = true;
			gBeepToPlay           = BEEP_1KHZ_60MS_OPTIONAL;
			return;
		}
	}

	// TODO: ???
//	if (Key > KEY_PTT)
//	{
//		Key = KEY_SIDE2;      // what's this doing ???
//	}

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
			MAIN_Key_DIGITS(Key, bKeyPressed, bKeyHeld);
			break;
		case KEY_MENU:
			MAIN_Key_MENU(bKeyPressed, bKeyHeld);
			break;
		case KEY_UP:
			MAIN_Key_UP_DOWN(bKeyPressed, bKeyHeld, 1);
			break;
		case KEY_DOWN:
			MAIN_Key_UP_DOWN(bKeyPressed, bKeyHeld, -1);
			break;
		case KEY_EXIT:
			MAIN_Key_EXIT(bKeyPressed, bKeyHeld);
			break;
		case KEY_STAR:
			MAIN_Key_STAR(bKeyPressed, bKeyHeld);
			break;
		case KEY_F:
			GENERIC_Key_F(bKeyPressed, bKeyHeld);
			break;
		case KEY_PTT:
			GENERIC_Key_PTT(bKeyPressed);
			break;
		default:
			if (!bKeyHeld && bKeyPressed)
				gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			break;
	}
}
