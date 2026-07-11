/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 以太网 PHY BSP 驱动实现。由 STM32 HAL 版本移植:
 *   - HAL_GPIO_WritePin + 板级引脚宏 -> devicetree gpio_dt_spec
 *   - HAL_Delay -> k_msleep
 *
 * PHY 复位引脚可选:板级 devicetree 若在 &mac 节点(或专用节点)提供
 * phy-reset-gpios 则执行硬复位;否则同原硬件由上电复位处理,本函数空转。
 */
#include "drivers/bsp_eth.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* 复位引脚在 chosen 节点 zephyr,eth-phy-reset 或 &mac 的 reset-gpios 上描述。
 * 用 DT_NODELABEL(mac) + reset_gpios 作为可选属性。
 */
#define ETH_PHY_NODE DT_NODELABEL(mac)

#if DT_NODE_HAS_PROP(ETH_PHY_NODE, reset_gpios)
static const struct gpio_dt_spec eth_reset =
	GPIO_DT_SPEC_GET(ETH_PHY_NODE, reset_gpios);
#endif

void BSP_ETH_PHY_Reset(void)
{
#if DT_NODE_HAS_PROP(ETH_PHY_NODE, reset_gpios)
	if (!gpio_is_ready_dt(&eth_reset)) {
		return;
	}

	/* reset-gpios 若声明为 ACTIVE_LOW,逻辑 1=复位有效 */
	(void)gpio_pin_configure_dt(&eth_reset, GPIO_OUTPUT_ACTIVE);
	k_msleep(200);
	(void)gpio_pin_set_dt(&eth_reset, 0);
	k_msleep(300);
#else
	/*
	 * 当前硬件将 ETH_nRST 上拉且未接到 MCU,PHY 复位由上电复位处理。
	 */
#endif
}
