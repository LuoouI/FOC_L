#include "My_ADC.h"
#include "Current_sample/Current_sample.h"
#include "Motor_Control/Motor_Control.h"
#include "adc/cy_adc.h"
#include "trigmux/cy_trigmux.h"

/*===========================================================================*/
/*  两路电流ADC硬件描述                                                       */
/*===========================================================================*/
static const AdcChannel_t AdcChannels[] =
{
    /* W相电流采样通道 */
    {
        PASS0_SAR2,
        PASS0_SAR2_CH0,
        CY_ADC_PIN_ADDRESS_AN0,
        CY_ADC_PORT_ADDRESS_SARMUX0,
        P18_0_PORT,
        P18_0_PIN,
        P18_0_AMUXA,
        pass_0_interrupts_sar_64_IRQn
    },
    /* U相电流采样通道 */
    {
        PASS0_SAR0,
        PASS0_SAR0_CH9,
        CY_ADC_PIN_ADDRESS_AN9,
        CY_ADC_PORT_ADDRESS_SARMUX0,
        P7_1_PORT,
        P7_1_PIN,
        P7_1_AMUXA,
        pass_0_interrupts_sar_9_IRQn
    }
};

/***********************************************
 * @brief : 配置SAR外设时钟
 * @param : ClockDst SAR外设时钟目标
 * @return: void
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
static void My_ADC_Clock_Init(en_clk_dst_t ClockDst)
{
    Cy_SysClk_PeriphAssignDivider(
        ClockDst,
        CY_SYSCLK_DIV_16_BIT,
        ADC_CLOCK_DIVIDER_INDEX);

    Cy_SysClk_PeriphSetDivider(
        CY_SYSCLK_DIV_16_BIT,
        ADC_CLOCK_DIVIDER_INDEX,
        ADC_CLOCK_DIVIDER_VALUE);

    Cy_SysClk_PeriphEnableDivider(
        CY_SYSCLK_DIV_16_BIT,
        ADC_CLOCK_DIVIDER_INDEX);
}

/***********************************************
 * @brief : 配置ADC模拟输入引脚
 * @param : Port GPIO端口
 * @param : Pin 端口内引脚编号
 * @param : Hsiom 模拟输入复用功能
 * @return: void
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
static void My_ADC_Pin_Init(volatile stc_GPIO_PRT_t *Port, uint32 Pin, en_hsiom_sel_t Hsiom)
{
    cy_stc_gpio_pin_config_t PinConfig = {0};

    PinConfig.driveMode = CY_GPIO_DM_ANALOG;
    PinConfig.hsiom = Hsiom;
    Cy_GPIO_Pin_Init(Port, Pin, &PinConfig);
}

/***********************************************
 * @brief : 初始化一个SAR模块
 * @param : Sar SAR模块
 * @param : ClockDst SAR模块时钟目标
 * @return: void
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
static void My_ADC_Sar_Init(volatile stc_PASS_SAR_t *Sar,en_clk_dst_t ClockDst)
{
    cy_stc_adc_config_t AdcConfig = {0};

    My_ADC_Clock_Init(ClockDst);

    AdcConfig.msbStretchMode = CY_ADC_MSB_STRETCH_MODE_1CYCLE;
    AdcConfig.sarMuxEnable = true;
    AdcConfig.adcEnable = true;
    AdcConfig.sarIpEnable = true;

    Cy_Adc_Init(Sar, &AdcConfig);
}

/***********************************************
 * @brief : 初始化一个SAR ADC通道
 * @param : AdcChannel ADC通道硬件描述
 * @return: void
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
static void My_ADC_Channel_Init(const AdcChannel_t *AdcChannel)
{
    cy_stc_adc_channel_config_t ChannelConfig;

    memset(&ChannelConfig, 0, sizeof(ChannelConfig));

    My_ADC_Pin_Init(AdcChannel->Port, AdcChannel->Pin, AdcChannel->Hsiom);
    Cy_Adc_Channel_DeInit(AdcChannel->Channel);

    ChannelConfig.triggerSelection = CY_ADC_TRIGGER_GENERIC0;
    ChannelConfig.channelPriority = 0u;
    ChannelConfig.preenptionType = CY_ADC_PREEMPTION_FINISH_RESUME;
    ChannelConfig.isGroupEnd = true;
    ChannelConfig.doneLevel = CY_ADC_DONE_LEVEL_LEVEL;
    ChannelConfig.pinAddress = AdcChannel->PinAddress;
    ChannelConfig.portAddress = AdcChannel->PortAddress;
    ChannelConfig.extMuxEnable = false;
    ChannelConfig.preconditionMode = CY_ADC_PRECONDITION_MODE_OFF;
    ChannelConfig.overlapDiagMode = CY_ADC_OVERLAP_DIAG_MODE_OFF;
    ChannelConfig.sampleTime = ADC_SAMPLE_TIME;
    ChannelConfig.calibrationValueSelect = CY_ADC_CALIBRATION_VALUE_REGULAR;
    ChannelConfig.postProcessingMode = CY_ADC_POST_PROCESSING_MODE_NONE;
    ChannelConfig.resultAlignment = CY_ADC_RESULT_ALIGNMENT_RIGHT;
    ChannelConfig.signExtention = CY_ADC_SIGN_EXTENTION_UNSIGNED;
    ChannelConfig.rightShift = 0u;
    ChannelConfig.mask.grpDone = true;

    Cy_Adc_Channel_Init(AdcChannel->Channel, &ChannelConfig);
    Cy_Adc_Channel_Enable(AdcChannel->Channel);
}

/***********************************************
 * @brief : 读取一个硬件触发SAR ADC通道的最近结果
 * @param : AdcChannel ADC通道硬件描述
 * @param : LastValue 上一次有效采样值
 * @return: ADC原始采样值
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
static uint16 My_ADC_ReadResult(const AdcChannel_t *AdcChannel, uint16 LastValue)
{
    uint16 AdcValue = 0u;
    cy_stc_adc_ch_status_t AdcStatus = {0};

    if (Cy_Adc_Channel_GetResult(
            AdcChannel->Channel,
            &AdcValue,
            &AdcStatus) != CY_ADC_SUCCESS)
    {
        return LastValue;
    }

    if (!AdcStatus.valid)
    {
        return LastValue;
    }

    return AdcValue;
}

/***********************************************
 * @brief : 检查两路ADC组转换完成标志
 * @param : ChannelIndex 当前中断对应的ADC通道索引
 * @return: true两路均已完成，false至少一路未完成
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
static bool My_ADC_IsBothGroupDone(uint32 ChannelIndex)
{
    uint32 OtherChannelIndex;
    uint32 OtherInterruptStatus;

    OtherChannelIndex = (ChannelIndex == 0u) ? 1u : 0u;
    OtherInterruptStatus =
        AdcChannels[OtherChannelIndex].Channel->unINTR_MASKED.u32Register;

    return ((OtherInterruptStatus &
             PASS_SAR_CH_INTR_MASKED_GRP_DONE_MASKED_Msk) != 0u);
}

/***********************************************
 * @brief : 处理单路ADC转换完成中断
 * @param : ChannelIndex ADC通道描述数组索引
 * @return: void
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
static void My_ADC_Interrupt_Handle(uint32 ChannelIndex)
{
    static volatile uint8 Adc1SampleDone = 0u;
    static volatile uint8 Adc2SampleDone = 0u;
    static uint16 AdcLastRawW = 0u;
    static uint16 AdcLastRawU = 0u;
    cy_stc_adc_interrupt_source_t InterruptStatus = {0};
    bool IsFirstInterrupt;
    bool BothGroupDoneAtEntry = false;

    IsFirstInterrupt = ((Adc1SampleDone == 0u) && (Adc2SampleDone == 0u));
    if (IsFirstInterrupt)
    {
        /* 在中断处理之前锁存两路硬件完成状态 */
        BothGroupDoneAtEntry = My_ADC_IsBothGroupDone(ChannelIndex);
    }

    if (Cy_Adc_Channel_GetInterruptMaskedStatus(
            AdcChannels[ChannelIndex].Channel,
            &InterruptStatus) != CY_ADC_SUCCESS)
    {
        return;
    }

    if (!InterruptStatus.grpDone)
    {
        return;
    }

    if (IsFirstInterrupt)
    {
        gpio_toggle_level(ADC_FIRST_ISR_DEBUG_PIN);
        if (BothGroupDoneAtEntry)
        {
            gpio_toggle_level(ADC_BOTH_DONE_DEBUG_PIN);
        }
    }

    if (ChannelIndex == 0u)
    {
        AdcLastRawW = My_ADC_ReadResult(&AdcChannels[0], AdcLastRawW);
        Adc1SampleDone = 1u;
    }
    else
    {
        AdcLastRawU = My_ADC_ReadResult(&AdcChannels[1], AdcLastRawU);
        Adc2SampleDone = 1u;
    }

    Cy_Adc_Channel_ClearInterruptStatus(
        AdcChannels[ChannelIndex].Channel,
        &InterruptStatus);

    if ((Adc1SampleDone != 0u) && (Adc2SampleDone != 0u))
    {
        Adc1SampleDone = 0u;
        Adc2SampleDone = 0u;

        Current_Sample_Update(AdcLastRawU, AdcLastRawW);
        Angle_Update();
        Current_Sample_Transform(Motor.Encoder.Electrical_angle);
        Motor_Control_Loop();
    }
}

