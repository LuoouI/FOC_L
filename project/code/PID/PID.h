#ifndef PID_H_
#define PID_H_

#include "zf_common_headfile.h"
#include "Function/Function.h"

#define LD   (0.031036f)            /* d轴电感，单位为mH */
#define LQ   (0.035016f)            /* q轴电感，单位为mH */
#define LS   (0.033026f)            /* 定子电感，单位为mH */
#define RS   (0.28659f)             /* 定子电阻，单位为欧姆 */
#define FOC_TS (1.0f / 20000.0f)    /* 电流环采样周期，单位为秒 */
#define DENOMINATOR     \
(2.0f * LS * 0.001f + RS * FOC_TS)  /* PI增益计算分母 */

#define PID_BANDWIDTH_MIN_HZ (1u)       /* 电流环带宽下限，单位为Hz */
#define PID_BANDWIDTH_MAX_HZ (5000u)    /* 电流环带宽上限，单位为Hz */

/*===========================================================================*/
/*  PID控制器                                                                */
/*===========================================================================*/
typedef struct
{
    float Kp;                   /* 比例增益 */
    float Ki;                   /* 连续时间积分增益 */
    float Kd;                   /* 微分增益 */
    float Tau;                  /* 微分低通滤波时间常数，单位为秒 */
    float T;                    /* 控制器采样周期，单位为秒 */

    float LimMin;               /* 控制器输出下限 */
    float LimMax;               /* 控制器输出上限 */
    float LimMinInt;            /* 积分项输出下限 */
    float LimMaxInt;            /* 积分项输出上限 */

    float Ek;                   /* 当前误差 */
    float last_Ek;              /* 上一次误差 */
    float Ek_sum;               /* 兼容旧接口的误差积分累加量 */
    float Integrator;           /* 积分项输出 */
    float PrevMeasurement;      /* 上一次测量值 */
    float Differentiator;       /* 经过低通滤波的微分项输出 */

    float P_Out;                /* 比例项输出 */
    float I_Out;                /* 积分项输出 */
    float D_Out;                /* 微分项输出 */
    float OUT;                  /* 控制器总输出 */
} PID_t;

/*==================================================== 基础函数 ====================================================*/
void        PID_Init                (PID_t *Pid, float Kp, float Ki, float Kd);

void        PID_Config              (PID_t *Pid,
                                     float SampleTime,
                                     float Tau,
                                     float OutputMin,
                                     float OutputMax,
                                     float IntegralMin,
                                     float IntegralMax);
void        PID_Clear               (PID_t *Pid);
float       PID_Update              (PID_t *Pid, float Setpoint, float Measurement);
void        PID_SetBandwidth        (PID_t *Pid,
                                     uint16 BandwidthHz,
                                     float InductanceMh,
                                     float ResistanceOhm);
void        PID_SetIntegralLimit    (PID_t *Pid, float IntegralLimit);
void        PID_BackCalculation     (PID_t *Pid, float ActualOutput);

float       PID_Calc                (PID_t *Pid, float Ref, float Fbk, float IntegralLimit);
/*==================================================== 基础函数 ====================================================*/

#endif
