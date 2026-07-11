/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 波形生成实现。由原 STM32 工程移植。
 */
#include "app/waveform_gen.h"

#include <errno.h>

#include <zephyr/kernel.h>

#include "app/voltage_sim.h"

#define WAVEFORM_GEN_MIN_PERIOD_MS 2U
#define WAVEFORM_GEN_SINE_TABLE_SIZE 64U

static const int16_t sineTableQ15[WAVEFORM_GEN_SINE_TABLE_SIZE] = {
  0, 3212, 6393, 9512, 12539, 15446, 18204, 20787,
  23170, 25329, 27245, 28898, 30273, 31357, 32138, 32610,
  32767, 32610, 32138, 31357, 30273, 28898, 27245, 25329,
  23170, 20787, 18204, 15446, 12539, 9512, 6393, 3212,
  0, -3212, -6393, -9512, -12539, -15446, -18204, -20787,
  -23170, -25329, -27245, -28898, -30273, -31357, -32138, -32610,
  -32767, -32610, -32138, -31357, -30273, -28898, -27245, -25329,
  -23170, -20787, -18204, -15446, -12539, -9512, -6393, -3212
};

static WaveformGen_Status waveformStatus;
static uint32_t waveformStartMs;
static uint16_t waveformLastOutputMv;

static uint8_t WaveformGen_IsValidCell(uint8_t cell)
{
  return ((cell >= 1U) && (cell <= VOLTAGE_SIM_CELL_COUNT)) ? 1U : 0U;
}

static uint8_t WaveformGen_IsValidVoltage(uint16_t millivolts)
{
  return ((millivolts >= VOLTAGE_SIM_MIN_MV) && (millivolts <= VOLTAGE_SIM_MAX_MV)) ? 1U : 0U;
}

static int WaveformGen_Start(void)
{
  waveformStartMs = k_uptime_get_32();
  waveformLastOutputMv = 0U;
  waveformStatus.active = 1U;
  waveformStatus.lastMillivolts = 0U;

  return 0;
}

int WaveformGen_StartSquare(uint8_t cell,
                            uint16_t lowMillivolts,
                            uint16_t highMillivolts,
                            uint32_t periodMs)
{
  if ((WaveformGen_IsValidCell(cell) == 0U) ||
      (WaveformGen_IsValidVoltage(lowMillivolts) == 0U) ||
      (WaveformGen_IsValidVoltage(highMillivolts) == 0U) ||
      (lowMillivolts >= highMillivolts) ||
      (periodMs < WAVEFORM_GEN_MIN_PERIOD_MS))
  {
    return -EIO;
  }

  waveformStatus.type = WAVEFORM_GEN_SQUARE;
  waveformStatus.cell = cell;
  waveformStatus.lowMillivolts = lowMillivolts;
  waveformStatus.highMillivolts = highMillivolts;
  waveformStatus.offsetMillivolts = 0U;
  waveformStatus.amplitudeMillivolts = 0U;
  waveformStatus.periodMs = periodMs;

  return WaveformGen_Start();
}

int WaveformGen_StartSine(uint8_t cell,
                          uint16_t offsetMillivolts,
                          uint16_t amplitudeMillivolts,
                          uint32_t periodMs)
{
  uint32_t minMv;
  uint32_t maxMv;

  minMv = (offsetMillivolts > amplitudeMillivolts) ?
          ((uint32_t)offsetMillivolts - amplitudeMillivolts) :
          0U;
  maxMv = (uint32_t)offsetMillivolts + amplitudeMillivolts;

  if ((WaveformGen_IsValidCell(cell) == 0U) ||
      (WaveformGen_IsValidVoltage(offsetMillivolts) == 0U) ||
      (amplitudeMillivolts == 0U) ||
      (minMv < VOLTAGE_SIM_MIN_MV) ||
      (maxMv > VOLTAGE_SIM_MAX_MV) ||
      (periodMs < WAVEFORM_GEN_MIN_PERIOD_MS))
  {
    return -EIO;
  }

  waveformStatus.type = WAVEFORM_GEN_SINE;
  waveformStatus.cell = cell;
  waveformStatus.lowMillivolts = 0U;
  waveformStatus.highMillivolts = 0U;
  waveformStatus.offsetMillivolts = offsetMillivolts;
  waveformStatus.amplitudeMillivolts = amplitudeMillivolts;
  waveformStatus.periodMs = periodMs;

  return WaveformGen_Start();
}

