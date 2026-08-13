#ifndef MY_TCPWM_H
#define MY_TCPWM_H

#include "zf_common_headfile.h"

#define TCPWM_PERIOD     (2000u)      //20kHz中心对齐PWM周期计数值

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

#endif // MY_TCPWM_H