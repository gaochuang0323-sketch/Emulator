/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * CAN 协议层:CAN ID 定义、小端打包/解包宏与收发帧处理。
 * 由 STM32 HAL 版本移植到 Zephyr —— 不再直接依赖 HAL。
 */
#ifndef APP_CAN_PROTOCOL_H_
#define APP_CAN_PROTOCOL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ==================================================================== */
/*  CAN ID 定义 — 基于 frame.csv                                        */
/* ==================================================================== */

/* --- 接收帧 (PC → 模拟器) --- */
#define CAN_ID_PC_MSG1      0x011U   /* 设通道 1~4 电压 */
#define CAN_ID_PC_MSG2      0x012U   /* 设通道 5~8 电压 */
#define CAN_ID_PC_MSG3      0x013U   /* 设通道 9~12 电压 */
#define CAN_ID_PC_MSG4      0x014U   /* 通道13 + 更新激活 + 波形 + 急停 */
#define CAN_ID_PC_MSG5      0x015U   /* 设温度 1~7 */
#define CAN_ID_PC_MSG6      0x016U   /* 设偏移 1~8 */
#define CAN_ID_PC_MSG7      0x017U   /* 设偏移 9~13 + 激活码 */

/* --- 发送帧 (模拟器 → PC) --- */
#define CAN_ID_RP_MSG2      0x01AU   /* 电压反馈 1~4, 500ms */
#define CAN_ID_RP_MSG3      0x01BU   /* 电压反馈 5~8, 500ms */
#define CAN_ID_RP_MSG4      0x01CU   /* 电压反馈 9~12, 500ms */
#define CAN_ID_RP_MSG5      0x01DU   /* 电压反馈13 + 波形, 500ms */
#define CAN_ID_RP_MSG6      0x01EU   /* 温度反馈, 500ms */
#define CAN_ID_RP_MSG7      0x01FU   /* 偏移反馈 1~8, 触发后500ms */
#define CAN_ID_RP_MSG8      0x020U   /* 偏移反馈 9~13, 触发后500ms */
#define CAN_ID_RP_MSG9      0x021U   /* 状态反馈, 500ms */

/* ==================================================================== */
/*  宏：Intel 小端打包/解包                                             */
/* ==================================================================== */

/* 从 buffer 读取 uint16 LE */
#define GET_U16_LE(ptr)  ((uint16_t)(ptr)[0] | ((uint16_t)(ptr)[1] << 8U))

/* 写入 uint16 LE 到 buffer */
#define SET_U16_LE(ptr, val) do {          \
    (ptr)[0] = (uint8_t)((val) & 0xFFU);   \
    (ptr)[1] = (uint8_t)(((val) >> 8U) & 0xFFU); \
} while (0)

/* ==================================================================== */
/*  API 函数声明                                                         */
/* ==================================================================== */

/* 初始化协议层 */
void CanProtocol_Init(void);

/**
 * 接收帧处理入口 — 由 BspCan_OnRxFrame 回调调用
 * @param id    CAN ID
 * @param data  8 字节数据指针
 * @param len   数据长度 (固定 8)
 */
void CanProtocol_ProcessRxFrame(uint32_t id, const uint8_t *data, uint8_t len);

/* ========== 周期性发送（由主循环每 500ms 调用）========== */
void CanProtocol_SendFeedbackVoltage1_4(void);   /* 0x01A */
void CanProtocol_SendFeedbackVoltage5_8(void);   /* 0x01B */
void CanProtocol_SendFeedbackVoltage9_12(void);  /* 0x01C */
void CanProtocol_SendFeedbackVoltage13(void);    /* 0x01D */
void CanProtocol_SendFeedbackTemp(void);         /* 0x01E */
void CanProtocol_SendFeedbackStatus(void);       /* 0x021 */

/* ========== 事件触发发送 ========== */
void CanProtocol_SendFeedbackOffset1_8(void);    /* 0x01F — 收到偏移更新后调用 */
void CanProtocol_SendFeedbackOffset9_13(void);   /* 0x020 — 收到偏移更新后调用 */

#ifdef __cplusplus
}
#endif

#endif /* APP_CAN_PROTOCOL_H_ */
