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

#ifndef MISC_H
#define MISC_H

#include <stdbool.h>
#include <stdint.h>

#ifndef ARRAY_SIZE
	#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

#define IS_MR_CHANNEL(x)           ((x) >= MR_CHANNEL_FIRST   && (x) <= MR_CHANNEL_LAST)
#define IS_FREQ_CHANNEL(x)         ((x) >= FREQ_CHANNEL_FIRST && (x) <= FREQ_CHANNEL_LAST)
#define IS_VALID_CHANNEL(x)        ((x) < LAST_CHANNEL)

#ifdef ENABLE_NOAA
	#define IS_NOAA_CHANNEL(x)     ((x) >= NOAA_CHANNEL_FIRST && (x) <= NOAA_CHANNEL_LAST)
	#define IS_NOT_NOAA_CHANNEL(x) ((x) >= MR_CHANNEL_FIRST   && (x) <= FREQ_CHANNEL_LAST)
#endif

enum {
	MR_CHANNEL_FIRST   = 0,
	MR_CHANNEL_LAST    = 199u,
	FREQ_CHANNEL_FIRST = 200u,
	FREQ_CHANNEL_LAST  = 206u,
	#ifdef ENABLE_NOAA
		NOAA_CHANNEL_FIRST = 207u,
		NOAA_CHANNEL_LAST  = 216u,
	#endif
	LAST_CHANNEL
};

enum {
	FLASHLIGHT_OFF = 0,
	FLASHLIGHT_ON,
	FLASHLIGHT_BLINK
};

enum {
	VFO_CONFIGURE_0 = 0,
	VFO_CONFIGURE_1,
	VFO_CONFIGURE_RELOAD
};

enum AlarmState_t {
	ALARM_STATE_OFF = 0,
	ALARM_STATE_TXALARM,
	ALARM_STATE_ALARM,
	ALARM_STATE_TX1750
};

typedef enum AlarmState_t AlarmState_t;

enum ReceptionMode_t {
	RX_MODE_NONE = 0,   // squelch close ?
	RX_MODE_DETECTED,   // signal detected
	RX_MODE_LISTENING   //
};

typedef enum ReceptionMode_t ReceptionMode_t;

enum CssScanMode_t
{
	CSS_SCAN_MODE_OFF = 0,
	CSS_SCAN_MODE_SCANNING,
	CSS_SCAN_MODE_FOUND,
};

typedef enum CssScanMode_t   CssScanMode_t;

extern const uint8_t         fm_resume_countdown_500ms;
extern const uint8_t         fm_radio_countdown_500ms;
extern const uint16_t        fm_play_countdown_scan_10ms;
extern const uint16_t        fm_play_countdown_noscan_10ms;
extern const uint16_t        fm_restore_countdown_10ms;

extern const uint8_t         menu_timeout_500ms;

extern const uint8_t         DTMF_RX_timeout_500ms;
extern const uint8_t         DTMF_RX_timeout_saved_500ms;
extern const uint8_t         DTMF_decode_ring_countdown_500ms;
extern const uint8_t         DTMF_txstop_countdown_500ms;

extern const uint8_t         key_input_timeout_500ms;

extern const uint16_t        key_repeat_delay_10ms;
extern const uint16_t        key_repeat_10ms;
extern const uint16_t        key_debounce_10ms;

extern const uint8_t         scan_delay_10ms;

extern const uint16_t        battery_save_count_10ms;

extern const uint16_t        power_save1_10ms;
extern const uint16_t        power_save2_10ms;

extern const uint16_t        NOAA_countdown_10ms;
extern const uint16_t        NOAA_countdown_2_10ms;
extern const uint16_t        NOAA_countdown_3_10ms;

extern const uint16_t        dual_watch_count_after_tx_10ms;
extern const uint16_t        dual_watch_count_after_rx_10ms;
extern const uint16_t        dual_watch_count_after_1_10ms;
extern const uint16_t        dual_watch_count_after_2_10ms;
extern const uint16_t        dual_watch_count_toggle_10ms;
extern const uint16_t        dual_watch_count_noaa_10ms;
extern const uint16_t        dual_watch_count_after_vox_10ms;

