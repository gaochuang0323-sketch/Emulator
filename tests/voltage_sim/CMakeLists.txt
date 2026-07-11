# SPDX-License-Identifier: Apache-2.0
#
# voltage_sim 纯逻辑单元测试(native_sim)。
# 用 DAC 桩替换真实 DAC81416 驱动,验证 mV<->码值换算、校准、故障注入状态机。
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(voltage_sim_test LANGUAGES C)

set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_include_directories(app PRIVATE
  ${APP_DIR}/app/include        # app/voltage_sim.h ...
  ${APP_DIR}/include            # drivers/dac81416.h, drivers/bsp_dac81416.h
)

target_sources(app PRIVATE
  src/main.c
  src/dac_stub.c                              # 被测代码的 DAC 依赖桩
  ${APP_DIR}/app/src/voltage_sim/voltage_sim.c # 被测源码(直接编译)
)
