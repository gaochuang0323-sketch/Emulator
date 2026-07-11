#include "app/fault_command.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "app/bms_response_log.h"
#include "drivers/bsp_can.h"
#include "drivers/bsp_sram.h"
#include "app/dac_safety.h"
#include "app/voltage_sim.h"
#include "app/waveform_engine.h"
#include "app/waveform_gen.h"

#define FAULT_COMMAND_DEFAULT_DURATION_MS 100U
#define FAULT_COMMAND_DEFAULT_SLEW_MV_PER_MS 3000U

static void FaultCommand_Write(FaultCommand_WriteFn write, void *context, const char *text)
{
  if ((write != NULL) && (text != NULL))
  {
    write(text, context);
  }
}

static void FaultCommand_WriteFormat(FaultCommand_WriteFn write,
                                     void *context,
                                     const char *format,
                                     ...)
{
  char buffer[512];
  va_list args;

  va_start(args, format);
  (void)vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  FaultCommand_Write(write, context, buffer);
}

static const char *FaultCommand_SkipSpaces(const char *text)
{
  while ((text != NULL) && (*text == ' '))
  {
    text++;
  }

  return text;
}

static void FaultCommand_WriteTextHelp(FaultCommand_WriteFn write, void *context)
{
  FaultCommand_Write(write, context, "\r\n[cmd] help\r\n");
  FaultCommand_Write(write, context, "  help\r\n");
  FaultCommand_Write(write, context, "  status\r\n");
  FaultCommand_Write(write, context, "  normal [mv]\r\n");
  FaultCommand_Write(write, context, "  set <cell> <mv>\r\n");
  FaultCommand_Write(write, context, "  ov <cell> [mv] [duration_ms] [slew_mv_per_ms]\r\n");
  FaultCommand_Write(write, context, "  uv <cell> [mv] [duration_ms] [slew_mv_per_ms]\r\n");
  FaultCommand_Write(write, context, "  diff <high_cell> <high_mv> <low_cell> <low_mv> [duration_ms] [slew_mv_per_ms]\r\n");
  FaultCommand_Write(write, context, "  wave status\r\n");
  FaultCommand_Write(write, context, "  wave stop\r\n");
  FaultCommand_Write(write, context, "  wave square <cell> <low_mv> <high_mv> <period_ms>\r\n");
  FaultCommand_Write(write, context, "  wave sine <cell> <offset_mv> <amplitude_mv> <period_ms>\r\n");
  FaultCommand_Write(write, context, "  wave sine all <offset_mv> <amplitude_mv> <period_ms>\r\n");
  FaultCommand_Write(write, context, "  wave high <cell> <freq_hz> <points> <amp_mv> <offset_mv>\r\n");
  FaultCommand_Write(write, context, "  cal status\r\n");
  FaultCommand_Write(write, context, "  cal set <cell> <offset_mv>\r\n");
  FaultCommand_Write(write, context, "  cal trim <cell> <target_mv> <measured_mv>\r\n");
  FaultCommand_Write(write, context, "  cal clear [cell]\r\n");
  FaultCommand_Write(write, context, "  test static <mv>\r\n");
  FaultCommand_Write(write, context, "  test slew <cell> <low_mv> <high_mv> <period_ms>\r\n");
  FaultCommand_Write(write, context, "  can status\r\n");
  FaultCommand_Write(write, context, "  can send <std_id_hex> <len> [byte_hex ...]\r\n");
  FaultCommand_Write(write, context, "  log status\r\n");
  FaultCommand_Write(write, context, "  log clear\r\n");
  FaultCommand_Write(write, context, "  log filter <std|ext> <id_hex> [mask_hex]\r\n");
  FaultCommand_Write(write, context, "  log filter off\r\n");
  FaultCommand_Write(write, context, "  log data <index> <mask_hex> <value_hex>\r\n");
  FaultCommand_Write(write, context, "  log data clear\r\n");
  FaultCommand_Write(write, context, "  safe status\r\n");
  FaultCommand_Write(write, context, "  safe stop [safe_mv]\r\n");
  FaultCommand_Write(write, context, "  safe release\r\n");
  FaultCommand_Write(write, context, "  emergency stop [safe_mv]\r\n");
  FaultCommand_Write(write, context, "  sram status\r\n");
  FaultCommand_Write(write, context, "  sram test [bytes]\r\n");
  FaultCommand_Write(write, context, "  clear [cell]\r\n");
  FaultCommand_Write(write, context, "  JSON examples: {\"cmd\":\"status\"}, {\"cmd\":\"cal\",\"action\":\"trim\",\"cell\":1,\"target_mv\":2500,\"measured_mv\":2485}\r\n");
}

static void FaultCommand_WriteTextWaveStatus(FaultCommand_WriteFn write, void *context)
{
  WaveformGen_Status status = WaveformGen_GetStatus();
  WaveformEngine_Status highStatus = WaveformEngine_GetStatus();

  FaultCommand_WriteFormat(write,
                           context,
                           "[wave] active=%u type=%s cell=%u period=%lums last=%umV\r\n",
                           status.active,
                           WaveformGen_GetTypeName(status.type),
                           status.cell,
                           (unsigned long)status.periodMs,
                           status.lastMillivolts);
  FaultCommand_WriteFormat(write,
                           context,
                           "[wave-high] running=%u buffer=%u samples=%lu errors=%lu\r\n",
                           highStatus.running,
                           highStatus.bufferIndex,
                           (unsigned long)highStatus.sampleCount,
                           (unsigned long)highStatus.errorCount);
}

static void FaultCommand_StopAllWaveforms(void)
{
  WaveformEngine_Stop();
  WaveformGen_Stop();
}

static void FaultCommand_WriteTextCanStatus(FaultCommand_WriteFn write, void *context)
{
  BspCan_Status status = BspCan_GetStatus();

  FaultCommand_WriteFormat(write,
                           context,
                           "\r\n[can] started=%u tx=%lu rx=%lu err=%lu last_id=0x%lX last_len=%u last_tick=%lu last_data=",
                           status.started,
                           (unsigned long)status.txCount,
                           (unsigned long)status.rxCount,
                           (unsigned long)status.errorCount,
                           (unsigned long)status.lastRxId,
                           status.lastRxLength,
                           (unsigned long)status.lastRxTick);

  for (uint8_t index = 0U; index < status.lastRxLength; index++)
  {
    FaultCommand_WriteFormat(write,
                             context,
                             "%02X%s",
                             status.lastRxData[index],
                             (index + 1U == status.lastRxLength) ? "" : " ");
  }
  FaultCommand_Write(write, context, "\r\n");
}

static void FaultCommand_WriteTextBmsResponseLog(FaultCommand_WriteFn write, void *context)
{
  BmsResponseLog_Status status = BmsResponseLog_GetStatus();

  FaultCommand_WriteFormat(write,
                           context,
                           "\r\n[log] armed=%u trigger=%s t1=%lums primary=C%02u/%umV secondary=C%02u/%umV can_after=%lu filter_miss=%lu captured=%u",
                           status.armed,
                           BmsResponseLog_GetTriggerName(status.triggerType),
                           (unsigned long)status.triggerTick,
                           status.primaryCell,
                           status.primaryMillivolts,
                           status.secondaryCell,
                           status.secondaryMillivolts,
                           (unsigned long)status.canFramesAfterTrigger,
                           (unsigned long)status.filterMissCount,
                           status.responseCaptured);

  FaultCommand_WriteFormat(write,
                           context,
                           "\r\n[log] filter enabled=%u type=%s id=0x%lX mask=0x%lX min_len=%u data=",
                           status.filter.enabled,
                           status.filter.isExtended ? "ext" : "std",
                           (unsigned long)status.filter.id,
                           (unsigned long)status.filter.mask,
                           status.filter.minLength);
  for (uint8_t index = 0U; index < BSP_CAN_MAX_DATA_LEN; index++)
  {
    FaultCommand_WriteFormat(write,
                             context,
                             "%s%02X/%02X",
                             (index == 0U) ? "" : " ",
                             status.filter.dataValue[index],
                             status.filter.dataMask[index]);
  }

  if (status.responseCaptured != 0U)
  {
    FaultCommand_WriteFormat(write,
                             context,
                             " t2=%lums delay=%lums id=0x%lX ext=%u len=%u data=",
                             (unsigned long)status.responseTick,
                             (unsigned long)status.responseDelayMs,
                             (unsigned long)status.responseCanId,
                             status.responseCanIsExtended,
                             status.responseCanLength);
    for (uint8_t index = 0U; index < status.responseCanLength; index++)
    {
      FaultCommand_WriteFormat(write,
                               context,
                               "%02X%s",
                               status.responseCanData[index],
                               (index + 1U == status.responseCanLength) ? "" : " ");
    }
  }

  FaultCommand_Write(write, context, "\r\n");
}

