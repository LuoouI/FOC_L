#include "My_TCPWM.h"

/*===========================================================================*/
/*  三相桥臂硬件描述                                                          */
/*===========================================================================*/
static const TCPWM_3PHASE_T TCPWM_3PHASE = 
{
    .a = {  TCPWM0_GRP0_CNT48, PCLK_TCPWM0_CLOCKS48,
            GPIO_PRT14, 0u, P14_0_TCPWM0_LINE48,
            GPIO_PRT14, 1u, P14_1_TCPWM0_LINE_COMPL48 },
    .b = {  TCPWM0_GRP0_CNT53, PCLK_TCPWM0_CLOCKS53,
            GPIO_PRT18, 4u, P18_4_TCPWM0_LINE53,
            GPIO_PRT18, 5u, P18_5_TCPWM0_LINE_COMPL53},
    .c = {  TCPWM0_GRP0_CNT51,PCLK_TCPWM0_CLOCKS51,
            GPIO_PRT18, 6u, P18_6_TCPWM0_LINE51,
            GPIO_PRT18, 7U, P18_7_TCPWM0_LINE_COMPL51},
};

/***********************************************
 * @brief : 翻转TCPWM中心对齐周期调试引脚
 * @param : /
 * @return: void
 * @date  : 2026-08-16
 * @author: LYF
 ************************************************/
static void TCPWM_Center_DebugPin_Toggle(void)
{
    gpio_toggle_level(TCPWM_CENTER_DEBUG_PIN);
}

/***********************************************
 * @brief : TCPWM中心对齐周期中断服务函数
 * @param : /
 * @return: void
 * @date  : 2026-08-16
 * @author: LYF
 ************************************************/
static void TCPWM_Center_ISR(void)
{
    if (Cy_Tcpwm_Counter_GetTC_IntrMasked(TCPWM0_GRP1_CNT0) != 0u)
    {
        Cy_Tcpwm_Counter_ClearTC_Intr(TCPWM0_GRP1_CNT0);
        TCPWM_Center_DebugPin_Toggle();
    }
}

/***********************************************
 * @brief : 初始化TCPWM中心对齐周期调试中断
 * @param : /
 * @return: void
 * @date  : 2026-08-16
 * @author: LYF
 ************************************************/
static void TCPWM_Center_Interrupt_Init(void)
{
    cy_stc_sysint_irq_t InterruptConfig;

    memset(&InterruptConfig, 0, sizeof(InterruptConfig));
    InterruptConfig.sysIntSrc = tcpwm_0_interrupts_256_IRQn;
    InterruptConfig.intIdx = CPUIntIdx4_IRQn;
    InterruptConfig.isEnabled = true;

    gpio_init(TCPWM_CENTER_DEBUG_PIN, GPO, GPIO_LOW, GPO_PUSH_PULL);
    Cy_Tcpwm_Counter_ClearTC_Intr(TCPWM0_GRP1_CNT0);
    interrupt_init(&InterruptConfig, TCPWM_Center_ISR, 1u);
}

/***********************************************
 * @brief : 初始化ADC专用CC1采样事件计数器
 * @param : /
 * @return: void
 * @date  : 2026-08-16
 * @author: LYF
 ************************************************/
