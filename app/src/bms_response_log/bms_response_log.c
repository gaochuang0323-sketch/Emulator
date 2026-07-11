/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * BMS 响应日志实现。由原 STM32 工程移植。
 */
#include "app/bms_response_log.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>

#include "drivers/bsp_can.h"

static BmsResponseLog_Status responseLog;
static BmsResponseLog_Filter responseFilter;

void BmsResponseLog_RecordFaultTrigger(BmsResponseLog_TriggerType type,
                                       uint8_t primaryCell,
                                       uint16_t primaryMillivolts,
                                       uint8_t secondaryCell,
                                       uint16_t secondaryMillivolts)
{
  memset(&responseLog, 0, sizeof(responseLog));

  responseLog.filter = responseFilter;
  responseLog.armed = 1U;
  responseLog.triggerType = type;
  responseLog.primaryCell = primaryCell;
  responseLog.secondaryCell = secondaryCell;
  responseLog.primaryMillivolts = primaryMillivolts;
  responseLog.secondaryMillivolts = secondaryMillivolts;
  responseLog.triggerTick = k_uptime_get_32();
}

void BmsResponseLog_Clear(void)
{
  BmsResponseLog_Filter filter = responseFilter;

  memset(&responseLog, 0, sizeof(responseLog));
  responseLog.filter = filter;
}

BmsResponseLog_Status BmsResponseLog_GetStatus(void)
{
  return responseLog;
}

const char *BmsResponseLog_GetTriggerName(BmsResponseLog_TriggerType type)
{
  switch (type)
  {
    case BMS_RESPONSE_LOG_TRIGGER_OV:
      return "OV";

    case BMS_RESPONSE_LOG_TRIGGER_UV:
      return "UV";

    case BMS_RESPONSE_LOG_TRIGGER_DIFF:
      return "DIFF";

    case BMS_RESPONSE_LOG_TRIGGER_NONE:
    default:
      return "NONE";
  }
}

int BmsResponseLog_SetFilter(uint8_t enabled,
                             uint8_t isExtended,
                             uint32_t id,
                             uint32_t mask)
{
  if (((isExtended == 0U) && ((id > 0x7FFUL) || (mask > 0x7FFUL))) ||
      ((isExtended != 0U) && ((id > 0x1FFFFFFFUL) || (mask > 0x1FFFFFFFUL))))
  {
    return -EIO;
  }

  responseFilter.enabled = enabled;
  responseFilter.isExtended = (isExtended != 0U) ? 1U : 0U;
  responseFilter.id = id;
  responseFilter.mask = mask;
  responseLog.filter = responseFilter;
  return 0;
}

void BmsResponseLog_DisableFilter(void)
{
  responseFilter.enabled = 0U;
  responseLog.filter = responseFilter;
}

int BmsResponseLog_SetDataFilter(uint8_t index, uint8_t mask, uint8_t value)
{
  if (index >= BSP_CAN_MAX_DATA_LEN)
  {
    return -EIO;
  }

  responseFilter.dataMask[index] = mask;
  responseFilter.dataValue[index] = value;
  if ((mask != 0U) && ((index + 1U) > responseFilter.minLength))
  {
    responseFilter.minLength = index + 1U;
  }
  responseLog.filter = responseFilter;
  return 0;
}

void BmsResponseLog_ClearDataFilter(void)
{
  memset(responseFilter.dataMask, 0, sizeof(responseFilter.dataMask));
  memset(responseFilter.dataValue, 0, sizeof(responseFilter.dataValue));
  responseFilter.minLength = 0U;
  responseLog.filter = responseFilter;
}

static uint8_t BmsResponseLog_FrameMatchesFilter(const BspCan_RxFrame *frame)
{
  const BmsResponseLog_Filter *filter = &responseLog.filter;

  if (filter->enabled == 0U)
  {
    return 1U;
  }

  if (frame->isExtended != filter->isExtended)
  {
    return 0U;
  }

  if ((frame->id & filter->mask) != (filter->id & filter->mask))
  {
    return 0U;
  }

  if (frame->length < filter->minLength)
  {
    return 0U;
  }

  for (uint8_t index = 0U; index < BSP_CAN_MAX_DATA_LEN; index++)
  {
    if ((filter->dataMask[index] != 0U) &&
        ((frame->data[index] & filter->dataMask[index]) !=
         (filter->dataValue[index] & filter->dataMask[index])))
    {
      return 0U;
    }
  }

  return 1U;
}

void BspCan_OnRxFrame(const BspCan_RxFrame *frame)
{
  if ((frame == NULL) || (responseLog.armed == 0U))
  {
    return;
  }

  responseLog.canFramesAfterTrigger++;

  if (responseLog.responseCaptured != 0U)
  {
    return;
  }

  if (BmsResponseLog_FrameMatchesFilter(frame) == 0U)
  {
    responseLog.filterMissCount++;
    return;
  }

  responseLog.responseCaptured = 1U;
  responseLog.responseTick = frame->tick;
  responseLog.responseDelayMs = frame->tick - responseLog.triggerTick;
  responseLog.responseCanId = frame->id;
  responseLog.responseCanIsExtended = frame->isExtended;
  responseLog.responseCanLength = frame->length;
  memcpy(responseLog.responseCanData, frame->data, frame->length);
}
