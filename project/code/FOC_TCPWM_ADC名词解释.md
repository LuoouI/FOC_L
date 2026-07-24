# CYT2BL3 FOC：TCPWM、TriggerMux 与 ADC 名词解释

## 1. 整体理解

在本工程中，各模块的分工可以先这样理解：

```text
TCPWM：决定什么时候采样
TriggerMux：决定触发信号送到哪里
PASS：管理模拟输入、ADC 和 ADC 触发
SAR：真正完成模拟量采样和模数转换
ADC 中断：通知 CPU 数据已经转换完成
```

可以把它类比成：

```text
TCPWM            = 闹钟
tr_out1          = 闹钟输出线
TriggerMux       = 电话交换机
PASS Generic0    = 广播频道
SAR0/SAR1/SAR2   = 三台 ADC 转换机器
ADC Channel      = 每台机器需要采集的具体输入
Group Done       = 本次采集任务全部完成的通知
```

完整链路为：

```text
TCPWM CC1 Match
    ↓
TCPWM tr_out1
    ↓
PERI TriggerMux
    ↓
PASS Generic Trigger
    ↓
SAR ADC Channel
    ↓
采样和转换
    ↓
Group Done
    ↓
FOC 电流环
```

---

## 2. 三种不同的 Group

这里出现了三种完全不同的 Group，不能混为一谈。

### 2.1 TCPWM Group

表示 TCPWM 计数器的硬件分组：

| TCPWM Group | 功能 |
| --- | --- |
| Group0 | 普通 16 位定时器组 |
| Group1 | 带电机控制功能的 16 位定时器组 |
| Group2 | 32 位定时器组 |

FOC 一般优先使用 Group1。

### 2.2 TriggerMux Group

表示触发路由硬件的分组。每组支持的触发来源和目标不同。

当前方案使用 TriggerMux Group6，是因为该组可以选择 TCPWM Group1 的 `tr_out1`，并把它送到 PASS 通用触发输入。

### 2.3 ADC Group

表示一次 ADC 触发后需要顺序转换的一组通道。

例如同一个 SAR 顺序采集三个通道：

```text
CH0：组首，直接接收硬件触发
CH1：不直接接收硬件触发
CH2：组尾，转换完成后产生 Group Done
```

三种 Group 之间没有编号对应关系。

---

## 3. TCPWM

TCPWM 可以理解为 Timer、Counter、PWM 的组合外设。

它可以完成：

- 定时
- 计数
- PWM 输出
- 编码器计数
- 输入捕获
- 产生其他外设的硬件触发

在 FOC 中，TCPWM 主要负责：

```text
1. 产生 U、V、W 三相互补 PWM
2. 在指定 PWM 位置触发 ADC
```

---

## 4. TCPWM0_GRP1_CNT0

```c
TCPWM0_GRP1_CNT0
```

拆开理解：

```text
TCPWM0：第 0 个 TCPWM 外设
GRP1：TCPWM 的电机控制计数器组
CNT0：Group1 中第 0 个计数器
```

FOC 三相 PWM 可以使用：

```text
TCPWM0_GRP1_CNT0：U 相
TCPWM0_GRP1_CNT1：V 相
TCPWM0_GRP1_CNT2：W 相
```

三个计数器必须同步计数和同步启动。

---

## 5. COUNTER

`COUNTER` 是计数器当前值。

中心对齐 PWM 的计数过程为：

```text
0 → PERIOD → 0 → PERIOD → 0
```

例如：

```text
PERIOD = 4000
```

计数过程为：

```text
0, 1, 2 ... 3999, 4000, 3999 ... 2, 1, 0
```

一次向上计数加一次向下计数，构成一个完整的中心对齐 PWM 周期。

---

## 6. PERIOD

`PERIOD` 是计数器的顶部值。

中心对齐 PWM 频率近似为：

```text
PWM 频率 = TCPWM 计数时钟 / (2 × PERIOD)
```

实际计算还需要考虑：

- TCPWM 输入时钟
- TCPWM 内部分频器
- PERIOD 是否包含计数端点

---

## 7. CC0 与 CC1

`CC` 表示 Compare/Capture，即比较/捕获寄存器。

PWM 模式主要使用比较功能。

### 7.1 CC0

`CC0` 通常控制 PWM 占空比。

```text
COUNTER == CC0
        ↓
产生 CC0 Match
        ↓
根据 TR_PWM_CTRL 改变 PWM 输出
```

