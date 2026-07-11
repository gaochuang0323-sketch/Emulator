/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * voltage_sim 纯逻辑单元测试。在 native_sim 上运行,无需硬件。
 */
#include <zephyr/ztest.h>

#include "app/voltage_sim.h"
#include "dac_stub.h"

static void *setup(void)
{
	return NULL;
}

static void before(void *fixture)
{
	ARG_UNUSED(fixture);
	DacStub_Reset();
}

ZTEST_SUITE(voltage_sim, NULL, setup, before, NULL, NULL);

/* ---- mV <-> DAC 码值换算 ---- */

ZTEST(voltage_sim, test_mv_to_code_full_scale)
{
	/* >= 满量程返回 16 位满码 */
	zassert_equal(VoltageSim_MillivoltsToDacCode(VOLTAGE_SIM_MAX_MV), 0xFFFFU);
	zassert_equal(VoltageSim_MillivoltsToDacCode(6000U), 0xFFFFU);
}

ZTEST(voltage_sim, test_mv_to_code_midscale)
{
	/* 2500mV(半量程)约对应半码 32768,允许 ±1 量化误差 */
	uint16_t code = VoltageSim_MillivoltsToDacCode(2500U);

	zassert_within(code, 32768U, 1U, "midscale code=%u", code);
}

ZTEST(voltage_sim, test_mv_to_code_monotonic)
{
	/* 单调递增 */
	zassert_true(VoltageSim_MillivoltsToDacCode(1000U) <
		     VoltageSim_MillivoltsToDacCode(2000U));
	zassert_true(VoltageSim_MillivoltsToDacCode(3000U) <
		     VoltageSim_MillivoltsToDacCode(4000U));
}

/* ---- 初始化 ---- */

ZTEST(voltage_sim, test_init_sets_all_cells_default)
{
	zassert_ok(VoltageSim_Init(VOLTAGE_SIM_DEFAULT_NORMAL_MV));

	/* 13 路全部写入 + 触发一次 LDAC 锁存 */
	zassert_equal(DacStub_GetWriteCount(), VOLTAGE_SIM_CELL_COUNT);
	zassert_equal(DacStub_GetLdacCalled(), 1U);

	for (uint8_t cell = 1U; cell <= VOLTAGE_SIM_CELL_COUNT; cell++) {
		zassert_equal(VoltageSim_GetLastCellVoltageMv(cell),
			      VOLTAGE_SIM_DEFAULT_NORMAL_MV);
	}
}

/* ---- 电压设置与范围校验 ---- */

ZTEST(voltage_sim, test_set_cell_valid)
{
	zassert_ok(VoltageSim_Init(VOLTAGE_SIM_DEFAULT_NORMAL_MV));
	DacStub_Reset();

	zassert_ok(VoltageSim_SetCellVoltageMv(7U, 4000U));
	zassert_equal(DacStub_GetWriteCalled(), 1U);
	zassert_equal(DacStub_GetLastChannel(), 6U);  /* cell 7 -> channel 6 */
	zassert_equal(VoltageSim_GetLastCellVoltageMv(7U), 4000U);
}

ZTEST(voltage_sim, test_set_cell_rejects_out_of_range)
{
	zassert_not_equal(VoltageSim_SetCellVoltageMv(0U, 3000U), 0);   /* cell 无效 */
	zassert_not_equal(VoltageSim_SetCellVoltageMv(14U, 3000U), 0);  /* cell 超范围 */
	zassert_not_equal(VoltageSim_SetCellVoltageMv(1U, 100U), 0);    /* 电压过低 */
	zassert_not_equal(VoltageSim_SetCellVoltageMv(1U, 9000U), 0);   /* 电压过高 */
}

/* ---- 校准偏移 ---- */

ZTEST(voltage_sim, test_calibration_offset_applied)
{
	zassert_ok(VoltageSim_Init(VOLTAGE_SIM_DEFAULT_NORMAL_MV));

	zassert_ok(VoltageSim_SetCellCalibrationOffsetMv(3U, 100));
	zassert_equal(VoltageSim_GetCellCalibrationOffsetMv(3U), 100);

	zassert_ok(VoltageSim_ClearCellCalibrationOffset(3U));
	zassert_equal(VoltageSim_GetCellCalibrationOffsetMv(3U), 0);
}

ZTEST(voltage_sim, test_calibration_offset_limit)
{
	/* 超过 ±VOLTAGE_SIM_CAL_OFFSET_LIMIT_MV 应被拒绝 */
	zassert_not_equal(VoltageSim_SetCellCalibrationOffsetMv(
		1U, VOLTAGE_SIM_CAL_OFFSET_LIMIT_MV + 1), 0);
}

/* ---- 故障注入 ---- */

ZTEST(voltage_sim, test_inject_over_voltage)
{
	zassert_ok(VoltageSim_Init(VOLTAGE_SIM_DEFAULT_NORMAL_MV));

	/* 阶跃过压(无速率) */
	zassert_ok(VoltageSim_InjectCellOverVoltage(5U, VOLTAGE_SIM_DEFAULT_OV_MV));
	zassert_equal(VoltageSim_GetActiveFaultCount(), 1U);

	VoltageSim_CellFaultInfo info = VoltageSim_GetCellFaultInfo(5U);

	zassert_equal(info.type, VOLTAGE_SIM_FAULT_OVER_VOLTAGE);
	zassert_equal(info.targetMillivolts, VOLTAGE_SIM_DEFAULT_OV_MV);
}

ZTEST(voltage_sim, test_inject_over_voltage_rejects_below_current)
{
	zassert_ok(VoltageSim_Init(VOLTAGE_SIM_DEFAULT_NORMAL_MV));

	/* 过压目标必须高于当前电压 */
	zassert_not_equal(VoltageSim_InjectCellOverVoltage(5U, 1000U), 0);
}

ZTEST(voltage_sim, test_clear_fault_restores)
{
	zassert_ok(VoltageSim_Init(VOLTAGE_SIM_DEFAULT_NORMAL_MV));
	zassert_ok(VoltageSim_InjectCellOverVoltage(5U, VOLTAGE_SIM_DEFAULT_OV_MV));
	zassert_equal(VoltageSim_GetActiveFaultCount(), 1U);

	zassert_ok(VoltageSim_ClearCellFault(5U));
	zassert_equal(VoltageSim_GetActiveFaultCount(), 0U);
	/* 恢复到注入前电压 */
	zassert_equal(VoltageSim_GetLastCellVoltageMv(5U), VOLTAGE_SIM_DEFAULT_NORMAL_MV);
}

ZTEST(voltage_sim, test_fault_type_names)
{
	zassert_mem_equal(VoltageSim_GetFaultTypeName(VOLTAGE_SIM_FAULT_OVER_VOLTAGE), "OV", 3);
	zassert_mem_equal(VoltageSim_GetFaultTypeName(VOLTAGE_SIM_FAULT_UNDER_VOLTAGE), "UV", 3);
	zassert_mem_equal(VoltageSim_GetFaultTypeName(VOLTAGE_SIM_FAULT_NONE), "NONE", 5);
}
