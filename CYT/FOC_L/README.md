# FOC_L 固件

基于 CYT2BL3 的三相无刷电机 FOC 控制固件，包含电流环、速度环、位置环、SVPWM、电流采样、编码器反馈、参数存储和上位机通信。

## 目录

- `.vscode/`：IAR VS Code 辅助配置
- `libraries/`：CYT2BL3 SDK 和逐飞底层库
- `project/code/`：FOC、电机控制、采样、滤波和通信代码
- `project/user/`：主函数和中断入口
- `project/iar/`：IAR 工程配置

## 开发环境

- MCU：CYT2BL3
- IDE：IAR Embedded Workbench 9.40.1
- 工程入口：`project/iar/cyt2bl3.eww`

## 相关文档

- 固件模块说明：`project/code/驱动代码说明.md`
- TCPWM 和 ADC 触发链路：`project/code/FOC_TCPWM触发ADC采样链路.md`
- TCPWM、TriggerMux、ADC 术语：`project/code/FOC_TCPWM_ADC名词解释.md`

## 注意事项

本固件为个人实验和开发版本。首次上电应使用限流电源并进行空载测试，使用者需要根据实际功率器件和硬件设计确认过流、过压、欠压、过温及急停保护。