在 FOC 中：

```text
U 相 CC0：U 相占空比
V 相 CC0：V 相占空比
W 相 CC0：W 相占空比
```

SVPWM 最终计算的就是三个 CC0 值。

### 7.2 CC1

`CC1` 是第二个独立比较点。

它可以只产生事件，不改变 PWM 输出，因此适合设置 ADC 采样时刻。

```text
COUNTER == CC1
        ↓
产生 CC1 Match
        ↓
通过 tr_out1 触发 ADC
```

建议配置：

```c
Pwm_config.cc1MatchMode = CY_TCPWM_PWM_TR_CTRL2_NO_CHANGE;
```

含义是 CC1 匹配时不改变 PWM 输出，只用于产生内部事件。

---

## 8. CC1_MATCH_UP_EN 与 CC1_MATCH_DOWN_EN

中心对齐模式会在向上计数和向下计数时各经过一次 CC1。

如果两个方向都允许：

```text
一个完整 PWM 周期产生两次 CC1 Match
```

如果每个完整 PWM 周期只想采样一次，可以只允许向上计数匹配：

```c
TCPWM0_GRP1_CNT0->unCTRL.stcField.u1CC1_MATCH_UP_EN   = 1u;
TCPWM0_GRP1_CNT0->unCTRL.stcField.u1CC1_MATCH_DOWN_EN = 0u;
```

如果需要一个周期采样两次，则两个方向都可以打开。

`CC0_MATCH_UP_EN` 和 `CC0_MATCH_DOWN_EN` 同理，只是它们控制 CC0 比较事件。

---

## 9. Overflow

`Overflow` 表示计数器到达顶部边界产生的事件。

中心对齐模式下可以理解为：

```text
3998 → 3999 → 4000 → 3999 → 3998
                  ↑
               Overflow
```

此时计数方向由向上变为向下。

---

## 10. Underflow

`Underflow` 表示计数器到达底部边界产生的事件。

```text
2 → 1 → 0 → 1 → 2
        ↑
    Underflow
```

此时计数方向由向下变为向上。

---

## 11. Terminal Count

`Terminal Count` 简称 `TC`，表示当前计数模式定义的周期终点。

它不一定永远等于 Overflow。

对于：

```c
CY_TCPWM_COUNTER_COUNT_UP_DOWN1
```

SDK 说明只在 Underflow 时产生 Terminal Count。因此可以把它理解为：

```text
一次完整的向上、向下计数结束
```

使用 TC 触发 ADC，一般每个完整 PWM 周期触发一次。

使用 CC1 则可以自由调整具体采样位置。

---

## 12. Match

`Match` 表示比较相等。

例如：

```text
COUNTER = 200
CC1     = 200
```

此时产生 `CC1 Match`。

Match 本身只是内部事件。它是否改变 PWM、产生中断或产生外设触发，由其他寄存器决定。

---

## 13. tr、tr_in 与 tr_out

`tr` 是 Trigger，即触发。

### 13.1 tr_out

`tr_out` 是某个外设产生的触发输出。

例如：

```text
TCPWM tr_out1
```

表示 TCPWM 的第 1 路触发输出。

### 13.2 tr_in

`tr_in` 是某个外设接收的触发输入。

例如：

```text
PASS tr_sar_gen_in[0]
```

表示 PASS 的 SAR 通用触发输入 0。

信号方向为：

```text
TCPWM tr_out1 → PASS tr_in
```

---

## 14. tr_out0 与 tr_out1

每个 TCPWM 计数器可以提供两路触发输出：

```text
tr_out0
tr_out1
```

两路输出可以选择不同的内部事件，例如：

```text
tr_out0 = Terminal Count
tr_out1 = CC1 Match
```

本 FOC 方案建议：

```text
tr_out1 = CC1 Match
```

---

## 15. TR_OUT_SEL

`TR_OUT_SEL` 是 Trigger Output Select，即触发输出选择寄存器。

它决定：

```text
什么内部事件产生 tr_out0
什么内部事件产生 tr_out1
```

例如：

```c
Pwm_config.trigger1EventCfg = CY_TCPWM_COUNTER_CC1_MATCH;
```

对应：

```text
TR_OUT_SEL.OUT1 = CC1_MATCH
```

因此形成：

```text
CC1 Match → tr_out1
```

---

## 16. TriggerMux

`Mux` 是 Multiplexer，即多路选择器。

