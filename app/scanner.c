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

#include "app/dtmf.h"
#include "app/generic.h"
#include "app/scanner.h"
#include "audio.h"
#include "driver/bk4819.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

dcs_code_type_t    gScanCssResultType;
uint8_t           gScanCssResultCode;
bool              gFlagStartScan;
bool              gFlagStopScan;
bool              gScanSingleFrequency;
SCAN_edit_state_t gScannerEditState;
uint8_t           gScanChannel;
uint32_t          gScanFrequency;
bool              gScanPauseMode;
SCAN_CssState_t   gScanCssState;
volatile bool     gScheduleScanListen = true;
volatile uint16_t gScanPauseDelayIn_10ms;
uint8_t           gScanProgressIndicator;
uint8_t           gScanHitCount;
bool              gScanUseCssResult;
int8_t            gScanStateDir;
bool              bScanKeepFrequency;

static void SCANNER_Key_DIGITS(key_code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	if (!bKeyHeld && bKeyPressed)
	{
		if (gScannerEditState == SCAN_EDIT_STATE_BUSY)
		{
			uint16_t Channel;

			gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

			INPUTBOX_Append(Key);

			gRequestDisplayScreen = DISPLAY_SCANNER;

			if (gInputBoxIndex < 3)
			{
				#ifdef ENABLE_VOICE
					g_another_voice_id = (voice_id_t)Key;
				#endif
				return;
			}

			gInputBoxIndex = 0;

			Channel = ((gInputBox[0] * 100) + (gInputBox[1] * 10) + gInputBox[2]) - 1;
			if (Channel <= USER_CHANNEL_LAST)
			{
				#ifdef ENABLE_VOICE
					g_another_voice_id = (voice_id_t)Key;
				#endif
				gShowChPrefix = RADIO_CheckValidChannel(Channel, false, 0);
				gScanChannel  = (uint8_t)Channel;
				return;
			}
		}

		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
	}
}

static void SCANNER_Key_EXIT(bool bKeyPressed, bool bKeyHeld)
{
	if (!bKeyHeld && bKeyPressed)
	{
		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

		switch (gScannerEditState)
		{
			case SCAN_EDIT_STATE_NONE:
				gRequestDisplayScreen    = DISPLAY_MAIN;

				g_eeprom.cross_vfo_rx_tx = gBackup_cross_vfo_rx_tx;
				gUpdateStatus            = true;
				gFlagStopScan            = true;
				gVfoConfigureMode        = VFO_CONFIGURE_RELOAD;
				gFlagResetVfos           = true;
				#ifdef ENABLE_VOICE
					g_another_voice_id      = VOICE_ID_CANCEL;
				#endif
				break;

			case SCAN_EDIT_STATE_BUSY:
				if (gInputBoxIndex > 0)
				{
					gInputBox[--gInputBoxIndex] = 10;
					gRequestDisplayScreen       = DISPLAY_SCANNER;
					break;
				}

				// Fallthrough

			case SCAN_EDIT_STATE_DONE:
				gScannerEditState     = SCAN_EDIT_STATE_NONE;
				#ifdef ENABLE_VOICE
					g_another_voice_id   = VOICE_ID_CANCEL;
				#endif
				gRequestDisplayScreen = DISPLAY_SCANNER;
				break;
		}
	}
}

