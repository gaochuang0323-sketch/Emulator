/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * CAN 收发 BSP 层 API。
 * 由 STM32 FDCAN HAL 版本移植到 Zephyr:去除 main.h 依赖,底层将接 Zephyr CAN
 * 子系统(<zephyr/drivers/can.h>),对上保持原有 API,返回值统一为 int。
 */
#ifndef INCLUDE_DRIVERS_BSP_CAN_H_
#define INCLUDE_DRIVERS_BSP_CAN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define BSP_CAN_MAX_DATA_LEN 8U

typedef struct {
	uint32_t id;
	uint32_t tick;
	uint8_t isExtended;
	uint8_t length;
	uint8_t data[BSP_CAN_MAX_DATA_LEN];
} BspCan_RxFrame;

typedef struct {
	uint8_t started;
	uint32_t txCount;
	uint32_t rxCount;
	uint32_t errorCount;
	uint32_t lastRxId;
	uint32_t lastRxTick;
	uint8_t lastRxIsExtended;
	uint8_t lastRxLength;
	uint8_t lastRxData[BSP_CAN_MAX_DATA_LEN];
} BspCan_Status;

int BspCan_Init(void);
int BspCan_SendClassic(uint32_t id,
		       uint8_t isExtended,
		       const uint8_t *data,
		       uint8_t length);
void BspCan_Process(void);
BspCan_Status BspCan_GetStatus(void);
void BspCan_OnRxFrame(const BspCan_RxFrame *frame);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_DRIVERS_BSP_CAN_H_ */