TriggerMux 的作用是从多个触发来源中选择一个，送到指定外设的触发输入。

```text
                 ┌─ TCPWM CNT0 tr_out1
                 ├─ TCPWM CNT1 tr_out1
PASS Generic0 ←──┼─ DMA tr_out
                 ├─ GPIO trigger
                 └─ Event Generator
```

TriggerMux 不负责产生采样时刻，它只负责连接和路由。

---

## 17. TriggerMux Group6

TriggerMux 内部有很多路由分组，每组支持不同的来源和目标。

当前方案使用 Group6：

```text
TCPWM Group1 CNT0 tr_out1
        ↓
TriggerMux Group6
        ↓
PASS Generic Trigger 0
```

`TriggerMux Group6` 与 `TCPWM Group1` 没有编号对应关系。

---

## 18. TRIG_IN_MUX_6_TCPWM_16M_TR_OUT10

```c
TRIG_IN_MUX_6_TCPWM_16M_TR_OUT10
```

拆开理解：

```text
TRIG_IN：TriggerMux 的输入候选信号
MUX_6：属于 TriggerMux Group6
TCPWM_16M：来源是 TCPWM 的 16 位电机控制计数器组
TR_OUT10：tr_out1[0]，即输出1、计数器0
```

需要特别注意：

```text
16M 中的 M 表示 Motor，不是 MHz
```

`TR_OUT10` 不是“第十号输出”，而是：

```text
TR_OUT1 + counter 0
```

类似地：

```text
TR_OUT11  = tr_out1[1]
TR_OUT12  = tr_out1[2]
TR_OUT110 = tr_out1[10]
```

---

## 19. TRIG_OUT_MUX_6_PASS_GEN_TR_IN0

```c
TRIG_OUT_MUX_6_PASS_GEN_TR_IN0
```

拆开理解：

```text
TRIG_OUT：TriggerMux 的输出目标
MUX_6：属于 TriggerMux Group6
PASS_GEN：目标是 PASS 通用触发模块
TR_IN0：PASS 通用触发输入0
```

因此下面的连接：

```c
Cy_TrigMux_Connect(
    TRIG_IN_MUX_6_TCPWM_16M_TR_OUT10,
    TRIG_OUT_MUX_6_PASS_GEN_TR_IN0,
    CY_TR_MUX_TR_INV_DISABLE,
    TRIGGER_TYPE_EDGE,
    0u);
```

可以翻译为：

```text
把 TCPWM Group1 CNT0 的 tr_out1
连接到 PASS 通用触发输入0
```

---

## 20. PERI

`PERI` 可以理解为芯片的外设基础设施模块。

它管理：

- TriggerMux
- 外设公共时钟
- 软件触发命令
- 一部分外设访问和控制功能

TCPWM 和 ADC 是功能外设，PERI 负责把它们连接起来。

---

## 21. One-to-One

`One-to-One` 是一对一固定专线。

它表示：

```text
一个固定触发来源 → 一个固定触发目标
```

软件可以设置：

- 是否打开专线
- 是否反相
- 使用边沿还是电平
- 调试时是否冻结

但不能改变专线两端。

例如：

```text
TCPWM Group1 CNT0 tr_out1 → SAR0 CH0
```

不能通过配置把它改成：

```text
TCPWM Group1 CNT0 tr_out1 → SAR1 CH4
```

---

## 22. PASS

PASS 可以理解为芯片的可编程模拟子系统。

它包含或管理：

- SAR ADC
- 模拟输入选择
- ADC 通道配置
- ADC 触发输入
- ADC 结果和状态
- ADC 中断

链路中的位置为：

```text
TriggerMux → PASS trigger input → SAR ADC
```

---

## 23. PASS0_EPASS_MMIO

`MMIO` 表示 Memory-Mapped I/O，即存储器映射寄存器。

```c
PASS0_EPASS_MMIO
```

表示 PASS0 的扩展配置寄存器区域。

例如：

```c
Cy_Adc_SetGenericTriggerInput(PASS0_EPASS_MMIO, 0u, 0u, 0u);
```

实际上是在配置该 SAR 的 `PASS_SAR_TR_IN_SEL` 寄存器。

---

## 24. SAR ADC

`SAR` 是 Successive Approximation Register，即逐次逼近型 ADC。
    
它通过逐位比较，把模拟电压转换成数字结果。

本芯片有三套独立的 SAR：

```text
SAR0
SAR1
SAR2
```

因此：

