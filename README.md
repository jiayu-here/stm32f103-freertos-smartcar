<a id="english"></a>
# STM32F103C8T6 / RCT6 FreeRTOS Multi-Mode Smart Car, Enhanced Edition

[English](#english) | [中文](#中文)

This is a two-wheel smart-car firmware project that can be compiled, flashed, and extended directly. Its primary target is the resource-constrained STM32F103C8T6 (64 KB Flash and 20 KB RAM), while STM32F103RCT6 is also supported.

The repository includes STM32 HAL, CMSIS, FreeRTOS, startup files, linker scripts, source code, prebuilt images, a wiring table, BOM, test scripts, and full documentation. STM32CubeMX is not required to build it.

## Feature Overview

### Core Features

- FreeRTOS preemptive scheduling with seven tasks and task-health monitoring
- TB6612FNG dual-motor control with 20 kHz PWM
- TIM3/TIM4 dual AB-phase encoder speed measurement
- Independent closed-loop PID speed control for both wheels
- Manual remote control, line following, obstacle avoidance, combined line-following and avoidance, and cruise mode
- Five-channel infrared line following with lost-line search
- HC-SR04 and SG90 left/right scanning obstacle avoidance
- HC-05 serial/Bluetooth control and online PID tuning
- SSD1306 OLED pages for system status and odometry
- Battery monitoring, low-voltage protection, emergency stop, buzzer, LEDs, logging, and an independent watchdog

### C8T6 Enhancements

- Last-page Flash parameter storage with CRC validation and append-only wear leveling
- Persistent PID, speed, acceleration, line-following gain, wheel, battery, and stall-protection parameters
- Target-speed acceleration and deceleration ramp to reduce startup shock and supply-voltage drops
- Motor-stall latching protection for high PWM with low encoder speed
- Two-encoder 2D odometry: X, Y, heading, and cumulative left/right distance
- MOVE for distance motion and TURN for angle rotation
- Eight-segment QMOVE/QTURN route queue with automatic sequential execution
- Diagnostics for FreeRTOS free heap, historical minimum heap, and task stack headroom
- RCC reset-cause reporting and eight in-RAM fault-history records

## First Use

Use the following order:

1. Read [Component Guide](docs/10_组件说明与选型.md) to confirm module voltage and specifications.
2. Follow [Hardware Wiring](docs/01_硬件接线.md), especially the HC-SR04 Echo voltage divider and the independent 5 V servo supply.
3. Read [From Zero to Running](docs/09_操作步骤_从零到运行.md); initially connect only the controller and ST-Link.
4. Lift the wheels and verify motor direction and encoder sign at low speed.
5. Calibrate wheel diameter, wheelbase, CPR, battery parameters, and PID, then issue SAVE.
6. Finally test line following, obstacle avoidance, distance motion, angle rotation, and route queues on the ground.

## One-Command Build

~~~powershell
# C8T6
.\scripts\build.ps1 -Device C8 -Clean

# RCT6
.\scripts\build.ps1 -Device RCT6 -Clean

# With ST-Link connected: build, flash, verify, and reset
.\scripts\flash.ps1 -Device C8
~~~

Prebuilt firmware is available in artifacts/C8 and artifacts/RCT6.

## Bluetooth Control

HC-05 and USART1 use 115200, 8-N-1 by default:

~~~powershell
python .\scripts\car_console.py COM5
~~~

After connecting, start with:

~~~text
VERSION
STATUS
CONFIG
DIAG
HELP
~~~

See [Bluetooth Serial Protocol](docs/03_蓝牙串口协议.md) for every command.

## Documentation Index

| Document | Contents |
|---|---|
| [Project Overview and Features](docs/00_项目总览与实现功能.md) | Project goals, implemented features, and delivery boundary |
| [Hardware Wiring](docs/01_硬件接线.md) | Pins, power, logic levels, voltage dividers, and first power-up |
| [Software Architecture](docs/02_软件架构.md) | Layers, tasks, priorities, communication, and data flow |
| [Bluetooth Serial Protocol](docs/03_蓝牙串口协议.md) | Commands, parameter ranges, return values, and examples |
| [Calibration and Debugging](docs/04_标定与调试.md) | Motors, encoders, PID, odometry, and sensors |
| [Acceptance Checklist](docs/05_验收清单.md) | Build and vehicle-level acceptance items |
| [Completion Status](docs/06_完成状态.md) | Completed work, vehicle checks still needed, and resource limits |
| [Enhanced Feature Principles](docs/07_增强功能原理.md) | Flash, ramps, stall protection, odometry, and routes |
| [Code Guide](docs/08_代码导读与注释说明.md) | Directories, key files, call relationships, and change points |
| [From Zero to Running](docs/09_操作步骤_从零到运行.md) | Installation, build, flashing, power-on, and test procedure |
| [Component Guide](docs/10_组件说明与选型.md) | Purpose, specifications, and cautions for each component |
| [Troubleshooting](docs/11_常见问题.md) | Flashing, reset, garbled text, and loss-of-control diagnostics |
| [Enhanced Edition Changes](docs/12_增强版更新说明.md) | New code, features, and documents relative to the original edition |

## Resource and Verification Boundaries

- The C8T6 link region is 63 KB; the final 1 KB is reserved for parameter records.
- The enhanced C8T6 firmware uses about 55.6 KB Flash and 13.5 KB static RAM, including a 10 KB FreeRTOS heap.
- Both C8T6 and RCT6 builds completed Arm GNU cross-compilation; host-side protocol and PID tests passed.
- The delivery environment did not include the physical car or ST-Link. Motor polarity, CPR, wheel dimensions, PID, and sensor thresholds must therefore be confirmed against the acceptance checklist on actual hardware.

## License

The project-owned application code, scripts, and documentation use the [MIT License](LICENSE). Third-party files remain under their respective licenses: STM32CubeF1_LICENSE.md and Middlewares/FreeRTOS/Source/LICENSE.

<a id="中文"></a>
# STM32F103C8T6 / RCT6 + FreeRTOS 多模式智能小车增强版

[English](#english) | [中文](#中文)

本工程是一套可以直接编译、烧录和二次开发的双轮智能小车固件。主目标是资源较小的 STM32F103C8T6（64 KB Flash、20 KB RAM），同时兼容 STM32F103RCT6。

工程已经随包提供 STM32 HAL、CMSIS、FreeRTOS、启动文件、链接脚本、源码、预编译镜像、接线表、BOM、测试脚本和完整使用文档，不依赖 STM32CubeMX 才能构建。

## 功能概览

### 基础功能

- FreeRTOS 七任务抢占式调度与任务健康监测
- TB6612FNG 双电机 20 kHz PWM 驱动
- TIM3/TIM4 双 AB 相编码器测速
- 左右轮独立 PID 速度闭环
- 手动遥控、自动循迹、自动避障、循迹避障融合、巡航
- 五路红外循迹与黑线丢失搜索
- HC-SR04 + SG90 左右扫描避障
- HC-05 串口/蓝牙控制和 PID 在线调整
- SSD1306 OLED 状态与里程计双页面
- 电池电压、低电保护、急停、蜂鸣器、LED、日志、独立看门狗

### C8T6 增强功能

- 最后一页 Flash 参数存储，CRC 校验和追加式磨损均衡
- PID、速度、加速度、循迹增益、轮径、轮距、CPR、电池和堵转参数掉电保存
- 目标速度加减速斜坡，降低启动冲击和电源跌落
- 高 PWM + 低编码器速度的电机堵转锁存保护
- 双编码器二维里程计：X、Y、航向、左右轮累计距离
- MOVE 定距离运动和 TURN 定角度转向
- 8 段 QMOVE/QTURN 路线队列与自动连续执行
- FreeRTOS 剩余堆、历史最小堆、各任务剩余栈诊断
- RCC 复位原因读取和 8 条 RAM 故障历史

## 第一次使用

建议严格按以下顺序进行：

1. 阅读 [组件说明](docs/10_组件说明与选型.md)，确认模块电压和规格。
2. 按 [硬件接线](docs/01_硬件接线.md) 连接，尤其注意 HC-SR04 Echo 分压和舵机独立 5 V。
3. 阅读 [从零到运行](docs/09_操作步骤_从零到运行.md)，先只接主控和 ST-Link。
4. 架空车轮，小速度检查电机方向与编码器符号。
5. 完成轮径、轮距、CPR、电池和 PID 标定后执行 SAVE。
6. 最后落地测试循迹、避障、定距、定角和路线队列。

## 一键构建

~~~powershell
# C8T6
.\scripts\build.ps1 -Device C8 -Clean

# RCT6
.\scripts\build.ps1 -Device RCT6 -Clean

# 连接 ST-Link 后编译、烧录、校验并复位
.\scripts\flash.ps1 -Device C8
~~~

预编译固件位于 artifacts/C8 和 artifacts/RCT6。

## 蓝牙控制

HC-05 和 USART1 默认为 115200、8-N-1：

~~~powershell
python .\scripts\car_console.py COM5
~~~

连接后先输入：

~~~text
VERSION
STATUS
CONFIG
DIAG
HELP
~~~

完整命令见 [蓝牙串口协议](docs/03_蓝牙串口协议.md)。

## 文档索引

| 文档 | 内容 |
|---|---|
| [项目总览与实现功能](docs/00_项目总览与实现功能.md) | 项目目标、所有功能、完成边界 |
| [硬件接线](docs/01_硬件接线.md) | 引脚、供电、电平、分压、首次通电 |
| [软件架构](docs/02_软件架构.md) | 分层、任务、优先级、通信、数据流 |
| [蓝牙串口协议](docs/03_蓝牙串口协议.md) | 全部命令、参数范围、返回值、示例 |
| [标定与调试](docs/04_标定与调试.md) | 电机、编码器、PID、里程计、传感器 |
| [验收清单](docs/05_验收清单.md) | 编译和实车逐项验收 |
| [完成状态](docs/06_完成状态.md) | 已完成、实车待验证、资源边界 |
| [增强功能原理](docs/07_增强功能原理.md) | Flash、斜坡、堵转、里程计、路线 |
| [代码导读](docs/08_代码导读与注释说明.md) | 目录、关键文件、调用关系、修改入口 |
| [从零到运行](docs/09_操作步骤_从零到运行.md) | 安装、编译、烧录、上电、测试步骤 |
| [组件说明与选型](docs/10_组件说明与选型.md) | 每个器件的用途、规格和注意事项 |
| [常见问题](docs/11_常见问题.md) | 无法烧录、复位、乱码、失控等排查 |
| [增强版更新说明](docs/12_增强版更新说明.md) | 相对原版新增的代码、功能和文档 |

## 资源与验证边界

- C8T6 链接区域为 63 KB，最后 1 KB 专门保留给参数记录。
- 增强版 C8T6 固件约 55.6 KB Flash，静态 RAM约 13.5 KB（含 10 KB FreeRTOS 堆）。
- C8T6 和 RCT6 均已完成 Arm GNU 交叉编译；协议/PID 主机测试通过。
- 交付环境没有连接实车和 ST-Link，因此电机极性、CPR、轮径、轮距、PID 与传感器阈值必须按验收清单在实际硬件上确认。

## 许可证

项目自有应用代码、脚本和文档采用 [MIT License](LICENSE)。第三方文件仍受各自许可证约束：STM32CubeF1_LICENSE.md 和 Middlewares/FreeRTOS/Source/LICENSE。
