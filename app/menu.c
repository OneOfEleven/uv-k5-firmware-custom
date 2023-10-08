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

#if !defined(ENABLE_OVERLAY)
	#include "ARMCM0.h"
#endif
#include "app/dtmf.h"
#include "app/generic.h"
#include "app/menu.h"
#include "app/scanner.h"
#include "audio.h"
#include "board.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/keyboard.h"
#include "frequencies.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#if defined(ENABLE_OVERLAY)
	#include "sram-overlay.h"
#endif
#include "ui/inputbox.h"
#include "ui/menu.h"
#include "ui/menu.h"
#include "ui/ui.h"

#ifndef ARRAY_SIZE
	#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

#ifdef ENABLE_F_CAL_MENU
	void writeXtalFreqCal(const int32_t value, const bool update_eeprom)
	{
		BK4819_WriteRegister(BK4819_REG_3B, 22656 + value);

		if (update_eeprom)
		{
			struct
			{
				int16_t  BK4819_XtalFreqLow;
				uint16_t EEPROM_1F8A;
				uint16_t EEPROM_1F8C;
				uint8_t  volume_gain;
				uint8_t  dac_gain;
			} __attribute__((packed)) misc;

			g_eeprom.BK4819_xtal_freq_low = value;

			// radio 1 .. 04 00 46 00 50 00 2C 0E
			// radio 2 .. 05 00 46 00 50 00 2C 0E
			//
			EEPROM_ReadBuffer(0x1F88, &misc, 8);
			misc.BK4819_XtalFreqLow = value;
			EEPROM_WriteBuffer(0x1F88, &misc);
		}
	}
#endif

void MENU_StartCssScan(int8_t Direction)
{
	gCssScanMode  = CSS_SCAN_MODE_SCANNING;
	gUpdateStatus = true;

	gMenuScrollDirection = Direction;

	RADIO_SelectVfos();

	MENU_SelectNextCode();

	gScanPauseDelayIn_10ms = scan_pause_delay_in_2_10ms;
	gScheduleScanListen    = false;
}

void MENU_StopCssScan(void)
{
	gCssScanMode  = CSS_SCAN_MODE_OFF;
	gUpdateStatus = true;

	RADIO_SetupRegisters(true);
}

int MENU_GetLimits(uint8_t Cursor, int32_t *pMin, int32_t *pMax)
{
	switch (Cursor)
	{
		case MENU_SQL:
			*pMin = 0;
			*pMax = 9;
			break;

		case MENU_STEP:
			*pMin = 0;
			*pMax = ARRAY_SIZE(StepFrequencyTable) - 1;
			break;

		case MENU_ABR:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_backlight) - 1;
			break;

		case MENU_F_LOCK:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_F_LOCK) - 1;
			break;

		case MENU_MDF:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_MDF) - 1;
			break;

		case MENU_TXP:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_TXP) - 1;
			break;

		case MENU_SFT_D:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_SFT_D) - 1;
			break;

		case MENU_TDR:
			*pMin = 0;
//			*pMax = ARRAY_SIZE(gSubMenu_TDR) - 1;
			*pMax = ARRAY_SIZE(gSubMenu_OFF_ON) - 1;
			break;

		case MENU_XB:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_XB) - 1;
			break;

		#ifdef ENABLE_VOICE
			case MENU_VOICE:
				*pMin = 0;
				*pMax = ARRAY_SIZE(gSubMenu_VOICE) - 1;
				break;
		#endif

		case MENU_SC_REV:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_SC_REV) - 1;
			break;

		case MENU_ROGER:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_ROGER) - 1;
			break;

		case MENU_PONMSG:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_PONMSG) - 1;
			break;

		case MENU_R_DCS:
		case MENU_T_DCS:
			*pMin = 0;
			*pMax = 208;
			//*pMax = (ARRAY_SIZE(DCS_Options) * 2);
			break;

		case MENU_R_CTCS:
		case MENU_T_CTCS:
			*pMin = 0;
			*pMax = ARRAY_SIZE(CTCSS_Options) - 1;
			break;

		case MENU_W_N:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_W_N) - 1;
			break;

		#ifdef ENABLE_ALARM
			case MENU_AL_MOD:
				*pMin = 0;
				*pMax = ARRAY_SIZE(gSubMenu_AL_MOD) - 1;
				break;
		#endif

		case MENU_SIDE1_SHORT:
		case MENU_SIDE1_LONG:
		case MENU_SIDE2_SHORT:
		case MENU_SIDE2_LONG:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_SIDE_BUTT) - 1;
			break;

		case MENU_RESET:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_RESET) - 1;
			break;

		case MENU_COMPAND:
		case MENU_ABR_ON_TX_RX:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_RX_TX) - 1;
			break;

		#ifdef ENABLE_AM_FIX_TEST1
			case MENU_AM_FIX_TEST1:
				*pMin = 0;
				*pMax = ARRAY_SIZE(gSubMenu_AM_fix_test1) - 1;
				break;
		#endif

		#ifdef ENABLE_AM_FIX
			case MENU_AM_FIX:
		#endif
		#ifdef ENABLE_AUDIO_BAR
			case MENU_MIC_BAR:
		#endif
		case MENU_BCL:
		case MENU_BEEP:
		case MENU_AUTOLK:
		case MENU_S_ADD1:
		case MENU_S_ADD2:
		case MENU_STE:
		case MENU_D_ST:
		case MENU_D_DCD:
		case MENU_D_LIVE_DEC:
		case MENU_AM:
		#ifdef ENABLE_NOAA
			case MENU_NOAA_S:
		#endif
		case MENU_350TX:
		case MENU_200TX:
		case MENU_500TX:
		case MENU_350EN:
		case MENU_SCREN:
		case MENU_TX_EN:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_OFF_ON) - 1;
			break;

		case MENU_SCR:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_SCRAMBLER) - 1;
			break;

		case MENU_TOT:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_TOT) - 1;
			break;

		#ifdef ENABLE_VOX
			case MENU_VOX:
		#endif
		case MENU_RP_STE:
			*pMin = 0;
			*pMax = 10;
			break;

		case MENU_MEM_CH:
		case MENU_1_CALL:
		case MENU_DEL_CH:
		case MENU_MEM_NAME:
			*pMin = 0;
			*pMax = USER_CHANNEL_LAST;
			break;

		case MENU_SLIST1:
		case MENU_SLIST2:
			*pMin = -1;
			*pMax = USER_CHANNEL_LAST;
			break;

		case MENU_SAVE:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_SAVE) - 1;
			break;

		case MENU_MIC:
			*pMin = 0;
			*pMax = 4;
			break;

		case MENU_S_LIST:
			*pMin = 0;
