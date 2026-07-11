/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * CAN BSP 驱动实现。由 STM32 FDCAN HAL 版本移植到 Zephyr CAN 子系统:
 *   - hfdcan1 + HAL_FDCAN_*        -> DEVICE_DT_GET(chosen zephyr,canbus) + can_*
 *   - FDCAN DLC 编解码             -> can_frame.dlc 直接是字节数(classic CAN <=8)
 *   - 轮询 RxFifo + HAL_FDCAN_GetRxMessage -> can_add_rx_filter 回调 + 消息队列
 *   - printf                        -> printk
 *   - HAL_GetTick()                 -> k_uptime_get_32()
 *
 * 接收采用 Zephyr 推荐的 filter 回调把帧投递到消息队列,BspCan_Process 在
 * 主循环上下文批量取出并转交 BspCan_OnRxFrame,保持原有"轮询处理"语义。
 */
#include "drivers/bsp_can.h"

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>

#define CAN_RX_MSGQ_DEPTH 16U

static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

static BspCan_Status canStatus;

/* 接收帧消息队列:filter 回调(中断上下文)入队,BspCan_Process 出队 */
CAN_MSGQ_DEFINE(can_rx_msgq, CAN_RX_MSGQ_DEPTH);
static int rx_filter_id = -1;

__weak void BspCan_OnRxFrame(const BspCan_RxFrame *frame)
{
	(void)frame;
}

int BspCan_Init(void)
{
	int ret;

	memset(&canStatus, 0, sizeof(canStatus));

	if (!device_is_ready(can_dev)) {
		canStatus.errorCount++;
		return -ENODEV;
	}

	/* 接受全部标准/扩展帧(掩码为 0) */
	const struct can_filter filter = {
		.flags = 0U,
		.id = 0U,
		.mask = 0U,
	};

	rx_filter_id = can_add_rx_filter_msgq(can_dev, &can_rx_msgq, &filter);
	if (rx_filter_id < 0) {
		canStatus.errorCount++;
		return rx_filter_id;
	}

	ret = can_start(can_dev);
	if (ret != 0) {
		canStatus.errorCount++;
		return ret;
	}

	canStatus.started = 1U;
	printk("[can] CAN started, classic CAN, accept all frames\r\n");
	return 0;
}

int BspCan_SendClassic(uint32_t id,
		       uint8_t isExtended,
		       const uint8_t *data,
		       uint8_t length)
{
	struct can_frame frame = {0};
	int ret;

	if ((canStatus.started == 0U) ||
	    (length > BSP_CAN_MAX_DATA_LEN) ||
	    ((length > 0U) && (data == NULL))) {
		canStatus.errorCount++;
		return -EINVAL;
	}

	if (((isExtended == 0U) && (id > 0x7FFU)) ||
	    ((isExtended != 0U) && (id > 0x1FFFFFFFU))) {
		canStatus.errorCount++;
		return -EINVAL;
	}

	frame.id = id;
	frame.flags = (isExtended != 0U) ? CAN_FRAME_IDE : 0U;
	frame.dlc = length;
	if (length > 0U) {
		memcpy(frame.data, data, length);
	}

	/* 非阻塞发送(超时 0 语义改为有限等待,保证 TX FIFO 可用) */
	ret = can_send(can_dev, &frame, K_MSEC(10), NULL, NULL);
	if (ret != 0) {
		canStatus.errorCount++;
		return ret;
	}

	canStatus.txCount++;
	return 0;
}

void BspCan_Process(void)
{
	struct can_frame rxFrame;

	while ((canStatus.started != 0U) &&
	       (k_msgq_get(&can_rx_msgq, &rxFrame, K_NO_WAIT) == 0)) {
		BspCan_RxFrame frame = {0};
		uint8_t length = rxFrame.dlc;
		uint8_t isExtended = (rxFrame.flags & CAN_FRAME_IDE) ? 1U : 0U;

		if (length > BSP_CAN_MAX_DATA_LEN) {
			length = BSP_CAN_MAX_DATA_LEN;
		}

		canStatus.rxCount++;
		canStatus.lastRxId = rxFrame.id;
		canStatus.lastRxTick = k_uptime_get_32();
		canStatus.lastRxIsExtended = isExtended;
		canStatus.lastRxLength = length;
		memcpy(canStatus.lastRxData, rxFrame.data, length);

		frame.id = rxFrame.id;
		frame.tick = canStatus.lastRxTick;
		frame.isExtended = isExtended;
		frame.length = length;
		memcpy(frame.data, rxFrame.data, length);
		BspCan_OnRxFrame(&frame);

		printk("[can] rx %s id=0x%X len=%u data=",
		       isExtended ? "ext" : "std",
		       (unsigned int)rxFrame.id,
		       length);
		for (uint8_t index = 0U; index < length; index++) {
			printk("%02X%s", rxFrame.data[index],
			       (index + 1U == length) ? "" : " ");
		}
		printk("\r\n");
	}
}

BspCan_Status BspCan_GetStatus(void)
{
	return canStatus;
}
