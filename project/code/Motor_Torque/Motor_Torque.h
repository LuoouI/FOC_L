#ifndef MOTOR_TORQUE_H
#define MOTOR_TORQUE_H

#include "zf_common_headfile.h"

/*===========================================================================*/
/*  电机电磁转矩估算数据                                                      */
/*===========================================================================*/
typedef struct
{
    float Kv;                  // 电机速度常数，单位为rpm/V，当前电机为420rpm/V
    float Kt;                  // 由Kv理想换算的转矩常数，单位为N*m/A
    float Gear_ratio;          // 减速比，电机转速与输出转速之比，无减速器时为1
    float Motor_torque;        // 经减速比换算后的理想转矩估算值，单位为N*m
    uint8 Ready;               // 转矩参数有效标志
}Torque_t;

extern volatile Torque_t Torque; // 电机转矩估算对象

/***********************************************
 * @brief : 根据q轴电流估算电机理想输出转矩
 * @param : Iq q轴电流，单位为A
 * @return: /
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
void Motor_Torque_Estimate(float Iq);

#endif
