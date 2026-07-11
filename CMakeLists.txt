# SPDX-License-Identifier: Apache-2.0
#
# BMS 故障模拟装置 —— 应用顶层 CMake
cmake_minimum_required(VERSION 3.20.0)

# 让 Zephyr 找到本仓库自带的 out-of-tree 资源:
#   - boards/     自定义开发板(换 MCU 隔离点)
#   - drivers/    自定义 BSP 驱动(换 DAC 隔离点)
#   - dts/        自定义 devicetree 绑定
set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(DTS_ROOT   ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/drivers)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bms_emulator LANGUAGES C)

# 应用业务代码
add_subdirectory(app)
