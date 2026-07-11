/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * DAC 安全保护:紧急停机与报警管理。
 * 由 STM32 HAL 版本移植到 Zephyr —— 返回值改为 int(0=成功, <0=errno),
 * 不再直接依赖 HAL。
 */
#ifndef APP_DAC_SAFETY_H_
#define APP_DAC_SAFETY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "app/voltage_sim.h"

#define DAC_SAFETY_DEFAULT_SAFE_MV VOLTAGE_SIM_DEFAULT_NORMAL_MV

typedef struct
{
  uint8_t emergencyActive;
  uint8_t alarmPinState;
  uint8_t alarmActive;
  uint8_t alarmLatched;
  uint32_t alarmCount;
  uint32_t lastAlarmTick;
  uint16_t safeMillivolts;
} DacSafety_Status;

void DacSafety_Init(uint16_t safeMillivolts);
void DacSafety_Process(uint32_t nowMs);
int DacSafety_EmergencyStop(uint16_t safeMillivolts);
void DacSafety_Release(void);
uint8_t DacSafety_IsOutputAllowed(void);
DacSafety_Status DacSafety_GetStatus(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DAC_SAFETY_H_ */