static void FaultCommand_WriteTextSafetyStatus(FaultCommand_WriteFn write, void *context)
{
  DacSafety_Status status = DacSafety_GetStatus();

  FaultCommand_WriteFormat(write,
                           context,
                           "\r\n[safe] emergency=%u safe_mv=%u alarm_pin=%u alarm_active=%u alarm_latched=%u alarm_count=%lu last_alarm_tick=%lu\r\n",
                           status.emergencyActive,
                           status.safeMillivolts,
                           status.alarmPinState,
                           status.alarmActive,
                           status.alarmLatched,
                           (unsigned long)status.alarmCount,
                           (unsigned long)status.lastAlarmTick);
}

static void FaultCommand_WriteTextSramStatus(FaultCommand_WriteFn write, void *context)
{
  BspSram_Status status = BspSram_GetStatus();

  FaultCommand_WriteFormat(write,
                           context,
                           "\r\n[sram] base=0x%08lX size=%lubytes bus=%u-bit last=%s test_bytes=%lu pass=%lu fail=%lu tick=%lu",
                           (unsigned long)status.baseAddress,
                           (unsigned long)status.sizeBytes,
                           status.busWidthBits,
                           BspSram_GetResultName(status.lastResult),
                           (unsigned long)status.lastTestBytes,
                           (unsigned long)status.passCount,
                           (unsigned long)status.failCount,
                           (unsigned long)status.lastTestTick);

  if (status.lastResult == -EIO)
  {
    FaultCommand_WriteFormat(write,
                             context,
                             " fail_addr=0x%08lX expected=0x%04X actual=0x%04X",
                             (unsigned long)status.lastFailedAddress,
                             status.expected,
                             status.actual);
  }

  FaultCommand_Write(write, context, "\r\n");
}

static void FaultCommand_WriteTextStatus(FaultCommand_WriteFn write, void *context)
{
  DacSafety_Status safety = DacSafety_GetStatus();

  FaultCommand_WriteFormat(write, context, "\r\n[status] active_faults=%u\r\n",
                           VoltageSim_GetActiveFaultCount());
  FaultCommand_WriteFormat(write,
                           context,
                           "  safety emergency=%u safe=%umV alarm_active=%u alarm_latched=%u alarm_count=%lu\r\n",
                           safety.emergencyActive,
                           safety.safeMillivolts,
                           safety.alarmActive,
                           safety.alarmLatched,
                           (unsigned long)safety.alarmCount);

  for (uint8_t cell = 1U; cell <= VOLTAGE_SIM_CELL_COUNT; cell++)
  {
    VoltageSim_CellFaultInfo info = VoltageSim_GetCellFaultInfo(cell);

    FaultCommand_WriteFormat(write, context, "  C%02u=%umV",
                             cell,
                             VoltageSim_GetLastCellVoltageMv(cell));
    if (VoltageSim_GetCellCalibrationOffsetMv(cell) != 0)
    {
      FaultCommand_WriteFormat(write,
                               context,
                               " cal=%dmV",
                               (int)VoltageSim_GetCellCalibrationOffsetMv(cell));
    }
    if (info.type != VOLTAGE_SIM_FAULT_NONE)
    {
      FaultCommand_WriteFormat(write,
                               context,
                               " fault=%s target=%umV restore=%umV duration=%lums slew=%umV/ms",
                               VoltageSim_GetFaultTypeName(info.type),
                               info.targetMillivolts,
                               info.restoreMillivolts,
                               (unsigned long)info.durationMs,
                               info.slewRateMvPerMs);
    }
    FaultCommand_Write(write, context, "\r\n");
  }
}

static void FaultCommand_WriteTextCalibrationStatus(FaultCommand_WriteFn write, void *context)
{
  FaultCommand_Write(write, context, "\r\n[cal]\r\n");

  for (uint8_t cell = 1U; cell <= VOLTAGE_SIM_CELL_COUNT; cell++)
  {
    FaultCommand_WriteFormat(write,
                             context,
                             "  C%02u offset=%dmV\r\n",
                             cell,
                             (int)VoltageSim_GetCellCalibrationOffsetMv(cell));
  }
}

static void FaultCommand_WriteTextResult(FaultCommand_WriteFn write,
                                         void *context,
                                         const char *name,
                                         int status)
{
  FaultCommand_WriteFormat(write, context, "[cmd] %s %s\r\n",
                           name,
                           (status == 0) ? "OK" : "ERR");
}

static void FaultCommand_WriteJsonResult(FaultCommand_WriteFn write,
                                         void *context,
                                         const char *cmd,
                                         int status)
{
  FaultCommand_WriteFormat(write, context, "{\"ok\":%s,\"cmd\":\"%s\"}\r\n",
                           (status == 0) ? "true" : "false",
                           cmd);
}

static void FaultCommand_WriteJsonStatus(FaultCommand_WriteFn write, void *context)
{
  WaveformGen_Status waveform = WaveformGen_GetStatus();
  DacSafety_Status safety = DacSafety_GetStatus();

  FaultCommand_WriteFormat(write,
                           context,
                           "{\"ok\":true,\"active_faults\":%u,\"safety\":{\"emergency\":%u,\"safe_mv\":%u,\"alarm_active\":%u,\"alarm_latched\":%u,\"alarm_count\":%lu},\"waveform\":{\"active\":%u,\"type\":\"%s\",\"cell\":%u,\"period_ms\":%lu,\"last_mv\":%u},\"cells\":[",
                           VoltageSim_GetActiveFaultCount(),
                           safety.emergencyActive,
                           safety.safeMillivolts,
                           safety.alarmActive,
                           safety.alarmLatched,
                           (unsigned long)safety.alarmCount,
                           waveform.active,
                           WaveformGen_GetTypeName(waveform.type),
                           waveform.cell,
                           (unsigned long)waveform.periodMs,
                           waveform.lastMillivolts);

  for (uint8_t cell = 1U; cell <= VOLTAGE_SIM_CELL_COUNT; cell++)
  {
    VoltageSim_CellFaultInfo info = VoltageSim_GetCellFaultInfo(cell);

    FaultCommand_WriteFormat(write,
                             context,
                             "%s{\"cell\":%u,\"mv\":%u,\"cal_mv\":%d,\"fault\":\"%s\",\"target_mv\":%u,\"restore_mv\":%u,\"duration_ms\":%lu,\"slew_mv_per_ms\":%u}",
                             (cell == 1U) ? "" : ",",
                             cell,
                             VoltageSim_GetLastCellVoltageMv(cell),
                             (int)VoltageSim_GetCellCalibrationOffsetMv(cell),
                             VoltageSim_GetFaultTypeName(info.type),
                             info.targetMillivolts,
                             info.restoreMillivolts,
                             (unsigned long)info.durationMs,
                             info.slewRateMvPerMs);
  }

  FaultCommand_Write(write, context, "]}\r\n");
}