int WaveformGen_StartSineAll(uint16_t offsetMillivolts,
                             uint16_t amplitudeMillivolts,
                             uint32_t periodMs)
{
  uint32_t minMv;
  uint32_t maxMv;

  minMv = (offsetMillivolts > amplitudeMillivolts) ?
          ((uint32_t)offsetMillivolts - amplitudeMillivolts) :
          0U;
  maxMv = (uint32_t)offsetMillivolts + amplitudeMillivolts;

  if ((WaveformGen_IsValidVoltage(offsetMillivolts) == 0U) ||
      (amplitudeMillivolts == 0U) ||
      (minMv < VOLTAGE_SIM_MIN_MV) ||
      (maxMv > VOLTAGE_SIM_MAX_MV) ||
      (periodMs < WAVEFORM_GEN_MIN_PERIOD_MS))
  {
    return -EIO;
  }

  waveformStatus.type = WAVEFORM_GEN_SINE_ALL;
  waveformStatus.cell = 0U;
  waveformStatus.lowMillivolts = 0U;
  waveformStatus.highMillivolts = 0U;
  waveformStatus.offsetMillivolts = offsetMillivolts;
  waveformStatus.amplitudeMillivolts = amplitudeMillivolts;
  waveformStatus.periodMs = periodMs;

  return WaveformGen_Start();
}

void WaveformGen_Stop(void)
{
  waveformStatus.active = 0U;
  waveformStatus.type = WAVEFORM_GEN_NONE;
}

int WaveformGen_Process(uint32_t nowMs)
{
  uint32_t phaseMs;
  uint16_t outputMv;

  if (waveformStatus.active == 0U)
  {
    return 0;
  }

  phaseMs = (nowMs - waveformStartMs) % waveformStatus.periodMs;

  if (waveformStatus.type == WAVEFORM_GEN_SQUARE)
  {
    outputMv = (phaseMs < (waveformStatus.periodMs / 2U)) ?
               waveformStatus.lowMillivolts :
               waveformStatus.highMillivolts;
  }
  else if ((waveformStatus.type == WAVEFORM_GEN_SINE) ||
           (waveformStatus.type == WAVEFORM_GEN_SINE_ALL))
  {
    uint32_t tableIndex = (phaseMs * WAVEFORM_GEN_SINE_TABLE_SIZE) / waveformStatus.periodMs;
    int32_t scaled = ((int32_t)waveformStatus.amplitudeMillivolts *
                      sineTableQ15[tableIndex]) / 32767;

    outputMv = (uint16_t)((int32_t)waveformStatus.offsetMillivolts + scaled);
  }
  else
  {
    return 0;
  }

  if (outputMv == waveformLastOutputMv)
  {
    return 0;
  }

  if (waveformStatus.type == WAVEFORM_GEN_SINE_ALL)
  {
    if (VoltageSim_SetAllCellsVoltageMv(outputMv) != 0)
    {
      WaveformGen_Stop();
      return -EIO;
    }
  }
  else if (VoltageSim_SetCellVoltageMv(waveformStatus.cell, outputMv) != 0)
  {
    WaveformGen_Stop();
    return -EIO;
  }

  waveformLastOutputMv = outputMv;
  waveformStatus.lastMillivolts = outputMv;
  return 0;
}

WaveformGen_Status WaveformGen_GetStatus(void)
{
  return waveformStatus;
}

const char *WaveformGen_GetTypeName(WaveformGen_Type type)
{
  switch (type)
  {
    case WAVEFORM_GEN_SQUARE:
      return "square";

    case WAVEFORM_GEN_SINE:
      return "sine";

    case WAVEFORM_GEN_SINE_ALL:
      return "sine_all";

    case WAVEFORM_GEN_NONE:
    default:
      return "none";
  }
}
