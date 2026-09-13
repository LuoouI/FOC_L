#ifndef SVPWM_H
#define SVPWM_H

#include "zf_common_headfile.h"

#define SVPWM_DUTY_MAX    (10000u) /* 占空比输出范围，10000对应100% */

/*===========================================================================*/
/*  SVPWM运行参数                                                            */
/*===========================================================================*/
typedef struct
{
    float VBUS;             /* 当前母线电压，单位为V */
    float V_Margin;         /* 线性调制区电压裕量，范围0~1 */
    float DQ_Limit;         /* 按实际占空比范围计算的d/q电压矢量上限，单位为V */
    uint16 DutyA;           /* 最近一次A相PWM占空比，范围0~9000 */
    uint16 DutyB;           /* 最近一次B相PWM占空比，范围0~9000 */
    uint16 DutyC;           /* 最近一次C相PWM占空比，范围0~9000 */
} SVPWM_t;

extern SVPWM_t SVPWM;

/*==================================================== 基础函数 ====================================================*/
void    VBUS_Get                    (void);
void    SVPWM_DQ_Limit_Update       (void);
float   foc_voltage_calc_duty       (float Ud,
                                     float Uq,
                                     uint16 ElectricalAngle,
                                     uint16 *DutyA,
                                     uint16 *DutyB,
                                     uint16 *DutyC);
void    SVPWM_DutyCache_Update      (uint16 DutyA, uint16 DutyB, uint16 DutyC);
/*==================================================== 基础函数 ====================================================*/

#endif