static void FaultCommand_WriteJsonCanStatus(FaultCommand_WriteFn write, void *context)
{
  BspCan_Status status = BspCan_GetStatus();

  FaultCommand_WriteFormat(write,
                           context,
                           "{\"ok\":true,\"can\":{\"started\":%u,\"tx\":%lu,\"rx\":%lu,\"err\":%lu,\"last_id\":%lu,\"last_id_hex\":\"0x%lX\",\"last_ext\":%u,\"last_len\":%u,\"last_tick\":%lu,\"last_data\":[",
                           status.started,
                           (unsigned long)status.txCount,
                           (unsigned long)status.rxCount,
                           (unsigned long)status.errorCount,
                           (unsigned long)status.lastRxId,
                           (unsigned long)status.lastRxId,
                           status.lastRxIsExtended,
                           status.lastRxLength,
                           (unsigned long)status.lastRxTick);

  for (uint8_t index = 0U; index < status.lastRxLength; index++)
  {
    FaultCommand_WriteFormat(write,
                             context,
                             "%s%u",
                             (index == 0U) ? "" : ",",
                             status.lastRxData[index]);
  }
  FaultCommand_Write(write, context, "]}}\r\n");
}

static void FaultCommand_WriteJsonBmsResponseLog(FaultCommand_WriteFn write, void *context)
{
  BmsResponseLog_Status status = BmsResponseLog_GetStatus();

  FaultCommand_WriteFormat(write,
                           context,
                           "{\"ok\":true,\"log\":{\"armed\":%u,\"trigger\":\"%s\",\"trigger_tick\":%lu,",
                           status.armed,
                           BmsResponseLog_GetTriggerName(status.triggerType),
                           (unsigned long)status.triggerTick);
  FaultCommand_WriteFormat(write,
                           context,
                           "\"primary_cell\":%u,\"primary_mv\":%u,\"secondary_cell\":%u,\"secondary_mv\":%u,",
                           status.primaryCell,
                           status.primaryMillivolts,
                           status.secondaryCell,
                           status.secondaryMillivolts);
  FaultCommand_WriteFormat(write,
                           context,
                           "\"can_after\":%lu,\"filter_miss\":%lu,\"captured\":%u,",
                           (unsigned long)status.canFramesAfterTrigger,
                           (unsigned long)status.filterMissCount,
                           status.responseCaptured);
  FaultCommand_WriteFormat(write,
                           context,
                           "\"filter\":{\"enabled\":%u,\"extended\":%u,\"id\":%lu,\"id_hex\":\"0x%lX\",",
                           status.filter.enabled,
                           status.filter.isExtended,
                           (unsigned long)status.filter.id,
                           (unsigned long)status.filter.id);
  FaultCommand_WriteFormat(write,
                           context,
                           "\"mask\":%lu,\"mask_hex\":\"0x%lX\",\"min_len\":%u},",
                           (unsigned long)status.filter.mask,
                           (unsigned long)status.filter.mask,
                           status.filter.minLength);
  FaultCommand_WriteFormat(write,
                           context,
                           "\"response_tick\":%lu,\"delay_ms\":%lu,\"response_id\":%lu,\"response_id_hex\":\"0x%lX\",",
                           (unsigned long)status.responseTick,
                           (unsigned long)status.responseDelayMs,
                           (unsigned long)status.responseCanId,
                           (unsigned long)status.responseCanId);
  FaultCommand_WriteFormat(write,
                           context,
                           "\"response_ext\":%u,\"response_len\":%u,\"response_data\":[",
                           status.responseCanIsExtended,
                           status.responseCanLength);

  for (uint8_t index = 0U; index < status.responseCanLength; index++)
  {
    FaultCommand_WriteFormat(write,
                             context,
                             "%s%u",
                             (index == 0U) ? "" : ",",
                             status.responseCanData[index]);
  }
  FaultCommand_Write(write, context, "]}}\r\n");
}

static void FaultCommand_WriteJsonSafetyStatus(FaultCommand_WriteFn write, void *context)
{
  DacSafety_Status status = DacSafety_GetStatus();

  FaultCommand_WriteFormat(write,
                           context,
                           "{\"ok\":true,\"safe\":{\"emergency\":%u,\"safe_mv\":%u,\"alarm_pin\":%u,\"alarm_active\":%u,\"alarm_latched\":%u,\"alarm_count\":%lu,\"last_alarm_tick\":%lu}}\r\n",
                           status.emergencyActive,
                           status.safeMillivolts,
                           status.alarmPinState,
                           status.alarmActive,
                           status.alarmLatched,
                           (unsigned long)status.alarmCount,
                           (unsigned long)status.lastAlarmTick);
}

static void FaultCommand_WriteJsonSramStatus(FaultCommand_WriteFn write,
                                             void *context,
                                             int commandResult)
{
  BspSram_Status status = BspSram_GetStatus();

  FaultCommand_WriteFormat(write,
                           context,
                           "{\"ok\":%s,\"sram\":{\"base\":%lu,\"base_hex\":\"0x%08lX\",",
                           (commandResult == 0) ? "true" : "false",
                           (unsigned long)status.baseAddress,
                           (unsigned long)status.baseAddress);
  FaultCommand_WriteFormat(write,
                           context,
                           "\"size_bytes\":%lu,\"bus_width_bits\":%u,\"last\":\"%s\",",
                           (unsigned long)status.sizeBytes,
                           status.busWidthBits,
                           BspSram_GetResultName(status.lastResult));
  FaultCommand_WriteFormat(write,
                           context,
                           "\"test_bytes\":%lu,\"pass\":%lu,\"fail\":%lu,\"tick\":%lu,",
                           (unsigned long)status.lastTestBytes,
                           (unsigned long)status.passCount,
                           (unsigned long)status.failCount,
                           (unsigned long)status.lastTestTick);
  FaultCommand_WriteFormat(write,
                           context,
                           "\"fail_addr\":%lu,\"fail_addr_hex\":\"0x%08lX\",",
                           (unsigned long)status.lastFailedAddress,
                           (unsigned long)status.lastFailedAddress);
  FaultCommand_WriteFormat(write,
                           context,
                           "\"expected\":%u,\"actual\":%u}}\r\n",
                           status.expected,
                           status.actual);
}

static int FaultCommand_RunNormal(uint16_t millivolts)
{
  if (DacSafety_IsOutputAllowed() == 0U)
  {
    return -EBUSY;
  }

  FaultCommand_StopAllWaveforms();
  return VoltageSim_SetAllCellsVoltageMv(millivolts);
}

static int FaultCommand_RunSet(uint8_t cell, uint16_t millivolts)
{
  if (DacSafety_IsOutputAllowed() == 0U)
  {
    return -EBUSY;
  }

  FaultCommand_StopAllWaveforms();
  return VoltageSim_SetCellVoltageMv(cell, millivolts);
}

static int FaultCommand_RunCalibrationTrim(uint8_t cell,
                                                         uint16_t targetMillivolts,
                                                         uint16_t measuredMillivolts)
{
  int32_t offset;

  offset = (int32_t)VoltageSim_GetCellCalibrationOffsetMv(cell) +
           (int32_t)targetMillivolts -
           (int32_t)measuredMillivolts;

  if ((offset < -VOLTAGE_SIM_CAL_OFFSET_LIMIT_MV) ||
      (offset > VOLTAGE_SIM_CAL_OFFSET_LIMIT_MV))
  {
    return -EIO;
  }

  return VoltageSim_SetCellCalibrationOffsetMv(cell, (int16_t)offset);
}

static int FaultCommand_RunOverVoltage(uint8_t cell,
                                                     uint16_t target,
                                                     uint32_t duration,
                                                     uint16_t slew)
{
  int status;

  if (DacSafety_IsOutputAllowed() == 0U)
  {
    return -EBUSY;
  }

  FaultCommand_StopAllWaveforms();
  status = VoltageSim_InjectCellOverVoltageRamp(cell, target, duration, slew);
  if (status == 0)
  {
    BmsResponseLog_RecordFaultTrigger(BMS_RESPONSE_LOG_TRIGGER_OV, cell, target, 0U, 0U);
  }

  return status;
}

