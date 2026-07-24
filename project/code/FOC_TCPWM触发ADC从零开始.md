# CYT2BL3 FOC：TCPWM 触发 ADC 从零开始

## 1. 先决定今天只做什么

不要一开始就写完整 FOC。第一阶段只完成下面这件事：

```text
一个 ADC 引脚上的电压
    -> 软件启动一次 ADC
    -> 程序读到会随输入电压变化的数值
```

这一步成功以后，再按顺序完成：

```text
ADC 软件触发
    -> TCPWM 独立计数
    -> TCPWM CC1 产生触发
    -> TriggerMux 把触发送给 PASS
    -> ADC 改为 GENERIC0 硬件触发
    -> ADC 完成中断
    -> 三相 PWM 和多路电流采样
    -> FOC 电流环
```

每一步都要先观察到正确现象，才能进入下一步。否则整条链路不工作时，很难判断问题在 ADC、TCPWM 还是 TriggerMux。

---

## 2. 写代码前先填硬件表

下面的内容必须从原理图和板卡引脚分配中确认，不能靠猜：

| 项目 | 需要填写的实际值 |
| --- | --- |
| U 相高侧 PWM 引脚 | 待填写 |
| U 相低侧 PWM 引脚 | 待填写 |
| V 相高侧 PWM 引脚 | 待填写 |
| V 相低侧 PWM 引脚 | 待填写 |
| W 相高侧 PWM 引脚 | 待填写 |
| W 相低侧 PWM 引脚 | 待填写 |
| Ia 运放输出引脚 | 待填写 |
| Ia 对应 SAR 和通道 | 待填写 |
| Ib 运放输出引脚 | 待填写 |
| Ib 对应 SAR 和通道 | 待填写 |
| Ic 运放输出引脚 | 待填写 |
| Ic 对应 SAR 和通道 | 待填写 |
| 电流采样方式 | 单电阻 / 双电阻 / 三电阻 / 相电流传感器 |
| 目标 PWM 频率 | 待填写 |
| TCPWM 实际输入时钟 | 待填写 |
| 死区时间 | 待填写 |

这里最关键的是 ADC 引脚。例如开源库中的枚举：

```c
ADC0_CH00_P06_0
```

表示：

```text
P06_0 引脚 -> SAR0 的模拟输入 0
```

如果 Ia、Ib、Ic 分别位于 SAR0、SAR1、SAR2，就可以让三套 SAR 真正并行转换。如果三路都在同一个 SAR，只能由一套 SAR 依次转换。

---

## 3. 工程中的代码应该放在哪里

当前工程可以这样分工：

```text
project/code/My_ADC/My_ADC.h
    放 ADC 对外函数声明、必要宏定义和结构体

project/code/My_ADC/My_ADC.c
    放 ADC 初始化、读取结果和中断处理

project/code/My_TCPWM/My_TCPWM.h
    放 TCPWM 对外函数声明、必要的频率和计数参数

project/code/My_TCPWM/My_TCPWM.c
    放 TCPWM 时钟、计数器、CC0、CC1 和 TriggerMux 配置

project/user/main_cm4.c
    只负责按顺序调用初始化函数，不在 main 中堆寄存器配置
```

建议先规划这些函数，暂时不要一次全部实现：

```c
void My_ADC_SoftwareTestInit(void);
uint16_t My_ADC_SoftwareRead(void);
void My_ADC_HardwareTriggerInit(void);
bool My_TCPWM_TriggerInit(void);
void My_TCPWM_Start(void);
```

第一天只实现前两个函数。

---

## 4. 第一步：先让 ADC 软件触发工作

当前开源库已经提供并实现了下面两个接口：

```c
void adc_init(adc_channel_enum adc_chn, adc_resolution_enum resolution);
uint16 adc_convert(adc_channel_enum adc_chn);
```

它们位于：

```text
libraries/zf_driver/zf_driver_adc.h
libraries/zf_driver/zf_driver_adc.c
```

因此第一版先用开源库验证引脚，不需要马上重写底层 ADC 驱动。

假设实际电流采样引脚确实是 `P06_0`，测试代码才可以写成：

```c
static uint16_t Current_adc_value;

adc_init(ADC0_CH00_P06_0, ADC_12BIT);

for (;;)
{
    Current_adc_value = adc_convert(ADC0_CH00_P06_0);
}
```

注意：`ADC0_CH00_P06_0` 只是演示。必须换成原理图中的真实引脚。

