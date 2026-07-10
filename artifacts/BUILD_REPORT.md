# 增强版构建与验证报告

- 构建日期：2026-07-10
- Arm 编译器：GNU Arm Embedded 14.2.1
- HAL/CMSIS：STM32Cube_FW_F1_V1.8.6
- RTOS：STM32CubeF1 随附 FreeRTOS Cortex-M3 移植
- 固件版本：2.0 C8-SAFE

## 容量

| 目标 | text | data | bss | 程序区上限 | 保留参数页 | 结果 |
|---|---:|---:|---:|---:|---:|---|
| STM32F103C8T6 | 55,168 B | 480 B | 13,064 B | 63 KB | 1 KB | 通过 |
| STM32F103RCT6 | 55,624 B | 480 B | 23,304 B | 254 KB | 2 KB | 通过 |

C8T6 Flash 实际占用按 text+data 约 55,648 B，距离 63 KB 链接上限约 8.9 KB。bss 已包含
10 KB FreeRTOS heap_4；任务栈运行时从该堆分配。

## 验证结果

- `scripts/test_host.ps1`：PID 与原版/增强版全部协议命令通过。
- `scripts/validate_project.py`：工程结构、UTF-8、29 个引脚、协议文档和 Flash 分区通过。
- `scripts/build.ps1`：C8T6/RCT6 均生成 ELF、HEX、BIN、MAP。
- CMake + Ninja：两个目标均完成独立构建。
- 两个 ELF 均存在 SVC、PendSV、SysTick、USART1、EXTI4、ControlTask、AppConfig_Save。
- 预编译 BIN/HEX 的 SHA256 记录于 `SHA256SUMS.txt`。

## 验证边界

交付环境没有连接 ST-Link 和实车，因此没有宣称完成实物烧录、电气安全、Flash 断电测试、
电机/编码器/传感器标定、路线精度或道路老化测试。这些项目已列入验收清单。
