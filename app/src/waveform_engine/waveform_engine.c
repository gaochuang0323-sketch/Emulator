/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 波形引擎实现。由 STM32 HAL 版本移植到 Zephyr。
 *
 * 【移植说明 —— 必读】
 * 本模块原实现深度依赖 STM32H7 的寄存器级机制,这些在 Zephyr 里没有可移植的
 * 等价物,无法机械翻译,需按目标平台重新设计硬件路径:
 *   - 循环 DMA 从 FMC 外部 SRAM 直灌 SPI1->TXDR(DMA_CIRCULAR)
 *   - TIM3 更新事件触发 DMA(DMA_REQUEST_TIM3_UP)实现 turbo 模式
 *   - 直接操作 SPI CR1_CSTART / CFG1_TXDMAEN / CR2_TSIZE 寄存器
 *   - SCB D-Cache 维护(SCB_CleanDCache_by_Addr)
 *   - HAL 的 DMA/SPI/TIM 完成中断回调
 * 在 Zephyr 上应改用:DMA 驱动(<zephyr/drivers/dma.h>)+ Counter/Timer +
 * SPI 异步 API(spi_transceive_signal / callback),或平台专用 DMA 链路。
 *
 * 因此下列**纯逻辑**已忠实移植(与原实现一致):
 *   - 状态/配置管理、参数校验、正弦表生成、DAC 帧构建、状态记账
 * 而**硬件启动/停止/中断路径**保留结构与状态更新,底层外设调用以 TODO 标出,
 * 待接入具体平台的 DMA/Timer/SPI 实现。不臆造寄存器操作。
 */
#include "app/waveform_engine.h"

#include <math.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "drivers/bsp_dac81416.h"
#include "drivers/dac81416.h"
#include "app/voltage_sim.h"
#include "app/waveform_gen.h"

LOG_MODULE_REGISTER(waveform_engine, CONFIG_LOG_DEFAULT_LEVEL);

#define WAVEFORM_ENGINE_MAX_CHANNELS      13U
#define WAVEFORM_ENGINE_MIN_TIMER_RATE_HZ 1U

typedef enum {
	WAVEFORM_ENGINE_MODE_STOPPED = 0,
	WAVEFORM_ENGINE_MODE_IRQ_DMA,
	WAVEFORM_ENGINE_MODE_TIM_DMA_TURBO
} WaveformEngine_Mode;

static WaveformEngine_Status engineStatus;
static WaveformEngine_Config activeConfig;

/* 帧表:原实现放在 FMC 外部 SRAM(BSP_SRAM_BASE_ADDRESS)供循环 DMA 直取。
 * 在平台 DMA 路径落地前,先用静态缓冲承载,保证纯逻辑可编译、可单元测试。
 * 接入 DMA 时可改为指向 SRAM 映射地址。
 */
static uint32_t frameTable[WAVEFORM_ENGINE_DMA_BUFFER_SIZE / sizeof(uint32_t)];
static uint32_t totalSamplePoints;
static uint32_t currentSamplePoint;
static volatile uint8_t txBusy;
static WaveformEngine_Mode engineMode;

static uint8_t WaveformEngine_IsValidConfig(const WaveformEngine_Config *config)
{
	uint32_t frameCount;
	uint32_t maxFrameCount = WAVEFORM_ENGINE_DMA_BUFFER_SIZE / sizeof(uint32_t);

	if ((config == NULL) ||
	    (config->sampleRate < WAVEFORM_ENGINE_MIN_TIMER_RATE_HZ) ||
	    (config->pointCount == 0U) ||
	    (config->pointCount > WAVEFORM_ENGINE_SINE_TABLE_SIZE) ||
	    (config->channels == 0U) ||
	    (config->channels > WAVEFORM_ENGINE_MAX_CHANNELS) ||
	    (config->amplitudeMv == 0U)) {
		return 0U;
	}

	frameCount = config->pointCount * config->channels;
	if ((frameCount == 0U) || (frameCount > maxFrameCount)) {
		return 0U;
	}

	for (uint8_t index = 0U; index < config->channels; index++) {
		if ((config->channelList[index] == 0U) ||
		    (config->channelList[index] > VOLTAGE_SIM_CELL_COUNT)) {
			return 0U;
		}
	}

	return 1U;
}

