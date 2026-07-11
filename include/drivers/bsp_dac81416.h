/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * DAC81416 —— 16 通道 16 位精密 DAC 驱动 API。
 * 由 STM32 HAL 版本移植到 Zephyr:去除对 spi.h / main.h / GPIO 宏的直接依赖,
 * SPI 与 GPIO(CS/RESET/LDAC/ALARM)改由 devicetree 绑定,返回值统一为 int。
 *
 * 上层(voltage_sim / waveform_engine)只调用本 API,不接触具体 MCU 外设 ——
 * 这是"换 MCU / 换 DAC"的隔离边界。
 */
#ifndef INCLUDE_DRIVERS_BSP_DAC81416_H_
#define INCLUDE_DRIVERS_BSP_DAC81416_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DAC_SPI_DEFAULT_TIMEOUT_MS 100U

/* GPIO 电平抽象(替代 HAL 的 GPIO_PinState) */
#define DAC_PIN_RESET 0
#define DAC_PIN_SET   1

void BSP_DAC81416_Init(void);
void BSP_DAC81416_SetTimeout(uint32_t timeoutMs);
uint32_t BSP_DAC81416_GetTimeout(void);

int DAC_SPI_Transmit(uint8_t *txData, uint16_t size);
int DAC_SPI_TransmitReceive(uint8_t *txData, uint8_t *rxData, uint16_t size);

/* CS 在整个调用期间保持拉低。24 位帧,frameCount=2 产生 DAC81416 读操作
 * 所需的 48 SCLK 事务。
 */
int DAC_SPI_TransmitFrames24(const uint32_t *txFrames, uint16_t frameCount);
int DAC_SPI_TransmitReceiveFrames24(const uint32_t *txFrames,
				    uint32_t *rxFrames,
				    uint16_t frameCount);

void DAC_SetResetPin(int state);
void DAC_ResetHardware(void);
void DAC_SetLDACPin(int state);
void DAC_LDAC_Update(void);
uint8_t DAC_ReadAlarmPin(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_DRIVERS_BSP_DAC81416_H_ */