static int FaultCommand_RunUnderVoltage(uint8_t cell,
                                                      uint16_t target,
                                                      uint32_t duration,
                                                      uint16_t slew)
{
  int status;

  if (DacSafety_IsOutputAllowed() == 0U)
  {
    return -EBUSY;
  }

  FaultCommand_StopAllWaveforms();
  status = VoltageSim_InjectCellUnderVoltageRamp(cell, target, duration, slew);
  if (status == 0)
  {
    BmsResponseLog_RecordFaultTrigger(BMS_RESPONSE_LOG_TRIGGER_UV, cell, target, 0U, 0U);
  }

  return status;
}

static int FaultCommand_RunDiff(uint8_t highCell,
                                              uint16_t highMv,
                                              uint8_t lowCell,
                                              uint16_t lowMv,
                                              uint32_t duration,
                                              uint16_t slew)
{
  int status;

  if (DacSafety_IsOutputAllowed() == 0U)
  {
    return -EBUSY;
  }

  FaultCommand_StopAllWaveforms();
  status = VoltageSim_InjectVoltageDifference(highCell, highMv, lowCell, lowMv, duration, slew);
  if (status == 0)
  {
    BmsResponseLog_RecordFaultTrigger(BMS_RESPONSE_LOG_TRIGGER_DIFF,
                                      highCell,
                                      highMv,
                                      lowCell,
                                      lowMv);
  }

  return status;
}

static int FaultCommand_ParseCanSendText(const char *line)
{
  unsigned long id;
  unsigned int length;
  int consumed = 0;
  const char *cursor;
  uint8_t data[BSP_CAN_MAX_DATA_LEN] = {0};

  if (sscanf(line, "can send %lx %u %n", &id, &length, &consumed) < 2)
  {
    return -EIO;
  }

  if ((id > 0x7FFUL) || (length > BSP_CAN_MAX_DATA_LEN))
  {
    return -EIO;
  }

  cursor = line + consumed;
  for (uint8_t index = 0U; index < length; index++)
  {
    char *endPtr;
    unsigned long value;

    cursor = FaultCommand_SkipSpaces(cursor);
    if ((cursor == NULL) || (*cursor == '\0'))
    {
      return -EIO;
    }

    value = strtoul(cursor, &endPtr, 16);
    if ((endPtr == cursor) || (value > 0xFFUL))
    {
      return -EIO;
    }

    data[index] = (uint8_t)value;
    cursor = endPtr;
  }

  return BspCan_SendClassic((uint32_t)id, 0U, data, (uint8_t)length);
}

static int FaultCommand_ParseLogFilterText(const char *line)
{
  char type[8];
  unsigned long id;
  unsigned long mask;
  int parsed;
  uint8_t isExtended;

  if (strcmp(line, "log filter off") == 0)
  {
    BmsResponseLog_DisableFilter();
    return 0;
  }

  mask = 0x7FFUL;
  parsed = sscanf(line, "log filter %7s %lx %lx", type, &id, &mask);
  if (parsed < 2)
  {
    return -EIO;
  }

  if (strcmp(type, "std") == 0)
  {
    isExtended = 0U;
  }
  else if (strcmp(type, "ext") == 0)
  {
    isExtended = 1U;
    if (parsed == 2)
    {
      mask = 0x1FFFFFFFUL;
    }
  }
  else
  {
    return -EIO;
  }

  return BmsResponseLog_SetFilter(1U, isExtended, (uint32_t)id, (uint32_t)mask);
}

static int FaultCommand_ParseLogDataText(const char *line)
{
  unsigned int index;
  unsigned int mask;
  unsigned int value;

  if (strcmp(line, "log data clear") == 0)
  {
    BmsResponseLog_ClearDataFilter();
    return 0;
  }

  if ((sscanf(line, "log data %u %x %x", &index, &mask, &value) != 3) ||
      (index >= BSP_CAN_MAX_DATA_LEN) ||
      (mask > 0xFFU) ||
      (value > 0xFFU))
  {
    return -EIO;
  }

  return BmsResponseLog_SetDataFilter((uint8_t)index, (uint8_t)mask, (uint8_t)value);
}

