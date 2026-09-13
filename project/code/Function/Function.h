#ifndef FUNCTION_H
#define FUNCTION_H

#include "zf_common_headfile.h"

#define PI                    (3.14159265358979323846f)         /* 圆周率 */
#define TWO_PI                (6.28318530717958647692f)         /* 2倍圆周率 */
#define HALF_PI               (1.57079632679489661923f)         /* 1/2圆周率 */
#define SQRT2                 (1.41421356237309504880f)         /* 2的平方根 */
#define SQRT3                 (1.73205080756887729353f)         /* 3的平方根 */

#define ANGLE_PERIOD          (32768u)               /* 单圈角度周期 */
#define ANGLE_HALF_PERIOD     (16384)                /* 半圈角度周期 */
#define ANGLE_QUARTER_PERIOD  (8192u)                /* 四分之一圈角度周期 */
#define ANGLE_MAX             (32767u)               /* 单圈角度最大值 */

/*===========================================================================*/
/*  角度解缠状态                                                              */
/*===========================================================================*/
typedef struct
{
    uint16 Last; /* 上一次单圈角度 */
    int32 Value; /* 当前连续角度 */
    uint8 Ready; /* 首次采样完成标志 */
} AngleUnwrap_t;

/*==================================================== 基础函数 ====================================================*/
float   Float_Limit             (float Value, float Min, float Max);
int32   Int_Limit               (int32 Value, int32 Min, int32 Max);
uint16  Angle_Wrap              (int32 Angle);
void    Angle_Unwrap_Clear      (AngleUnwrap_t *Unwrap);
int32   Angle_Unwrap            (AngleUnwrap_t *Unwrap, uint16 Angle);
/*==================================================== 基础函数 ====================================================*/

#endif
