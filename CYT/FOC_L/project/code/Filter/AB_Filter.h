#ifndef AB_FILTER_H
#define AB_FILTER_H

#include "zf_common_headfile.h"

/*===========================================================================*/
/*  AB滤波器参数                                                              */
/*===========================================================================*/
typedef struct
{
    float Ts;                               /* 采样周期，单位为秒 */
    float Bw_hz;                            /* 滤波器等效带宽，单位为Hz */
} ABFilterParam_t;

/*===========================================================================*/
/*  AB滤波器数据描述                                                         */
/*===========================================================================*/
typedef struct
{
    float A;                                /* 位置修正系数 */
    float B;                                /* 速度修正系数 */
    float Theta;                            /* 滤波后的角度，单位为弧度 */
    float Omega;                            /* 滤波后的角速度，单位为弧度每秒 */
    float PrevAng;                          /* 上一次测量角度，单位为弧度 */
    ABFilterParam_t Param;                  /* AB滤波器参数 */
    uint8 First;                            /* 首次更新标志 */
} ABFilter_t;

extern ABFilter_t Angle;                    /* 角度滤波器 */

/*==================================================== 基础函数 ====================================================*/
void    ABFilter_Init           (ABFilter_t *Flt, const ABFilterParam_t *Param);
float   ABFilter_Update         (ABFilter_t *Flt, float MeasAng);
/*==================================================== 基础函数 ====================================================*/

#endif
