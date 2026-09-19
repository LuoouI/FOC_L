#ifndef MOTOR_CONTROL_PLL_H
#define MOTOR_CONTROL_PLL_H

#include "zf_common_headfile.h"

/*===========================================================================*/
/*  锁相环角度跟踪对象                                                        */
/*===========================================================================*/
typedef struct
{
    float Bandwidth;                        /* PLL自然频率带宽，单位为Hz */
    float Kp;                               /* 由带宽计算的PLL比例增益，单位为rad/s */
    float Ki;                               /* 由带宽计算的PLL积分增益，单位为rad/s^2 */
    float Integral_sum;                     /* PLL积分项，单位为rad/s */
    float Phase_error;                      /* PLL鉴相误差，单位为rad */

    uint16 Mechanical_angle_est;            /* 估算机械角度，范围0~32767 */
    uint16 Electrical_angle_est;            /* 估算电角度，范围0~32767 */

    float Omega_est;                        /* 估算电角速度，单位为rad/s */
    float Mechanical_angle_rad;             /* PLL内部机械角度，单位为rad */
    float Electrical_angle_rad;             /* PLL内部未补偿电角度，单位为rad */
    int8 Direction;                         /* 当前观测方向，取值为+1或-1 */
} PLL_t;

/*==================================================== 基础函数 ====================================================*/
void    PLL_SetBandwidth (PLL_t *Pll,
                          float Bandwidth_hz);
void    PLL_Reset        (PLL_t *Pll,
                          int8 Direction);
void    PLL_Update       (PLL_t *Pll,
                          float Phase_error,
                          float Sample_time,
                          uint8 Pole_pairs);
/*==================================================== 基础函数 ====================================================*/

#endif
