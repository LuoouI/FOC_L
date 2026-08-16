#ifndef MY_TCPWM_H
#define MY_TCPWM_H

#include "zf_common_headfile.h"

#define TCPWM_PERIOD             (2000u)      // 20kHz中心对齐PWM周期计数值
#define TCPWM_DUTY_MAX           (10000u)     // PWM占空比最大值，对应100%
#define TCPWM_ADC_SAMPLE_COUNT   (200u)       // ADC采样事件比较值，需要按硬件建立时间调整
#define TCPWM_CENTER_DEBUG_PIN   (P23_3)      // 中心对齐周期基准调试引脚

/*===========================================================================*/
/*  单相桥臂硬件描述                                                          */
/*===========================================================================*/
typedef struct
{
    volatile stc_TCPWM_GRP_CNT_t *timer;         // TCPWM 通道
    en_clk_dst_t                  clock_dst;     // 外设时钟

    volatile stc_GPIO_PRT_t      *port_h;        // 高桥端口
    uint32                        pin_h;         // 高桥引脚
    en_hsiom_sel_t                hsiom_h;       // 高桥复用

    volatile stc_GPIO_PRT_t      *port_l;        // 低桥端口
    uint32                        pin_l;         // 低桥引脚
    en_hsiom_sel_t                hsiom_l;       // 低桥复用

} TCPWM_PHASE_t;

/*===========================================================================*/
/*  三相桥硬件描述                                                            */
/*===========================================================================*/
typedef struct
{
    TCPWM_PHASE_t a;
    TCPWM_PHASE_t b;
    TCPWM_PHASE_t c;
} TCPWM_3PHASE_T;

/***********************************************
 * @brief : 初始化三相中心对齐互补PWM
 * @param : /
 * @return: void
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
void My_TCPWM_Init(void);

/***********************************************
 * @brief : 启动三相PWM
 * @param : /
 * @return: void
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
void My_TCPWM_Start(void);

/***********************************************
 * @brief : 设置三相PWM占空比
 * @param : DutyA A相占空比，范围0~10000
 * @param : DutyB B相占空比，范围0~10000
 * @param : DutyC C相占空比，范围0~10000
 * @return: void
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
void My_TCPWM_SetDuty(uint16 DutyA, uint16 DutyB, uint16 DutyC);

#endif // MY_TCPWM_H