extern const uint16_t        scan_pause_delay_in_1_10ms;
extern const uint16_t        scan_pause_delay_in_2_10ms;
extern const uint16_t        scan_pause_delay_in_3_10ms;
extern const uint16_t        scan_pause_delay_in_4_10ms;

extern const uint16_t        gMax_bat_v;
extern const uint16_t        gMin_bat_v;

extern const uint8_t         gMicGain_dB2[5];

extern bool                  gSetting_350TX;
extern bool                  gSetting_KILLED;
extern bool                  gSetting_200TX;
extern bool                  gSetting_500TX;
extern bool                  gSetting_350EN;
extern bool                  gSetting_TX_EN;
extern uint8_t               gSetting_F_LOCK;
extern bool                  gSetting_ScrambleEnable;

#ifdef ENABLE_AM_FIX
	extern bool              gSetting_AM_fix;
#endif
#ifdef ENABLE_AM_FIX_TEST1
	extern uint8_t           gSetting_AM_fix_test1;
#endif
#ifdef ENABLE_AUDIO_BAR
	extern bool              gSetting_mic_bar;
#endif
extern bool                  gSetting_live_DTMF_decoder;
extern uint8_t               gSetting_battery_text;

extern bool                  gMonitor;

extern const uint32_t        gDefaultAesKey[4];
extern uint32_t              gCustomAesKey[4];
extern bool                  bHasCustomAesKey;
extern uint32_t              gChallenge[4];
extern uint8_t               gTryCount;

extern uint8_t               gEEPROM_1EC0_0[8];
extern uint8_t               gEEPROM_1EC0_1[8];
extern uint8_t               gEEPROM_1EC0_2[8];
extern uint8_t               gEEPROM_1EC0_3[8];

extern uint16_t              gEEPROM_RSSI_CALIB[3][4];

extern uint16_t              gEEPROM_1F8A;
extern uint16_t              gEEPROM_1F8C;

extern uint8_t               gMR_ChannelAttributes[207];

extern volatile uint16_t     gBatterySaveCountdown_10ms;

extern volatile bool         gPowerSaveCountdownExpired;
extern volatile bool         gSchedulePowerSave;

extern volatile bool         gScheduleDualWatch;

extern volatile uint16_t     gDualWatchCountdown_10ms;
extern volatile bool         gDualWatchCountdownExpired;
extern bool                  gDualWatchActive;

extern volatile bool         gNextTimeslice500ms;
extern volatile uint16_t     gTxTimerCountdown;
extern volatile uint16_t     gTailNoteEliminationCountdown_10ms;
#ifdef ENABLE_FMRADIO
	extern volatile uint16_t gFmPlayCountdown_10ms;
#endif
#ifdef ENABLE_NOAA
	extern volatile uint16_t gNOAA_Countdown_10ms;
#endif
extern bool                  gEnableSpeaker;
extern uint8_t               gKeyInputCountdown;
extern uint8_t               gKeyLockCountdown;
extern uint8_t               gRTTECountdown;
extern bool                  bIsInLockScreen;
extern uint8_t               gUpdateStatus;
extern uint8_t               gFoundCTCSS;
extern uint8_t               gFoundCDCSS;
extern bool                  gEndOfRxDetectedMaybe;

extern int16_t               gVFO_RSSI[2];
extern uint8_t               gVFO_RSSI_bar_level[2];

extern uint8_t               gReducedService;
extern uint8_t               gBatteryVoltageIndex;
extern CssScanMode_t         gCssScanMode;
extern bool                  gUpdateRSSI;
extern AlarmState_t          gAlarmState;
extern uint8_t               gVoltageMenuCountdown;
extern bool                  gPttWasReleased;
extern bool                  gPttWasPressed;
extern bool                  gFlagReconfigureVfos;
extern uint8_t               gVfoConfigureMode;
extern bool                  gFlagResetVfos;
extern bool                  gRequestSaveVFO;
extern uint8_t               gRequestSaveChannel;
extern bool                  gRequestSaveSettings;
#ifdef ENABLE_FMRADIO
	extern bool              gRequestSaveFM;
