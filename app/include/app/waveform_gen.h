/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 波形生成:方波/正弦波逐周期电压输出。
 * 由 STM32 HAL 版本移植到 Zephyr —— 返回值改为 int(0=成功, <0=errno),
 * 不再直接依赖 HAL。
 */
#ifndef APP_WAVEFORM_GEN_H_
#define APP_WAVEFORM_GEN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  WAVEFORM_GEN_NONE = 0,
  WAVEFORM_GEN_SQUARE,
  WAVEFORM_GEN_SINE,
  WAVEFORM_GEN_SINE_ALL
} WaveformGen_Type;

typedef struct
{
  WaveformGen_Type type;
  uint8_t active;
  uint8_t cell;
  uint16_t lowMillivolts;
  uint16_t highMillivolts;
  uint16_t offsetMillivolts;
  uint16_t amplitudeMillivolts;
  uint32_t periodMs;
  uint16_t lastMillivolts;
} WaveformGen_Status;

int WaveformGen_StartSquare(uint8_t cell,
			    uint16_t lowMillivolts,
			    uint16_t highMillivolts,
			    uint32_t periodMs);
int WaveformGen_StartSine(uint8_t cell,
			  uint16_t offsetMillivolts,
			  uint16_t amplitudeMillivolts,
			  uint32_t periodMs);
int WaveformGen_StartSineAll(uint16_t offsetMillivolts,
			     uint16_t amplitudeMillivolts,
			     uint32_t periodMs);
void WaveformGen_Stop(void);
int WaveformGen_Process(uint32_t nowMs);
WaveformGen_Status WaveformGen_GetStatus(void);
const char *WaveformGen_GetTypeName(WaveformGen_Type type);

#ifdef __cplusplus
}
#endif

#endif /* APP_WAVEFORM_GEN_H_ */