static void TCPWM_ADC_Trigger_Init(void)
{
    cy_stc_tcpwm_pwm_config_t AdcTriggerConfig;

    Cy_SysClk_PeriphAssignDivider(
        PCLK_TCPWM0_CLOCKS256,
        CY_SYSCLK_DIV_16_BIT,
        2u);

    Cy_SysClk_PeriphSetDivider(
        CY_SYSCLK_DIV_16_BIT,
        2u,
        0u);

    Cy_SysClk_PeriphEnableDivider(
        CY_SYSCLK_DIV_16_BIT,
        2u);

    memset(&AdcTriggerConfig, 0, sizeof(AdcTriggerConfig));
    Cy_Tcpwm_Pwm_DeInit(TCPWM0_GRP1_CNT0);

    AdcTriggerConfig.pwmMode            = CY_TCPWM_PWM_MODE_DEADTIME;
    AdcTriggerConfig.clockPrescaler     = CY_TCPWM_PRESCALER_DIVBY_1;
    AdcTriggerConfig.debug_pause        = false;
    AdcTriggerConfig.deadTime           = 0u;
    AdcTriggerConfig.runMode            = CY_TCPWM_PWM_CONTINUOUS;
    AdcTriggerConfig.countDirection     = CY_TCPWM_COUNTER_COUNT_UP_DOWN1;
    AdcTriggerConfig.cc0MatchMode       = CY_TCPWM_PWM_TR_CTRL2_NO_CHANGE;
    AdcTriggerConfig.overflowMode       = CY_TCPWM_PWM_TR_CTRL2_NO_CHANGE;
    AdcTriggerConfig.underflowMode      = CY_TCPWM_PWM_TR_CTRL2_NO_CHANGE;
    AdcTriggerConfig.cc1MatchMode       = CY_TCPWM_PWM_TR_CTRL2_NO_CHANGE;
    AdcTriggerConfig.period             = TCPWM_PERIOD;
    AdcTriggerConfig.compare0           = TCPWM_PERIOD / 2u;
    AdcTriggerConfig.compare1           = TCPWM_ADC_SAMPLE_COUNT;
    AdcTriggerConfig.compare1_buff      = TCPWM_ADC_SAMPLE_COUNT;
    /* COUNT_UP_DOWN1的TC仅在向下计数到零时产生，作为中心对齐周期基准 */
    AdcTriggerConfig.interruptSources   = CY_TCPWM_INT_ON_TC;
    AdcTriggerConfig.killMode           = CY_TCPWM_PWM_NOT_STOP_ON_KILL;
    AdcTriggerConfig.startInputMode     = CY_TCPWM_INPUT_RISING_EDGE;
    AdcTriggerConfig.startInput         = CY_TCPWM_INPUT_TRIG0;
    AdcTriggerConfig.countInputMode     = CY_TCPWM_INPUT_LEVEL;
    AdcTriggerConfig.countInput         = CY_TCPWM_INPUT1;
    AdcTriggerConfig.pwmOnDisable       = CY_TCPWM_PWM_OUT_MODE_LOW;
    AdcTriggerConfig.trigger0EventCfg   = CY_TCPWM_COUNTER_DISABLED;
    AdcTriggerConfig.trigger1EventCfg   = CY_TCPWM_COUNTER_CC1_MATCH;

    Cy_Tcpwm_Pwm_Init(TCPWM0_GRP1_CNT0, &AdcTriggerConfig);

    TCPWM0_GRP1_CNT0->unCTRL.stcField.u1CC0_MATCH_UP_EN    = 0u;
    TCPWM0_GRP1_CNT0->unCTRL.stcField.u1CC0_MATCH_DOWN_EN  = 0u;
    TCPWM0_GRP1_CNT0->unCTRL.stcField.u1CC1_MATCH_UP_EN    = 1u;
    TCPWM0_GRP1_CNT0->unCTRL.stcField.u1CC1_MATCH_DOWN_EN  = 0u;

    // TCPWM_Center_Interrupt_Init();
    Cy_Tcpwm_Pwm_Enable(TCPWM0_GRP1_CNT0);
}

/***********************************************
 * @brief : 初始化单相桥臂PWM引脚
 * @param : phase 单相桥臂硬件描述
 * @return: void
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
static void TCPWM_GPIO_Init(const TCPWM_PHASE_t *phase)
{
    cy_stc_gpio_pin_config_t GPIO_config;

    memset(&GPIO_config, 0, sizeof(GPIO_config));

    GPIO_config.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    GPIO_config.hsiom = phase->hsiom_h;
    Cy_GPIO_Pin_Init(phase->port_h, phase->pin_h, &GPIO_config);
    GPIO_config.hsiom = phase->hsiom_l;
    Cy_GPIO_Pin_Init(phase->port_l, phase->pin_l, &GPIO_config);
}

/***********************************************
 * @brief : 初始化TCPWM公共时钟分频器
 * @param : /
 * @return: void
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
static void TCPWM_Clock_Init(const TCPWM_PHASE_t *phase)
{
    Cy_SysClk_PeriphAssignDivider(
        phase->clock_dst,
        CY_SYSCLK_DIV_16_BIT,
        2u);

    Cy_SysClk_PeriphSetDivider(
        CY_SYSCLK_DIV_16_BIT,
        2u,
        0u);

    Cy_SysClk_PeriphEnableDivider(
        CY_SYSCLK_DIV_16_BIT,
        2u);
}

/***********************************************
 * @brief : TCPWM初始化
 * @param : /
 * @return: void
 * @date  : 2026-07-27
 * @author: LYF
 ************************************************/