### 怎么判断第一步成功

1. 在调试器 Watch 窗口观察 `Current_adc_value`。
2. ADC 输入接地时，结果应接近 0。
3. ADC 输入电压升高时，结果应明显增大。
4. 12 位 ADC 满量程附近应接近 4095。

此时不要接高压母线，也不要让功率管开始开关。先用安全的直流电压或电流采样运放静态输出测试。

如果数值不变化，依次检查：

```text
引脚枚举是否正确
    -> 原理图上的网络是否真的接到该引脚
    -> GPIO 是否处于模拟模式
    -> SAR 编号和通道是否正确
    -> ADC 输入电压是否超出允许范围
```

---

## 5. 第二步：让 TCPWM 独立运行

这一步先不连接 ADC。目标只是让 `TCPWM0_GRP1_CNT0` 按中心对齐模式计数。

建议暂定：

```text
TCPWM0_GRP1_CNT0：主计数器，同时负责产生 ADC 触发
CC0：PWM 占空比
CC1：ADC 采样位置
tr_out1：输出 CC1 Match 事件
```

中心对齐计数过程为：

```text
0 -> PERIOD -> 0 -> PERIOD -> 0
```

频率近似关系：

```text
PWM 频率 = TCPWM 计数时钟 / (2 * PERIOD)
```

例如 TCPWM 计数时钟为 80 MHz、PWM 目标为 20 kHz：

```text
PERIOD = 80,000,000 / (2 * 20,000) = 2000
```

这只是计算示例。必须先确认 `TCPWM0_GRP1_CNT0` 的真实输入时钟。Group1 CNT0 对应的外设时钟目标是 `PCLK_TCPWM0_CLOCKS256`，不是 `PCLK_TCPWM0_CLOCKS0`。

TCPWM 的关键配置关系是：

```c
Pwm_config.countDirection   = CY_TCPWM_COUNTER_COUNT_UP_DOWN1;
Pwm_config.compare0         = Pwm_period_count / 2u;
Pwm_config.compare1         = Adc_sample_count;
Pwm_config.cc1MatchMode     = CY_TCPWM_PWM_TR_CTRL2_NO_CHANGE;
Pwm_config.trigger1EventCfg = CY_TCPWM_COUNTER_CC1_MATCH;
```

含义：

```text
计数器上下计数
CC0 先给 50% 占空比
COUNTER 等于 CC1 时产生事件
CC1 不改变 PWM 引脚电平
CC1 Match 被送到 tr_out1
```

### 怎么判断第二步成功

在调试器中观察：

```c
TCPWM0_GRP1_CNT0->unCOUNTER.u32Register
```

它应该在 0 和 PERIOD 之间连续变化。如果已经配置 PWM 复用引脚，也可以用示波器确认 PWM 频率。

---

## 6. 第三步：只保留每周期一次 CC1 触发

中心对齐计数会在向上和向下计数时各经过一次 CC1。两个方向都使能会导致每个 PWM 周期触发两次 ADC。

当前工程 SDK 的 `Cy_Tcpwm_Pwm_Init()` 没有恢复下面四个方向使能位，所以初始化后必须显式设置：

```c
TCPWM0_GRP1_CNT0->unCTRL.stcField.u1CC0_MATCH_UP_EN   = 1u;
TCPWM0_GRP1_CNT0->unCTRL.stcField.u1CC0_MATCH_DOWN_EN = 1u;
TCPWM0_GRP1_CNT0->unCTRL.stcField.u1CC1_MATCH_UP_EN   = 1u;
TCPWM0_GRP1_CNT0->unCTRL.stcField.u1CC1_MATCH_DOWN_EN = 0u;
```

这样每个完整中心对齐 PWM 周期只在向上计数经过 CC1 时触发一次。

第一版 `CC1` 不要设为 0。先使用：

```text
CC1 = 死区计数 + 运放建立时间计数 + 安全余量
```

---

## 7. 第四步：接通 TCPWM 到 ADC 的触发链路

前面两步都成功后，再加入下面这条连接：

```text
TCPWM0_GRP1_CNT0 CC1 Match
    -> TCPWM tr_out1[256]
    -> TriggerMux Group6
    -> PASS Generic Trigger 0
    -> SAR 的 GENERIC0
    -> ADC Channel
```

### 7.1 配置 TriggerMux

当前芯片头文件中已经确认存在以下两个路由常量：

