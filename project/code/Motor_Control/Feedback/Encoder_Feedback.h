#ifndef MOTOR_CONTROL_ENCODER_FEEDBACK_H
#define MOTOR_CONTROL_ENCODER_FEEDBACK_H

#include "zf_common_headfile.h"

/*===========================================================================*/
/*  编码器配置及角度反馈                                                      */
/*===========================================================================*/
typedef struct
{
    menc15a_module_enum Sensor_id;      /* 编码器模块编号 */
    int8 Direction;                     /* 编码器方向，取值为+1或-1 */
    uint16 Zero_offset;                 /* 机械角零偏 */
    uint16 Mechanical_angle;            /* 机械角，范围0~32767 */
    uint16 Electrical_angle;            /* 电角度，范围0~32767 */
    float Spd_rpm;                      /* 滤波后的机械转速，单位为转/分钟 */
} Motor_Encoder_t;

/*==================================================== 基础函数 ====================================================*/
void    Angle_Update                      (void);
float   Motor_Control_GetMechanicalDegree (void);
void    RPM_Cal                           (void);
/*==================================================== 基础函数 ====================================================*/

#endif