//			*pMax = 1;
			*pMax = 2;
			break;

		case MENU_D_RSP:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_D_RSP) - 1;
			break;

		case MENU_PTT_ID:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_PTT_ID) - 1;
			break;

		case MENU_BAT_TXT:
			*pMin = 0;
			*pMax = ARRAY_SIZE(gSubMenu_BAT_TXT) - 1;
			break;

		case MENU_D_HOLD:
			*pMin = DTMF_HOLD_MIN;
			*pMax = DTMF_HOLD_MAX;
			break;

		case MENU_D_PRE:
			*pMin = 3;
			*pMax = 99;
			break;

		case MENU_D_LIST:
			*pMin = 1;
			*pMax = 16;
			break;

		#ifdef ENABLE_F_CAL_MENU
			case MENU_F_CALI:
				*pMin = -50;
				*pMax = +50;
				break;
		#endif

		case MENU_BATCAL:
			*pMin = 1600;  // 0
			*pMax = 2200;  // 2300
			break;

		default:
			return -1;
	}

	return 0;
}

void MENU_AcceptSetting(void)
{
	int32_t        Min;
	int32_t        Max;
	uint8_t        Code;
	FREQ_Config_t *pConfig = &gTxVfo->freq_config_rx;

	if (!MENU_GetLimits(gMenuCursor, &Min, &Max))
	{
		if (gSubMenuSelection < Min) gSubMenuSelection = Min;
		else
		if (gSubMenuSelection > Max) gSubMenuSelection = Max;
	}

	switch (gMenuCursor)
	{
		default:
			return;

		case MENU_SQL:
			g_eeprom.squelch_level = gSubMenuSelection;
			gVfoConfigureMode     = VFO_CONFIGURE;
			break;

		case MENU_STEP:
			gTxVfo->step_setting = gSubMenuSelection;
			if (IS_FREQ_CHANNEL(gTxVfo->channel_save))
			{
				gRequestSaveChannel = 1;
				return;
			}
			return;

		case MENU_TXP:
			gTxVfo->output_power = gSubMenuSelection;
			gRequestSaveChannel = 1;
			return;

		case MENU_T_DCS:
			pConfig = &gTxVfo->freq_config_tx;

			// Fallthrough

		case MENU_R_DCS:
			if (gSubMenuSelection == 0)
			{
				if (pConfig->code_type != CODE_TYPE_DIGITAL && pConfig->code_type != CODE_TYPE_REVERSE_DIGITAL)
				{
					gRequestSaveChannel = 1;
					return;
				}
				Code               = 0;
				pConfig->code_type = CODE_TYPE_OFF;
			}
			else
			if (gSubMenuSelection < 105)
			{
				pConfig->code_type = CODE_TYPE_DIGITAL;
				Code               = gSubMenuSelection - 1;
			}
			else
			{
				pConfig->code_type = CODE_TYPE_REVERSE_DIGITAL;
				Code               = gSubMenuSelection - 105;
			}

			pConfig->code       = Code;
			gRequestSaveChannel = 1;
			return;

		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

		case MENU_T_CTCS:
			pConfig = &gTxVfo->freq_config_tx;
		case MENU_R_CTCS:
			if (gSubMenuSelection == 0)
			{
				if (pConfig->code_type != CODE_TYPE_CONTINUOUS_TONE)
				{
					gRequestSaveChannel = 1;
					return;
				}
				Code              = 0;
				pConfig->code     = Code;
				pConfig->code_type = CODE_TYPE_OFF;

				BK4819_ExitSubAu();
			}
			else
			{
				pConfig->code_type = CODE_TYPE_CONTINUOUS_TONE;
				Code              = gSubMenuSelection - 1;
				pConfig->code     = Code;

				BK4819_SetCTCSSFrequency(CTCSS_Options[Code]);
			}

			gRequestSaveChannel = 1;
			return;

		#pragma GCC diagnostic pop

		case MENU_SFT_D:
			gTxVfo->tx_offset_freq_dir = gSubMenuSelection;
			gRequestSaveChannel                   = 1;
			return;

		case MENU_OFFSET:
			gTxVfo->tx_offset_freq = gSubMenuSelection;
			gRequestSaveChannel         = 1;
			return;

		case MENU_W_N:
			gTxVfo->channel_bandwidth = gSubMenuSelection;
			gRequestSaveChannel       = 1;
			return;

		case MENU_SCR:
			gTxVfo->scrambling_type = gSubMenuSelection;
			#if 0
				if (gSubMenuSelection > 0 && gSetting_ScrambleEnable)
					BK4819_EnableScramble(gSubMenuSelection - 1);
				else
					BK4819_DisableScramble();
			#endif
			gRequestSaveChannel     = 1;
			return;

		case MENU_BCL:
			gTxVfo->busy_channel_lock = gSubMenuSelection;
			gRequestSaveChannel       = 1;
			return;

		case MENU_MEM_CH:
			gTxVfo->channel_save = gSubMenuSelection;
			#if 0
				g_eeprom.user_channel[0] = gSubMenuSelection;
			#else
				g_eeprom.user_channel[g_eeprom.tx_vfo] = gSubMenuSelection;
			#endif
			gRequestSaveChannel = 2;
			gVfoConfigureMode   = VFO_CONFIGURE_RELOAD;
			gFlagResetVfos      = true;
			return;

		case MENU_MEM_NAME:
			{	// trailing trim
				for (int i = 9; i >= 0; i--)
				{
					if (edit[i] != ' ' && edit[i] != '_' && edit[i] != 0x00 && edit[i] != 0xff)
						break;
					edit[i] = ' ';
				}
			}

			// save the channel name
			memset(gTxVfo->name, 0, sizeof(gTxVfo->name));
			memmove(gTxVfo->name, edit, 10);
			SETTINGS_SaveChannel(gSubMenuSelection, g_eeprom.tx_vfo, gTxVfo, 3);
			gFlagReconfigureVfos = true;
			return;

		case MENU_SAVE:
			g_eeprom.battery_save = gSubMenuSelection;
			break;

		#ifdef ENABLE_VOX
			case MENU_VOX:
				g_eeprom.vox_switch = gSubMenuSelection != 0;
				if (g_eeprom.vox_switch)
					g_eeprom.vox_level = gSubMenuSelection - 1;
				BOARD_EEPROM_LoadMoreSettings();
				gFlagReconfigureVfos = true;
				gUpdateStatus        = true;
				break;
		#endif

		case MENU_ABR:
			g_eeprom.backlight = gSubMenuSelection;
			break;

		case MENU_ABR_ON_TX_RX:
			gSetting_backlight_on_tx_rx = gSubMenuSelection;
			break;

		case MENU_TDR:
//			g_eeprom.dual_watch   = gSubMenuSelection;
			g_eeprom.dual_watch   = (gSubMenuSelection > 0) ? 1 + g_eeprom.tx_vfo : DUAL_WATCH_OFF;

			gFlagReconfigureVfos = true;
			gUpdateStatus        = true;
			break;

		case MENU_XB:
			#ifdef ENABLE_NOAA
				if (IS_NOAA_CHANNEL(g_eeprom.screen_channel[0]))
					return;
				if (IS_NOAA_CHANNEL(g_eeprom.screen_channel[1]))
					return;
			#endif

			g_eeprom.cross_vfo_rx_tx = gSubMenuSelection;
			gFlagReconfigureVfos     = true;
			gUpdateStatus            = true;
			break;

		case MENU_BEEP:
			g_eeprom.beep_control = gSubMenuSelection;
			break;

		case MENU_TOT:
			g_eeprom.tx_timeout_timer = gSubMenuSelection;
			break;

		#ifdef ENABLE_VOICE
			case MENU_VOICE:
				g_eeprom.voice_prompt = gSubMenuSelection;
				gUpdateStatus        = true;
				break;
		#endif

		case MENU_SC_REV:
			g_eeprom.scan_resume_mode = gSubMenuSelection;
			break;

		case MENU_MDF:
			g_eeprom.channel_display_mode = gSubMenuSelection;
			break;

		case MENU_AUTOLK:
			g_eeprom.auto_keypad_lock = gSubMenuSelection;
			gKeyLockCountdown        = 30;
			break;

		case MENU_S_ADD1:
			gTxVfo->scanlist_1_participation = gSubMenuSelection;
			SETTINGS_UpdateChannel(gTxVfo->channel_save, gTxVfo, true);
			gVfoConfigureMode = VFO_CONFIGURE;
			gFlagResetVfos    = true;
			return;

		case MENU_S_ADD2:
			gTxVfo->scanlist_2_participation = gSubMenuSelection;
			SETTINGS_UpdateChannel(gTxVfo->channel_save, gTxVfo, true);
			gVfoConfigureMode = VFO_CONFIGURE;
			gFlagResetVfos    = true;
			return;

		case MENU_STE:
			g_eeprom.tail_note_elimination = gSubMenuSelection;
			break;

		case MENU_RP_STE:
			g_eeprom.repeater_tail_tone_elimination = gSubMenuSelection;
			break;

		case MENU_MIC:
			g_eeprom.mic_sensitivity = gSubMenuSelection;
			BOARD_EEPROM_LoadMoreSettings();
			gFlagReconfigureVfos = true;
			break;

		#ifdef ENABLE_AUDIO_BAR
			case MENU_MIC_BAR:
				gSetting_mic_bar = gSubMenuSelection;
				break;
		#endif

		case MENU_COMPAND:
			gTxVfo->compander = gSubMenuSelection;
			SETTINGS_UpdateChannel(gTxVfo->channel_save, gTxVfo, true);
			gVfoConfigureMode = VFO_CONFIGURE;
			gFlagResetVfos    = true;
//			gRequestSaveChannel = 1;
			return;

		case MENU_1_CALL:
			g_eeprom.chan_1_call = gSubMenuSelection;
			break;

		case MENU_S_LIST:
			g_eeprom.scan_list_default = gSubMenuSelection;
			break;

		#ifdef ENABLE_ALARM
			case MENU_AL_MOD:
				g_eeprom.alarm_mode = gSubMenuSelection;
				break;
		#endif

		case MENU_D_ST:
			g_eeprom.DTMF_side_tone = gSubMenuSelection;
			break;

		case MENU_D_RSP:
			g_eeprom.DTMF_decode_response = gSubMenuSelection;
			break;

		case MENU_D_HOLD:
			g_eeprom.DTMF_auto_reset_time = gSubMenuSelection;
			break;

		case MENU_D_PRE:
			g_eeprom.DTMF_preload_time = gSubMenuSelection * 10;
			break;

		case MENU_PTT_ID:
			gTxVfo->DTMF_ptt_id_tx_mode = gSubMenuSelection;
			gRequestSaveChannel         = 1;
			return;

		case MENU_BAT_TXT:
			gSetting_battery_text = gSubMenuSelection;
			break;

		case MENU_D_DCD:
			gTxVfo->DTMF_decoding_enable = gSubMenuSelection;
			DTMF_clear_RX();
			gRequestSaveChannel = 1;
			return;

		case MENU_D_LIVE_DEC:
			gSetting_live_DTMF_decoder = gSubMenuSelection;
			gDTMF_RX_live_timeout = 0;
			memset(gDTMF_RX_live, 0, sizeof(gDTMF_RX_live));
			if (!gSetting_live_DTMF_decoder)
				BK4819_DisableDTMF();
			gFlagReconfigureVfos     = true;
			gUpdateStatus            = true;
			break;

		case MENU_D_LIST:
			gDTMF_chosen_contact = gSubMenuSelection - 1;
			if (gIsDtmfContactValid)
			{
				GUI_SelectNextDisplay(DISPLAY_MAIN);
				gDTMF_InputMode       = true;
				gDTMF_InputBox_Index  = 3;
				memmove(gDTMF_InputBox, gDTMF_ID, 4);
				gRequestDisplayScreen = DISPLAY_INVALID;
			}
			return;

		case MENU_PONMSG:
			g_eeprom.pwr_on_display_mode = gSubMenuSelection;
			break;

		case MENU_ROGER:
			g_eeprom.roger_mode = gSubMenuSelection;
			break;

		case MENU_AM:
			gTxVfo->am_mode     = gSubMenuSelection;
			gRequestSaveChannel = 1;
			return;

		#ifdef ENABLE_AM_FIX
			case MENU_AM_FIX:
				gSetting_AM_fix = gSubMenuSelection;
				gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
				gFlagResetVfos    = true;
				break;
		#endif

		#ifdef ENABLE_AM_FIX_TEST1
			case MENU_AM_FIX_TEST1:
				gSetting_AM_fix_test1 = gSubMenuSelection;
				gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
				gFlagResetVfos    = true;
				break;
		#endif

		#ifdef ENABLE_NOAA
			case MENU_NOAA_S:
				g_eeprom.NOAA_auto_scan = gSubMenuSelection;
				gFlagReconfigureVfos   = true;
				break;
		#endif

		case MENU_DEL_CH:
			SETTINGS_UpdateChannel(gSubMenuSelection, NULL, false);
			gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
			gFlagResetVfos    = true;
			return;

		case MENU_SIDE1_SHORT:
			g_eeprom.key1_short_press_action = gSubMenuSelection;
			break;

		case MENU_SIDE1_LONG:
			g_eeprom.key1_long_press_action = gSubMenuSelection;
			break;

		case MENU_SIDE2_SHORT:
			g_eeprom.key2_short_press_action = gSubMenuSelection;
			break;

		case MENU_SIDE2_LONG:
			g_eeprom.key2_long_press_action = gSubMenuSelection;
			break;

		case MENU_RESET:
			BOARD_FactoryReset(gSubMenuSelection);
			return;

		case MENU_350TX:
			gSetting_350TX = gSubMenuSelection;
			break;

		case MENU_F_LOCK:
			gSetting_F_LOCK = gSubMenuSelection;
			break;

		case MENU_200TX:
			gSetting_200TX = gSubMenuSelection;
			break;

		case MENU_500TX:
			gSetting_500TX = gSubMenuSelection;
			break;

		case MENU_350EN:
			gSetting_350EN       = gSubMenuSelection;
			gVfoConfigureMode    = VFO_CONFIGURE_RELOAD;
			gFlagResetVfos       = true;
			break;

		case MENU_SCREN:
			gSetting_ScrambleEnable = gSubMenuSelection;
			gFlagReconfigureVfos    = true;
			break;

		case MENU_TX_EN:
			gSetting_TX_EN = gSubMenuSelection;
			break;

		#ifdef ENABLE_F_CAL_MENU
			case MENU_F_CALI:
				writeXtalFreqCal(gSubMenuSelection, true);
				return;
		#endif

		case MENU_BATCAL:
		{
			uint16_t buf[4];

			gBatteryCalibration[0] = (520ul * gSubMenuSelection) / 760;  // 5.20V empty, blinking above this value, reduced functionality below
			gBatteryCalibration[1] = (700ul * gSubMenuSelection) / 760;  // 7.00V,  ~5%, 1 bars above this value
			gBatteryCalibration[2] = (745ul * gSubMenuSelection) / 760;  // 7.45V, ~17%, 2 bars above this value
			gBatteryCalibration[3] =          gSubMenuSelection;         // 7.6V,  ~29%, 3 bars above this value
			gBatteryCalibration[4] = (788ul * gSubMenuSelection) / 760;  // 7.88V, ~65%, 4 bars above this value
			gBatteryCalibration[5] = 2300;
			EEPROM_WriteBuffer(0x1F40, gBatteryCalibration);

			EEPROM_ReadBuffer( 0x1F48, buf, sizeof(buf));
			buf[0] = gBatteryCalibration[4];
			buf[1] = gBatteryCalibration[5];
			EEPROM_WriteBuffer(0x1F48, buf);

			break;
		}
	}

	gRequestSaveSettings = true;
}

