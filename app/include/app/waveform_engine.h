/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 波形引擎:高频正弦波 DMA 输出与正弦表预计算。
 * 由 STM32 HAL 版本移植到 Zephyr —— 返回值改为 int(0=成功, <0=errno),
 * 不再直接依赖 HAL。
 */
#ifndef APP_WAVEFORM_ENGINE_H_
#define APP_WAVEFORM_ENGINE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define WAVEFORM_ENGINE_SINE_TABLE_SIZE  1024U   /* 正弦表最大点数 */
#define WAVEFORM_ENGINE_DMA_BUFFER_SIZE  8192U   /* DMA 缓冲区大小（字节）*/

/* 波形参数配置 */
typedef struct
{
  uint8_t  active;                         /* 1=运行中 */
  uint8_t  channels;                       /* 输出通道数（1 或 13）*/
  uint8_t  channelList[13];                /* 通道号列表 */
  uint32_t sampleRate;                     /* 采样率（Hz）*/
  uint32_t pointCount;                     /* 每周期点数 */
  uint32_t cycleCount;                     /* 缓冲区内周期数 */
  uint16_t amplitudeMv;                    /* 正弦波幅值（mV）*/
  uint16_t offsetMv;                       /* 正弦波中心电压（mV）*/
} WaveformEngine_Config;

/* 波形引擎状态 */
typedef struct
{
  volatile uint8_t  running;               /* DMA 是否正在运行 */
  volatile uint8_t  bufferIndex;           /* 当前使用的缓冲区（0 或 1）*/
  uint32_t sampleCount;                    /* 已输出采样点数 */
  uint32_t errorCount;                     /* DMA 错误计数 */
} WaveformEngine_Status;

/* ===== 接口函数 ===== */

/* 初始化波形引擎（清空状态）*/
void WaveformEngine_Init(void);

/* 启动高频正弦波输出
   config: 波形参数配置（采样率/点数/幅值/通道等）
   返回 0 成功，<0 参数错误 */
int WaveformEngine_StartSine(WaveformEngine_Config *config);

/* 停止高频波形输出
   停止 TIM、DMA，恢复 SPI 到正常模式 */
void WaveformEngine_Stop(void);

/* 查询波形引擎状态 */
WaveformEngine_Status WaveformEngine_GetStatus(void);

/* 在外部 SRAM 中预计算正弦波形表
   table:     SRAM 地址指针
   points:    表长度（点数）
   amplitude: 幅值（mV）
   offset:    中心电压（mV）
   返回 0 成功 */
int WaveformEngine_GenerateSineTable(uint16_t *table,
				     uint32_t points,
				     uint16_t amplitude,
				     uint16_t offset);

/* ===== DMA 回调（由中断处理函数调用）===== */
void WaveformEngine_DmaHalfComplete(void);
void WaveformEngine_DmaComplete(void);
void WaveformEngine_DmaError(void);
void WaveformEngine_TimerElapsed(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_WAVEFORM_ENGINE_H_ */
