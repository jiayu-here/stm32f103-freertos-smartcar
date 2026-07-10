# GitHub 发布说明

## 仓库定位

这是一个可编译、可烧录、可标定的 STM32F103 + FreeRTOS 两轮智能小车工程。公开仓库包含
完整源码、第三方依赖副本、接线/BOM、预编译固件、校验和与从零运行文档。

## 快速验证

```powershell
.\scripts\test_all.ps1
.\scripts\build.ps1 -Device C8 -Clean
```

## Release 附件

v2.0.0 同时提供 C8T6 和 RCT6 的 HEX、BIN 以及 SHA256 校验表。烧录前必须确认所用芯片
型号和 Flash 容量，不可把 C8T6 镜像用于 RCT6，也不可反向使用。

## 硬件验证声明

软件已经通过交叉编译和主机测试；本次发布不宣称实车接线、参数、PID、避障效果或长期可靠性
已经在每一种底盘上验证。请按 `docs/05_验收清单.md` 完成硬件验收。
