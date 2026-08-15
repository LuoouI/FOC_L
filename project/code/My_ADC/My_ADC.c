#include "My_ADC.h"

static volatile uint16 Adc1Value;
static volatile uint16 Adc2Value;

/***********************************************
 * @brief : 配置SAR外设时钟
 * @param : ClockDst SAR外设时钟目标
 * @return: void
 * @date  : 2026-08-14
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
 * @date  : 2026-08-14
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
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
static void My_ADC_Sar_Init(
    volatile stc_PASS_SAR_t *Sar,
    en_clk_dst_t ClockDst)
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
 * @param : Channel ADC通道
 * @param : PinAddress SAR模拟输入地址
 * @param : Port ADC模拟输入GPIO端口
 * @param : Pin ADC模拟输入GPIO引脚编号
 * @return: void
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
static void My_ADC_Channel_Init(
    volatile stc_PASS_SAR_CH_t *Channel,
    cy_en_adc_pin_address_t PinAddress,
    volatile stc_GPIO_PRT_t *Port,
    uint32 Pin)
{
    cy_stc_adc_channel_config_t ChannelConfig = {0};

    My_ADC_Pin_Init(Port, Pin);
    Cy_Adc_Channel_DeInit(Channel);

    ChannelConfig.triggerSelection = CY_ADC_TRIGGER_OFF;
    ChannelConfig.channelPriority = 0u;
    ChannelConfig.preenptionType = CY_ADC_PREEMPTION_FINISH_RESUME;
    ChannelConfig.isGroupEnd = true;
    ChannelConfig.doneLevel = CY_ADC_DONE_LEVEL_LEVEL;
    ChannelConfig.pinAddress = PinAddress;
    ChannelConfig.portAddress = CY_ADC_PORT_ADDRESS_SARMUX0;
    ChannelConfig.extMuxEnable = true;
    ChannelConfig.preconditionMode = CY_ADC_PRECONDITION_MODE_OFF;
    ChannelConfig.overlapDiagMode = CY_ADC_OVERLAP_DIAG_MODE_OFF;
    ChannelConfig.sampleTime = ADC_SAMPLE_TIME;
    ChannelConfig.calibrationValueSelect = CY_ADC_CALIBRATION_VALUE_REGULAR;
    ChannelConfig.postProcessingMode = CY_ADC_POST_PROCESSING_MODE_NONE;
    ChannelConfig.resultAlignment = CY_ADC_RESULT_ALIGNMENT_RIGHT;
    ChannelConfig.signExtention = CY_ADC_SIGN_EXTENTION_UNSIGNED;
    ChannelConfig.rightShift = 0u;

    Cy_Adc_Channel_Init(Channel, &ChannelConfig);
    Cy_Adc_Channel_Enable(Channel);
}

/***********************************************
 * @brief : 软件触发并读取一个SAR ADC通道
 * @param : Sar SAR模块
 * @param : Channel ADC通道
 * @return: ADC原始采样值
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
static uint16 My_ADC_ReadChannel(
    volatile stc_PASS_SAR_t *Sar,
    volatile stc_PASS_SAR_CH_t *Channel)
{
    uint16 AdcValue = 0u;
    cy_stc_adc_ch_status_t AdcStatus = {0};

    Cy_Adc_Channel_SoftwareTrigger(Channel);

    while (Sar->unSTATUS.stcField.u1BUSY != 0u)
    {
    }

    if (Cy_Adc_Channel_GetResult(Channel, &AdcValue, &AdcStatus) != CY_ADC_SUCCESS)
    {
        return 0u;
    }

    if (!AdcStatus.valid)
    {
        return 0u;
    }

    return AdcValue;
}

void My_ADC_Init(void)
{
    adc_init(ADC_V_PIN, ADC_12BIT);

    My_ADC_Sar_Init(PASS0_SAR2, PCLK_PASS0_CLOCK_SAR2);

    My_ADC_Channel_Init(
        PASS0_SAR2_CH0,
        (cy_en_adc_pin_address_t)(ADC_1_PIN % 32u),
        GPIO_PRT18,
        0u);

    My_ADC_Channel_Init(
        PASS0_SAR2_CH1,
        (cy_en_adc_pin_address_t)(ADC_2_PIN % 32u),
        GPIO_PRT18,
        1u);
}

void My_ADC_Sample(void)
{
    Adc1Value = My_ADC_ReadChannel(PASS0_SAR2, PASS0_SAR2_CH0);
    Adc2Value = My_ADC_ReadChannel(PASS0_SAR2, PASS0_SAR2_CH1);
}

uint16 My_ADC_GetAdc1Value(void)
{
    return Adc1Value;
}

uint16 My_ADC_GetAdc2Value(void)
{
    return Adc2Value;
}

uint16 My_ADC_GetBatteryRawValue(void)
{
    return adc_convert(ADC_V_PIN);
}

float My_ADC_GetBatteryVoltage(void)
{
    uint16 AdcValue;
    float AdcVoltage;

    AdcValue = adc_mean_filter_convert(ADC_V_PIN, 16u);
    AdcVoltage = (float)AdcValue * ADC_REF_VOLTAGE / ADC_MAX_VALUE;

    return AdcVoltage * BATTERY_DIVIDER_RATIO * BATTERY_VOLTAGE_CALIBRATION;
}
