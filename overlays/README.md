# overlays —— 分场景配置

用于在不改板级默认配置的前提下,叠加特定场景的 devicetree overlay 与 Kconfig 片段。

用法示例:

```bash
# 用 WiFi 场景配置构建
west build -b bms_emu_h743 . -- \
  -DEXTRA_DTC_OVERLAY_FILE=overlays/wifi.overlay \
  -DEXTRA_CONF_FILE=overlays/wifi.conf
```

约定文件(按需新建):
- `eth.overlay` / `eth.conf`   —— 有线以太网场景
- `wifi.overlay` / `wifi.conf` —— WiFi 场景
- `debug.conf`                 —— 打开日志/shell 的调试配置
