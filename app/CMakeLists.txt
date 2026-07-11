# SPDX-License-Identifier: Apache-2.0
#
# 应用业务代码。仅依赖 Zephyr 子系统 API 与自定义 BSP 驱动 API,不含 SoC/HAL 代码。

target_include_directories(app PRIVATE
  include                          # app/include/app/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/../include  # drivers/*.h (BSP 驱动对外 API)
)

target_sources(app PRIVATE
  src/main.c
  src/voltage_sim/voltage_sim.c
  src/dac_safety/dac_safety.c
  src/waveform_gen/waveform_gen.c
  src/waveform_engine/waveform_engine.c
  src/fault_command/fault_command.c
  src/fault_console/fault_console.c
  src/tcp_json_server/tcp_json_server.c
  src/can_protocol/can_protocol.c
  src/bms_response_log/bms_response_log.c
)