```text
SAR0 一个通道
SAR1 一个通道
SAR2 一个通道
```

可以并行采样和转换。

但：

```text
SAR0 CH0
SAR0 CH1
SAR0 CH2
```

仍然共用一套 SAR0 转换核心，只能依次转换。

---

## 25. ADC Channel

ADC Channel 是 SAR 内的一份通道配置，不等于一台独立 ADC。

每个通道保存自己的：

- 模拟输入地址
- 采样时间
- 触发源
- 优先级
- 结果处理方式
- 中断配置

例如：

```text
SAR0：一台 ADC 转换器
SAR0 CH0：SAR0 的第0个通道配置
SAR0 CH1：SAR0 的第1个通道配置
```

---

## 26. pinAddress

`pinAddress` 表示 SAR 内部需要采集的模拟输入编号。

需要区分：

```text
ADC Channel：通道配置槽
pinAddress：该配置槽选择的实际模拟输入
```

二者经常使用相同编号，但概念并不相同。

---

## 27. tr_sar_ch_in

`tr_sar_ch_in` 是 SAR Channel 的专用触发输入。

它用于一对一触发链路：

```text
TCPWM tr_out1
    ↓
PERI One-to-One
    ↓
PASS tr_sar_ch_in
    ↓
固定 ADC Channel
```

ADC 通道配置为：

```c
Adc_channel_config.triggerSelection = CY_ADC_TRIGGER_TCPWM;
```

就是监听这条专用触发输入。

---

## 28. tr_sar_gen_in

`gen` 是 Generic，即通用。

`tr_sar_gen_in` 是 PASS 的通用 SAR 触发输入，例如：

```text
tr_sar_gen_in[0]
```

它不是某个固定 ADC 通道的专线，而是一条可以再分配给 SAR0、SAR1、SAR2 的通用触发线。

---

## 29. GENERIC0~4

每个 SAR 内部有多个通用触发选择入口：

```text
GENERIC0
GENERIC1
GENERIC2
GENERIC3
GENERIC4
```

`GENERIC0` 不一定永远连接 PASS 通用触发线0。

具体连接关系由 `PASS_SAR_TR_IN_SEL.IN0_SEL` 决定。

例如：

```c
Cy_Adc_SetGenericTriggerInput(PASS0_EPASS_MMIO, 1u, 0u, 3u);
```

表示：

```text
SAR1 的 GENERIC0 ← PASS 通用触发线3
```

参数依次为：

```text
PASS 实例、SAR 编号、GENERIC 编号、PASS 通用触发线编号
```

---

## 30. SEL

`SEL` 是 Select，即选择。

`PASS_SAR_CH_TR_CTL.SEL` 决定 ADC 通道使用哪个触发源：

| SEL | SDK 枚举 | 含义 |
| ---: | --- | --- |
| 0 | `CY_ADC_TRIGGER_OFF` | 不直接接收外部触发 |
| 1 | `CY_ADC_TRIGGER_TCPWM` | 一对一 TCPWM 触发 |
| 2 | `CY_ADC_TRIGGER_GENERIC0` | 通用触发0 |
| 3 | `CY_ADC_TRIGGER_GENERIC1` | 通用触发1 |
| 4 | `CY_ADC_TRIGGER_GENERIC2` | 通用触发2 |
| 5 | `CY_ADC_TRIGGER_GENERIC3` | 通用触发3 |
| 6 | `CY_ADC_TRIGGER_GENERIC4` | 通用触发4 |
| 7 | `CY_ADC_TRIGGER_CONTINUOUS` | 连续触发 |

---

## 31. CY_ADC_TRIGGER_OFF

`CY_ADC_TRIGGER_OFF` 不一定表示通道永远不转换。

在同一个 SAR 的通道组中，它通常表示：

```text
这个通道不直接接收硬件触发
由组内前一个通道带着继续转换
```

例如：

```text
CH0：GENERIC0，组首
CH1：OFF
CH2：OFF，isGroupEnd=true
```

一次触发后的顺序为：

```text
CH0 → CH1 → CH2 → Group Done
```

---

## 32. isGroupEnd

`isGroupEnd` 表示该通道是不是当前 ADC 转换组的最后一个通道。

例如：

```text
CH0：isGroupEnd=false
CH1：isGroupEnd=false
CH2：isGroupEnd=true
```

CH2 转换完成后，本组结束并可以产生 Group Done。

如果某个 SAR 只采一个电流通道，该通道本身就是组首和组尾：

