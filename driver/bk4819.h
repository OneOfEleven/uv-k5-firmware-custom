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

#ifndef DRIVER_BK4819_h
#define DRIVER_BK4819_h

#include <stdbool.h>
#include <stdint.h>

#include "driver/bk4819-regs.h"

enum BK4819_af_type_e
{
	BK4819_AF_MUTE      =  0u,  //
	BK4819_AF_FM        =  1u,  // FM
	BK4819_AF_TONE      =  2u,  //
	BK4819_AF_BEEP      =  3u,  //
	BK4819_AF_BASEBAND1 =  4u,  // SSB
	BK4819_AF_BASEBAND2 =  5u,  // SSB
	BK4819_AF_CTCO      =  6u,  // strange LF audio .. maybe the CTCSS LF line ?
	BK4819_AF_AM        =  7u,  // AM
	BK4819_AF_FSKO      =  8u,  // nothing
	BK4819_AF_UNKNOWN3  =  9u,  // distorted
	BK4819_AF_UNKNOWN4  = 10u,  // nothing at all
	BK4819_AF_UNKNOWN5  = 11u,  // distorted
	BK4819_AF_UNKNOWN6  = 12u,  // distorted
	BK4819_AF_UNKNOWN7  = 13u,  // interesting
	BK4819_AF_UNKNOWN8  = 14u,  // interesting
	BK4819_AF_UNKNOWN9  = 15u   // not a lot
};
typedef enum BK4819_af_type_e BK4819_af_type_t;

enum BK4819_filter_bandwidth_e
{
	BK4819_FILTER_BW_WIDE = 0,   // 25kHz
	BK4819_FILTER_BW_NARROW,     // 12.5kHz
	BK4819_FILTER_BW_NARROWER    // 6.25kHz
};
typedef enum BK4819_filter_bandwidth_e BK4819_filter_bandwidth_t;

enum BK4819_CSS_scan_result_e
{
	BK4819_CSS_RESULT_NOT_FOUND = 0,
	BK4819_CSS_RESULT_CTCSS,
	BK4819_CSS_RESULT_CDCSS
};
typedef enum BK4819_CSS_scan_result_e BK4819_CSS_scan_result_t;

extern bool g_rx_idle_mode;

void     BK4819_Init(void);
uint16_t BK4819_ReadRegister(const uint8_t Register);
void     BK4819_WriteRegister(const uint8_t Register, uint16_t Data);
void     BK4819_WriteU8(uint8_t Data);
void     BK4819_WriteU16(uint16_t Data);

void     BK4819_DisableAGC(void);
void     BK4819_EnableAGC(void);

void     BK4819_set_GPIO_pin(bk4819_gpio_pin_t Pin, bool bSet);

void     BK4819_SetCDCSSCodeWord(uint32_t CodeWord);
void     BK4819_SetCTCSSFrequency(uint32_t BaudRate);
void     BK4819_SetTailDetection(const uint32_t freq_10Hz);
void     BK4819_EnableVox(uint16_t Vox1Threshold, uint16_t Vox0Threshold);

void     BK4819_set_TX_deviation(const bool narrow);

void     BK4819_SetFilterBandwidth(const BK4819_filter_bandwidth_t Bandwidth, const bool weak_no_different);

void     BK4819_SetupPowerAmplifier(const uint8_t bias, const uint32_t frequency);
void     BK4819_set_rf_frequency(const uint32_t frequency, const bool trigger_update);
void     BK4819_SetupSquelch(
			uint8_t SquelchOpenRSSIThresh,
			uint8_t SquelchCloseRSSIThresh,
			uint8_t SquelchOpenNoiseThresh,
			uint8_t SquelchCloseNoiseThresh,
			uint8_t SquelchCloseGlitchThresh,
			uint8_t SquelchOpenGlitchThresh);

void     BK4819_SetAF(BK4819_af_type_t AF);
void     BK4819_RX_TurnOn(void);
void     BK4819_set_rf_filter_path(uint32_t Frequency);
void     BK4819_DisableScramble(void);
void     BK4819_EnableScramble(uint8_t Type);

bool     BK4819_CompanderEnabled(void);
void     BK4819_SetCompander(const unsigned int mode);

void     BK4819_DisableVox(void);
void     BK4819_DisableDTMF(void);
void     BK4819_EnableDTMF(void);
void     BK4819_StartTone1(const uint16_t frequency, const unsigned int level, const bool set_dac);
void     BK4819_StopTones(const bool tx);
void     BK4819_PlayTone(const unsigned int tone_Hz, const unsigned int delay, const unsigned int level);
void     BK4819_EnterTxMute(void);
void     BK4819_ExitTxMute(void);
void     BK4819_Sleep(void);
void     BK4819_TurnsOffTones_TurnsOnRX(void);

#ifdef ENABLE_AIRCOPY
	void BK4819_SetupAircopy(const unsigned int packet_size);
	void BK4819_start_aircopy_fsk_rx(const unsigned int packet_size);
#endif

void     BK4819_reset_fsk(void);
void     BK4819_Idle(void);
void     BK4819_PrepareTransmit(void);
void     BK4819_TxOn_Beep(void);
void     BK4819_ExitSubAu(void);

void     BK4819_Conditional_RX_TurnOn(void);

void     BK4819_EnterDTMF_TX(bool bLocalLoopback);
void     BK4819_ExitDTMF_TX(bool bKeep);
void     BK4819_EnableTXLink(void);

void     BK4819_PlayDTMF(char Code);
void     BK4819_PlayDTMFString(const char *pString, bool bDelayFirst, uint16_t FirstCodePersistTime, uint16_t HashCodePersistTime, uint16_t CodePersistTime, uint16_t CodeInternalTime);

void     BK4819_TransmitTone(bool bLocalLoopback, uint32_t Frequency);

void     BK4819_GenTail(uint8_t Tail);
void     BK4819_EnableCDCSS(void);
void     BK4819_EnableCTCSS(void);

uint16_t BK4819_GetRSSI(void);
uint8_t  BK4819_GetGlitchIndicator(void);
uint8_t  BK4819_GetExNoiceIndicator(void);
uint16_t BK4819_GetVoiceAmplitudeOut(void);
uint8_t  BK4819_GetAfTxRx(void);

bool     BK4819_GetFrequencyScanResult(uint32_t *pFrequency);
BK4819_CSS_scan_result_t BK4819_GetCxCSSScanResult(uint32_t *pCdcssFreq, uint16_t *pCtcssFreq);
void     BK4819_DisableFrequencyScan(void);
void     BK4819_EnableFrequencyScan(void);
void     BK4819_SetScanFrequency(uint32_t Frequency);

void     BK4819_StopScan(void);

uint8_t  BK4819_GetDTMF_5TONE_Code(void);

uint8_t  BK4819_get_CDCSS_code_type(void);
uint8_t  BK4819_GetCTCShift(void);
uint8_t  BK4819_GetCTCType(void);

void     BK4819_PlayRoger(void);

#ifdef ENABLE_MDC1200
	void BK4819_enable_mdc1200_rx(const bool enable);
	void BK4819_send_MDC1200(const uint8_t op, const uint8_t arg, const uint16_t id);
#endif

void     BK4819_Enable_AfDac_DiscMode_TxDsp(void);

void     BK4819_GetVoxAmp(uint16_t *pResult);
void     BK4819_SetScrambleFrequencyControlWord(uint32_t Frequency);
void     BK4819_PlayDTMFEx(bool bLocalLoopback, char Code);

#endif
