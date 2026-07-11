# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_BSP_DAC81416 bsp_dac81416.c dac81416.c)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)
