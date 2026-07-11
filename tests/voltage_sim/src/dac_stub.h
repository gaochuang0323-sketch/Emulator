/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * voltage_sim 测试用 DAC 桩的检查接口。
 */
#ifndef TESTS_VOLTAGE_SIM_DAC_STUB_H_
#define TESTS_VOLTAGE_SIM_DAC_STUB_H_

#include <stdint.h>

void DacStub_Reset(void);
void DacStub_SetWriteResult(int result);
uint8_t DacStub_GetWriteCalled(void);
uint8_t DacStub_GetLastChannel(void);
uint16_t DacStub_GetLastValue(void);
uint32_t DacStub_GetWriteCount(void);
uint8_t DacStub_GetLdacCalled(void);
void DacStub_ClearLdacCalled(void);

#endif /* TESTS_VOLTAGE_SIM_DAC_STUB_H_ */