static uint32_t WaveformEngine_BuildDacFrame(uint8_t cell, uint16_t dacCode)
{
	uint8_t channel = (uint8_t)(cell - 1U);
	uint8_t command = (uint8_t)(DAC81416_CMD_WRITE | ((DAC81416_REG_DAC0 + channel) & 0x3FU));

	return ((uint32_t)command << 16) | (uint32_t)dacCode;
}

/*
 * 采样率 -> 定时器配置。原实现直接算 TIM3 预分频/自动重装并写寄存器。
 * Zephyr 上应改用 counter_set_channel_alarm / PWM / 平台定时器。
 * 这里仅做参数合法性检查,硬件配置留待平台实现。
 */
static int WaveformEngine_SetSampleRate(uint32_t sampleRate)
{
	if (sampleRate == 0U) {
		return -EINVAL;
	}

	/* TODO(platform): 用 Zephyr Counter/Timer 按 sampleRate 配置周期性触发源 */
	return 0;
}

/*
 * turbo 模式:TIM 触发的循环 DMA。需平台 DMA 驱动实现。
 */
static int WaveformEngine_StartTimDmaTurbo(void)
{
	if (activeConfig.channels != 1U) {
		return -EINVAL;
	}

	/* TODO(platform): 配置循环 DMA(frameTable -> SPI TXDR),由定时器更新事件触发 */
	engineMode = WAVEFORM_ENGINE_MODE_TIM_DMA_TURBO;
	return 0;
}

/*
 * IRQ+DMA 模式:定时器中断里逐点发起 SPI DMA。需平台定时器中断 + SPI 异步实现。
 */
static int WaveformEngine_StartIrqDma(void)
{
	/* TODO(platform): 启动周期性定时器中断,回调驱动 WaveformEngine_TimerElapsed */
	engineMode = WAVEFORM_ENGINE_MODE_IRQ_DMA;
	return 0;
}

static void WaveformEngine_StopHardware(void)
{
	/* TODO(platform): 停止定时器、终止 DMA、恢复 SPI 到正常模式 */
	DAC_SetLDACPin(DAC_PIN_SET);
	txBusy = 0U;
	engineMode = WAVEFORM_ENGINE_MODE_STOPPED;
}

void WaveformEngine_Init(void)
{
	memset(&engineStatus, 0, sizeof(engineStatus));
	memset(&activeConfig, 0, sizeof(activeConfig));
	totalSamplePoints = 0U;
	currentSamplePoint = 0U;
	txBusy = 0U;
	engineMode = WAVEFORM_ENGINE_MODE_STOPPED;
}

int WaveformEngine_GenerateSineTable(uint16_t *table,
				     uint32_t points,
				     uint16_t amplitude,
				     uint16_t offset)
{
	if ((table == NULL) ||
	    (points == 0U) ||
	    (points > WAVEFORM_ENGINE_SINE_TABLE_SIZE)) {
		return -EINVAL;
	}

	for (uint32_t i = 0U; i < points; i++) {
		double rad = 2.0 * 3.141592653589793 * (double)i / (double)points;
		double mv = (double)offset + (double)amplitude * sin(rad);

		if (mv < (double)VOLTAGE_SIM_MIN_MV) {
			mv = (double)VOLTAGE_SIM_MIN_MV;
		}
		if (mv > (double)VOLTAGE_SIM_MAX_MV) {
			mv = (double)VOLTAGE_SIM_MAX_MV;
		}

		table[i] = VoltageSim_MillivoltsToDacCode((uint16_t)mv);
	}

	return 0;
}

