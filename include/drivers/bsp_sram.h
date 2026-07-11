/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 外部 SRAM(FMC)BSP 层 API,用于波形引擎 DMA 缓冲。
 * 由 STM32 HAL 版本移植到 Zephyr:去除 main.h 依赖,返回值统一为 int。
 */
#ifndef INCLUDE_DRIVERS_BSP_SRAM_H_
#define INCLUDE_DRIVERS_BSP_SRAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define BSP_SRAM_BASE_ADDRESS       0x60000000UL
#define BSP_SRAM_SIZE_BYTES         (1024UL * 1024UL)
#define BSP_SRAM_DEFAULT_TEST_BYTES (64UL * 1024UL)
#define BSP_SRAM_BUS_WIDTH_BITS     16U

typedef struct {
	uint32_t baseAddress;
	uint32_t sizeBytes;
	uint8_t busWidthBits;
	uint32_t lastTestBytes;
	uint32_t lastTestTick;
	uint32_t lastFailedAddress;
	uint32_t passCount;
	uint32_t failCount;
	uint16_t expected;
	uint16_t actual;
	int lastResult;
} BspSram_Status;

void BspSram_Init(void);
int BspSram_RunSelfTest(uint32_t bytes);
BspSram_Status BspSram_GetStatus(void);
const char *BspSram_GetResultName(int result);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_DRIVERS_BSP_SRAM_H_ */
