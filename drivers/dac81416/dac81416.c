/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * DAC81416 芯片级驱动实现。由 STM32 HAL 版本移植:
 *   HAL_StatusTypeDef -> int(0=成功, <0=errno);HAL_Delay -> k_msleep。
 * 帧构建/寄存器逻辑与原实现一致。
 */
#include "drivers/dac81416.h"

#include <stddef.h>
#include <errno.h>

#include <zephyr/kernel.h>

#define DAC81416_READ_TRANSACTION_FRAMES 2U

static int dacLastStatus;

static uint8_t DAC81416_IsValidRange(uint8_t range)
{
	switch (range) {
	case DAC81416_RANGE_0V_TO_5V:
	case DAC81416_RANGE_0V_TO_10V:
	case DAC81416_RANGE_0V_TO_20V:
	case DAC81416_RANGE_0V_TO_40V:
	case DAC81416_RANGE_NEG_5V_TO_5V:
	case DAC81416_RANGE_NEG_10V_TO_10V:
	case DAC81416_RANGE_NEG_20V_TO_20V:
	case DAC81416_RANGE_NEG_2V5_TO_2V5:
		return 1U;

	default:
		return 0U;
	}
}

static uint16_t DAC81416_BuildRangeRegister(uint8_t range)
{
	uint16_t value = (uint16_t)(range & 0x0FU);

	return (uint16_t)(value | (value << 4) | (value << 8) | (value << 12));
}

static uint32_t DAC81416_BuildFrame(uint8_t rw, uint8_t addr, uint16_t data)
{
	uint32_t command = (uint32_t)((rw & DAC81416_CMD_READ) | (addr & 0x3FU));

	return (uint32_t)((command << 16) | data);
}

int DAC81416_Init(void)
{
	BSP_DAC81416_Init();
	DAC_ResetHardware();

	dacLastStatus = DAC_WriteRegister(DAC81416_CMD_WRITE,
					  DAC81416_REG_SPICONFIG,
					  DAC81416_SPICONFIG_ACTIVE_VALUE);
	if (dacLastStatus != 0) {
		return dacLastStatus;
	}

	dacLastStatus = DAC_EnableInternalRef(1U);
	if (dacLastStatus != 0) {
		return dacLastStatus;
	}

	k_msleep(1);

	dacLastStatus = DAC_ConfigAllChannels0To5V();
	if (dacLastStatus != 0) {
		return dacLastStatus;
	}

	dacLastStatus = DAC_PowerDownChannels(0x0000U);
	return dacLastStatus;
}

int DAC_WriteRegister(uint8_t cmd, uint8_t addr, uint16_t data)
{
	uint32_t tx;
	uint8_t rw = (cmd & DAC81416_CMD_READ) ? DAC81416_CMD_READ : DAC81416_CMD_WRITE;

	tx = DAC81416_BuildFrame(rw, addr, data);
	dacLastStatus = DAC_SPI_TransmitFrames24(&tx, 1U);
	return dacLastStatus;
}

uint16_t DAC_ReadRegister(uint8_t cmd, uint8_t addr)
{
	uint32_t tx[DAC81416_READ_TRANSACTION_FRAMES];
	uint32_t rx[DAC81416_READ_TRANSACTION_FRAMES] = {0U, 0U};
	uint8_t readCmd = (cmd & DAC81416_CMD_READ) ? cmd : DAC81416_CMD_READ;

	tx[0] = DAC81416_BuildFrame(readCmd, addr, 0x0000U);
	tx[1] = DAC81416_BuildFrame(DAC81416_CMD_WRITE, DAC81416_REG_NOP, 0x0000U);
	dacLastStatus = DAC_SPI_TransmitReceiveFrames24(tx, rx, DAC81416_READ_TRANSACTION_FRAMES);
	if (dacLastStatus != 0) {
		return 0U;
	}

	return (uint16_t)(rx[1] & 0xFFFFU);
}

int DAC_SoftwareReset(void)
{
	dacLastStatus = DAC_WriteRegister(DAC81416_CMD_WRITE,
					  DAC81416_REG_TRIGGER,
					  DAC81416_TRIGGER_SOFT_RESET);
	k_msleep(1);
	return dacLastStatus;
}

int DAC_WriteChannel(uint8_t channel, uint16_t value)
{
	if (channel >= DAC81416_CHANNEL_COUNT) {
		dacLastStatus = -EINVAL;
		return dacLastStatus;
	}

	dacLastStatus = DAC_WriteRegister(DAC81416_CMD_WRITE,
					  (uint8_t)(DAC81416_REG_DAC0 + channel),
					  value);
	return dacLastStatus;
}

int DAC_WriteAllChannels(uint16_t *values)
{
	int status = 0;

	if (values == NULL) {
		dacLastStatus = -EINVAL;
		return dacLastStatus;
	}

	for (uint8_t channel = 0U; channel < DAC81416_CHANNEL_COUNT; channel++) {
		status = DAC_WriteChannel(channel, values[channel]);
		if (status != 0) {
			dacLastStatus = status;
			return status;
		}
	}

	DAC_LDAC_Update();
	dacLastStatus = 0;
	return dacLastStatus;
}

int DAC_EnableInternalRef(uint8_t enable)
{
	uint16_t reg = DAC_ReadRegister(DAC81416_CMD_READ, DAC81416_REG_GENCONFIG);

	if (dacLastStatus != 0) {
		return dacLastStatus;
	}

	if (enable != 0U) {
		reg &= (uint16_t)~DAC81416_GENCONFIG_REF_PWDWN;
	} else {
		reg |= DAC81416_GENCONFIG_REF_PWDWN;
	}

	dacLastStatus = DAC_WriteRegister(DAC81416_CMD_WRITE, DAC81416_REG_GENCONFIG, reg);
	return dacLastStatus;
}

int DAC_SetAllChannelsRange(uint8_t range)
{
	uint16_t rangeValue;

	if (DAC81416_IsValidRange(range) == 0U) {
		dacLastStatus = -EINVAL;
		return dacLastStatus;
	}

	rangeValue = DAC81416_BuildRangeRegister(range);

	for (uint8_t index = 0U; index < 4U; index++) {
		dacLastStatus = DAC_WriteRegister(DAC81416_CMD_WRITE,
						  (uint8_t)(DAC81416_REG_DACRANGE0 + index),
						  rangeValue);
		if (dacLastStatus != 0) {
			return dacLastStatus;
		}
	}

	return dacLastStatus;
}

int DAC_ConfigAllChannels0To5V(void)
{
	return DAC_SetAllChannelsRange(DAC81416_RANGE_0V_TO_5V);
}

int DAC_PowerDownChannels(uint16_t powerDownMask)
{
	dacLastStatus = DAC_WriteRegister(DAC81416_CMD_WRITE,
					  DAC81416_REG_DACPWDWN,
					  powerDownMask);
	return dacLastStatus;
}

int DAC_TriggerLDAC(void)
{
	dacLastStatus = DAC_WriteRegister(DAC81416_CMD_WRITE,
					  DAC81416_REG_TRIGGER,
					  DAC81416_TRIGGER_LDAC);
	return dacLastStatus;
}

int DAC_GetLastStatus(void)
{
	return dacLastStatus;
}