int WaveformEngine_StartSine(WaveformEngine_Config *config)
{
	uint16_t sineCodes[WAVEFORM_ENGINE_SINE_TABLE_SIZE];

	if (config == NULL) {
		return -EINVAL;
	}

	if (config->cycleCount == 0U) {
		config->cycleCount = 1U;
	}

	if (WaveformEngine_IsValidConfig(config) == 0U) {
		return -EINVAL;
	}

	if (engineStatus.running != 0U) {
		WaveformEngine_Stop();
	}

	WaveformGen_Stop();

	if (WaveformEngine_GenerateSineTable(sineCodes,
					     config->pointCount,
					     config->amplitudeMv,
					     config->offsetMv) != 0) {
		return -EIO;
	}

	for (uint32_t point = 0U; point < config->pointCount; point++) {
		for (uint8_t ch = 0U; ch < config->channels; ch++) {
			uint32_t frameIndex = (point * config->channels) + ch;

			frameTable[frameIndex] =
				WaveformEngine_BuildDacFrame(config->channelList[ch],
							     sineCodes[point]);
		}
	}

	if (WaveformEngine_SetSampleRate(config->sampleRate) != 0) {
		return -EIO;
	}

	memcpy(&activeConfig, config, sizeof(activeConfig));
	activeConfig.active = 1U;
	totalSamplePoints = activeConfig.pointCount;
	currentSamplePoint = 0U;
	txBusy = 0U;

	engineStatus.running = 1U;
	engineStatus.bufferIndex = 0U;
	engineStatus.sampleCount = 0U;
	engineStatus.errorCount = 0U;

	if (((activeConfig.channels == 1U) ? WaveformEngine_StartTimDmaTurbo()
					   : WaveformEngine_StartIrqDma()) != 0) {
		WaveformEngine_StopHardware();
		activeConfig.active = 0U;
		engineStatus.running = 0U;
		return -EIO;
	}

	printk("[wave-high] started sample=%luHz points=%lu ch=%u mode=%s\r\n",
	       (unsigned long)activeConfig.sampleRate,
	       (unsigned long)activeConfig.pointCount,
	       activeConfig.channels,
	       (engineMode == WAVEFORM_ENGINE_MODE_TIM_DMA_TURBO) ? "tim-dma" : "irq-dma");

	return 0;
}

void WaveformEngine_Stop(void)
{
	if (engineStatus.running == 0U) {
		return;
	}

	WaveformEngine_StopHardware();
	activeConfig.active = 0U;
	engineStatus.running = 0U;

	printk("[wave-high] stopped samples=%lu errors=%lu\r\n",
	       (unsigned long)engineStatus.sampleCount,
	       (unsigned long)engineStatus.errorCount);
}

WaveformEngine_Status WaveformEngine_GetStatus(void)
{
	return engineStatus;
}

void WaveformEngine_TimerElapsed(void)
{
	uint32_t frameOffset;

	if ((engineStatus.running == 0U) || (engineMode != WAVEFORM_ENGINE_MODE_IRQ_DMA)) {
		return;
	}

	if (txBusy != 0U) {
		engineStatus.errorCount++;
		return;
	}

	frameOffset = currentSamplePoint * activeConfig.channels;

	/* TODO(platform): 从 frameTable[frameOffset] 起发起 activeConfig.channels 帧的
	 * 异步 SPI 传输(24 位帧);完成回调里脉冲 LDAC 并清 txBusy。
	 */
	(void)frameOffset;

	txBusy = 1U;
	currentSamplePoint++;
	if (currentSamplePoint >= totalSamplePoints) {
		currentSamplePoint = 0U;
	}
}

void WaveformEngine_DmaHalfComplete(void)
{
	engineStatus.bufferIndex = 1U;
}

void WaveformEngine_DmaComplete(void)
{
	engineStatus.bufferIndex = 0U;
}

void WaveformEngine_DmaError(void)
{
	engineStatus.errorCount++;
	WaveformEngine_StopHardware();
	activeConfig.active = 0U;
	engineStatus.running = 0U;
}
