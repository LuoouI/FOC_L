#include "My_TCPWM.h"
#include "Function/Function.h"
#include "trigmux/cy_trigmux.h"

/*===========================================================================*/
/*  三相桥臂硬件描述                                                          */
/*===========================================================================*/
static const TCPWM_3PHASE_t TCPWM_3PHASE =
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
 * @brief : 初始化PWM占空比同步及ADC采样主计数器
 * @param : /
 * @return: void
 * @date  : 2026-08-18
 * @author: L
 ************************************************/
static void TCPWM_Master_Trigger_Init(void)
{
    cy_stc_tcpwm_pwm_config_t MasterTriggerConfig;

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

    memset(&MasterTriggerConfig, 0, sizeof(MasterTriggerConfig));
    Cy_Tcpwm_Pwm_DeInit(TCPWM0_GRP1_CNT0);

    MasterTriggerConfig.pwmMode            = CY_TCPWM_PWM_MODE_DEADTIME;
    MasterTriggerConfig.clockPrescaler     = CY_TCPWM_PRESCALER_DIVBY_1;
    MasterTriggerConfig.debug_pause        = false;
    MasterTriggerConfig.deadTime           = 0u;
    MasterTriggerConfig.runMode            = CY_TCPWM_PWM_CONTINUOUS;
    MasterTriggerConfig.countDirection     = CY_TCPWM_COUNTER_COUNT_UP_DOWN1;
    MasterTriggerConfig.cc0MatchMode       = CY_TCPWM_PWM_TR_CTRL2_NO_CHANGE;
    MasterTriggerConfig.overflowMode       = CY_TCPWM_PWM_TR_CTRL2_NO_CHANGE;
    MasterTriggerConfig.underflowMode      = CY_TCPWM_PWM_TR_CTRL2_NO_CHANGE;
    MasterTriggerConfig.cc1MatchMode       = CY_TCPWM_PWM_TR_CTRL2_NO_CHANGE;
    MasterTriggerConfig.period             = TCPWM_PERIOD;
    MasterTriggerConfig.compare0           = TCPWM_PERIOD / 2u;
    MasterTriggerConfig.compare1           = TCPWM_ADC_SAMPLE_COUNT;
    MasterTriggerConfig.compare1_buff      = TCPWM_ADC_SAMPLE_COUNT;
    MasterTriggerConfig.interruptSources   = CY_TCPWM_INT_NONE;
    MasterTriggerConfig.killMode           = CY_TCPWM_PWM_NOT_STOP_ON_KILL;
    /* 公共tr_all_cnt_in[0]对应TCPWM的TRIG3输入。 */
    MasterTriggerConfig.startInputMode     = CY_TCPWM_INPUT_RISING_EDGE;
    MasterTriggerConfig.startInput         = CY_TCPWM_INPUT_TRIG3;
    MasterTriggerConfig.countInputMode     = CY_TCPWM_INPUT_LEVEL;
    MasterTriggerConfig.countInput         = CY_TCPWM_INPUT1;
    MasterTriggerConfig.pwmOnDisable       = CY_TCPWM_PWM_OUT_MODE_LOW;
    MasterTriggerConfig.trigger0EventCfg   = CY_TCPWM_COUNTER_TERMINAL_COUNT;
    MasterTriggerConfig.trigger1EventCfg   = CY_TCPWM_COUNTER_CC1_MATCH;

    Cy_Tcpwm_Pwm_Init(TCPWM0_GRP1_CNT0, &MasterTriggerConfig);

    /* SDK初始化函数的计数初值判断有误，中心对齐模式明确从1开始计数 */
    TCPWM0_GRP1_CNT0->unCOUNTER.u32Register = CY_TCPWM_CNT_UP_DOWN_INIT_VAL;

    TCPWM0_GRP1_CNT0->unCTRL.stcField.u1CC0_MATCH_UP_EN    = 0u;
    TCPWM0_GRP1_CNT0->unCTRL.stcField.u1CC0_MATCH_DOWN_EN  = 0u;
    TCPWM0_GRP1_CNT0->unCTRL.stcField.u1CC1_MATCH_UP_EN    = 1u;
    TCPWM0_GRP1_CNT0->unCTRL.stcField.u1CC1_MATCH_DOWN_EN  = 0u;

    Cy_Tcpwm_Pwm_Enable(TCPWM0_GRP1_CNT0);
}

