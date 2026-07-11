/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * BMS 故障模拟装置 —— 应用入口。
 * 负责各子模块初始化与任务编排;不直接触碰硬件寄存器。
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app/voltage_sim.h"
#include "app/dac_safety.h"
#include "app/fault_console.h"
#include "app/tcp_json_server.h"
#include "app/can_protocol.h"

LOG_MODULE_REGISTER(bms_app, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("BMS Emulator starting...");

	/* TODO: 初始化顺序与任务编排由业务实现补全 */
	(void)VoltageSim_Init(VOLTAGE_SIM_DEFAULT_NORMAL_MV);
	DacSafety_Init(DAC_SAFETY_DEFAULT_SAFE_MV);
	CanProtocol_Init();
	FaultConsole_Init();
	TcpJsonServer_Start();

	return 0;
}
