#ifndef AB_FILTER_H
#define AB_FILTER_H

#include "zf_common_headfile.h"

/*===========================================================================*/
/*  AB滤波器数据描述                                                         */
/*===========================================================================*/
typedef struct
{
    float Ts;                 // 采样周期，单位为秒
    float A;                  // 位置修正系数
    float B;                  // 速度修正系数
    float ThetaFilter;        // 滤波后的角度，单位为弧度
    float OmegaFilter;        // 滤波后的角速度，单位为弧度每秒
    float PreviousAngle;      // 上一次测量角度，单位为弧度
    uint8 FirstFlag;          // 首次更新标志
} ABFilter_t;

/***********************************************
 * @brief : 初始化AB滤波器参数
 * @param : Filter 滤波器结构体指针
 * @param : SampleTime 采样周期，单位为秒
 * @param : ResponseRate 响应速度
 * @return: 无
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
void ABFilter_Init(ABFilter_t *Filter, float SampleTime, float ResponseRate);

/***********************************************
 * @brief : 更新AB滤波器并输出角速度
 * @param : Filter 滤波器结构体指针
 * @param : MeasuredAngle 编码器测量角度，单位为弧度
 * @return: 滤波后的角速度，单位为弧度每秒
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
float ABFilter_Update(ABFilter_t *Filter, float MeasuredAngle);

extern ABFilter_t Angle;     // 角度滤波器

#endif
