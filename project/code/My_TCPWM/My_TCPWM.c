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
    TCPWM_config.pwmOnDisable       = CY_TCPWM_PWM_OUT_MODE_LOW;
    TCPWM_config.trigger0EventCfg   = CY_TCPWM_COUNTER_DISABLED;
    TCPWM_config.trigger1EventCfg   = CY_TCPWM_COUNTER_DISABLED;
    
    Cy_Tcpwm_Pwm_Init(phase->timer, &TCPWM_config);
    Cy_Tcpwm_Pwm_Enable(phase->timer);

}