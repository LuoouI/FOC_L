#ifndef SVPWM_H
#define SVPWM_H

#include "zf_common_headfile.h"

#define SVPWM_DUTY_MAX    (10000u)    // 占空比输出范围，10000对应100%

/*===========================================================================*/
/*  SVPWM运行参数                                                            */
/*===========================================================================*/
typedef struct
{
    float VBUS;                         // 当前母线电压，单位为V
    float V_Margin;                     // 线性调制区电压裕量，范围0~1
    float DQ_Limit;                     // d/q电压矢量幅值上限，单位为V
} SVPWM_t;

extern SVPWM_t SVPWM;

/***********************************************
 * @brief : 采样并更新母线电压，同时刷新d/q电压限幅
 * @param : /
 * @return: void
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
void VBUS_Get(void);

/***********************************************
 * @brief : 根据母线电压和调制裕量更新d/q电压限幅
 * @param : /
 * @return: void
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
void SVPWM_DQ_Limit_Update(void);

/***********************************************
 * @brief : 将d/q轴电压指令转换为三相PWM占空比
 * @param : Ud d轴电压，单位为V
 * @param : Uq q轴电压，单位为V
 * @param : ElectricalAngle 电角度，0~32767对应0~2PI
 * @param : DutyA A相占空比，范围0~10000，可为空
 * @param : DutyB B相占空比，范围0~10000，可为空
 * @param : DutyC C相占空比，范围0~10000，可为空
 * @return: void
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
void foc_voltage_calc_duty(float Ud,
                           float Uq,
                           uint16 ElectricalAngle,
                           uint16 *DutyA,
                           uint16 *DutyB,
                           uint16 *DutyC);

#endif
