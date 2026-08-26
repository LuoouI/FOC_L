#ifndef FUNCTION_H
#define FUNCTION_H

#include "zf_common_headfile.h"

#define PI                    (3.14159265358979323846f)    // 圆周率
#define TWO_PI                (6.28318530717958647692f)    // 2倍圆周率
#define HALF_PI               (1.57079632679489661923f)    // 1/2圆周率
#define SQRT2                 (1.41421356237309504880f)    // 2的平方根
#define SQRT3                 (1.73205080756887729353f)    // 3的平方根

#define ANGLE_PERIOD          (32768u)                     // 单圈角度周期
#define ANGLE_HALF_PERIOD     (16384)                      // 半圈角度周期
#define ANGLE_QUARTER_PERIOD  (8192u)                      // 四分之一圈角度周期
#define ANGLE_MAX             (32767u)                     // 单圈角度最大值

/*===========================================================================*/
/*  角度解缠状态                                                              */
/*===========================================================================*/
typedef struct
{
    uint16 Last;        // 上一次单圈角度
    int32 Value;        // 当前连续角度
    uint8 Ready;        // 首次采样完成标志
} AngleUnwrap_t;

/***********************************************
 * @brief : 对浮点数进行双边限幅
 * @param : Value 待限幅数值
 * @param : Min 下限
 * @param : Max 上限
 * @return: 限幅后的数值
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
float Float_Limit(float Value, float Min, float Max);

/***********************************************
 * @brief : 对32位有符号整数进行双边限幅
 * @param : Value 待限幅数值
 * @param : Min 下限
 * @param : Max 上限
 * @return: 限幅后的数值
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
int32 Int_Limit(int32 Value, int32 Min, int32 Max);

/***********************************************
 * @brief : 将角度归一化到一个周期内
 * @param : Angle 待归一化角度
 * @return: 0~32767范围内的单圈角度
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
uint16 Angle_Wrap(int32 Angle);

/***********************************************
 * @brief : 清除角度解缠状态
 * @param : Unwrap 角度解缠状态
 * @return: void
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
void Angle_Unwrap_Clear(AngleUnwrap_t *Unwrap);

/***********************************************
 * @brief : 将单圈角度转换为连续多圈角度，相邻采样变化需小于半圈
 * @param : Unwrap 角度解缠状态，首次使用前需清除
 * @param : Angle 当前单圈角度，范围0~32767
 * @return: 连续多圈角度
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
int32 Angle_Unwrap(AngleUnwrap_t *Unwrap, uint16 Angle);

#endif