```c
Adc_channel_config.isGroupEnd = true;
```

---

## 33. Group Done

`Group Done` 表示一个 ADC 转换组已经全部完成。

```text
PWM 触发
   ↓
组内 ADC 通道全部转换
   ↓
Group Done
   ↓
进入 ADC 中断
   ↓
读取电流并执行 FOC
```

配置：

```c
Adc_channel_config.mask.grpDone = true;
```

---

## 34. Edge Trigger

边沿触发只关心信号发生跳变的瞬间。

```text
低电平 → 高电平
         ↑
       触发一次
```

当前使用：

```c
TRIGGER_TYPE_EDGE
```

它适合 ADC 启动、DMA 请求等单次硬件事件。

TriggerMux 会把触发同步到接收外设时钟域，并形成接收时钟域内的触发脉冲。

---

## 35. Level Trigger

电平触发表示只要信号保持有效电平，触发条件就一直存在。

```text
________████████████________
        整段时间有效
```

ADC 周期启动通常优先使用 Edge，避免有效电平保持期间形成持续请求。

---

## 36. Trigger Synchronization

TCPWM 和 PASS/SAR 可能处于不同的时钟域。

TriggerMux 需要进行同步：

```text
TCPWM 时钟域
      ↓
触发同步器
      ↓
PASS/SAR 时钟域
```

所以 ADC 真正开始采样的时刻相对于 CC1 会有固定的时钟延迟。

这个延迟一般是确定的，可以通过调整 CC1 进行补偿。

---

## 37. Sample 与 Conversion

### 37.1 Sample

采样阶段是 ADC 内部采样电容连接模拟输入：

```text
模拟输入 → 采样电容
```

采样时间由：

```c
Adc_channel_config.sampleTime
```

决定。

### 37.2 Conversion

采样结束后，SAR 才开始逐次逼近转换：

```text
采样电压 → SAR 逐位比较 → 数字结果
```

完整时序为：

```text
CC1 Trigger
    ↓
触发同步延迟
    ↓
ADC Sample
    ↓
SAR Conversion
    ↓
Result Valid / Group Done
    ↓
CPU 读取结果
```

触发时刻和 ADC 结果产生时刻不是同一个时刻。

---

## 38. 一对一触发链路翻译

```text
TCPWM Group1 CNT0
    ↓ CC1 Match
TCPWM tr_out1[256]
    ↓
PERI One-to-One
    ↓
PASS tr_sar_ch_in[0]
    ↓
SAR0 CH0，SEL=TCPWM
    ↓
ADC 采样和转换
```

翻译成人话：

> PWM 主计数器运行到 CC1 时产生事件，这个事件经过芯片内部固定专线送到 SAR0 CH0，SAR0 CH0 随即开始采样。

---

## 39. 通用触发链路翻译

```text
TCPWM Group1 CNT0
    ↓ CC1 Match
TCPWM tr_out1[256]
    ↓
TriggerMux Group6
    ↓
PASS tr_sar_gen_in[0]
    ↓
SAR0/SAR1/SAR2 GENERIC0
    ↓
各 SAR 的 ADC Channel
    ↓
ADC 采样和转换
```

翻译成人话：

> PWM 主计数器运行到 CC1 时产生事件。TriggerMux 把这个事件送到 PASS 通用触发线0。SAR0、SAR1、SAR2都监听这条线，因此三套 ADC 可以在同一个 PWM 时刻开始采样。

---

## 40. 常见缩写速查表

| 缩写 | 含义 |
| --- | --- |
| TCPWM | Timer/Counter/PWM 外设 |
| CNT | Counter，计数器 |
| GRP | Group，硬件分组 |
| CC | Compare/Capture，比较/捕获 |
| TC | Terminal Count，周期终点事件 |
| TR | Trigger，触发 |
| IN | Input，输入 |
| OUT | Output，输出 |
| MUX | Multiplexer，多路选择器 |
| SEL | Select，选择 |
| CTL | Control，控制 |
| CMD | Command，命令 |
| INTR | Interrupt，中断 |
| CH | Channel，通道 |
| GEN | Generic，通用 |
| PASS | 可编程模拟子系统 |
| SAR | Successive Approximation Register ADC |
| MMIO | Memory-Mapped I/O，存储器映射寄存器 |
| PCLK | Peripheral Clock，外设时钟 |
| PWM | Pulse Width Modulation，脉宽调制 |

