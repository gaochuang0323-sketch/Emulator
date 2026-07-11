/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * TCP JSON 服务器:通过 TCP 提供 JSON 控制接口。
 * 由 STM32 HAL 版本移植到 Zephyr —— 返回值改为 int(0=成功, <0=errno),
 * 不再直接依赖 HAL。
 */
#ifndef APP_TCP_JSON_SERVER_H_
#define APP_TCP_JSON_SERVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define TCP_JSON_SERVER_PORT 5000U

void TcpJsonServer_Start(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_TCP_JSON_SERVER_H_ */
