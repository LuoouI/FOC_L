# FOC_L

CYT2BL3 FOC 电机驱动整套工程，包含固件、上位机、PCB 工程和电机调试台结构件。

## 目录

- `project/`：MCU 固件源码和用户程序
- `上位机/`：Windows 电机调试上位机及串口协议说明
- `PCB/`：驱动板 PCB 工程
- `电机调试台/`：调试台 STEP、STL 和 SolidWorks 模型

## 当前状态

本仓库为个人实验和开发版本，当前保持私有。固件依赖对应的 CYT2BL3 SDK、底层库和本地 IDE 工程环境；上位机首次使用时进入 `上位机` 目录安装依赖。

首次上电请使用限流电源并进行空载测试。使用者需要根据自己的功率器件和硬件设计确认过流、过压、欠压、过温及急停保护。

## 相关说明

- 固件模块说明：`project/code/驱动代码说明.md`
- TCPWM 和 ADC 触发链路：`project/code/FOC_TCPWM触发ADC采样链路.md`
- TCPWM、TriggerMux、ADC 术语：`project/code/FOC_TCPWM_ADC名词解释.md`
- 上位机串口协议：`上位机/docs/SERIAL_PROTOCOL.md`