/***********************************************
 * @brief : ADC1转换完成中断服务函数
 * @param : /
 * @return: void
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
static void My_ADC1_ISR(void)
{
    My_ADC_Interrupt_Handle(0u);
}

/***********************************************
 * @brief : ADC2转换完成中断服务函数
 * @param : /
 * @return: void
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
static void My_ADC2_ISR(void)
{
    My_ADC_Interrupt_Handle(1u);
}

/***********************************************
 * @brief : 初始化两路ADC转换完成中断
 * @param : /
 * @return: void
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
static void My_ADC_Interrupt_Init(void)
{
    cy_stc_sysint_irq_t InterruptConfig;

    memset(&InterruptConfig, 0, sizeof(InterruptConfig));
    InterruptConfig.sysIntSrc = AdcChannels[0].InterruptSource;
    InterruptConfig.intIdx = CPUIntIdx5_IRQn;
    InterruptConfig.isEnabled = true;
    interrupt_init(&InterruptConfig, My_ADC1_ISR, 2u);

    InterruptConfig.sysIntSrc = AdcChannels[1].InterruptSource;
    InterruptConfig.intIdx = CPUIntIdx6_IRQn;
    interrupt_init(&InterruptConfig, My_ADC2_ISR, 2u);
}

/***********************************************
 * @brief : 配置TCPWM CC1到PASS通用ADC触发线
 * @param : /
 * @return: void
 * @date  : 2026-08-16
 * @author: L   
 ************************************************/