void MENU_SelectNextCode(void)
{
	int32_t UpperLimit;

	if (gMenuCursor == MENU_R_DCS)
		UpperLimit = 208;
		//UpperLimit = ARRAY_SIZE(DCS_Options);
	else
	if (gMenuCursor == MENU_R_CTCS)
		UpperLimit = ARRAY_SIZE(CTCSS_Options) - 1;
	else
		return;

	gSubMenuSelection = NUMBER_AddWithWraparound(gSubMenuSelection, gMenuScrollDirection, 1, UpperLimit);

	if (gMenuCursor == MENU_R_DCS)
	{
		if (gSubMenuSelection > 104)
		{
			gSelectedcode_type = CODE_TYPE_REVERSE_DIGITAL;
			gSelectedCode     = gSubMenuSelection - 105;
		}
		else
		{
			gSelectedcode_type = CODE_TYPE_DIGITAL;
			gSelectedCode     = gSubMenuSelection - 1;
		}

	}
	else
	{
		gSelectedcode_type = CODE_TYPE_CONTINUOUS_TONE;
		gSelectedCode     = gSubMenuSelection - 1;
	}

	RADIO_SetupRegisters(true);

	gScanPauseDelayIn_10ms = (gSelectedcode_type == CODE_TYPE_CONTINUOUS_TONE) ? scan_pause_delay_in_3_10ms : scan_pause_delay_in_4_10ms;

	gUpdateDisplay = true;
}

