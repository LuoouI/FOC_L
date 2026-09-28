#include "PID.h"
#include "float.h"

/***********************************************
 * @brief : 对PID积分项进行限幅
 * @param : Pid PID控制器对象
 * @return: void
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
static void PID_LimitIntegrator(PID_t *Pid)
{
    Pid->Integrator = Float_Limit(
        Pid->Integrator,
        Pid->LimMinInt,
        Pid->LimMaxInt);
}

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
void PID_Init(PID_t *Pid, float Kp, float Ki, float Kd)
{
    Pid->Kp = Kp;
    Pid->Ki = Ki;
    Pid->Kd = Kd;
    Pid->Tau = FOC_TS;
    Pid->T = FOC_TS;

    /* 默认不限制输出，实际控制环应通过PID_Config设置安全范围。 */
    Pid->LimMin = -FLT_MAX;
    Pid->LimMax = FLT_MAX;
    Pid->LimMinInt = -FLT_MAX;
    Pid->LimMaxInt = FLT_MAX;

    PID_Clear(Pid);
}

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
                float IntegralMax)
{
    Pid->T = SampleTime;
    Pid->Tau = Tau;
    Pid->LimMin = OutputMin;
    Pid->LimMax = OutputMax;
    Pid->LimMinInt = IntegralMin;
    Pid->LimMaxInt = IntegralMax;

    PID_LimitIntegrator(Pid);
    Pid->OUT = Float_Limit(Pid->OUT, Pid->LimMin, Pid->LimMax);
}

/***********************************************
 * @brief : PID状态清零
 * @param : Pid PID控制器对象
 * @return: void
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
void PID_Clear(PID_t *Pid)
{
    Pid->Ek = 0.0f;
    Pid->last_Ek = 0.0f;
    Pid->Ek_sum = 0.0f;
    Pid->Integrator = 0.0f;
    Pid->PrevMeasurement = 0.0f;
    Pid->Differentiator = 0.0f;
    Pid->P_Out = 0.0f;
    Pid->I_Out = 0.0f;
    Pid->D_Out = 0.0f;
    Pid->OUT = 0.0f;
}

/***********************************************
 * @brief : 使用设定值和测量值计算PID输出
 * @param : Pid PID控制器对象
 * @param : Setpoint 目标设定值
 * @param : Measurement 当前测量值
 * @return: 限幅后的PID输出
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
float PID_Update(PID_t *Pid, float Setpoint, float Measurement)
{
    float Error;
    float SampleTime;
    float FilterDenominator;

    SampleTime = Pid->T;
    Error = Setpoint - Measurement;
    Pid->Ek = Error;

    /* 梯形积分比单纯累加当前误差更适合固定周期数字控制器。 */
    Pid->Integrator +=
        0.5f * Pid->Ki * SampleTime * (Error + Pid->last_Ek);
    PID_LimitIntegrator(Pid);

    /* 对测量值微分，设定值阶跃不会直接形成微分冲击。 */
    if (Pid->Kd == 0.0f)
    {
        Pid->Differentiator = 0.0f;
    }
    else if (Pid->Tau > 0.0f)
    {
        FilterDenominator = 2.0f * Pid->Tau + SampleTime;
        Pid->Differentiator =
            -(2.0f * Pid->Kd * (Measurement - Pid->PrevMeasurement) +
              (2.0f * Pid->Tau - SampleTime) * Pid->Differentiator) /
            FilterDenominator;
    }
    else
    {
        Pid->Differentiator =
            -Pid->Kd * (Measurement - Pid->PrevMeasurement) / SampleTime;
    }

    Pid->P_Out = Pid->Kp * Error;
    Pid->I_Out = Pid->Integrator;
    Pid->D_Out = Pid->Differentiator;
    Pid->OUT = Pid->P_Out + Pid->I_Out + Pid->D_Out;
    Pid->OUT = Float_Limit(Pid->OUT, Pid->LimMin, Pid->LimMax);

    /* 保留旧字段的可观察状态，便于已有调试代码继续使用。 */
    if (Pid->Ki != 0.0f)
    {
        Pid->Ek_sum = Pid->Integrator / Pid->Ki;
    }
    else
    {
        Pid->Ek_sum = 0.0f;
    }
    Pid->last_Ek = Error;
    Pid->PrevMeasurement = Measurement;

    return Pid->OUT;
}

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
                      float ResistanceOhm)
{
    float Omega;

    Omega = TWO_PI * (float)BandwidthHz;
    Pid->Kp = Omega * InductanceMh / 1000.0f;
    Pid->Ki = Omega * ResistanceOhm;
}

/***********************************************
 * @brief : 设置PID积分项对称限幅并约束当前积分状态
 * @param : Pid PID控制器对象
 * @param : IntegralLimit 积分项输出绝对限幅
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void PID_SetIntegralLimit(PID_t *Pid, float IntegralLimit)
{
    Pid->LimMinInt = -IntegralLimit;
    Pid->LimMaxInt = IntegralLimit;
    PID_LimitIntegrator(Pid);
    Pid->I_Out = Pid->Integrator;
    if (Pid->Ki != 0.0f)
    {
        Pid->Ek_sum = Pid->Integrator / Pid->Ki;
    }
    else
    {
        Pid->Ek_sum = 0.0f;
    }
}

/***********************************************
 * @brief : 根据执行器实际输出对PID积分器进行反算抗饱和
 * @param : Pid PID控制器对象
 * @param : ActualOutput 执行器经过限幅后的实际输出
 * @return: 无，修正结果在下一控制周期生效
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void PID_BackCalculation(PID_t *Pid, float ActualOutput)
{
    float SampleTime;
    float TrackingGain;

    if ((Pid->Kp == 0.0f) || (Pid->Ki == 0.0f))
    {
        return;
    }

    TrackingGain = Pid->Ki / Pid->Kp;
    SampleTime = Pid->T;

    /* 以Kp/Ki作为跟踪时间常数，使积分器回跟执行器实际输出。 */
    Pid->Integrator +=
        TrackingGain * (ActualOutput - Pid->OUT) * SampleTime;
    PID_LimitIntegrator(Pid);

    Pid->I_Out = Pid->Integrator;
    Pid->Ek_sum = Pid->Integrator / Pid->Ki;
}

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
float PID_Calc(PID_t *Pid, float Ref, float Fbk, float IntegralLimit)
{
    Pid->LimMinInt = -IntegralLimit;
    Pid->LimMaxInt = IntegralLimit;

    return PID_Update(Pid, Ref, Fbk);
}
