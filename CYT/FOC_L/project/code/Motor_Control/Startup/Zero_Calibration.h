#ifndef MOTOR_CONTROL_ZERO_CALIBRATION_H
#define MOTOR_CONTROL_ZERO_CALIBRATION_H

#include "zf_common_headfile.h"

/*===========================================================================*/
/*  电机零点校准参数                                                          */
/*===========================================================================*/
typedef struct
{
    float Voltage;          /* 零点校准d轴电压 */
    uint16 Ramp_count;      /* 校准锁定电压渐升步数 */
    uint16 Ramp_ms;         /* 校准锁定电压渐升步间隔 */
    uint16 Hold_ms;         /* 校准起始定位保持时间 */
    uint16 Step_count;      /* 零点牵引步数 */
    uint16 Step_ms;         /* 零点牵引步间隔 */
    uint16 Sample_count;    /* 零点位置平均采样次数 */
    uint16 Sample_ms;       /* 零点位置采样间隔 */
    int32 Min_travel;       /* 判定编码器有效的最小累计行程 */
} Motor_ZeroCalib_t;

extern const Motor_ZeroCalib_t Motor_zeroCalib;    /* 电机零点校准参数 */

/*==================================================== 基础函数 ====================================================*/
void    Zero_Calibration (void);
/*==================================================== 基础函数 ====================================================*/

#endif
