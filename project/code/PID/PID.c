#include "PID.h"

void PID_Init(PID_t *pid, float kp, float ki, float kd)
{
    pid->Kp      = kp;
    pid->Ki      = ki;
    pid->Kd      = kd;
    pid->Ek      = 0.0f;
    pid->last_Ek = 0.0f;
    pid->Ek_sum  = 0.0f;
    pid->P_Out   = 0.0f;
    pid->I_Out   = 0.0f;
    pid->D_Out   = 0.0f;
    pid->OUT     = 0.0f;
}

void PID_Clear(PID_t *pid)
{
    pid->Ek      = 0.0f;
    pid->last_Ek = 0.0f;
    pid->Ek_sum  = 0.0f;
    pid->P_Out   = 0.0f;
    pid->I_Out   = 0.0f;
    pid->D_Out   = 0.0f;
    pid->OUT     = 0.0f;
}

float PID_Calc(PID_t *pid, float ref, float fbk, float integral_limit)
{
    pid->Ek      = ref - fbk;
    pid->Ek_sum += pid->Ek;
    pid->Ek_sum  = Float_Limit(pid->Ek_sum, -integral_limit, integral_limit);

    pid->P_Out = pid->Kp * pid->Ek;
    pid->I_Out = pid->Ki * pid->Ek_sum;
    pid->D_Out = pid->Kd * (pid->Ek - pid->last_Ek);
    pid->OUT   = pid->P_Out + pid->I_Out + pid->D_Out;

    pid->last_Ek = pid->Ek;
    return pid->OUT;
}