/***********************************************
 * @brief : 初始化三相PWM占空比硬件同步触发链路
 * @param : /
 * @return: void
 * @date  : 2026-08-18
 * @author: L
 ************************************************/
static void TCPWM_Duty_Sync_Init(void)
{
    (void)Cy_TrigMux_Connect(
        TRIG_IN_MUX_4_TCPWM_16M_TR_OUT00,
        TRIG_OUT_MUX_4_TCPWM_ALL_CNT_TR_IN1,
        CY_TR_MUX_TR_INV_DISABLE,
        TRIGGER_TYPE_EDGE,
        0u);
}

/***********************************************
 * @brief : 初始化单相桥臂PWM引脚
 * @param : phase 单相桥臂硬件描述
 * @return: void
 * @date  : 2026-08-14
 * @author: L
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
 * @author: L
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
 * @author: L
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
    /* 公共tr_all_cnt_in[1]对应TCPWM的TRIG4输入，用于三相同步交换CC0。 */
    TCPWM_config.switchInputMode    = CY_TCPWM_INPUT_RISING_EDGE;
    TCPWM_config.switchInput        = CY_TCPWM_INPUT_TRIG4;
    TCPWM_config.reloadInputMode    = CY_TCPWM_INPUT_LEVEL;
    TCPWM_config.reloadInput        = CY_TCPWM_INPUT0;
    TCPWM_config.countInputMode     = CY_TCPWM_INPUT_LEVEL;
    TCPWM_config.countInput         = 1uL;
    /* Group0三相计数器使用公共触发线对应的TRIG3输入。 */
    TCPWM_config.startInputMode     = CY_TCPWM_INPUT_RISING_EDGE;
    TCPWM_config.startInput         = CY_TCPWM_INPUT_TRIG3;
    TCPWM_config.pwmOnDisable       = CY_TCPWM_PWM_OUT_MODE_LOW;
    TCPWM_config.trigger0EventCfg   = CY_TCPWM_COUNTER_DISABLED;
    TCPWM_config.trigger1EventCfg   = CY_TCPWM_COUNTER_DISABLED;
    
    Cy_Tcpwm_Pwm_Init(phase->timer, &TCPWM_config);

    /* SDK初始化函数的计数初值判断有误，中心对齐模式明确从1开始计数 */
    phase->timer->unCOUNTER.u32Register = CY_TCPWM_CNT_UP_DOWN_INIT_VAL;

    /* 当前SDK初始化函数未配置比较匹配方向，需要手动补齐 */
    phase->timer->unCTRL.stcField.u1CC0_MATCH_UP_EN   = 1u;
    phase->timer->unCTRL.stcField.u1CC0_MATCH_DOWN_EN = 1u;
    phase->timer->unCTRL.stcField.u1CC1_MATCH_UP_EN   = 0u;
    phase->timer->unCTRL.stcField.u1CC1_MATCH_DOWN_EN = 0u;

    /* 等待公共硬件触发沿后再开始计数。 */
    Cy_Tcpwm_Pwm_Enable(phase->timer);

}

/***********************************************
 * @brief : 初始化单相桥臂全部TCPWM资源
 * @param : phase 单相桥臂硬件描述
 * @return: void
 * @date  : 2026-08-14
 * @author: L
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
 * @author: L
 ************************************************/
static uint32 TCPWM_DutyToCompare(uint16 Duty)
{
    Duty = (uint16)Int_Limit(
        (int32)Duty,
        0,
        (int32)TCPWM_DUTY_OUTPUT_LIMIT);

    return TCPWM_PERIOD -
           ((uint32)TCPWM_PERIOD * Duty / TCPWM_DUTY_MAX);
}

void My_TCPWM_Init(void)
{
    TCPWM_SinglePhase_Init(&TCPWM_3PHASE.a);
    TCPWM_SinglePhase_Init(&TCPWM_3PHASE.b);
    TCPWM_SinglePhase_Init(&TCPWM_3PHASE.c);
    TCPWM_Master_Trigger_Init();
    TCPWM_Duty_Sync_Init();
}

void My_TCPWM_Start(void)
{
    /* 公共触发线在芯片内部扇出到各计数器的TRIG3。 */
    /* 只发送一次启动沿，保持三相PWM和ADC的同步相位。 */
    (void)Cy_TrigMux_SwTrigger(
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
}
