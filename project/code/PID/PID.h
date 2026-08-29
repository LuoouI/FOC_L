#ifndef PID_H_
#define PID_H_

#include "zf_common_headfile.h"
#include "Function/Function.h"

#define LD   (0.031036f)               // d轴电感，单位为mH
#define LQ   (0.035016f)               // q轴电感，单位为mH
#define RS   (0.28659f)                // 定子电阻，单位为欧姆
#define FOC_TS (1.0f / 20000.0f)       // 电流环采样周期，单位为秒

#define PID_BANDWIDTH_MIN_HZ (1u)       // 电流环带宽下限，单位为Hz
#define PID_BANDWIDTH_MAX_HZ (5000u)    // 电流环带宽上限，单位为Hz

/*===========================================================================*/
/*  PID控制器                                                                */
/*===========================================================================*/
typedef struct
{
    float Kp;                       // 比例增益
    float Ki;                       // 连续时间积分增益
    float Kd;                       // 微分增益
    float Tau;                      // 微分低通滤波时间常数，单位为秒
    float T;                        // 控制器采样周期，单位为秒

    float LimMin;                   // 控制器输出下限
    float LimMax;                   // 控制器输出上限
    float LimMinInt;                // 积分项输出下限
    float LimMaxInt;                // 积分项输出上限

    float Ek;                       // 当前误差
    float last_Ek;                  // 上一次误差
    float Ek_sum;                   // 兼容旧接口的误差积分累加量
    float Integrator;               // 积分项输出
    float PrevMeasurement;          // 上一次测量值
    float Differentiator;           // 经过低通滤波的微分项输出

    float P_Out;                    // 比例项输出
    float I_Out;                    // 积分项输出
    float D_Out;                    // 微分项输出
    float OUT;                      // 控制器总输出
} PID_t;

/***********************************************
 * @brief : 初始化PID控制器并清除运行状态
 * @param : Pid PID控制器对象
 * @param : Kp 比例增益
 * @param : Ki 连续时间积分增益
 * @param : Kd 微分增益
 * @return: void
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
void PID_Init(PID_t *Pid, float Kp, float Ki, float Kd);

/***********************************************
 * @brief : 配置PID采样周期、微分滤波和输出限幅
 * @param : Pid PID控制器对象
 * @param : SampleTime 采样周期，单位为秒
 * @param : Tau 微分低通滤波时间常数，单位为秒，填0时使用未滤波微分
 * @param : OutputMin 控制器输出下限
 * @param : OutputMax 控制器输出上限
 * @param : IntegralMin 积分项输出下限
 * @param : IntegralMax 积分项输出上限
 * @return: void
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
void PID_Config(PID_t *Pid,
                float SampleTime,
                float Tau,
                float OutputMin,
                float OutputMax,
                float IntegralMin,
                float IntegralMax);

/***********************************************
 * @brief : PID状态清零
 * @param : Pid PID控制器对象
 * @return: void
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
void PID_Clear(PID_t *Pid);

/***********************************************
 * @brief : 使用设定值和测量值计算PID输出
 * @param : Pid PID控制器对象
 * @param : Setpoint 目标设定值
 * @param : Measurement 当前测量值
 * @return: 限幅后的PID输出
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
float PID_Update(PID_t *Pid, float Setpoint, float Measurement);

/***********************************************
 * @brief : 根据电流环带宽和电机参数计算PI增益
 * @param : Pid PID控制器对象
 * @param : BandwidthHz 目标带宽，单位为Hz
 * @param : InductanceMh 电感，单位为mH
 * @param : ResistanceOhm 定子电阻，单位为欧姆
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void PID_SetBandwidth(PID_t *Pid,
                      uint16 BandwidthHz,
                      float InductanceMh,
                      float ResistanceOhm);

/***********************************************
 * @brief : 设置PID积分项对称限幅并约束当前积分状态
 * @param : Pid PID控制器对象
 * @param : IntegralLimit 积分项输出绝对限幅
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void PID_SetIntegralLimit(PID_t *Pid, float IntegralLimit);

/***********************************************
 * @brief : 根据执行器实际输出对PID积分器进行反算抗饱和
 * @param : Pid PID控制器对象
 * @param : ActualOutput 执行器经过限幅后的实际输出
 * @return: 无，修正结果在下一控制周期生效
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void PID_BackCalculation(PID_t *Pid, float ActualOutput);

/***********************************************
 * @brief : 兼容旧接口的PID计算函数，积分项输出限幅为正负IntegralLimit
 * @param : Pid PID控制器对象
 * @param : Ref 目标设定值
 * @param : Fbk 当前反馈值
 * @param : IntegralLimit 积分项输出绝对限幅
 * @return: 限幅后的PID输出
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
float PID_Calc(PID_t *Pid, float Ref, float Fbk, float IntegralLimit);

#endif
