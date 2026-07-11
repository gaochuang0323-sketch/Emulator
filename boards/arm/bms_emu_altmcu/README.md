# 备选主控板(换 MCU 隔离点)

换 MCU 时在此新建完整板级目录,**app/ 与 drivers/ 无需改动**。一块板需要:

```
bms_emu_altmcu/
├── bms_emu_altmcu.dts          # 用新 MCU 的 SoC dtsi 重写硬件描述
├── bms_emu_altmcu_defconfig
├── bms_emu_altmcu.yaml
├── Kconfig.board
├── Kconfig.defconfig
└── board.cmake
```

配套改动:
- `west.yml`:把 `name-allowlist` 里的 `hal_stm32` 换成新 MCU 的 HAL 模块。
- devicetree 里保持 `voltage-sim` / `ad5686` / `ntc-switch` 节点的 `compatible` 不变,
  只改引脚、SPI 实例、时钟等平台相关属性。

参照 `../bms_emu_h743/` 作为模板。
