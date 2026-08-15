#include "My_ADC.h"

AdcData_t MyAdc = {0};

/*===========================================================================*/
/*  两路电流ADC硬件描述                                                       */
/*===========================================================================*/
static const AdcChannel_t AdcChannels[] =
{
    {
        PASS0_SAR2,
        PASS0_SAR2_CH0,
        CY_ADC_PIN_ADDRESS_AN0,
        P18_0_PORT,
        P18_0_PIN
    },
    {
        PASS0_SAR0,
        PASS0_SAR0_CH8,
        CY_ADC_PIN_ADDRESS_AN8,
        P7_0_PORT,
        P7_0_PIN
    }
};

/***********************************************
 * @brief : 配置SAR外设时钟
 * @param : ClockDst SAR外设时钟目标
 * @return: void
 * @date  : 2026-08-15
 * @author: LYF
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
 * @return: void
 * @date  : 2026-08-15
 * @author: LYF
 ************************************************/
static void My_ADC_Pin_Init(volatile stc_GPIO_PRT_t *Port, uint32 Pin)
{
    cy_stc_gpio_pin_config_t PinConfig = {0};

    PinConfig.driveMode = CY_GPIO_DM_ANALOG;
    Cy_GPIO_Pin_Init(Port, Pin, &PinConfig);
}

/***********************************************
 * @brief : 初始化一个SAR模块
 * @param : Sar SAR模块
 * @param : ClockDst SAR模块时钟目标
 * @return: void
 * @date  : 2026-08-15
 * @author: LYF
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
 * @author: LYF
 ************************************************/
static void My_ADC_Channel_Init(const AdcChannel_t *AdcChannel)
{
    cy_stc_adc_channel_config_t ChannelConfig;

    memset(&ChannelConfig, 0, sizeof(ChannelConfig));

    My_ADC_Pin_Init(AdcChannel->Port, AdcChannel->Pin);
    Cy_Adc_Channel_DeInit(AdcChannel->Channel);

    ChannelConfig.triggerSelection = CY_ADC_TRIGGER_OFF;
    ChannelConfig.channelPriority = 0u;
    ChannelConfig.preenptionType = CY_ADC_PREEMPTION_FINISH_RESUME;
    ChannelConfig.isGroupEnd = true;
    ChannelConfig.doneLevel = CY_ADC_DONE_LEVEL_LEVEL;
    ChannelConfig.pinAddress = AdcChannel->PinAddress;
    ChannelConfig.portAddress = CY_ADC_PORT_ADDRESS_SARMUX0;
    ChannelConfig.extMuxEnable = false;
    ChannelConfig.preconditionMode = CY_ADC_PRECONDITION_MODE_OFF;
    ChannelConfig.overlapDiagMode = CY_ADC_OVERLAP_DIAG_MODE_OFF;
    ChannelConfig.sampleTime = ADC_SAMPLE_TIME;
    ChannelConfig.calibrationValueSelect = CY_ADC_CALIBRATION_VALUE_REGULAR;
    ChannelConfig.postProcessingMode = CY_ADC_POST_PROCESSING_MODE_NONE;
    ChannelConfig.resultAlignment = CY_ADC_RESULT_ALIGNMENT_RIGHT;
    ChannelConfig.signExtention = CY_ADC_SIGN_EXTENTION_UNSIGNED;
    ChannelConfig.rightShift = 0u;

    Cy_Adc_Channel_Init(AdcChannel->Channel, &ChannelConfig);
    Cy_Adc_Channel_Enable(AdcChannel->Channel);
}

/***********************************************
 * @brief : 软件触发并读取一个SAR ADC通道
 * @param : AdcChannel ADC通道硬件描述
 * @return: ADC原始采样值
 * @date  : 2026-08-15
 * @author: LYF
 ************************************************/
static uint16 My_ADC_ReadChannel(const AdcChannel_t *AdcChannel)
{
    uint16 AdcValue = 0u;
    cy_stc_adc_ch_status_t AdcStatus = {0};

    Cy_Adc_Channel_SoftwareTrigger(AdcChannel->Channel);

    while (AdcChannel->Sar->unSTATUS.stcField.u1BUSY != 0u)
    {
    }

    if (Cy_Adc_Channel_GetResult(
            AdcChannel->Channel,
            &AdcValue,
            &AdcStatus) != CY_ADC_SUCCESS)
    {
        return 0u;
    }

    if (!AdcStatus.valid)
    {
        return 0u;
    }

    return AdcValue;
}

void My_ADC_Current_Init(void)
{
    uint32 ChannelIndex;

    MyAdc.Adc1Raw = 0u;
    MyAdc.Adc2Raw = 0u;
    MyAdc.BatteryRaw = 0u;
    MyAdc.BatteryVoltage = 0.0f;
    MyAdc.SampleReady = 0u;

    My_ADC_Sar_Init(PASS0_SAR0, PCLK_PASS0_CLOCK_SAR0);
    My_ADC_Sar_Init(PASS0_SAR2, PCLK_PASS0_CLOCK_SAR2);
    // My_ADC_Sar_Init(PASS0_SAR1, PCLK_PASS0_CLOCK_SAR1);
    
    for (ChannelIndex = 0u;
         ChannelIndex < (sizeof(AdcChannels) / sizeof(AdcChannels[0]));
         ChannelIndex++)
    {
        My_ADC_Channel_Init(&AdcChannels[ChannelIndex]);
    }
}

void My_ADC_Voltage_Init(void)
{
    adc_init(ADC_V_PIN, ADC_12BIT);
}

void My_ADC_Sample(void)
{
    MyAdc.SampleReady = 0u;
    MyAdc.Adc1Raw = My_ADC_ReadChannel(&AdcChannels[0]);
    MyAdc.Adc2Raw = My_ADC_ReadChannel(&AdcChannels[1]);
    MyAdc.SampleReady = 1u;
}

uint16 My_ADC_GetAdc1Value(void)
{
    return MyAdc.Adc1Raw;
}

uint16 My_ADC_GetAdc2Value(void)
{
    return MyAdc.Adc2Raw;
}

uint16 My_ADC_GetBatteryRawValue(void)
{
    MyAdc.BatteryRaw = adc_convert(ADC_V_PIN);
    return MyAdc.BatteryRaw;
}

float My_ADC_GetBatteryVoltage(void)
{
    MyAdc.BatteryRaw = adc_mean_filter_convert(ADC_V_PIN, 16u);
    MyAdc.BatteryVoltage = (float)MyAdc.BatteryRaw * ADC_REF_VOLTAGE / ADC_MAX_VALUE;
    MyAdc.BatteryVoltage *= BATTERY_DIVIDER_RATIO * BATTERY_VOLTAGE_CALIBRATION;

    return MyAdc.BatteryVoltage;
}
