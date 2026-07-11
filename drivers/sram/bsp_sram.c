/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 外部 SRAM(FMC)BSP 驱动实现。由 STM32 HAL 版本移植:
 *   - HAL_StatusTypeDef 状态码 -> int(0=OK, -EIO=错误, -EBUSY=进行中)
 *   - HAL_GetTick() -> k_uptime_get_32()
 *   - __DSB() -> barrier_dmem_fence_full()(Zephyr 屏障)
 * SRAM 经 FMC 映射到固定地址,访问逻辑与原实现一致。
 */
#include "drivers/bsp_sram.h"

#include <string.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/barrier.h>

/* 自检状态码(替代原 HAL_OK/HAL_ERROR/HAL_BUSY 语义) */
#define SRAM_RESULT_OK   0
#define SRAM_RESULT_ERR  (-EIO)
#define SRAM_RESULT_BUSY (-EBUSY)

static BspSram_Status sramStatus;

static void BspSram_RecordFailure(uint32_t index, uint16_t expected, uint16_t actual)
{
	sramStatus.lastFailedAddress = BSP_SRAM_BASE_ADDRESS + (index * sizeof(uint16_t));
	sramStatus.expected = expected;
	sramStatus.actual = actual;
	sramStatus.lastResult = SRAM_RESULT_ERR;
	sramStatus.failCount++;
}

static int BspSram_WriteAndVerifyPattern(uint32_t halfwordCount, uint16_t pattern)
{
	volatile uint16_t *memory = (volatile uint16_t *)BSP_SRAM_BASE_ADDRESS;

	for (uint32_t index = 0U; index < halfwordCount; index++) {
		memory[index] = pattern;
	}
	barrier_dmem_fence_full();

	for (uint32_t index = 0U; index < halfwordCount; index++) {
		uint16_t actual = memory[index];

		if (actual != pattern) {
			BspSram_RecordFailure(index, pattern, actual);
			return SRAM_RESULT_ERR;
		}
	}

	return SRAM_RESULT_OK;
}

static int BspSram_WriteAndVerifyAddressPattern(uint32_t halfwordCount)
{
	volatile uint16_t *memory = (volatile uint16_t *)BSP_SRAM_BASE_ADDRESS;

	for (uint32_t index = 0U; index < halfwordCount; index++) {
		memory[index] = (uint16_t)(0xA5A5U ^ index);
	}
	barrier_dmem_fence_full();

	for (uint32_t index = 0U; index < halfwordCount; index++) {
		uint16_t expected = (uint16_t)(0xA5A5U ^ index);
		uint16_t actual = memory[index];

		if (actual != expected) {
			BspSram_RecordFailure(index, expected, actual);
			return SRAM_RESULT_ERR;
		}
	}

	return SRAM_RESULT_OK;
}

void BspSram_Init(void)
{
	memset(&sramStatus, 0, sizeof(sramStatus));
	sramStatus.baseAddress = BSP_SRAM_BASE_ADDRESS;
	sramStatus.sizeBytes = BSP_SRAM_SIZE_BYTES;
	sramStatus.busWidthBits = BSP_SRAM_BUS_WIDTH_BITS;
	sramStatus.lastResult = SRAM_RESULT_OK;
}

int BspSram_RunSelfTest(uint32_t bytes)
{
	static const uint16_t patterns[] = {0x0000U, 0xFFFFU, 0xAAAAU, 0x5555U};
	uint32_t halfwordCount;

	if (bytes == 0U) {
		bytes = BSP_SRAM_DEFAULT_TEST_BYTES;
	}

	if (bytes > BSP_SRAM_SIZE_BYTES) {
		bytes = BSP_SRAM_SIZE_BYTES;
	}

	bytes &= ~1UL;
	if (bytes == 0U) {
		return SRAM_RESULT_ERR;
	}

	sramStatus.lastTestBytes = bytes;
	sramStatus.lastTestTick = k_uptime_get_32();
	sramStatus.lastFailedAddress = 0U;
	sramStatus.expected = 0U;
	sramStatus.actual = 0U;
	sramStatus.lastResult = SRAM_RESULT_BUSY;

	halfwordCount = bytes / sizeof(uint16_t);

	if (BspSram_WriteAndVerifyAddressPattern(halfwordCount) != SRAM_RESULT_OK) {
		return SRAM_RESULT_ERR;
	}

	for (uint32_t index = 0U; index < (sizeof(patterns) / sizeof(patterns[0])); index++) {
		if (BspSram_WriteAndVerifyPattern(halfwordCount, patterns[index]) != SRAM_RESULT_OK) {
			return SRAM_RESULT_ERR;
		}
	}

	sramStatus.lastResult = SRAM_RESULT_OK;
	sramStatus.passCount++;
	return SRAM_RESULT_OK;
}

BspSram_Status BspSram_GetStatus(void)
{
	return sramStatus;
}

const char *BspSram_GetResultName(int result)
{
	switch (result) {
	case SRAM_RESULT_OK:
		return "OK";
	case SRAM_RESULT_ERR:
		return "ERR";
	case SRAM_RESULT_BUSY:
		return "BUSY";
	default:
		return "UNKNOWN";
	}
}
