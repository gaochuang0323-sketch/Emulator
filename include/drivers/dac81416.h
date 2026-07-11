/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * DAC81416 芯片级驱动 —— 寄存器/通道抽象,构建 24 位帧并经 bsp_dac81416 SPI 传输。
 * 由 STM32 HAL 版本移植到 Zephyr:返回值 HAL_StatusTypeDef -> int(0=成功, <0=errno)。
 * 上层 voltage_sim / waveform_engine 通过本层的 DAC_WriteChannel 等访问 DAC。
 */
#ifndef INCLUDE_DRIVERS_DAC81416_H_
#define INCLUDE_DRIVERS_DAC81416_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "drivers/bsp_dac81416.h"

#define DAC81416_CHANNEL_COUNT 16U

#define DAC81416_CMD_WRITE 0x00U
#define DAC81416_CMD_READ  0xC0U

#define DAC81416_REG_NOP        0x00U
#define DAC81416_REG_DEVICEID   0x01U
#define DAC81416_REG_STATUS     0x02U
#define DAC81416_REG_SPICONFIG  0x03U
#define DAC81416_REG_GENCONFIG  0x04U
#define DAC81416_REG_BRDCONFIG  0x05U
#define DAC81416_REG_SYNCCONFIG 0x06U
#define DAC81416_REG_DACPWDWN   0x09U
#define DAC81416_REG_DACRANGE0  0x0AU
#define DAC81416_REG_DACRANGE1  0x0BU
#define DAC81416_REG_DACRANGE2  0x0CU
#define DAC81416_REG_DACRANGE3  0x0DU
#define DAC81416_REG_TRIGGER    0x0EU
#define DAC81416_REG_BRDCAST    0x0FU
#define DAC81416_REG_DAC0       0x10U

#define DAC81416_SPICONFIG_RESET_VALUE  0x0AA4U
#define DAC81416_SPICONFIG_ACTIVE_VALUE 0x0A84U
#define DAC81416_SPICONFIG_DEV_PWDWN    (1U << 5)
#define DAC81416_SPICONFIG_SDO_EN       (1U << 2)

#define DAC81416_GENCONFIG_REF_PWDWN    (1U << 14)

#define DAC81416_TRIGGER_LDAC           (1U << 4)
#define DAC81416_TRIGGER_SOFT_RESET     0x1010U

#define DAC81416_RANGE_0V_TO_5V         0x0U
#define DAC81416_RANGE_0V_TO_10V        0x1U
#define DAC81416_RANGE_0V_TO_20V        0x2U
#define DAC81416_RANGE_0V_TO_40V        0x4U
#define DAC81416_RANGE_NEG_5V_TO_5V     0x9U
#define DAC81416_RANGE_NEG_10V_TO_10V   0xAU
#define DAC81416_RANGE_NEG_20V_TO_20V   0xCU
#define DAC81416_RANGE_NEG_2V5_TO_2V5   0xEU

int DAC81416_Init(void);
int DAC_WriteRegister(uint8_t cmd, uint8_t addr, uint16_t data);
uint16_t DAC_ReadRegister(uint8_t cmd, uint8_t addr);
int DAC_SoftwareReset(void);
int DAC_WriteChannel(uint8_t channel, uint16_t value);
int DAC_WriteAllChannels(uint16_t *values);
int DAC_EnableInternalRef(uint8_t enable);
int DAC_SetAllChannelsRange(uint8_t range);
int DAC_ConfigAllChannels0To5V(void);
int DAC_PowerDownChannels(uint16_t powerDownMask);
int DAC_TriggerLDAC(void);
int DAC_GetLastStatus(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_DRIVERS_DAC81416_H_ */
