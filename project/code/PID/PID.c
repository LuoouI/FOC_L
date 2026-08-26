#include "PID.h"
#include "float.h"

/***********************************************
 * @brief : 将上下限整理为从小到大的有效范围
 * @param : MinValue 原始下限
 * @param : MaxValue 原始上限
 * @return: void
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
static void PID_SortLimit(float *MinValue, float *MaxValue)
{
    float Temp;

    if (*MinValue > *MaxValue)
    {
        Temp = *MinValue;
        *MinValue = *MaxValue;
        *MaxValue = Temp;
    }
}

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

void PID_Init(PID_t *Pid, float Kp, float Ki, float Kd)
{
    if (Pid == NULL)
    {
        return;
    }

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

void PID_Config(PID_t *Pid,
                float SampleTime,
                float Tau,
                float OutputMin,
                float OutputMax,
                float IntegralMin,
                float IntegralMax)
{
    if (Pid == NULL)
    {
        return;
    }

    if (SampleTime > 0.0f)
    {
        Pid->T = SampleTime;
    }
    else
    {
        Pid->T = FOC_TS;
    }

    Pid->Tau = Tau;
    if (Pid->Tau < 0.0f)
    {
        Pid->Tau = 0.0f;
    }
    Pid->LimMin = OutputMin;
    Pid->LimMax = OutputMax;
    Pid->LimMinInt = IntegralMin;
    Pid->LimMaxInt = IntegralMax;

    PID_SortLimit(&Pid->LimMin, &Pid->LimMax);
    PID_SortLimit(&Pid->LimMinInt, &Pid->LimMaxInt);
    PID_LimitIntegrator(Pid);
    Pid->OUT = Float_Limit(Pid->OUT, Pid->LimMin, Pid->LimMax);
}

void PID_Clear(PID_t *Pid)
{
    if (Pid == NULL)
    {
        return;
    }

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

float PID_Update(PID_t *Pid, float Setpoint, float Measurement)
{
    float Error;
    float SampleTime;
    float FilterDenominator;

    if (Pid == NULL)
    {
        return 0.0f;
    }

    SampleTime = Pid->T;
    if (SampleTime <= 0.0f)
    {
        SampleTime = FOC_TS;
        Pid->T = SampleTime;
    }

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

float PID_Calc(PID_t *Pid, float Ref, float Fbk, float IntegralLimit)
{
    if (Pid == NULL)
    {
        return 0.0f;
    }

    if (IntegralLimit < 0.0f)
    {
        IntegralLimit = -IntegralLimit;
    }

    Pid->LimMinInt = -IntegralLimit;
    Pid->LimMaxInt = IntegralLimit;

    return PID_Update(Pid, Ref, Fbk);
}