```c
cy_en_trigmux_status_t Trigger_status;

Trigger_status = Cy_TrigMux_Connect(
    TRIG_IN_MUX_6_TCPWM_16M_TR_OUT10,
    TRIG_OUT_MUX_6_PASS_GEN_TR_IN0,
    CY_TR_MUX_TR_INV_DISABLE,
    TRIGGER_TYPE_EDGE,
    0u);
```

必须检查：

```c
if (Trigger_status != CY_TRIGMUX_SUCCESS)
{
    /* 初始化失败，暂时不要启动 PWM */
}
```

### 7.2 让 SAR 监听 PASS Generic Trigger 0

如果只测试 SAR0：

```c
Cy_Adc_SetGenericTriggerInput(PASS0_EPASS_MMIO, 0u, 0u, 0u);
```

四个参数依次表示：

```text
PASS0、SAR0、SAR 内部 GENERIC0、PASS 通用触发线0
```

以后需要三套 SAR 一起监听时再增加：

```c
Cy_Adc_SetGenericTriggerInput(PASS0_EPASS_MMIO, 1u, 0u, 0u);
Cy_Adc_SetGenericTriggerInput(PASS0_EPASS_MMIO, 2u, 0u, 0u);
```

### 7.3 ADC 通道改为硬件触发

软件测试通过后，重新初始化该 ADC 通道，并设置：

```c
Adc_channel_config.triggerSelection = CY_ADC_TRIGGER_GENERIC0;
```

通道使能以后，不再周期调用：

```c
Cy_Adc_Channel_SoftwareTrigger();
```

也不再使用会主动写启动命令的 `adc_convert()` 作为周期采样入口。此时由 TCPWM 自动开始转换，CPU 只读取结果。

---

## 8. 第五步：先轮询结果，再写中断

硬件触发刚接通时，先不要马上加入 FOC 中断。先用一个计数变量或调试器确认 ADC 结果会随着 PWM 周期更新。

确认触发次数正确以后，再配置 ADC Group Done 中断。中断中第一版只做三件事：

```text
读取 ADC 结果
    -> 清除 ADC 中断标志
    -> Adc_interrupt_count 加 1
```

20 kHz PWM 且每周期触发一次时，一秒后 `Adc_interrupt_count` 应增加约 20000。若使用 GPIO 在每次中断中翻转一次，示波器看到的 GPIO 方波约为 10 kHz，因为翻转两次才构成一个完整方波周期。

只有中断频率和 ADC 数值都正确后，才把 Clarke、Park、PI 和 SVPWM 放进这条中断链路。

---

## 9. 最终初始化顺序

最终的 `main` 应保持清楚的初始化次序：

```c
int main(void)
{
    clock_init(SYSTEM_CLOCK_160M);
    debug_init();

    My_ADC_HardwareTriggerInit();
    My_TCPWM_TriggerInit();
    My_TCPWM_Start();

    for (;;)
    {
        /* 主循环只处理非实时任务 */
    }
}
```

`My_ADC_HardwareTriggerInit()` 内部顺序：

```text
配置 ADC 时钟
    -> 配置模拟 GPIO
    -> 初始化 SAR
    -> 初始化 ADC Channel，triggerSelection = GENERIC0
    -> 选择 Generic Trigger 输入
    -> 清标志
    -> 使能 ADC Channel
```

`My_TCPWM_TriggerInit()` 内部顺序：

```text
配置 TCPWM 时钟
    -> 配置 TCPWM
    -> 设置 CC0/CC1 方向使能
    -> 连接 TriggerMux
    -> 检查返回值
```

所有模块准备好以后，最后调用 `My_TCPWM_Start()`。这样不会在初始化尚未完成时产生一组残缺 ADC 数据。

---

## 10. 你现在立刻要做的事情

现在先不要写 TriggerMux。只完成下面四项：

1. 从原理图找出一个电流采样运放输出引脚。
2. 在 `zf_driver_adc.h` 中找到这个引脚对应的 `ADCx_CHxx_Pxx_x` 枚举。
3. 用 `adc_init()` 和 `adc_convert()` 做软件触发测试。
4. 在调试器中确认结果会随输入电压变化。

完成这四项以后，下一段代码才是 `My_TCPWM_TriggerInit()`。如果第一项无法确定，就先提供原理图中 Ia、Ib、Ic 的网络名和 MCU 管脚，后面的 SAR、通道和初始化代码才能写成真实可编译版本。

