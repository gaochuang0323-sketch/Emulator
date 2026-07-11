# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_BSP_ETH bsp_eth.c)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include)
