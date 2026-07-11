/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * DAC81416 驱动实现 —— Zephyr 版本(样板)。
 *
 * 由原 STM32 HAL 版本移植:
 *   - hspi1 直调          -> devicetree spi_dt_spec + spi_transceive()
 *   - CS GPIO 手动拉低/高  -> 交给 SPI 子系统按 cs-gpios 自动管理
 *   - RESET/LDAC/ALARM    -> devicetree 带外 gpio_dt_spec(dac_ctrl 节点)
 *   - osMutex             -> k_mutex
 *   - HAL_Delay           -> k_msleep / k_busy_wait
 *
 * 24 位帧按 MSB-first、3 字节大端在 8 位字长 SPI 上传输。
 */
#include "drivers/bsp_dac81416.h"

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bsp_dac81416, CONFIG_LOG_DEFAULT_LEVEL);

/* SPI 数据节点 */
#define DAC_SPI_NODE  DT_NODELABEL(dac81416)
/* 带外控制引脚节点(RESET / LDAC / ALARM) */
#define DAC_CTRL_NODE DT_NODELABEL(dac_ctrl)

#define DAC_FRAME_BYTES 3U   /* 24 位帧 = 3 字节 */
#define DAC_MAX_FRAMES  16U  /* DAC81416 共 16 通道,单次事务帧数上限 */

static const struct spi_dt_spec dac_spi =
	SPI_DT_SPEC_GET(DAC_SPI_NODE,
			SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
			0);

static const struct gpio_dt_spec dac_reset =
	GPIO_DT_SPEC_GET(DAC_CTRL_NODE, reset_gpios);
static const struct gpio_dt_spec dac_ldac =
	GPIO_DT_SPEC_GET(DAC_CTRL_NODE, ldac_gpios);
static const struct gpio_dt_spec dac_alarm =
	GPIO_DT_SPEC_GET(DAC_CTRL_NODE, alarm_gpios);

static struct k_mutex dac_spi_mutex;
static uint32_t dac_spi_timeout_ms = DAC_SPI_DEFAULT_TIMEOUT_MS;

/* 把 24 位帧数组打包成大端字节缓冲 */
static void frames_to_bytes(const uint32_t *frames, uint16_t count, uint8_t *buf)
{
	for (uint16_t i = 0; i < count; i++) {
		buf[i * DAC_FRAME_BYTES + 0U] = (uint8_t)(frames[i] >> 16);
		buf[i * DAC_FRAME_BYTES + 1U] = (uint8_t)(frames[i] >> 8);
		buf[i * DAC_FRAME_BYTES + 2U] = (uint8_t)(frames[i]);
	}
}

static void bytes_to_frames(const uint8_t *buf, uint16_t count, uint32_t *frames)
{
	for (uint16_t i = 0; i < count; i++) {
		frames[i] = ((uint32_t)buf[i * DAC_FRAME_BYTES + 0U] << 16) |
			    ((uint32_t)buf[i * DAC_FRAME_BYTES + 1U] << 8) |
			    ((uint32_t)buf[i * DAC_FRAME_BYTES + 2U]);
	}
}

void BSP_DAC81416_Init(void)
{
	k_mutex_init(&dac_spi_mutex);

	if (!spi_is_ready_dt(&dac_spi)) {
		LOG_ERR("DAC SPI bus not ready");
		return;
	}
	if (!gpio_is_ready_dt(&dac_reset) ||
	    !gpio_is_ready_dt(&dac_ldac) ||
	    !gpio_is_ready_dt(&dac_alarm)) {
		LOG_ERR("DAC control GPIOs not ready");
		return;
	}

	/* RESET / LDAC 默认高(非激活),ALARM 为输入 */
	(void)gpio_pin_configure_dt(&dac_reset, GPIO_OUTPUT_INACTIVE);
	(void)gpio_pin_configure_dt(&dac_ldac, GPIO_OUTPUT_INACTIVE);
	(void)gpio_pin_configure_dt(&dac_alarm, GPIO_INPUT);
}

void BSP_DAC81416_SetTimeout(uint32_t timeoutMs)
{
	dac_spi_timeout_ms = timeoutMs;
}

uint32_t BSP_DAC81416_GetTimeout(void)
{
	return dac_spi_timeout_ms;
}

int DAC_SPI_Transmit(uint8_t *txData, uint16_t size)
{
	uint32_t frame;

	if ((txData == NULL) || (size != DAC_FRAME_BYTES)) {
		return -EINVAL;
	}

	frame = ((uint32_t)txData[0] << 16) |
		((uint32_t)txData[1] << 8) |
		(uint32_t)txData[2];

	return DAC_SPI_TransmitFrames24(&frame, 1U);
}

