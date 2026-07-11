/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * BMS 响应日志:记录故障触发后 BMS 的 CAN 响应与延迟。
 * 由 STM32 HAL 版本移植到 Zephyr —— 返回值改为 int(0=成功, <0=errno),
 * 不再直接依赖 HAL。
 */
#ifndef APP_BMS_RESPONSE_LOG_H_
#define APP_BMS_RESPONSE_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "drivers/bsp_can.h"

typedef enum
{
  BMS_RESPONSE_LOG_TRIGGER_NONE = 0,
  BMS_RESPONSE_LOG_TRIGGER_OV,
  BMS_RESPONSE_LOG_TRIGGER_UV,
  BMS_RESPONSE_LOG_TRIGGER_DIFF
} BmsResponseLog_TriggerType;

typedef struct
{
  uint8_t enabled;
  uint8_t isExtended;
  uint32_t id;
  uint32_t mask;
  uint8_t minLength;
  uint8_t dataMask[BSP_CAN_MAX_DATA_LEN];
  uint8_t dataValue[BSP_CAN_MAX_DATA_LEN];
} BmsResponseLog_Filter;

typedef struct
{
  uint8_t armed;
  BmsResponseLog_Filter filter;
  BmsResponseLog_TriggerType triggerType;
  uint8_t primaryCell;
  uint8_t secondaryCell;
  uint16_t primaryMillivolts;
  uint16_t secondaryMillivolts;
  uint32_t triggerTick;
  uint32_t canFramesAfterTrigger;
  uint32_t filterMissCount;
  uint8_t responseCaptured;
  uint32_t responseTick;
  uint32_t responseDelayMs;
  uint32_t responseCanId;
  uint8_t responseCanIsExtended;
  uint8_t responseCanLength;
  uint8_t responseCanData[BSP_CAN_MAX_DATA_LEN];
} BmsResponseLog_Status;

void BmsResponseLog_RecordFaultTrigger(BmsResponseLog_TriggerType type,
                                       uint8_t primaryCell,
                                       uint16_t primaryMillivolts,
                                       uint8_t secondaryCell,
                                       uint16_t secondaryMillivolts);
void BmsResponseLog_Clear(void);
BmsResponseLog_Status BmsResponseLog_GetStatus(void);
const char *BmsResponseLog_GetTriggerName(BmsResponseLog_TriggerType type);
int BmsResponseLog_SetFilter(uint8_t enabled,
                             uint8_t isExtended,
                             uint32_t id,
                             uint32_t mask);
void BmsResponseLog_DisableFilter(void);
int BmsResponseLog_SetDataFilter(uint8_t index, uint8_t mask, uint8_t value);
void BmsResponseLog_ClearDataFilter(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_BMS_RESPONSE_LOG_H_ */
