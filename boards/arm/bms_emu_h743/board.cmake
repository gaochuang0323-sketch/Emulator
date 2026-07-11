# SPDX-License-Identifier: Apache-2.0
#
# 烧录/调试配置。默认走 OpenOCD + ST-Link。
board_runner_args(openocd "--tcl-port=6666")
board_runner_args(jlink "--device=STM32H743ZI" "--speed=4000")

include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
