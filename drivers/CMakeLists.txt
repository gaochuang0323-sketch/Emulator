# SPDX-License-Identifier: Apache-2.0
#
# 作为 ZEPHYR_EXTRA_MODULES 被引入,聚合各自定义 BSP 驱动子目录。
# 每个子目录是一个换件隔离点:换 DAC 只动 dac81416/,换 MCU 只动板级 dts。
add_subdirectory(dac81416)
add_subdirectory(can)
add_subdirectory(eth)
add_subdirectory(sram)