int DAC_SPI_TransmitReceive(uint8_t *txData, uint8_t *rxData, uint16_t size)
{
	uint32_t txFrame;
	uint32_t rxFrame = 0U;
	int ret;

	if ((txData == NULL) || (rxData == NULL) || (size != DAC_FRAME_BYTES)) {
		return -EINVAL;
	}

	txFrame = ((uint32_t)txData[0] << 16) |
		  ((uint32_t)txData[1] << 8) |
		  (uint32_t)txData[2];

	ret = DAC_SPI_TransmitReceiveFrames24(&txFrame, &rxFrame, 1U);
	if (ret == 0) {
		rxData[0] = (uint8_t)(rxFrame >> 16);
		rxData[1] = (uint8_t)(rxFrame >> 8);
		rxData[2] = (uint8_t)rxFrame;
	}

	return ret;
}

int DAC_SPI_TransmitFrames24(const uint32_t *txFrames, uint16_t frameCount)
{
	uint8_t tx_buf[DAC_FRAME_BYTES * DAC_MAX_FRAMES];
	int ret;

	if ((txFrames == NULL) || (frameCount == 0U) ||
	    (frameCount > DAC_MAX_FRAMES)) {
		return -EINVAL;
	}

	frames_to_bytes(txFrames, frameCount, tx_buf);

	const struct spi_buf buf = {
		.buf = tx_buf,
		.len = (size_t)frameCount * DAC_FRAME_BYTES,
	};
	const struct spi_buf_set tx = { .buffers = &buf, .count = 1U };

	ret = k_mutex_lock(&dac_spi_mutex, K_MSEC(dac_spi_timeout_ms));
	if (ret != 0) {
		return ret;
	}

	/* CS 由 SPI 子系统按 cs-gpios 在整次传输期间自动保持拉低 */
	ret = spi_write_dt(&dac_spi, &tx);

	k_mutex_unlock(&dac_spi_mutex);
	return ret;
}

int DAC_SPI_TransmitReceiveFrames24(const uint32_t *txFrames,
				    uint32_t *rxFrames,
				    uint16_t frameCount)
{
	uint8_t tx_buf[DAC_FRAME_BYTES * DAC_MAX_FRAMES];
	uint8_t rx_buf[DAC_FRAME_BYTES * DAC_MAX_FRAMES];
	int ret;

	if ((txFrames == NULL) || (rxFrames == NULL) || (frameCount == 0U) ||
	    (frameCount > DAC_MAX_FRAMES)) {
		return -EINVAL;
	}

	frames_to_bytes(txFrames, frameCount, tx_buf);

	const struct spi_buf txb = {
		.buf = tx_buf,
		.len = (size_t)frameCount * DAC_FRAME_BYTES,
	};
	const struct spi_buf rxb = {
		.buf = rx_buf,
		.len = (size_t)frameCount * DAC_FRAME_BYTES,
	};
	const struct spi_buf_set tx = { .buffers = &txb, .count = 1U };
	const struct spi_buf_set rx = { .buffers = &rxb, .count = 1U };

	ret = k_mutex_lock(&dac_spi_mutex, K_MSEC(dac_spi_timeout_ms));
	if (ret != 0) {
		return ret;
	}

	ret = spi_transceive_dt(&dac_spi, &tx, &rx);

	k_mutex_unlock(&dac_spi_mutex);

	if (ret == 0) {
		bytes_to_frames(rx_buf, frameCount, rxFrames);
	}

	return ret;
}

void DAC_SetResetPin(int state)
{
	/* reset-gpios 为 ACTIVE_LOW:逻辑值 1=复位有效(引脚拉低) */
	(void)gpio_pin_set_dt(&dac_reset, state ? 0 : 1);
}

void DAC_ResetHardware(void)
{
	DAC_SetResetPin(DAC_PIN_RESET);
	k_msleep(10);
	DAC_SetResetPin(DAC_PIN_SET);
	k_msleep(10);
}

void DAC_SetLDACPin(int state)
{
	(void)gpio_pin_set_dt(&dac_ldac, state ? 0 : 1);
}

void DAC_LDAC_Update(void)
{
	/* 脉冲 LDAC:拉低锁存 -> 拉高。1us 足够 DAC81416 采样 */
	DAC_SetLDACPin(DAC_PIN_RESET);
	k_busy_wait(1);
	DAC_SetLDACPin(DAC_PIN_SET);
}

uint8_t DAC_ReadAlarmPin(void)
{
	return (gpio_pin_get_dt(&dac_alarm) > 0) ? 1U : 0U;
}