static void MENU_ClampSelection(int8_t Direction)
{
	int32_t Min;
	int32_t Max;

	if (!MENU_GetLimits(gMenuCursor, &Min, &Max))
	{
		int32_t Selection = gSubMenuSelection;
		if (Selection < Min) Selection = Min;
		else
		if (Selection > Max) Selection = Max;
		gSubMenuSelection = NUMBER_AddWithWraparound(Selection, Direction, Min, Max);
	}
}

void MENU_ShowCurrentSetting(void)
{
	switch (gMenuCursor)
	{
		case MENU_SQL:
			gSubMenuSelection = g_eeprom.squelch_level;
			break;

		case MENU_STEP:
			gSubMenuSelection = gTxVfo->step_setting;
			break;

		case MENU_TXP:
			gSubMenuSelection = gTxVfo->output_power;
			break;

		case MENU_R_DCS:
			switch (gTxVfo->freq_config_rx.code_type)
			{
				case CODE_TYPE_DIGITAL:
					gSubMenuSelection = gTxVfo->freq_config_rx.code + 1;
					break;
				case CODE_TYPE_REVERSE_DIGITAL:
					gSubMenuSelection = gTxVfo->freq_config_rx.code + 105;
					break;
				default:
					gSubMenuSelection = 0;
					break;
			}
			break;

		case MENU_RESET:
			gSubMenuSelection = 0;
			break;

		case MENU_R_CTCS:
			gSubMenuSelection = (gTxVfo->freq_config_rx.code_type == CODE_TYPE_CONTINUOUS_TONE) ? gTxVfo->freq_config_rx.code + 1 : 0;
			break;

		case MENU_T_DCS:
			switch (gTxVfo->freq_config_tx.code_type)
			{
				case CODE_TYPE_DIGITAL:
					gSubMenuSelection = gTxVfo->freq_config_tx.code + 1;
					break;
				case CODE_TYPE_REVERSE_DIGITAL:
					gSubMenuSelection = gTxVfo->freq_config_tx.code + 105;
					break;
				default:
					gSubMenuSelection = 0;
					break;
			}
			break;

		case MENU_T_CTCS:
			gSubMenuSelection = (gTxVfo->freq_config_tx.code_type == CODE_TYPE_CONTINUOUS_TONE) ? gTxVfo->freq_config_tx.code + 1 : 0;
			break;

		case MENU_SFT_D:
			gSubMenuSelection = gTxVfo->tx_offset_freq_dir;
			break;

		case MENU_OFFSET:
			gSubMenuSelection = gTxVfo->tx_offset_freq;
			break;

		case MENU_W_N:
			gSubMenuSelection = gTxVfo->channel_bandwidth;
			break;

		case MENU_SCR:
			gSubMenuSelection = gTxVfo->scrambling_type;
			break;

		case MENU_BCL:
			gSubMenuSelection = gTxVfo->busy_channel_lock;
			break;

		case MENU_MEM_CH:
			#if 0
				gSubMenuSelection = g_eeprom.user_channel[0];
			#else
				gSubMenuSelection = g_eeprom.user_channel[g_eeprom.tx_vfo];
			#endif
			break;

		case MENU_MEM_NAME:
			gSubMenuSelection = g_eeprom.user_channel[g_eeprom.tx_vfo];
			break;

		case MENU_SAVE:
			gSubMenuSelection = g_eeprom.battery_save;
			break;

		#ifdef ENABLE_VOX
			case MENU_VOX:
				gSubMenuSelection = g_eeprom.vox_switch ? g_eeprom.vox_level + 1 : 0;
				break;
		#endif

		case MENU_ABR:
			gSubMenuSelection = g_eeprom.backlight;

			gBacklightCountdown = 0;
			GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);  	// turn the backlight ON while in backlight menu
			break;

		case MENU_ABR_ON_TX_RX:
			gSubMenuSelection = gSetting_backlight_on_tx_rx;
			break;

		case MENU_TDR:
//			gSubMenuSelection = g_eeprom.dual_watch;
			gSubMenuSelection = (g_eeprom.dual_watch == DUAL_WATCH_OFF) ? 0 : 1;
			break;

		case MENU_XB:
			gSubMenuSelection = g_eeprom.cross_vfo_rx_tx;
			break;

		case MENU_BEEP:
			gSubMenuSelection = g_eeprom.beep_control;
			break;

		case MENU_TOT:
			gSubMenuSelection = g_eeprom.tx_timeout_timer;
			break;

		#ifdef ENABLE_VOICE
			case MENU_VOICE:
				gSubMenuSelection = g_eeprom.voice_prompt;
				break;
		#endif

		case MENU_SC_REV:
			gSubMenuSelection = g_eeprom.scan_resume_mode;
			break;

		case MENU_MDF:
			gSubMenuSelection = g_eeprom.channel_display_mode;
			break;

		case MENU_AUTOLK:
			gSubMenuSelection = g_eeprom.auto_keypad_lock;
			break;

		case MENU_S_ADD1:
			gSubMenuSelection = gTxVfo->scanlist_1_participation;
			break;

		case MENU_S_ADD2:
			gSubMenuSelection = gTxVfo->scanlist_2_participation;
			break;

		case MENU_STE:
			gSubMenuSelection = g_eeprom.tail_note_elimination;
			break;

		case MENU_RP_STE:
			gSubMenuSelection = g_eeprom.repeater_tail_tone_elimination;
			break;

		case MENU_MIC:
			gSubMenuSelection = g_eeprom.mic_sensitivity;
			break;

		#ifdef ENABLE_AUDIO_BAR
			case MENU_MIC_BAR:
				gSubMenuSelection = gSetting_mic_bar;
				break;
		#endif

		case MENU_COMPAND:
			gSubMenuSelection = gTxVfo->compander;
			return;

		case MENU_1_CALL:
			gSubMenuSelection = g_eeprom.chan_1_call;
			break;

		case MENU_S_LIST:
			gSubMenuSelection = g_eeprom.scan_list_default;
			break;

		case MENU_SLIST1:
			gSubMenuSelection = RADIO_FindNextChannel(0, 1, true, 0);
			break;

		case MENU_SLIST2:
			gSubMenuSelection = RADIO_FindNextChannel(0, 1, true, 1);
			break;

		#ifdef ENABLE_ALARM
			case MENU_AL_MOD:
				gSubMenuSelection = g_eeprom.alarm_mode;
				break;
		#endif

		case MENU_D_ST:
			gSubMenuSelection = g_eeprom.DTMF_side_tone;
			break;

		case MENU_D_RSP:
			gSubMenuSelection = g_eeprom.DTMF_decode_response;
			break;

		case MENU_D_HOLD:
			gSubMenuSelection = g_eeprom.DTMF_auto_reset_time;

			if (gSubMenuSelection <= DTMF_HOLD_MIN)
				gSubMenuSelection = DTMF_HOLD_MIN;
			else
			if (gSubMenuSelection <= 10)
				gSubMenuSelection = 10;
			else
			if (gSubMenuSelection <= 20)
				gSubMenuSelection = 20;
			else
			if (gSubMenuSelection <= 30)
				gSubMenuSelection = 30;
			else
			if (gSubMenuSelection <= 40)
				gSubMenuSelection = 40;
			else
			if (gSubMenuSelection <= 50)
				gSubMenuSelection = 50;
			else
			if (gSubMenuSelection < DTMF_HOLD_MAX)
				gSubMenuSelection = 50;
			else
				gSubMenuSelection = DTMF_HOLD_MAX;

			break;

		case MENU_D_PRE:
			gSubMenuSelection = g_eeprom.DTMF_preload_time / 10;
			break;

		case MENU_PTT_ID:
			gSubMenuSelection = gTxVfo->DTMF_ptt_id_tx_mode;
			break;

		case MENU_BAT_TXT:
			gSubMenuSelection = gSetting_battery_text;
			return;

		case MENU_D_DCD:
			gSubMenuSelection = gTxVfo->DTMF_decoding_enable;
			break;

		case MENU_D_LIST:
			gSubMenuSelection = gDTMF_chosen_contact + 1;
			break;

		case MENU_D_LIVE_DEC:
			gSubMenuSelection = gSetting_live_DTMF_decoder;
			break;

		case MENU_PONMSG:
			gSubMenuSelection = g_eeprom.pwr_on_display_mode;
			break;

		case MENU_ROGER:
			gSubMenuSelection = g_eeprom.roger_mode;
			break;

		case MENU_AM:
			gSubMenuSelection = gTxVfo->am_mode;
			break;

		#ifdef ENABLE_AM_FIX
			case MENU_AM_FIX:
				gSubMenuSelection = gSetting_AM_fix;
				break;
		#endif

		#ifdef ENABLE_AM_FIX_TEST1
			case MENU_AM_FIX_TEST1:
				gSubMenuSelection = gSetting_AM_fix_test1;
				break;
		#endif

		#ifdef ENABLE_NOAA
			case MENU_NOAA_S:
				gSubMenuSelection = g_eeprom.NOAA_auto_scan;
				break;
		#endif

		case MENU_DEL_CH:
			#if 0
				gSubMenuSelection = RADIO_FindNextChannel(g_eeprom.user_channel[0], 1, false, 1);
			#else
				gSubMenuSelection = RADIO_FindNextChannel(g_eeprom.user_channel[g_eeprom.tx_vfo], 1, false, 1);
			#endif
			break;

		case MENU_SIDE1_SHORT:
			gSubMenuSelection = g_eeprom.key1_short_press_action;
			break;

		case MENU_SIDE1_LONG:
			gSubMenuSelection = g_eeprom.key1_long_press_action;
			break;

		case MENU_SIDE2_SHORT:
			gSubMenuSelection = g_eeprom.key2_short_press_action;
			break;

		case MENU_SIDE2_LONG:
			gSubMenuSelection = g_eeprom.key2_long_press_action;
			break;

		case MENU_350TX:
			gSubMenuSelection = gSetting_350TX;
			break;

		case MENU_F_LOCK:
			gSubMenuSelection = gSetting_F_LOCK;
			break;

		case MENU_200TX:
			gSubMenuSelection = gSetting_200TX;
			break;

		case MENU_500TX:
			gSubMenuSelection = gSetting_500TX;
			break;

		case MENU_350EN:
			gSubMenuSelection = gSetting_350EN;
			break;

		case MENU_SCREN:
			gSubMenuSelection = gSetting_ScrambleEnable;
			break;

		case MENU_TX_EN:
			gSubMenuSelection = gSetting_TX_EN;
			break;

		#ifdef ENABLE_F_CAL_MENU
			case MENU_F_CALI:
				gSubMenuSelection = g_eeprom.BK4819_xtal_freq_low;
				break;
		#endif

		case MENU_BATCAL:
			gSubMenuSelection = gBatteryCalibration[3];
			break;

		default:
			return;
	}
}