static void SCANNER_Key_MENU(bool bKeyPressed, bool bKeyHeld)
{
	uint8_t Channel;

	if (bKeyHeld)
		return;

	if (!bKeyPressed)
		return;

	if (gScanCssState == SCAN_CSS_STATE_OFF && !gScanSingleFrequency)
	{
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	if (gScanCssState == SCAN_CSS_STATE_SCANNING)
	{
		if (gScanSingleFrequency)
		{
			gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			return;
		}
	}

	if (gScanCssState == SCAN_CSS_STATE_FAILED)
	{
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

	switch (gScannerEditState)
	{
		case SCAN_EDIT_STATE_NONE:
			if (!gScanSingleFrequency)
			{

				#if 0
					uint32_t Freq250 = FREQUENCY_FloorToStep(gScanFrequency, 250, 0);
					uint32_t Freq625 = FREQUENCY_FloorToStep(gScanFrequency, 625, 0);

					int16_t Delta250 = (int16_t)gScanFrequency - (int16_t)Freq250;
					int16_t Delta625;

					if (125 < Delta250)
					{
						Delta250 = 250 - Delta250;
						Freq250 += 250;
					}

					Delta625 = (int16_t)gScanFrequency - (int16_t)Freq625;

					if (312 < Delta625)
					{
						Delta625 = 625 - Delta625;
						Freq625 += 625;
					}

					if (Delta625 < Delta250)
					{
						gStepSetting = STEP_6_25kHz;
						gScanFrequency = Freq625;
					}
					else
					{
						gStepSetting = STEP_2_5kHz;
						gScanFrequency = Freq250;
					}
				#else

					#ifdef ENABLE_1250HZ_STEP
						const step_setting_t small_step = STEP_1_25kHz;
						const step_setting_t big_step   = STEP_6_25kHz;
					#else
						const step_setting_t small_step = STEP_2_5kHz;
						const step_setting_t big_step   = STEP_6_25kHz;
					#endif

					const uint32_t small_step_freq = StepFrequencyTable[small_step];
					const uint32_t big_step_freq   = StepFrequencyTable[big_step];

					uint32_t freq_small_step = FREQUENCY_FloorToStep(gScanFrequency, small_step_freq, 0);
					uint32_t freq_big_step   = FREQUENCY_FloorToStep(gScanFrequency, big_step_freq,   0);

					int32_t delta_small_step = (int32_t)gScanFrequency - freq_small_step;
					int32_t delta_big_step   = (int32_t)gScanFrequency - freq_big_step;

					if (delta_small_step > 125)
					{
						delta_small_step = StepFrequencyTable[small_step] - delta_small_step;
						freq_big_step += small_step_freq;
					}

					delta_big_step = (int32_t)gScanFrequency - freq_big_step;

					if (delta_big_step > 312)
					{
						delta_big_step = big_step_freq - delta_big_step;
						freq_big_step += big_step_freq;
					}

					if (delta_small_step >= delta_big_step)
					{
						gStepSetting   = small_step;
						gScanFrequency = freq_small_step;
					}
					else
					{
						gStepSetting   = big_step;
						gScanFrequency = freq_big_step;
					}
				#endif
			}

			if (gTxVfo->channel_save <= USER_CHANNEL_LAST)
			{
				gScannerEditState = SCAN_EDIT_STATE_BUSY;
				gScanChannel      = gTxVfo->channel_save;
				gShowChPrefix     = RADIO_CheckValidChannel(gTxVfo->channel_save, false, 0);
			}
			else
			{
				gScannerEditState = SCAN_EDIT_STATE_DONE;
			}

			gScanCssState = SCAN_CSS_STATE_FOUND;

			#ifdef ENABLE_VOICE
				g_another_voice_id = VOICE_ID_MEMORY_CHANNEL;
			#endif

			gRequestDisplayScreen = DISPLAY_SCANNER;
			gUpdateStatus = true;
			break;

		case SCAN_EDIT_STATE_BUSY:
			if (gInputBoxIndex == 0)
			{
				gBeepToPlay           = BEEP_1KHZ_60MS_OPTIONAL;
				gRequestDisplayScreen = DISPLAY_SCANNER;
				gScannerEditState     = SCAN_EDIT_STATE_DONE;
			}
			break;

		case SCAN_EDIT_STATE_DONE:
			if (!gScanSingleFrequency)
			{
				RADIO_InitInfo(gTxVfo, gTxVfo->channel_save, gScanFrequency);

				if (gScanUseCssResult)
				{
					gTxVfo->freq_config_rx.code_type = gScanCssResultType;
					gTxVfo->freq_config_rx.code     = gScanCssResultCode;
				}

				gTxVfo->freq_config_tx = gTxVfo->freq_config_rx;
				gTxVfo->step_setting   = gStepSetting;
			}
			else
			{
				RADIO_ConfigureChannel(0, VFO_CONFIGURE_RELOAD);
				RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);

				gTxVfo->freq_config_rx.code_type = gScanCssResultType;
				gTxVfo->freq_config_rx.code     = gScanCssResultCode;
				gTxVfo->freq_config_tx.code_type = gScanCssResultType;
				gTxVfo->freq_config_tx.code     = gScanCssResultCode;
			}

			if (gTxVfo->channel_save <= USER_CHANNEL_LAST)
			{
				Channel = gScanChannel;
				g_eeprom.user_channel[g_eeprom.tx_vfo] = Channel;
			}
			else
			{
				Channel = gTxVfo->band + FREQ_CHANNEL_FIRST;
				g_eeprom.freq_channel[g_eeprom.tx_vfo] = Channel;
			}

			gTxVfo->channel_save                  = Channel;
			g_eeprom.screen_channel[g_eeprom.tx_vfo] = Channel;
			gRequestSaveChannel                   = 2;

			#ifdef ENABLE_VOICE
				g_another_voice_id = VOICE_ID_CONFIRM;
			#endif

			gScannerEditState = SCAN_EDIT_STATE_NONE;

			gRequestDisplayScreen = DISPLAY_SCANNER;
			break;

		default:
			gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
			break;
	}
}

static void SCANNER_Key_STAR(bool bKeyPressed, bool bKeyHeld)
{
	if (!bKeyHeld && bKeyPressed)
	{
		gBeepToPlay    = BEEP_1KHZ_60MS_OPTIONAL;
		gFlagStartScan = true;
	}
	return;
}

static void SCANNER_Key_UP_DOWN(bool bKeyPressed, bool pKeyHeld, int8_t Direction)
{
	if (pKeyHeld)
	{
		if (!bKeyPressed)
			return;
	}
	else
	{
		if (!bKeyPressed)
			return;

		gInputBoxIndex = 0;
		gBeepToPlay    = BEEP_1KHZ_60MS_OPTIONAL;
	}

	if (gScannerEditState == SCAN_EDIT_STATE_BUSY)
	{
		gScanChannel          = NUMBER_AddWithWraparound(gScanChannel, Direction, 0, USER_CHANNEL_LAST);
		gShowChPrefix         = RADIO_CheckValidChannel(gScanChannel, false, 0);
		gRequestDisplayScreen = DISPLAY_SCANNER;
	}
	else
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}

void SCANNER_ProcessKeys(key_code_t Key, bool bKeyPressed, bool bKeyHeld)
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
			SCANNER_Key_DIGITS(Key, bKeyPressed, bKeyHeld);
			break;
		case KEY_MENU:
			SCANNER_Key_MENU(bKeyPressed, bKeyHeld);
			break;
		case KEY_UP:
			SCANNER_Key_UP_DOWN(bKeyPressed, bKeyHeld,  1);
			break;
		case KEY_DOWN:
			SCANNER_Key_UP_DOWN(bKeyPressed, bKeyHeld, -1);
			break;
		case KEY_EXIT:
			SCANNER_Key_EXIT(bKeyPressed, bKeyHeld);
			break;
		case KEY_STAR:
			SCANNER_Key_STAR(bKeyPressed, bKeyHeld);
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

void SCANNER_Start(void)
{
	uint8_t  BackupStep;
	uint16_t BackupStepFreq;

	BK4819_StopScan();

	RADIO_SelectVfos();

	#ifdef ENABLE_NOAA
		if (IS_NOAA_CHANNEL(gRxVfo->channel_save))
			gRxVfo->channel_save = FREQ_CHANNEL_FIRST + BAND6_400MHz;
	#endif

	BackupStep     = gRxVfo->step_setting;
	BackupStepFreq = gRxVfo->step_freq;

	RADIO_InitInfo(gRxVfo, gRxVfo->channel_save, gRxVfo->pRX->frequency);

	gRxVfo->step_setting  = BackupStep;
	gRxVfo->step_freq = BackupStepFreq;

	RADIO_SetupRegisters(true);

	#ifdef ENABLE_NOAA
		gIsNoaaMode = false;
	#endif

	if (gScanSingleFrequency)
	{
		gScanCssState  = SCAN_CSS_STATE_SCANNING;
		gScanFrequency = gRxVfo->pRX->frequency;
		gStepSetting   = gRxVfo->step_setting;

		BK4819_PickRXFilterPathBasedOnFrequency(gScanFrequency);
		BK4819_SetScanFrequency(gScanFrequency);
	}
	else
	{
		gScanCssState  = SCAN_CSS_STATE_OFF;
		gScanFrequency = 0xFFFFFFFF;

		BK4819_PickRXFilterPathBasedOnFrequency(0xFFFFFFFF);
		BK4819_EnableFrequencyScan();
	}

	DTMF_clear_RX();

	gScanDelay_10ms        = scan_freq_css_delay_10ms;
	gScanCssResultCode     = 0xFF;
	gScanCssResultType     = 0xFF;
	gScanHitCount          = 0;
	gScanUseCssResult      = false;
	g_CxCSS_tailL_found     = false;
	g_CDCSS_lost           = false;
	g_CDCSS_code_type         = 0;
	g_CTCSS_lost           = false;
	#ifdef ENABLE_VOX
		g_vox_lost         = false;
	#endif
	g_SquelchLost          = false;
	gScannerEditState      = SCAN_EDIT_STATE_NONE;
	gScanProgressIndicator = 0;
//	gFlagStartScan         = false;

	gUpdateStatus = true;
}

void SCANNER_Stop(void)
{
	const uint8_t Previous = gRestoreUSER_CHANNEL;

	if (gScanStateDir == SCAN_OFF)
		return;   // but, but, we weren't !
	
	gScanStateDir = SCAN_OFF;

	if (!bScanKeepFrequency)
	{
		if (gNextChannel <= USER_CHANNEL_LAST)
		{
			g_eeprom.user_channel[g_eeprom.rx_vfo]     = gRestoreUSER_CHANNEL;
			g_eeprom.screen_channel[g_eeprom.rx_vfo] = Previous;

			RADIO_ConfigureChannel(g_eeprom.rx_vfo, VFO_CONFIGURE_RELOAD);
		}
		else
		{
			gRxVfo->freq_config_rx.frequency = gRestoreFrequency;
			RADIO_ApplyOffset(gRxVfo);
			RADIO_ConfigureSquelchAndOutputPower(gRxVfo);
		}
		RADIO_SetupRegisters(true);
		gUpdateDisplay = true;
		return;
	}

	if (gRxVfo->channel_save > USER_CHANNEL_LAST)
	{
		RADIO_ApplyOffset(gRxVfo);
		RADIO_ConfigureSquelchAndOutputPower(gRxVfo);
		SETTINGS_SaveChannel(gRxVfo->channel_save, g_eeprom.rx_vfo, gRxVfo, 1);
		return;
	}

	SETTINGS_SaveVfoIndices();

	gUpdateStatus = true;
}
