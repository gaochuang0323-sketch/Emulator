/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 以太网 PHY BSP 层 API。
 * 由 STM32 HAL 版本移植到 Zephyr:去除 main.h 依赖。
 */
#ifndef INCLUDE_DRIVERS_BSP_ETH_H_
#define INCLUDE_DRIVERS_BSP_ETH_H_

#ifdef __cplusplus
extern "C" {
#endif

void BSP_ETH_PHY_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_DRIVERS_BSP_ETH_H_ */
