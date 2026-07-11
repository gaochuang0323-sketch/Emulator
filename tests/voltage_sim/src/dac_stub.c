/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * voltage_sim 依赖的 DAC 桩:替代 drivers/dac81416 与 bsp_dac81416,
 * 只实现被测代码实际调用的符号(DAC_WriteChannel / DAC_LDAC_Update),
 * 并记录调用以供断言。移植自原工程 test/support/stubs.c 的 DAC 部分。
 */
#include "drivers/dac81416.h"
#include "drivers/bsp_dac81416.h"

#include "dac_stub.h"

static int dacWriteResult;
static uint8_t dacWriteCalled;
static uint8_t dacLastChannel;
static uint16_t dacLastValue;
static uint32_t dacWriteCount;
static uint8_t ldacCalled;

/* ---- 被测代码调用的 DAC 接口 ---- */

int DAC_WriteChannel(uint8_t channel, uint16_t value)
{
	dacWriteCalled = 1U;
	dacLastChannel = channel;
	dacLastValue = value;
	dacWriteCount++;
	return dacWriteResult;
}

void DAC_LDAC_Update(void)
{
	ldacCalled = 1U;
}

/* ---- 测试检查接口 ---- */

void DacStub_Reset(void)
{
	dacWriteResult = 0;
	dacWriteCalled = 0U;
	dacLastChannel = 0U;
	dacLastValue = 0U;
	dacWriteCount = 0U;
	ldacCalled = 0U;
}

void DacStub_SetWriteResult(int result)
{
	dacWriteResult = result;
}

uint8_t DacStub_GetWriteCalled(void)
{
	return dacWriteCalled;
}

uint8_t DacStub_GetLastChannel(void)
{
	return dacLastChannel;
}

uint16_t DacStub_GetLastValue(void)
{
	return dacLastValue;
}

uint32_t DacStub_GetWriteCount(void)
{
	return dacWriteCount;
}

uint8_t DacStub_GetLdacCalled(void)
{
	return ldacCalled;
}

void DacStub_ClearLdacCalled(void)
{
	ldacCalled = 0U;
}
