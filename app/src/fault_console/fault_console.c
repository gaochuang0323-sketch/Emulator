/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 故障控制台实现。由 STM32 HAL 版本移植:
 *   - HAL_UART_Receive(&huart1, ...) -> uart_poll_in(console_dev, ...)
 *   - printf -> printk
 * 行缓冲与命令分发逻辑(FaultCommand_ExecuteLine)与原实现一致。
 */
#include "app/fault_console.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "app/fault_command.h"

#define FAULT_CONSOLE_LINE_SIZE 160U
#define FAULT_CONSOLE_RX_BUDGET 32U

/* 控制台 UART:使用 chosen zephyr,console 指向的设备 */
static const struct device *const console_dev =
	DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

static char consoleLine[FAULT_CONSOLE_LINE_SIZE];
static uint16_t consoleLineLength;

static void FaultConsole_Write(const char *text, void *context)
{
	(void)context;
	printk("%s", text);
}

void FaultConsole_Init(void)
{
	consoleLineLength = 0U;
	printk("[cmd] console ready, type 'help' or JSON {\"cmd\":\"help\"}\r\n");
}

void FaultConsole_Process(void)
{
	unsigned char data;

	if (!device_is_ready(console_dev)) {
		return;
	}

	for (uint32_t budget = 0U; budget < FAULT_CONSOLE_RX_BUDGET; budget++) {
		if (uart_poll_in(console_dev, &data) != 0) {
			break;
		}

		if ((data == '\r') || (data == '\n')) {
			if (consoleLineLength > 0U) {
				consoleLine[consoleLineLength] = '\0';
				FaultCommand_ExecuteLine(consoleLine, FaultConsole_Write, NULL);
				consoleLineLength = 0U;
			}
			continue;
		}

		if (consoleLineLength < (FAULT_CONSOLE_LINE_SIZE - 1U)) {
			consoleLine[consoleLineLength++] = (char)data;
		} else {
			consoleLineLength = 0U;
			printk("[cmd] line too long\r\n");
		}
	}
}