#endif
extern uint8_t               gKeypadLocked;
extern bool                  gFlagPrepareTX;
extern bool                  gFlagAcceptSetting;
extern bool                  gFlagRefreshSetting;
extern bool                  gFlagSaveVfo;
extern bool                  gFlagSaveSettings;
extern bool                  gFlagSaveChannel;
#ifdef ENABLE_FMRADIO
	extern bool              gFlagSaveFM;
#endif
extern uint8_t               gDTMF_RequestPending;
extern bool                  g_CDCSS_Lost;
extern uint8_t               gCDCSSCodeType;
extern bool                  g_CTCSS_Lost;
extern bool                  g_CxCSS_TAIL_Found;
extern bool                  g_VOX_Lost;
extern bool                  g_SquelchLost;
extern uint8_t               gFlashLightState;
extern bool                  gVOX_NoiseDetected;
extern uint16_t              gVoxResumeCountdown;
extern uint16_t              gVoxPauseCountdown;
extern volatile uint16_t     gFlashLightBlinkCounter;
extern bool                  gFlagEndTransmission;
extern uint16_t              gLowBatteryCountdown;
extern uint8_t               gNextMrChannel;
extern ReceptionMode_t       gRxReceptionMode;
extern uint8_t               gRestoreMrChannel;
extern uint8_t               gCurrentScanList;
extern uint8_t               gPreviousMrChannel;
extern uint32_t              gRestoreFrequency;
extern uint8_t               gRxVfoIsActive;
extern uint8_t               gAlarmToneCounter;
extern uint16_t              gAlarmRunningCounter;
extern bool                  gKeyBeingHeld;
extern bool                  gPttIsPressed;
extern uint8_t               gPttDebounceCounter;
extern uint8_t               gMenuListCount;
extern uint8_t               gBackupCROSS_BAND_RX_TX;
extern uint8_t               gScanDelay_10ms;
#ifdef ENABLE_AIRCOPY
	extern uint8_t           gAircopySendCountdown;
#endif
extern uint8_t               gFSKWriteIndex;
extern uint8_t               gNeverUsed;
#ifdef ENABLE_NOAA
	extern bool              gIsNoaaMode;
	extern uint8_t           gNoaaChannel;
#endif
extern volatile bool         gNextTimeslice;
extern bool                  gUpdateDisplay;
#ifdef ENABLE_FMRADIO
	extern uint8_t           gFM_ChannelPosition;
#endif
extern bool                  gF_LOCK;
extern uint8_t               gShowChPrefix;
extern volatile uint8_t      gFoundCDCSSCountdown_10ms;
extern volatile uint8_t      gFoundCTCSSCountdown_10ms;
extern volatile uint16_t     gVoxStopCountdown_10ms;
extern volatile bool         gTxTimeoutReached;
extern volatile bool         gNextTimeslice40ms;
#ifdef ENABLE_NOAA
	extern volatile uint16_t gNOAACountdown_10ms;
	extern volatile bool     gScheduleNOAA;
#endif
extern volatile bool         gFlagTailNoteEliminationComplete;
#ifdef ENABLE_FMRADIO
	extern volatile bool     gScheduleFM;
#endif
extern int16_t               gCurrentRSSI[2];   // now one per VFO
extern uint8_t               gIsLocked;
extern volatile uint8_t      boot_counter_10ms;

void    NUMBER_Get(char *pDigits, uint32_t *pInteger);
void    NUMBER_ToDigits(uint32_t Value, char *pDigits);
int32_t NUMBER_AddWithWraparound(int32_t Base, int32_t Add, int32_t LowerLimit, int32_t UpperLimit);

#endif