static void My_ADC_Trigger_Init(void)
{
    cy_en_trigmux_status_t TriggerStatus;
    cy_en_adc_status_t AdcStatus;

    AdcStatus = Cy_Adc_SetGenericTriggerInput(
        PASS0_EPASS_MMIO,
        0u,
        0u,
        0u);
    if (AdcStatus != CY_ADC_SUCCESS)
    {
        return;
    }

    AdcStatus = Cy_Adc_SetGenericTriggerInput(
        PASS0_EPASS_MMIO,
        2u,
        0u,
        0u);
    if (AdcStatus != CY_ADC_SUCCESS)
    {
        return;
    }

    TriggerStatus = Cy_TrigMux_Connect(
        TRIG_IN_MUX_6_TCPWM_16M_TR_OUT10,
        TRIG_OUT_MUX_6_PASS_GEN_TR_IN0,
        CY_TR_MUX_TR_INV_DISABLE,
        TRIGGER_TYPE_EDGE,
        0u);

    if (TriggerStatus != CY_TRIGMUX_SUCCESS)
    {
        return;
    }
}

/***********************************************
 * @brief : 初始化两路电流ADC及硬件触发中断
 * @param : /
 * @return: void
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
static void My_ADC_CurrentHardware_Init(void)
{
    uint32 ChannelIndex;

    gpio_init(ADC_FIRST_ISR_DEBUG_PIN, GPO, GPIO_LOW, GPO_PUSH_PULL);
    gpio_init(ADC_BOTH_DONE_DEBUG_PIN, GPO, GPIO_LOW, GPO_PUSH_PULL);

    My_ADC_Sar_Init(PASS0_SAR0, PCLK_PASS0_CLOCK_SAR0);
    My_ADC_Sar_Init(PASS0_SAR2, PCLK_PASS0_CLOCK_SAR2);
    My_ADC_Trigger_Init();
    
    for (ChannelIndex = 0u;
         ChannelIndex < (sizeof(AdcChannels) / sizeof(AdcChannels[0]));
         ChannelIndex++)
    {
        My_ADC_Channel_Init(&AdcChannels[ChannelIndex]);
    }

    My_ADC_Interrupt_Init();
}

/***********************************************
 * @brief : 初始化母线电压ADC
 * @param : /
 * @return: void
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
static void My_ADC_VoltageHardware_Init(void)
{
    adc_init(ADC_V_PIN, ADC_12BIT);
}

void My_ADC_Init(void)
{
    Current_Sample_Init();
    My_ADC_VoltageHardware_Init();
    My_ADC_CurrentHardware_Init();
}

uint16 My_ADC_GetBatteryRawValue(void)
{
    return adc_convert(ADC_V_PIN);
}

float My_ADC_GetBatteryVoltage(void)
{
    uint16 BatteryRaw;
    float BatteryVoltage;

    BatteryRaw = My_ADC_GetBatteryRawValue();
    BatteryVoltage = (float)BatteryRaw * ADC_REF_VOLTAGE / ADC_MAX_VALUE;
    BatteryVoltage *= BATTERY_DIVIDER_RATIO * BATTERY_VOLTAGE_CALIBRATION;

    return BatteryVoltage;
}