static void TCPWM_Phase_Init(const TCPWM_PHASE_t *phase)
{
    cy_stc_tcpwm_pwm_config_t TCPWM_config;

    memset(&TCPWM_config, 0, sizeof(TCPWM_config));

    Cy_Tcpwm_Pwm_DeInit(phase->timer);

    TCPWM_config.pwmMode            = CY_TCPWM_PWM_MODE_DEADTIME;
    TCPWM_config.clockPrescaler     = CY_TCPWM_PRESCALER_DIVBY_1;
    TCPWM_config.debug_pause        = false;
    TCPWM_config.deadTime           = 10;
    TCPWM_config.runMode            = CY_TCPWM_PWM_CONTINUOUS;
    TCPWM_config.countDirection     = CY_TCPWM_COUNTER_COUNT_UP_DOWN1;
    TCPWM_config.cc0MatchMode       = CY_TCPWM_PWM_TR_CTRL2_INVERT;
    TCPWM_config.overflowMode       = CY_TCPWM_PWM_TR_CTRL2_SET;
    TCPWM_config.underflowMode      = CY_TCPWM_PWM_TR_CTRL2_CLEAR;
    TCPWM_config.cc1MatchMode       = CY_TCPWM_PWM_TR_CTRL2_NO_CHANGE;
    TCPWM_config.period             = TCPWM_PERIOD;
    TCPWM_config.compare0           = TCPWM_PERIOD / 2u;
    TCPWM_config.compare0_buff      = TCPWM_PERIOD / 2u;
    TCPWM_config.enableCompare0Swap = true;
    TCPWM_config.killMode           = CY_TCPWM_PWM_NOT_STOP_ON_KILL;
    TCPWM_config.countInputMode     = CY_TCPWM_INPUT_LEVEL;
    TCPWM_config.countInput         = 1uL;
    TCPWM_config.startInputMode     = CY_TCPWM_INPUT_RISING_EDGE;
    TCPWM_config.startInput         = CY_TCPWM_INPUT_TRIG0;
    TCPWM_config.pwmOnDisable       = CY_TCPWM_PWM_OUT_MODE_LOW;
    TCPWM_config.trigger0EventCfg   = CY_TCPWM_COUNTER_DISABLED;
    TCPWM_config.trigger1EventCfg   = CY_TCPWM_COUNTER_DISABLED;
    
    Cy_Tcpwm_Pwm_Init(phase->timer, &TCPWM_config);

    /* 当前SDK初始化函数未配置比较匹配方向，需要手动补齐 */
    phase->timer->unCTRL.stcField.u1CC0_MATCH_UP_EN   = 1u;
    phase->timer->unCTRL.stcField.u1CC0_MATCH_DOWN_EN = 1u;
    phase->timer->unCTRL.stcField.u1CC1_MATCH_UP_EN   = 0u;
    phase->timer->unCTRL.stcField.u1CC1_MATCH_DOWN_EN = 0u;

    Cy_Tcpwm_Pwm_Enable(phase->timer);
}

/***********************************************
 * @brief : 初始化单相桥臂全部TCPWM资源
 * @param : phase 单相桥臂硬件描述
 * @return: void
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
static void TCPWM_SinglePhase_Init(const TCPWM_PHASE_t *phase)
{
    TCPWM_Clock_Init(phase);
    TCPWM_GPIO_Init(phase);
    TCPWM_Phase_Init(phase);
}

/***********************************************
 * @brief : 将万分比占空比转换为TCPWM比较值
 * @param : Duty 占空比，范围0~10000
 * @return: TCPWM比较值
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
static uint32 TCPWM_DutyToCompare(uint16 Duty)
{
    if (Duty > TCPWM_DUTY_MAX)
    {
        Duty = TCPWM_DUTY_MAX;
    }

    return TCPWM_PERIOD -
           ((uint32)TCPWM_PERIOD * Duty / TCPWM_DUTY_MAX);
}

void My_TCPWM_Init(void)
{
    TCPWM_SinglePhase_Init(&TCPWM_3PHASE.a);
    TCPWM_SinglePhase_Init(&TCPWM_3PHASE.b);
    TCPWM_SinglePhase_Init(&TCPWM_3PHASE.c);
    TCPWM_ADC_Trigger_Init();
}

void My_TCPWM_Start(void)
{
    /* 使用TCPWM公共触发输入，让三相PWM和ADC事件计数器同步启动 */
    Cy_TrigMux_SwTrigger(
        TRIG_OUT_MUX_4_TCPWM_ALL_CNT_TR_IN0,
        TRIGGER_TYPE_EDGE,
        1u);
}

void My_TCPWM_SetDuty(uint16 DutyA, uint16 DutyB, uint16 DutyC)
{
    Cy_Tcpwm_Pwm_SetCompare0_Buff(
        TCPWM_3PHASE.a.timer,
        TCPWM_DutyToCompare(DutyA));
    Cy_Tcpwm_Pwm_SetCompare0_Buff(
        TCPWM_3PHASE.b.timer,
        TCPWM_DutyToCompare(DutyB));
    Cy_Tcpwm_Pwm_SetCompare0_Buff(
        TCPWM_3PHASE.c.timer,
        TCPWM_DutyToCompare(DutyC));

    Cy_Tcpwm_TriggerCapture0(TCPWM_3PHASE.a.timer);
    Cy_Tcpwm_TriggerCapture0(TCPWM_3PHASE.b.timer);
    Cy_Tcpwm_TriggerCapture0(TCPWM_3PHASE.c.timer);
}
