/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 故障命令解析:将文本命令行解析并执行。
 * 由 STM32 HAL 版本移植到 Zephyr —— 返回值改为 int(0=成功, <0=errno),
 * 不再直接依赖 HAL。
 */
#ifndef APP_FAULT_COMMAND_H_
#define APP_FAULT_COMMAND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void (*FaultCommand_WriteFn)(const char *text, void *context);

void FaultCommand_ExecuteLine(const char *line, FaultCommand_WriteFn write, void *context);

#ifdef __cplusplus
}
#endif

#endif /* APP_FAULT_COMMAND_H_ */
