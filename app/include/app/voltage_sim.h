/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 电压模拟:13 路单体电压输出与过压/欠压故障注入。
 * 由 STM32 HAL 版本移植到 Zephyr —— 返回值改为 int(0=成功, <0=errno),
 * 底层 DAC 访问通过自定义 dac81416 驱动 API,不再直接依赖 HAL。
 */
#ifndef APP_VOLTAGE_SIM_H_
#define APP_VOLTAGE_SIM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define VOLTAGE_SIM_CELL_COUNT          13U
#define VOLTAGE_SIM_MIN_MV              500U
#define VOLTAGE_SIM_MAX_MV              5000U
#define VOLTAGE_SIM_DEFAULT_NORMAL_MV   3200U
#define VOLTAGE_SIM_DEFAULT_OV_MV       4500U
#define VOLTAGE_SIM_DEFAULT_UV_MV       1000U
#define VOLTAGE_SIM_CAL_OFFSET_LIMIT_MV 500

typedef enum {
	VOLTAGE_SIM_FAULT_NONE = 0,
	VOLTAGE_SIM_FAULT_OVER_VOLTAGE,
	VOLTAGE_SIM_FAULT_UNDER_VOLTAGE
} VoltageSim_FaultType;

typedef struct {
	VoltageSim_FaultType type;
	uint16_t targetMillivolts;
	uint16_t restoreMillivolts;
	uint32_t durationMs;
	uint16_t slewRateMvPerMs;
} VoltageSim_CellFaultInfo;

int VoltageSim_Init(uint16_t defaultMillivolts);
int VoltageSim_SetCellVoltageMv(uint8_t cell, uint16_t millivolts);
int VoltageSim_SetAllCellsVoltageMv(uint16_t millivolts);
int VoltageSim_InjectCellOverVoltage(uint8_t cell, uint16_t targetMillivolts);
int VoltageSim_InjectCellUnderVoltage(uint8_t cell, uint16_t targetMillivolts);
int VoltageSim_InjectCellOverVoltageFor(uint8_t cell,
					uint16_t targetMillivolts,
					uint32_t durationMs);
int VoltageSim_InjectCellUnderVoltageFor(uint8_t cell,
					 uint16_t targetMillivolts,
					 uint32_t durationMs);
int VoltageSim_InjectCellOverVoltageRamp(uint8_t cell,
					 uint16_t targetMillivolts,
					 uint32_t durationMs,
					 uint16_t slewRateMvPerMs);
int VoltageSim_InjectCellUnderVoltageRamp(uint8_t cell,
					  uint16_t targetMillivolts,
					  uint32_t durationMs,
					  uint16_t slewRateMvPerMs);
int VoltageSim_InjectVoltageDifference(uint8_t highCell,
				       uint16_t highMillivolts,
				       uint8_t lowCell,
				       uint16_t lowMillivolts,
				       uint32_t durationMs,
				       uint16_t slewRateMvPerMs);
int VoltageSim_ClearCellFault(uint8_t cell);
int VoltageSim_ClearAllFaults(void);
int VoltageSim_Process(uint32_t nowMs);
int VoltageSim_SetCellCalibrationOffsetMv(uint8_t cell, int16_t offsetMillivolts);
int VoltageSim_ClearCellCalibrationOffset(uint8_t cell);
void VoltageSim_ClearAllCalibrationOffsets(void);
int16_t VoltageSim_GetCellCalibrationOffsetMv(uint8_t cell);
uint16_t VoltageSim_MillivoltsToDacCode(uint16_t millivolts);
uint16_t VoltageSim_GetLastCellVoltageMv(uint8_t cell);
VoltageSim_CellFaultInfo VoltageSim_GetCellFaultInfo(uint8_t cell);
uint8_t VoltageSim_GetActiveFaultCount(void);
const char *VoltageSim_GetFaultTypeName(VoltageSim_FaultType type);

#ifdef __cplusplus
}
#endif

#endif /* APP_VOLTAGE_SIM_H_ */
