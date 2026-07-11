/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 故障控制台:交互式故障命令控制台。
 * 由 STM32 HAL 版本移植到 Zephyr —— 返回值改为 int(0=成功, <0=errno),
 * 不再直接依赖 HAL。
 */
#ifndef APP_FAULT_CONSOLE_H_
#define APP_FAULT_CONSOLE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void FaultConsole_Init(void);
void FaultConsole_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_FAULT_CONSOLE_H_ */
