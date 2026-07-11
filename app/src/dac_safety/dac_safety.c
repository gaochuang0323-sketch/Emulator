/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * DAC 安全保护实现。由原 STM32 工程移植。
 */
#include "app/dac_safety.h"

#include <errno.h>

#include <zephyr/kernel.h>

#include "drivers/bsp_dac81416.h"
#include "app/waveform_gen.h"

#define DAC_SAFETY_ALARM_POLL_MS 50U

static DacSafety_Status safetyStatus;
static uint32_t lastAlarmPollMs;

static uint8_t DacSafety_IsValidSafeVoltage(uint16_t millivolts)
{
  return ((millivolts >= VOLTAGE_SIM_MIN_MV) && (millivolts <= VOLTAGE_SIM_MAX_MV)) ? 1U : 0U;
}

void DacSafety_Init(uint16_t safeMillivolts)
{
  if (DacSafety_IsValidSafeVoltage(safeMillivolts) == 0U)
  {
    safeMillivolts = DAC_SAFETY_DEFAULT_SAFE_MV;
  }

  safetyStatus.safeMillivolts = safeMillivolts;
  safetyStatus.alarmPinState = DAC_ReadAlarmPin();
  safetyStatus.alarmActive = (safetyStatus.alarmPinState == 0U) ? 1U : 0U;
}

void DacSafety_Process(uint32_t nowMs)
{
  uint8_t alarmPin;
  uint8_t alarmActive;

  if ((nowMs - lastAlarmPollMs) < DAC_SAFETY_ALARM_POLL_MS)
  {
    return;
  }
  lastAlarmPollMs = nowMs;

  alarmPin = DAC_ReadAlarmPin();
  alarmActive = (alarmPin == 0U) ? 1U : 0U;
  safetyStatus.alarmPinState = alarmPin;
  safetyStatus.alarmActive = alarmActive;

  if ((alarmActive != 0U) && (safetyStatus.alarmLatched == 0U))
  {
    safetyStatus.alarmLatched = 1U;
    safetyStatus.alarmCount++;
    safetyStatus.lastAlarmTick = nowMs;
    printk("[safe] DAC alarm asserted, entering emergency stop\r\n");
    (void)DacSafety_EmergencyStop(safetyStatus.safeMillivolts);
  }
}

int DacSafety_EmergencyStop(uint16_t safeMillivolts)
{
  int status;

  if (DacSafety_IsValidSafeVoltage(safeMillivolts) == 0U)
  {
    return -EIO;
  }

  safetyStatus.safeMillivolts = safeMillivolts;
  safetyStatus.emergencyActive = 1U;

  WaveformGen_Stop();
  (void)VoltageSim_ClearAllFaults();
  status = VoltageSim_SetAllCellsVoltageMv(safeMillivolts);
  if (status != 0)
  {
    printk("[safe] emergency stop output update failed\r\n");
    return status;
  }

  printk("[safe] emergency stop active, output=%umV\r\n", safeMillivolts);
  return 0;
}

void DacSafety_Release(void)
{
  safetyStatus.emergencyActive = 0U;
  safetyStatus.alarmLatched = 0U;
  printk("[safe] emergency stop released\r\n");
}

uint8_t DacSafety_IsOutputAllowed(void)
{
  return (safetyStatus.emergencyActive == 0U) ? 1U : 0U;
}

DacSafety_Status DacSafety_GetStatus(void)
{
  return safetyStatus;
}
