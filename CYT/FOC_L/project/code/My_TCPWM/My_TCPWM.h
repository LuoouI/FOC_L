#ifndef MY_TCPWM_H
#define MY_TCPWM_H

#include "zf_common_headfile.h"

#define TCPWM_PERIOD             (2000u)            /* 20kHz中心对齐PWM周期计数值 */
#define TCPWM_DUTY_MAX           (10000u)           /* PWM占空比最大值，对应100% */
#define TCPWM_DUTY_OUTPUT_LIMIT  (9000u)            /* 三相PWM实际输出上限，对应90% */
#define TCPWM_ADC_SAMPLE_COUNT   (100u)             /* ADC采样事件比较值，避开PWM谷底开关尖峰 */

/*===========================================================================*/
/*  单相桥臂硬件描述                                                          */
/*===========================================================================*/
typedef struct
{
    volatile stc_TCPWM_GRP_CNT_t *timer;        /* TCPWM 通道 */
    en_clk_dst_t                  clock_dst;    /* 外设时钟 */

    volatile stc_GPIO_PRT_t      *port_h;       /* 高桥端口 */
    uint32                        pin_h;        /* 高桥引脚 */
    en_hsiom_sel_t                hsiom_h;      /* 高桥复用 */

    volatile stc_GPIO_PRT_t      *port_l;       /* 低桥端口 */
    uint32                        pin_l;        /* 低桥引脚 */
    en_hsiom_sel_t                hsiom_l;      /* 低桥复用 */

} TCPWM_PHASE_t;

/*===========================================================================*/
/*  三相桥硬件描述                                                            */
/*===========================================================================*/
typedef struct
{
    TCPWM_PHASE_t a;    /* A相桥臂 */
    TCPWM_PHASE_t b;    /* B相桥臂 */
    TCPWM_PHASE_t c;    /* C相桥臂 */
} TCPWM_3PHASE_t;

/*==================================================== 基础函数 ====================================================*/
void    My_TCPWM_Init           (void);
void    My_TCPWM_Start          (void);
void    My_TCPWM_SetDuty        (uint16 DutyA, uint16 DutyB, uint16 DutyC);
/*==================================================== 基础函数 ====================================================*/

#endif /* MY_TCPWM_H */
