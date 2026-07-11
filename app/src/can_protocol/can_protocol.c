/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * CAN 协议层实现(骨架)。实现细节由原 STM32 工程移植填充。
 */
#include "app/can_protocol.h"

void CanProtocol_Init(void)
{
}

void CanProtocol_ProcessRxFrame(uint32_t id, const uint8_t *data, uint8_t len)
{
	(void)id;
	(void)data;
	(void)len;
}

void CanProtocol_SendFeedbackVoltage1_4(void)
{
}

void CanProtocol_SendFeedbackVoltage5_8(void)
{
}

void CanProtocol_SendFeedbackVoltage9_12(void)
{
}

void CanProtocol_SendFeedbackVoltage13(void)
{
}

void CanProtocol_SendFeedbackTemp(void)
{
}

void CanProtocol_SendFeedbackStatus(void)
{
}

void CanProtocol_SendFeedbackOffset1_8(void)
{
}

void CanProtocol_SendFeedbackOffset9_13(void)
{
}