static void MENU_Key_0_to_9(key_code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	uint8_t  Offset;
	int32_t  Min;
	int32_t  Max;
	uint16_t Value = 0;

	if (bKeyHeld || !bKeyPressed)
		return;

	gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

	if (gMenuCursor == MENU_MEM_NAME && edit_index >= 0)
	{	// currently editing the channel name

		if (edit_index < 10)
		{
			if (Key >= KEY_0 && Key <= KEY_9)
			{
				edit[edit_index] = '0' + Key - KEY_0;

				if (++edit_index >= 10)
				{	// exit edit
					gFlagAcceptSetting  = false;
					gAskForConfirmation = 1;
				}

				gRequestDisplayScreen = DISPLAY_MENU;
			}
		}

		return;
	}

	INPUTBOX_Append(Key);

	gRequestDisplayScreen = DISPLAY_MENU;

	if (!gIsInSubMenu)
	{
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

		switch (gInputBoxIndex)
		{
			case 2:
				gInputBoxIndex = 0;

				Value = (gInputBox[0] * 10) + gInputBox[1];

				if (Value > 0 && Value <= gMenuListCount)
				{
					gMenuCursor         = Value - 1;
					gFlagRefreshSetting = true;
					return;
				}

				if (Value <= gMenuListCount)
					break;

				gInputBox[0]   = gInputBox[1];
				gInputBoxIndex = 1;

			case 1:
				Value = gInputBox[0];
				if (Value > 0 && Value <= gMenuListCount)
				{
					gMenuCursor         = Value - 1;
					gFlagRefreshSetting = true;
					return;
				}
				break;
		}

		#pragma GCC diagnostic pop

		gInputBoxIndex = 0;

		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	if (gMenuCursor == MENU_OFFSET)
	{
		uint32_t Frequency;

		if (gInputBoxIndex < 6)
		{	// invalid frequency
			#ifdef ENABLE_VOICE
				g_another_voice_id = (voice_id_t)Key;
			#endif
			return;
		}

		#ifdef ENABLE_VOICE
			g_another_voice_id = (voice_id_t)Key;
		#endif

		NUMBER_Get(gInputBox, &Frequency);
		gSubMenuSelection = FREQUENCY_FloorToStep(Frequency + 75, gTxVfo->step_freq, 0);

		gInputBoxIndex = 0;
		return;
	}

	if (gMenuCursor == MENU_MEM_CH || gMenuCursor == MENU_DEL_CH || gMenuCursor == MENU_1_CALL || gMenuCursor == MENU_MEM_NAME)
	{	// enter 3-digit channel number

		if (gInputBoxIndex < 3)
		{
			#ifdef ENABLE_VOICE
				g_another_voice_id   = (voice_id_t)Key;
			#endif
			gRequestDisplayScreen = DISPLAY_MENU;
			return;
		}

		gInputBoxIndex = 0;

		Value = ((gInputBox[0] * 100) + (gInputBox[1] * 10) + gInputBox[2]) - 1;

		if (Value <= USER_CHANNEL_LAST)
		{
			#ifdef ENABLE_VOICE
				g_another_voice_id = (voice_id_t)Key;
			#endif
			gSubMenuSelection = Value;
			return;
		}

		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	if (MENU_GetLimits(gMenuCursor, &Min, &Max))
	{
		gInputBoxIndex = 0;
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	Offset = (Max >= 100) ? 3 : (Max >= 10) ? 2 : 1;

	switch (gInputBoxIndex)
	{
		case 1:
			Value = gInputBox[0];
			break;
		case 2:
			Value = (gInputBox[0] *  10) + gInputBox[1];
			break;
		case 3:
			Value = (gInputBox[0] * 100) + (gInputBox[1] * 10) + gInputBox[2];
			break;
	}

	if (Offset == gInputBoxIndex)
		gInputBoxIndex = 0;

	if (Value <= Max)
	{
		gSubMenuSelection = Value;
		return;
	}

	gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}

static void MENU_Key_EXIT(bool bKeyPressed, bool bKeyHeld)
{
	if (bKeyHeld || !bKeyPressed)
		return;

	gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

	if (gCssScanMode == CSS_SCAN_MODE_OFF)
	{
		if (gIsInSubMenu)
		{
			if (gInputBoxIndex == 0 || gMenuCursor != MENU_OFFSET)
			{
				gAskForConfirmation = 0;
				gIsInSubMenu        = false;
				gInputBoxIndex      = 0;
				gFlagRefreshSetting = true;

				#ifdef ENABLE_VOICE
					g_another_voice_id = VOICE_ID_CANCEL;
				#endif
			}
			else
				gInputBox[--gInputBoxIndex] = 10;

			// ***********************

			gRequestDisplayScreen = DISPLAY_MENU;
			return;
		}

		#ifdef ENABLE_VOICE
			g_another_voice_id = VOICE_ID_CANCEL;
		#endif

		gRequestDisplayScreen = DISPLAY_MAIN;

		if (g_eeprom.backlight == 0)
		{
			gBacklightCountdown = 0;
			GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);	// turn the backlight OFF
		}
	}
	else
	{
		MENU_StopCssScan();

		#ifdef ENABLE_VOICE
			g_another_voice_id   = VOICE_ID_SCANNING_STOP;
		#endif

		gRequestDisplayScreen = DISPLAY_MENU;
	}

	gPttWasReleased = true;
}

static void MENU_Key_MENU(const bool bKeyPressed, const bool bKeyHeld)
{
	if (bKeyHeld || !bKeyPressed)
		return;

	gBeepToPlay           = BEEP_1KHZ_60MS_OPTIONAL;
	gRequestDisplayScreen = DISPLAY_MENU;

	if (!gIsInSubMenu)
	{
		#ifdef ENABLE_VOICE
			if (gMenuCursor != MENU_SCR)
				g_another_voice_id = MenuList[MenuList_sorted[gMenuCursor]].voice_id;
		#endif

		#if 1
			if (gMenuCursor == MENU_DEL_CH || gMenuCursor == MENU_MEM_NAME)
				if (!RADIO_CheckValidChannel(gSubMenuSelection, false, 0))
					return;  // invalid channel
		#endif

		gAskForConfirmation = 0;
		gIsInSubMenu        = true;

//		if (gMenuCursor != MENU_D_LIST)
		{
			gInputBoxIndex      = 0;
			edit_index          = -1;
		}

		return;
	}

	if (gMenuCursor == MENU_MEM_NAME)
	{
		if (edit_index < 0)
		{	// enter channel name edit mode
			if (!RADIO_CheckValidChannel(gSubMenuSelection, false, 0))
				return;

			BOARD_fetchChannelName(edit, gSubMenuSelection);

			// pad the channel name out with '_'
			edit_index = strlen(edit);
			while (edit_index < 10)
				edit[edit_index++] = '_';
			edit[edit_index] = 0;
			edit_index = 0;  // 'edit_index' is going to be used as the cursor position

			// make a copy so we can test for change when exiting the menu item
			memmove(edit_original, edit, sizeof(edit_original));

			return;
		}
		else
		if (edit_index >= 0 && edit_index < 10)
		{	// editing the channel name characters

			if (++edit_index < 10)
				return;	// next char

			// exit
			if (memcmp(edit_original, edit, sizeof(edit_original)) == 0)
			{	// no change - drop it
				gFlagAcceptSetting  = false;
				gIsInSubMenu        = false;
				gAskForConfirmation = 0;
			}
			else
			{
				gFlagAcceptSetting  = false;
				gAskForConfirmation = 0;
			}
		}
	}

	// exiting the sub menu

	if (gIsInSubMenu)
	{
		if (gMenuCursor == MENU_RESET  ||
			gMenuCursor == MENU_MEM_CH ||
			gMenuCursor == MENU_DEL_CH ||
			gMenuCursor == MENU_MEM_NAME)
		{
			switch (gAskForConfirmation)
			{
				case 0:
					gAskForConfirmation = 1;
					break;

				case 1:
					gAskForConfirmation = 2;

					UI_DisplayMenu();

					if (gMenuCursor == MENU_RESET)
					{
						#ifdef ENABLE_VOICE
							AUDIO_SetVoiceID(0, VOICE_ID_CONFIRM);
							AUDIO_PlaySingleVoice(true);
						#endif

						MENU_AcceptSetting();

						#if defined(ENABLE_OVERLAY)
							overlay_FLASH_RebootToBootloader();
						#else
							NVIC_SystemReset();
						#endif
					}

					gFlagAcceptSetting  = true;
					gIsInSubMenu        = false;
					gAskForConfirmation = 0;
			}
		}
		else
		{
			gFlagAcceptSetting = true;
			gIsInSubMenu       = false;
		}
	}

	if (gCssScanMode != CSS_SCAN_MODE_OFF)
	{
		gCssScanMode  = CSS_SCAN_MODE_OFF;
		gUpdateStatus = true;
	}

	#ifdef ENABLE_VOICE
		if (gMenuCursor == MENU_SCR)
			g_another_voice_id = (gSubMenuSelection == 0) ? VOICE_ID_SCRAMBLER_OFF : VOICE_ID_SCRAMBLER_ON;
		else
			g_another_voice_id = VOICE_ID_CONFIRM;
	#endif

	gInputBoxIndex = 0;
}

static void MENU_Key_STAR(const bool bKeyPressed, const bool bKeyHeld)
{
	if (bKeyHeld || !bKeyPressed)
		return;

	gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

	if (gMenuCursor == MENU_MEM_NAME && edit_index >= 0)
	{	// currently editing the channel name

		if (edit_index < 10)
		{
			edit[edit_index] = '-';

			if (++edit_index >= 10)
			{	// exit edit
				gFlagAcceptSetting  = false;
				gAskForConfirmation = 1;
			}

			gRequestDisplayScreen = DISPLAY_MENU;
		}

		return;
	}

	RADIO_SelectVfos();

	#ifdef ENABLE_NOAA
		if (IS_NOT_NOAA_CHANNEL(gRxVfo->channel_save) && gRxVfo->am_mode == 0)
	#else
		if (gRxVfo->am_mode == 0)
	#endif
	{
		if (gMenuCursor == MENU_R_CTCS || gMenuCursor == MENU_R_DCS)
		{	// scan CTCSS or DCS to find the tone/code of the incoming signal

			if (gCssScanMode == CSS_SCAN_MODE_OFF)
			{
				MENU_StartCssScan(1);
				gRequestDisplayScreen = DISPLAY_MENU;
				#ifdef ENABLE_VOICE
					AUDIO_SetVoiceID(0, VOICE_ID_SCANNING_BEGIN);
					AUDIO_PlaySingleVoice(1);
				#endif
			}
			else
			{
				MENU_StopCssScan();
				gRequestDisplayScreen = DISPLAY_MENU;
				#ifdef ENABLE_VOICE
					g_another_voice_id       = VOICE_ID_SCANNING_STOP;
				#endif
			}
		}

		gPttWasReleased = true;
		return;
	}

	gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}

static void MENU_Key_UP_DOWN(bool bKeyPressed, bool bKeyHeld, int8_t Direction)
{
	uint8_t VFO;
	uint8_t Channel;
	bool    bCheckScanList;

	if (gMenuCursor == MENU_MEM_NAME && gIsInSubMenu && edit_index >= 0)
	{	// change the character
		if (bKeyPressed && edit_index < 10 && Direction != 0)
		{
			const char   unwanted[] = "$%&!\"':;?^`|{}";
			char         c          = edit[edit_index] + Direction;
			unsigned int i          = 0;
			while (i < sizeof(unwanted) && c >= 32 && c <= 126)
			{
				if (c == unwanted[i++])
				{	// choose next character
					c += Direction;
					i = 0;
				}
			}
			edit[edit_index] = (c < 32) ? 126 : (c > 126) ? 32 : c;

			gRequestDisplayScreen = DISPLAY_MENU;
		}
		return;
	}

	if (!bKeyHeld)
	{
		if (!bKeyPressed)
			return;

		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

		gInputBoxIndex = 0;
	}
	else
	if (!bKeyPressed)
		return;

	if (gCssScanMode != CSS_SCAN_MODE_OFF)
	{
		MENU_StartCssScan(Direction);

		gPttWasReleased       = true;
		gRequestDisplayScreen = DISPLAY_MENU;
		return;
	}

	if (!gIsInSubMenu)
	{
		gMenuCursor = NUMBER_AddWithWraparound(gMenuCursor, -Direction, 0, gMenuListCount - 1);

		gFlagRefreshSetting = true;

		gRequestDisplayScreen = DISPLAY_MENU;

		if (gMenuCursor != MENU_ABR && g_eeprom.backlight == 0)
		{
			gBacklightCountdown = 0;
			GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);	// turn the backlight OFF
		}

		return;
	}

	if (gMenuCursor == MENU_OFFSET)
	{
		int32_t Offset = (Direction * gTxVfo->step_freq) + gSubMenuSelection;
		if (Offset < 99999990)
		{
			if (Offset < 0)
				Offset = 99999990;
		}
		else
			Offset = 0;

		gSubMenuSelection     = FREQUENCY_FloorToStep(Offset, gTxVfo->step_freq, 0);
		gRequestDisplayScreen = DISPLAY_MENU;
		return;
	}

	VFO = 0;

	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="

	switch (gMenuCursor)
	{
		case MENU_DEL_CH:
		case MENU_1_CALL:
		case MENU_MEM_NAME:
			bCheckScanList = false;
			break;

		case MENU_SLIST2:
			VFO = 1;
		case MENU_SLIST1:
			bCheckScanList = true;
			break;

		default:
			MENU_ClampSelection(Direction);
			gRequestDisplayScreen = DISPLAY_MENU;
			return;
	}

	#pragma GCC diagnostic pop

	Channel = RADIO_FindNextChannel(gSubMenuSelection + Direction, Direction, bCheckScanList, VFO);
	if (Channel != 0xFF)
		gSubMenuSelection = Channel;

	gRequestDisplayScreen = DISPLAY_MENU;
}

void MENU_ProcessKeys(key_code_t Key, bool bKeyPressed, bool bKeyHeld)
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
			MENU_Key_0_to_9(Key, bKeyPressed, bKeyHeld);
			break;
		case KEY_MENU:
			MENU_Key_MENU(bKeyPressed, bKeyHeld);
			break;
		case KEY_UP:
			MENU_Key_UP_DOWN(bKeyPressed, bKeyHeld,  1);
			break;
		case KEY_DOWN:
			MENU_Key_UP_DOWN(bKeyPressed, bKeyHeld, -1);
			break;
		case KEY_EXIT:
			MENU_Key_EXIT(bKeyPressed, bKeyHeld);
			break;
		case KEY_STAR:
			MENU_Key_STAR(bKeyPressed, bKeyHeld);
			break;
		case KEY_F:
			if (gMenuCursor == MENU_MEM_NAME && edit_index >= 0)
			{	// currently editing the channel name
				if (!bKeyHeld && bKeyPressed)
				{
					gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
					if (edit_index < 10)
					{
						edit[edit_index] = ' ';
						if (++edit_index >= 10)
						{	// exit edit
							gFlagAcceptSetting  = false;
							gAskForConfirmation = 1;
						}
						gRequestDisplayScreen = DISPLAY_MENU;
					}
				}
				break;
			}

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

	if (gScreenToDisplay == DISPLAY_MENU)
	{
		if (gMenuCursor == MENU_VOL ||
			#ifdef ENABLE_F_CAL_MENU
				gMenuCursor == MENU_F_CALI ||
		    #endif
			gMenuCursor == MENU_BATCAL)
		{
			gMenuCountdown = menu_timeout_long_500ms;
		}
		else
		{
			gMenuCountdown = menu_timeout_500ms;
		}
	}
}