static void FaultCommand_ExecuteText(const char *line, FaultCommand_WriteFn write, void *context)
{
  unsigned int cell;
  unsigned int millivolts;
  unsigned int target;
  unsigned long duration;
  unsigned int slew;
  unsigned int lowMv;
  unsigned int highMv;
  unsigned int centerMv;
  unsigned int amplitudeMv;
  unsigned long periodMs;
  int calOffsetMv;
  unsigned int measuredMv;

  if ((strcmp(line, "help") == 0) || (strcmp(line, "?") == 0))
  {
    FaultCommand_WriteTextHelp(write, context);
  }
  else if (strcmp(line, "status") == 0)
  {
    FaultCommand_WriteTextStatus(write, context);
  }
  else if (strncmp(line, "normal", 6U) == 0)
  {
    millivolts = VOLTAGE_SIM_DEFAULT_NORMAL_MV;
    (void)sscanf(line, "normal %u", &millivolts);
    FaultCommand_WriteTextResult(write,
                                 context,
                                 "normal",
                                 FaultCommand_RunNormal((uint16_t)millivolts));
  }
  else if (sscanf(line, "set %u %u", &cell, &millivolts) == 2)
  {
    FaultCommand_WriteTextResult(write,
                                 context,
                                 "set",
                                 FaultCommand_RunSet((uint8_t)cell, (uint16_t)millivolts));
  }
  else if (strncmp(line, "ov ", 3U) == 0)
  {
    cell = 0U;
    target = VOLTAGE_SIM_DEFAULT_OV_MV;
    duration = FAULT_COMMAND_DEFAULT_DURATION_MS;
    slew = FAULT_COMMAND_DEFAULT_SLEW_MV_PER_MS;
    if (sscanf(line, "ov %u %u %lu %u", &cell, &target, &duration, &slew) < 1)
    {
      FaultCommand_WriteTextResult(write, context, "ov", -EIO);
      return;
    }

    FaultCommand_WriteTextResult(write,
                                 context,
                                 "ov",
                                 FaultCommand_RunOverVoltage((uint8_t)cell,
                                                             (uint16_t)target,
                                                             (uint32_t)duration,
                                                             (uint16_t)slew));
  }
  else if (strncmp(line, "uv ", 3U) == 0)
  {
    cell = 0U;
    target = VOLTAGE_SIM_DEFAULT_UV_MV;
    duration = FAULT_COMMAND_DEFAULT_DURATION_MS;
    slew = FAULT_COMMAND_DEFAULT_SLEW_MV_PER_MS;
    if (sscanf(line, "uv %u %u %lu %u", &cell, &target, &duration, &slew) < 1)
    {
      FaultCommand_WriteTextResult(write, context, "uv", -EIO);
      return;
    }

    FaultCommand_WriteTextResult(write,
                                 context,
                                 "uv",
                                 FaultCommand_RunUnderVoltage((uint8_t)cell,
                                                              (uint16_t)target,
                                                              (uint32_t)duration,
                                                              (uint16_t)slew));
  }
  else if (strncmp(line, "diff ", 5U) == 0)
  {
    unsigned int highCell;
    unsigned int highMv;
    unsigned int lowCell;
    unsigned int lowMv;

    duration = FAULT_COMMAND_DEFAULT_DURATION_MS;
    slew = FAULT_COMMAND_DEFAULT_SLEW_MV_PER_MS;
    if (sscanf(line,
               "diff %u %u %u %u %lu %u",
               &highCell,
               &highMv,
               &lowCell,
               &lowMv,
               &duration,
               &slew) < 4)
    {
      FaultCommand_WriteTextResult(write, context, "diff", -EIO);
      return;
    }

    FaultCommand_WriteTextResult(write,
                                 context,
                                 "diff",
                                 FaultCommand_RunDiff((uint8_t)highCell,
                                                      (uint16_t)highMv,
                                                      (uint8_t)lowCell,
                                                      (uint16_t)lowMv,
                                                      (uint32_t)duration,
                                                      (uint16_t)slew));
  }
  else if (strcmp(line, "wave status") == 0)
  {
    FaultCommand_WriteTextWaveStatus(write, context);
  }
  else if (strcmp(line, "wave stop") == 0)
  {
    FaultCommand_StopAllWaveforms();
    FaultCommand_WriteTextResult(write, context, "wave", 0);
  }
  else if (strncmp(line, "wave high ", 10U) == 0)
  {
    unsigned int freqHz;
    unsigned int points;
    WaveformEngine_Config cfg;

    if (DacSafety_IsOutputAllowed() == 0U)
    {
      FaultCommand_WriteTextResult(write, context, "wave-high", -EBUSY);
      return;
    }

    if (sscanf(line,
               "wave high %u %u %u %u %u",
               &cell,
               &freqHz,
               &points,
               &amplitudeMv,
               &centerMv) != 5)
    {
      FaultCommand_WriteTextResult(write, context, "wave-high", -EIO);
      return;
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.active = 1U;
    cfg.channels = 1U;
    cfg.channelList[0] = (uint8_t)cell;
    cfg.sampleRate = (uint32_t)freqHz * (uint32_t)points;
    cfg.pointCount = (uint32_t)points;
    cfg.cycleCount = 1U;
    cfg.amplitudeMv = (uint16_t)amplitudeMv;
    cfg.offsetMv = (uint16_t)centerMv;

    FaultCommand_WriteTextResult(write,
                                 context,
                                 "wave-high",
                                 WaveformEngine_StartSine(&cfg));
  }
  else if (strncmp(line, "wave square ", 12U) == 0)
  {
    if (DacSafety_IsOutputAllowed() == 0U)
    {
      FaultCommand_WriteTextResult(write, context, "wave", -EBUSY);
      return;
    }

    if (sscanf(line,
               "wave square %u %u %u %lu",
               &cell,
               &lowMv,
               &highMv,
               &periodMs) != 4)
    {
      FaultCommand_WriteTextResult(write, context, "wave", -EIO);
      return;
    }

    WaveformEngine_Stop();
    FaultCommand_WriteTextResult(write,
                                 context,
                                 "wave",
                                 WaveformGen_StartSquare((uint8_t)cell,
                                                         (uint16_t)lowMv,
                                                         (uint16_t)highMv,
                                                         (uint32_t)periodMs));
  }
  else if (strncmp(line, "wave sine all ", 14U) == 0)
  {
    if (DacSafety_IsOutputAllowed() == 0U)
    {
      FaultCommand_WriteTextResult(write, context, "wave", -EBUSY);
      return;
    }

    if (sscanf(line,
               "wave sine all %u %u %lu",
               &centerMv,
               &amplitudeMv,
               &periodMs) != 3)
    {
      FaultCommand_WriteTextResult(write, context, "wave", -EIO);
      return;
    }

    WaveformEngine_Stop();
    FaultCommand_WriteTextResult(write,
                                 context,
                                 "wave",
                                 WaveformGen_StartSineAll((uint16_t)centerMv,
                                                          (uint16_t)amplitudeMv,
                                                          (uint32_t)periodMs));
  }
  else if (strncmp(line, "wave sine ", 10U) == 0)
  {
    if (DacSafety_IsOutputAllowed() == 0U)
    {
      FaultCommand_WriteTextResult(write, context, "wave", -EBUSY);
      return;
    }

    if (sscanf(line,
               "wave sine %u %u %u %lu",
               &cell,
               &centerMv,
               &amplitudeMv,
               &periodMs) != 4)
    {
      FaultCommand_WriteTextResult(write, context, "wave", -EIO);
      return;
    }

    WaveformEngine_Stop();
    FaultCommand_WriteTextResult(write,
                                 context,
                                 "wave",
                                 WaveformGen_StartSine((uint8_t)cell,
                                                       (uint16_t)centerMv,
                                                       (uint16_t)amplitudeMv,
                                                       (uint32_t)periodMs));
  }
  else if (strcmp(line, "cal status") == 0)
  {
    FaultCommand_WriteTextCalibrationStatus(write, context);
  }
  else if (strncmp(line, "cal set ", 8U) == 0)
  {
    if (DacSafety_IsOutputAllowed() == 0U)
    {
      FaultCommand_WriteTextResult(write, context, "cal", -EBUSY);
      return;
    }

    if (sscanf(line, "cal set %u %d", &cell, &calOffsetMv) != 2)
    {
      FaultCommand_WriteTextResult(write, context, "cal", -EIO);
      return;
    }

    FaultCommand_WriteTextResult(write,
                                 context,
                                 "cal",
                                 VoltageSim_SetCellCalibrationOffsetMv((uint8_t)cell,
                                                                        (int16_t)calOffsetMv));
  }
  else if (strncmp(line, "cal trim ", 9U) == 0)
  {
    if (DacSafety_IsOutputAllowed() == 0U)
    {
      FaultCommand_WriteTextResult(write, context, "cal", -EBUSY);
      return;
    }

    if (sscanf(line, "cal trim %u %u %u", &cell, &target, &measuredMv) != 3)
    {
      FaultCommand_WriteTextResult(write, context, "cal", -EIO);
      return;
    }

    FaultCommand_WriteTextResult(write,
                                 context,
                                 "cal",
                                 FaultCommand_RunCalibrationTrim((uint8_t)cell,
                                                                 (uint16_t)target,
                                                                 (uint16_t)measuredMv));
  }
  else if (strncmp(line, "cal clear", 9U) == 0)
  {
    if (DacSafety_IsOutputAllowed() == 0U)
    {
      FaultCommand_WriteTextResult(write, context, "cal", -EBUSY);
      return;
    }

    if (sscanf(line, "cal clear %u", &cell) == 1)
    {
      FaultCommand_WriteTextResult(write,
                                   context,
                                   "cal",
                                   VoltageSim_ClearCellCalibrationOffset((uint8_t)cell));
    }
    else
    {
      VoltageSim_ClearAllCalibrationOffsets();
      FaultCommand_WriteTextResult(write, context, "cal", 0);
    }
  }
  else if (strncmp(line, "test static ", 12U) == 0)
  {
    if (sscanf(line, "test static %u", &millivolts) != 1)
    {
      FaultCommand_WriteTextResult(write, context, "test", -EIO);
      return;
    }

    FaultCommand_WriteTextResult(write,
                                 context,
                                 "test",
                                 FaultCommand_RunNormal((uint16_t)millivolts));
  }
  else if (strncmp(line, "test slew ", 10U) == 0)
  {
    if (DacSafety_IsOutputAllowed() == 0U)
    {
      FaultCommand_WriteTextResult(write, context, "test", -EBUSY);
      return;
    }

    if (sscanf(line,
               "test slew %u %u %u %lu",
               &cell,
               &lowMv,
               &highMv,
               &periodMs) != 4)
    {
      FaultCommand_WriteTextResult(write, context, "test", -EIO);
      return;
    }

    FaultCommand_WriteTextResult(write,
                                 context,
                                 "test",
                                 WaveformGen_StartSquare((uint8_t)cell,
                                                         (uint16_t)lowMv,
                                                         (uint16_t)highMv,
                                                         (uint32_t)periodMs));
  }
  else if (strcmp(line, "can status") == 0)
  {
    FaultCommand_WriteTextCanStatus(write, context);
  }
  else if (strncmp(line, "can send ", 9U) == 0)
  {
    FaultCommand_WriteTextResult(write,
                                 context,
                                 "can",
                                 FaultCommand_ParseCanSendText(line));
  }
  else if (strcmp(line, "log status") == 0)
  {
    FaultCommand_WriteTextBmsResponseLog(write, context);
  }
  else if (strcmp(line, "log clear") == 0)
  {
    BmsResponseLog_Clear();
    FaultCommand_WriteTextResult(write, context, "log", 0);
  }
  else if (strncmp(line, "log filter ", 11U) == 0)
  {
    FaultCommand_WriteTextResult(write,
                                 context,
                                 "log",
                                 FaultCommand_ParseLogFilterText(line));
    FaultCommand_WriteTextBmsResponseLog(write, context);
  }
  else if (strncmp(line, "log data ", 9U) == 0)
  {
    FaultCommand_WriteTextResult(write,
                                 context,
                                 "log",
                                 FaultCommand_ParseLogDataText(line));
    FaultCommand_WriteTextBmsResponseLog(write, context);
  }
  else if (strcmp(line, "safe status") == 0)
  {
    FaultCommand_WriteTextSafetyStatus(write, context);
  }
  else if ((strncmp(line, "safe stop", 9U) == 0) ||
           (strncmp(line, "emergency stop", 14U) == 0))
  {
    millivolts = DAC_SAFETY_DEFAULT_SAFE_MV;
    (void)sscanf(line, "safe stop %u", &millivolts);
    (void)sscanf(line, "emergency stop %u", &millivolts);
    FaultCommand_StopAllWaveforms();
    FaultCommand_WriteTextResult(write,
                                 context,
                                 "safe",
                                 DacSafety_EmergencyStop((uint16_t)millivolts));
  }
  else if (strcmp(line, "safe release") == 0)
  {
    DacSafety_Release();
    FaultCommand_WriteTextResult(write, context, "safe", 0);
  }
  else if (strcmp(line, "sram status") == 0)
  {
    FaultCommand_WriteTextSramStatus(write, context);
  }
  else if (strncmp(line, "sram test", 9U) == 0)
  {
    unsigned long bytes = BSP_SRAM_DEFAULT_TEST_BYTES;
    int status;

    (void)sscanf(line, "sram test %lu", &bytes);
    status = BspSram_RunSelfTest((uint32_t)bytes);
    FaultCommand_WriteTextResult(write, context, "sram", status);
    FaultCommand_WriteTextSramStatus(write, context);
  }
  else if (strncmp(line, "clear", 5U) == 0)
  {
    if (DacSafety_IsOutputAllowed() == 0U)
    {
      FaultCommand_WriteTextResult(write, context, "clear", -EBUSY);
      return;
    }

    FaultCommand_StopAllWaveforms();
    if (sscanf(line, "clear %u", &cell) == 1)
    {
      FaultCommand_WriteTextResult(write, context, "clear", VoltageSim_ClearCellFault((uint8_t)cell));
    }
    else
    {
      FaultCommand_WriteTextResult(write, context, "clear", VoltageSim_ClearAllFaults());
    }
  }
  else if (*line != '\0')
  {
    FaultCommand_WriteFormat(write, context, "[cmd] unknown: %s\r\n", line);
  }
}

static const char *FaultCommand_FindJsonKey(const char *json, const char *key)
{
  char pattern[32];

  (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  return strstr(json, pattern);
}

static uint8_t FaultCommand_JsonGetString(const char *json,
                                          const char *key,
                                          char *value,
                                          uint32_t valueSize)
{
  const char *cursor = FaultCommand_FindJsonKey(json, key);
  uint32_t index = 0U;

  if ((cursor == NULL) || (value == NULL) || (valueSize == 0U))
  {
    return 0U;
  }

  cursor = strchr(cursor, ':');
  if (cursor == NULL)
  {
    return 0U;
  }

  cursor++;
  while (*cursor == ' ')
  {
    cursor++;
  }

  if (*cursor != '"')
  {
    return 0U;
  }

  cursor++;
  while ((*cursor != '\0') && (*cursor != '"') && (index < (valueSize - 1U)))
  {
    value[index++] = *cursor++;
  }
  value[index] = '\0';

  return (*cursor == '"') ? 1U : 0U;
}

static uint8_t FaultCommand_JsonGetUint(const char *json, const char *key, uint32_t *value)
{
  const char *cursor = FaultCommand_FindJsonKey(json, key);

  if ((cursor == NULL) || (value == NULL))
  {
    return 0U;
  }

  cursor = strchr(cursor, ':');
  if (cursor == NULL)
  {
    return 0U;
  }

  cursor++;
  while (*cursor == ' ')
  {
    cursor++;
  }

  *value = (uint32_t)strtoul(cursor, NULL, 10);
  return 1U;
}

static uint8_t FaultCommand_JsonGetInt(const char *json, const char *key, int32_t *value)
{
  const char *cursor = FaultCommand_FindJsonKey(json, key);

  if ((cursor == NULL) || (value == NULL))
  {
    return 0U;
  }

  cursor = strchr(cursor, ':');
  if (cursor == NULL)
  {
    return 0U;
  }

  cursor++;
  while (*cursor == ' ')
  {
    cursor++;
  }

  *value = (int32_t)strtol(cursor, NULL, 10);
  return 1U;
}

static uint32_t FaultCommand_JsonGetUintDefault(const char *json, const char *key, uint32_t defaultValue)
{
  uint32_t value = defaultValue;

  (void)FaultCommand_JsonGetUint(json, key, &value);
  return value;
}

static void FaultCommand_ExecuteJson(const char *json, FaultCommand_WriteFn write, void *context)
{
  char cmd[16];
  char type[16];
  uint32_t cell;
  uint32_t mv;
  uint32_t duration;
  uint32_t slew;
  int status = -EIO;

  if (FaultCommand_JsonGetString(json, "cmd", cmd, sizeof(cmd)) == 0U)
  {
    FaultCommand_Write(write, context, "{\"ok\":false,\"error\":\"missing_cmd\"}\r\n");
    return;
  }

  if (strcmp(cmd, "status") == 0)
  {
    FaultCommand_WriteJsonStatus(write, context);
    return;
  }

  if (strcmp(cmd, "help") == 0)
  {
    FaultCommand_Write(write,
                       context,
                       "{\"ok\":true,\"commands\":[\"status\",\"normal\",\"set\",\"ov\",\"uv\",\"diff\",\"wave\",\"wave_stop\",\"cal\",\"test\",\"can\",\"log\",\"safe\",\"sram\",\"clear\"]}\r\n");
    return;
  }

  if (strcmp(cmd, "normal") == 0)
  {
    mv = FaultCommand_JsonGetUintDefault(json, "mv", VOLTAGE_SIM_DEFAULT_NORMAL_MV);
    status = FaultCommand_RunNormal((uint16_t)mv);
  }
  else if (strcmp(cmd, "set") == 0)
  {
    if ((FaultCommand_JsonGetUint(json, "cell", &cell) != 0U) &&
        (FaultCommand_JsonGetUint(json, "mv", &mv) != 0U))
    {
      status = FaultCommand_RunSet((uint8_t)cell, (uint16_t)mv);
    }
  }
  else if ((strcmp(cmd, "ov") == 0) || (strcmp(cmd, "over_voltage") == 0))
  {
    if (FaultCommand_JsonGetUint(json, "cell", &cell) != 0U)
    {
      mv = FaultCommand_JsonGetUintDefault(json, "mv", VOLTAGE_SIM_DEFAULT_OV_MV);
      duration = FaultCommand_JsonGetUintDefault(json, "duration_ms", FAULT_COMMAND_DEFAULT_DURATION_MS);
      slew = FaultCommand_JsonGetUintDefault(json, "slew_mv_per_ms", FAULT_COMMAND_DEFAULT_SLEW_MV_PER_MS);
      status = FaultCommand_RunOverVoltage((uint8_t)cell, (uint16_t)mv, duration, (uint16_t)slew);
    }
  }
  else if ((strcmp(cmd, "uv") == 0) || (strcmp(cmd, "under_voltage") == 0))
  {
    if (FaultCommand_JsonGetUint(json, "cell", &cell) != 0U)
    {
      mv = FaultCommand_JsonGetUintDefault(json, "mv", VOLTAGE_SIM_DEFAULT_UV_MV);
      duration = FaultCommand_JsonGetUintDefault(json, "duration_ms", FAULT_COMMAND_DEFAULT_DURATION_MS);
      slew = FaultCommand_JsonGetUintDefault(json, "slew_mv_per_ms", FAULT_COMMAND_DEFAULT_SLEW_MV_PER_MS);
      status = FaultCommand_RunUnderVoltage((uint8_t)cell, (uint16_t)mv, duration, (uint16_t)slew);
    }
  }
  else if (strcmp(cmd, "diff") == 0)
  {
    uint32_t highCell;
    uint32_t highMv;
    uint32_t lowCell;
    uint32_t lowMv;

    if ((FaultCommand_JsonGetUint(json, "high_cell", &highCell) != 0U) &&
        (FaultCommand_JsonGetUint(json, "high_mv", &highMv) != 0U) &&
        (FaultCommand_JsonGetUint(json, "low_cell", &lowCell) != 0U) &&
        (FaultCommand_JsonGetUint(json, "low_mv", &lowMv) != 0U))
    {
      duration = FaultCommand_JsonGetUintDefault(json, "duration_ms", FAULT_COMMAND_DEFAULT_DURATION_MS);
      slew = FaultCommand_JsonGetUintDefault(json, "slew_mv_per_ms", FAULT_COMMAND_DEFAULT_SLEW_MV_PER_MS);
      status = FaultCommand_RunDiff((uint8_t)highCell,
                                    (uint16_t)highMv,
                                    (uint8_t)lowCell,
                                    (uint16_t)lowMv,
                                    duration,
                                    (uint16_t)slew);
    }
  }
  else if (strcmp(cmd, "wave") == 0)
  {
    if (FaultCommand_JsonGetString(json, "type", type, sizeof(type)) != 0U)
    {
      if ((strcmp(type, "stop") != 0) && (DacSafety_IsOutputAllowed() == 0U))
      {
        status = -EBUSY;
      }
      else if (strcmp(type, "square") == 0)
      {
        uint32_t low;
        uint32_t high;
        uint32_t period;

        if ((FaultCommand_JsonGetUint(json, "cell", &cell) != 0U) &&
            (FaultCommand_JsonGetUint(json, "low_mv", &low) != 0U) &&
            (FaultCommand_JsonGetUint(json, "high_mv", &high) != 0U))
        {
          period = FaultCommand_JsonGetUintDefault(json, "period_ms", 1000U);
          WaveformEngine_Stop();
          status = WaveformGen_StartSquare((uint8_t)cell,
                                           (uint16_t)low,
                                           (uint16_t)high,
                                           period);
        }
      }
      else if (strcmp(type, "sine") == 0)
      {
        uint32_t offset;
        uint32_t amplitude;
        uint32_t period;

        if ((FaultCommand_JsonGetUint(json, "cell", &cell) != 0U) &&
            (FaultCommand_JsonGetUint(json, "offset_mv", &offset) != 0U) &&
            (FaultCommand_JsonGetUint(json, "amplitude_mv", &amplitude) != 0U))
        {
          period = FaultCommand_JsonGetUintDefault(json, "period_ms", 1000U);
          WaveformEngine_Stop();
          status = WaveformGen_StartSine((uint8_t)cell,
                                         (uint16_t)offset,
                                         (uint16_t)amplitude,
                                         period);
        }
      }
      else if (strcmp(type, "sine_all") == 0)
      {
        uint32_t offset;
        uint32_t amplitude;
        uint32_t period;

        if ((FaultCommand_JsonGetUint(json, "offset_mv", &offset) != 0U) &&
            (FaultCommand_JsonGetUint(json, "amplitude_mv", &amplitude) != 0U))
        {
          period = FaultCommand_JsonGetUintDefault(json, "period_ms", 1000U);
          WaveformEngine_Stop();
          status = WaveformGen_StartSineAll((uint16_t)offset,
                                            (uint16_t)amplitude,
                                            period);
        }
      }
      else if (strcmp(type, "high") == 0)
      {
        uint32_t freq;
        uint32_t points;
        uint32_t amplitude;
        uint32_t offset;
        WaveformEngine_Config cfg;

        if ((FaultCommand_JsonGetUint(json, "cell", &cell) != 0U) &&
            (FaultCommand_JsonGetUint(json, "freq_hz", &freq) != 0U) &&
            (FaultCommand_JsonGetUint(json, "points", &points) != 0U) &&
            (FaultCommand_JsonGetUint(json, "amplitude_mv", &amplitude) != 0U) &&
            (FaultCommand_JsonGetUint(json, "offset_mv", &offset) != 0U))
        {
          memset(&cfg, 0, sizeof(cfg));
          cfg.active = 1U;
          cfg.channels = 1U;
          cfg.channelList[0] = (uint8_t)cell;
          cfg.sampleRate = freq * points;
          cfg.pointCount = points;
          cfg.cycleCount = 1U;
          cfg.amplitudeMv = (uint16_t)amplitude;
          cfg.offsetMv = (uint16_t)offset;
          status = WaveformEngine_StartSine(&cfg);
        }
      }
      else if (strcmp(type, "stop") == 0)
      {
        FaultCommand_StopAllWaveforms();
        status = 0;
      }
    }
  }
  else if (strcmp(cmd, "wave_stop") == 0)
  {
    FaultCommand_StopAllWaveforms();
    status = 0;
  }
  else if (strcmp(cmd, "cal") == 0)
  {
    char action[16];

    if (FaultCommand_JsonGetString(json, "action", action, sizeof(action)) != 0U)
    {
      if (strcmp(action, "status") == 0)
      {
        FaultCommand_Write(write, context, "{\"ok\":true,\"calibration\":[");
        for (uint8_t calCell = 1U; calCell <= VOLTAGE_SIM_CELL_COUNT; calCell++)
        {
          FaultCommand_WriteFormat(write,
                                   context,
                                   "%s{\"cell\":%u,\"offset_mv\":%d}",
                                   (calCell == 1U) ? "" : ",",
                                   calCell,
                                   (int)VoltageSim_GetCellCalibrationOffsetMv(calCell));
        }
        FaultCommand_Write(write, context, "]}\r\n");
        return;
      }
      else if (strcmp(action, "set") == 0)
      {
        int32_t signedOffset;

        if (DacSafety_IsOutputAllowed() == 0U)
        {
          status = -EBUSY;
        }
        else if ((FaultCommand_JsonGetUint(json, "cell", &cell) != 0U) &&
            (FaultCommand_JsonGetInt(json, "offset_mv", &signedOffset) != 0U))
        {
          status = VoltageSim_SetCellCalibrationOffsetMv((uint8_t)cell,
                                                         (int16_t)signedOffset);
        }
      }
      else if (strcmp(action, "trim") == 0)
      {
        uint32_t targetMv;
        uint32_t measuredMv;

        if (DacSafety_IsOutputAllowed() == 0U)
        {
          status = -EBUSY;
        }
        else if ((FaultCommand_JsonGetUint(json, "cell", &cell) != 0U) &&
            (FaultCommand_JsonGetUint(json, "target_mv", &targetMv) != 0U) &&
            (FaultCommand_JsonGetUint(json, "measured_mv", &measuredMv) != 0U))
        {
          status = FaultCommand_RunCalibrationTrim((uint8_t)cell,
                                                  (uint16_t)targetMv,
                                                  (uint16_t)measuredMv);
        }
      }
      else if (strcmp(action, "clear") == 0)
      {
        if (DacSafety_IsOutputAllowed() == 0U)
        {
          status = -EBUSY;
        }
        else if (FaultCommand_JsonGetUint(json, "cell", &cell) != 0U)
        {
          status = VoltageSim_ClearCellCalibrationOffset((uint8_t)cell);
        }
        else
        {
          VoltageSim_ClearAllCalibrationOffsets();
          status = 0;
        }
      }
    }
  }
  else if (strcmp(cmd, "test") == 0)
  {
    if (FaultCommand_JsonGetString(json, "type", type, sizeof(type)) != 0U)
    {
      if (strcmp(type, "static") == 0)
      {
        if (FaultCommand_JsonGetUint(json, "mv", &mv) != 0U)
        {
          status = FaultCommand_RunNormal((uint16_t)mv);
        }
      }
      else if (strcmp(type, "slew") == 0)
      {
        uint32_t low;
        uint32_t high;
        uint32_t period;

        if (DacSafety_IsOutputAllowed() == 0U)
        {
          status = -EBUSY;
        }
        else if ((FaultCommand_JsonGetUint(json, "cell", &cell) != 0U) &&
            (FaultCommand_JsonGetUint(json, "low_mv", &low) != 0U) &&
            (FaultCommand_JsonGetUint(json, "high_mv", &high) != 0U))
        {
          period = FaultCommand_JsonGetUintDefault(json, "period_ms", 1000U);
          status = WaveformGen_StartSquare((uint8_t)cell,
                                           (uint16_t)low,
                                           (uint16_t)high,
                                           period);
        }
      }
    }
  }
  else if (strcmp(cmd, "can") == 0)
  {
    char action[16];

    if (FaultCommand_JsonGetString(json, "action", action, sizeof(action)) != 0U)
    {
      if (strcmp(action, "status") == 0)
      {
        FaultCommand_WriteJsonCanStatus(write, context);
        return;
      }
      else if (strcmp(action, "send") == 0)
      {
        uint32_t id;
        uint32_t length;
        uint32_t extended;
        uint8_t data[BSP_CAN_MAX_DATA_LEN] = {0};

        if ((FaultCommand_JsonGetUint(json, "id", &id) != 0U) &&
            (FaultCommand_JsonGetUint(json, "len", &length) != 0U) &&
            (length <= BSP_CAN_MAX_DATA_LEN))
        {
          extended = FaultCommand_JsonGetUintDefault(json, "extended", 0U);
          status = 0;

          for (uint8_t index = 0U; index < length; index++)
          {
            char key[8];
            uint32_t value;

            (void)snprintf(key, sizeof(key), "d%u", index);
            if ((FaultCommand_JsonGetUint(json, key, &value) == 0U) || (value > 0xFFU))
            {
              status = -EIO;
              break;
            }
            data[index] = (uint8_t)value;
          }

          if (status == 0)
          {
            status = BspCan_SendClassic(id, (extended != 0U) ? 1U : 0U, data, (uint8_t)length);
          }
        }
      }
    }
  }
  else if (strcmp(cmd, "log") == 0)
  {
    char action[16];

    if (FaultCommand_JsonGetString(json, "action", action, sizeof(action)) != 0U)
    {
      if (strcmp(action, "status") == 0)
      {
        FaultCommand_WriteJsonBmsResponseLog(write, context);
        return;
      }
      else if (strcmp(action, "clear") == 0)
      {
        BmsResponseLog_Clear();
        status = 0;
      }
      else if (strcmp(action, "filter") == 0)
      {
        char typeName[8];
        uint32_t id;
        uint32_t mask;
        uint8_t isExtended = 0U;

        if (FaultCommand_JsonGetString(json, "type", typeName, sizeof(typeName)) == 0U)
        {
          FaultCommand_WriteJsonResult(write, context, cmd, -EIO);
          return;
        }

        if (strcmp(typeName, "off") == 0)
        {
          BmsResponseLog_DisableFilter();
          FaultCommand_WriteJsonBmsResponseLog(write, context);
          return;
        }

        if (strcmp(typeName, "std") == 0)
        {
          isExtended = 0U;
          mask = FaultCommand_JsonGetUintDefault(json, "mask", 0x7FFU);
        }
        else if (strcmp(typeName, "ext") == 0)
        {
          isExtended = 1U;
          mask = FaultCommand_JsonGetUintDefault(json, "mask", 0x1FFFFFFFU);
        }
        else
        {
          FaultCommand_WriteJsonResult(write, context, cmd, -EIO);
          return;
        }

        if (FaultCommand_JsonGetUint(json, "id", &id) != 0U)
        {
          status = BmsResponseLog_SetFilter(1U, isExtended, id, mask);
          FaultCommand_WriteJsonBmsResponseLog(write, context);
          return;
        }
      }
      else if (strcmp(action, "data") == 0)
      {
        uint32_t index;
        uint32_t mask;
        uint32_t value;

        if ((FaultCommand_JsonGetUint(json, "index", &index) != 0U) &&
            (FaultCommand_JsonGetUint(json, "mask", &mask) != 0U) &&
            (FaultCommand_JsonGetUint(json, "value", &value) != 0U) &&
            (mask <= 0xFFU) &&
            (value <= 0xFFU))
        {
          status = BmsResponseLog_SetDataFilter((uint8_t)index,
                                                (uint8_t)mask,
                                                (uint8_t)value);
          FaultCommand_WriteJsonBmsResponseLog(write, context);
          return;
        }
      }
      else if (strcmp(action, "data_clear") == 0)
      {
        BmsResponseLog_ClearDataFilter();
        FaultCommand_WriteJsonBmsResponseLog(write, context);
        return;
      }
    }
  }
  else if (strcmp(cmd, "safe") == 0)
  {
    char action[16];

    if (FaultCommand_JsonGetString(json, "action", action, sizeof(action)) != 0U)
    {
      if (strcmp(action, "status") == 0)
      {
        FaultCommand_WriteJsonSafetyStatus(write, context);
        return;
      }
      else if ((strcmp(action, "stop") == 0) || (strcmp(action, "emergency_stop") == 0))
      {
        mv = FaultCommand_JsonGetUintDefault(json, "mv", DAC_SAFETY_DEFAULT_SAFE_MV);
        FaultCommand_StopAllWaveforms();
        status = DacSafety_EmergencyStop((uint16_t)mv);
      }
      else if (strcmp(action, "release") == 0)
      {
        DacSafety_Release();
        status = 0;
      }
    }
  }
  else if (strcmp(cmd, "sram") == 0)
  {
    char action[16];

    if (FaultCommand_JsonGetString(json, "action", action, sizeof(action)) != 0U)
    {
      if (strcmp(action, "status") == 0)
      {
        FaultCommand_WriteJsonSramStatus(write, context, 0);
        return;
      }
      else if (strcmp(action, "test") == 0)
      {
        uint32_t bytes = FaultCommand_JsonGetUintDefault(json,
                                                         "bytes",
                                                         BSP_SRAM_DEFAULT_TEST_BYTES);
        status = BspSram_RunSelfTest(bytes);
        FaultCommand_WriteJsonSramStatus(write, context, status);
        return;
      }
    }
  }
  else if (strcmp(cmd, "clear") == 0)
  {
    if (DacSafety_IsOutputAllowed() == 0U)
    {
      status = -EBUSY;
    }
    else
    {
      FaultCommand_StopAllWaveforms();
      if (FaultCommand_JsonGetUint(json, "cell", &cell) != 0U)
      {
        status = VoltageSim_ClearCellFault((uint8_t)cell);
      }
      else
      {
        status = VoltageSim_ClearAllFaults();
      }
    }
  }
  else
  {
    FaultCommand_Write(write, context, "{\"ok\":false,\"error\":\"unknown_cmd\"}\r\n");
    return;
  }

  FaultCommand_WriteJsonResult(write, context, cmd, status);
}

void FaultCommand_ExecuteLine(const char *line, FaultCommand_WriteFn write, void *context)
{
  line = FaultCommand_SkipSpaces(line);

  if (line == NULL)
  {
    return;
  }

  if (*line == '{')
  {
    FaultCommand_ExecuteJson(line, write, context);
  }
  else
  {
    FaultCommand_ExecuteText(line, write, context);
  }
}
