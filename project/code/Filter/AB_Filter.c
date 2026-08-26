#include "AB_Filter.h"
#include "Function/Function.h"

ABFilter_t Angle = {0};     // 角度滤波器

/* 初始化AB滤波器 */
void ABFilter_Init(ABFilter_t *Flt, const ABFilterParam_t *Param)
{
    if ((Flt == NULL) ||
        (Param == NULL) ||
        (Param->Ts <= 0.0f) ||
        (Param->Bw <= 0.0f))
    {
        return;
    }

    Flt->Param = *Param;
    Flt->A = Param->Bw * Param->Ts;
    Flt->B = 0.5f * Flt->A * Flt->A;
    Flt->Theta = 0.0f;
    Flt->Omega = 0.0f;
    Flt->PrevAng = 0.0f;
    Flt->First = 0U;
}

/* 更新AB滤波器并输出角速度 */
float ABFilter_Update(ABFilter_t *Flt, float MeasAng)
{
    float AngErr;
    float ThetaPred;

    if ((Flt == NULL) || (Flt->Param.Ts <= 0.0f))
    {
        return 0.0f;
    }

    if (Flt->First == 0U)
    {
        Flt->Theta = MeasAng;
        Flt->Omega = 0.0f;
        Flt->PrevAng = MeasAng;
        Flt->First = 1U;
        return 0.0f;
    }

    AngErr = MeasAng - Flt->Theta;

    if (AngErr > PI)
    {
        AngErr -= TWO_PI;
    }
    else if (AngErr < -PI)
    {
        AngErr += TWO_PI;
    }

    ThetaPred = Flt->Theta +
                Flt->Omega * Flt->Param.Ts;

    Flt->Theta = ThetaPred + Flt->A * AngErr;
    Flt->Omega = Flt->Omega +
                 (Flt->B / Flt->Param.Ts) * AngErr;

    while (Flt->Theta >= TWO_PI)
    {
        Flt->Theta -= TWO_PI;
    }

    while (Flt->Theta < 0.0f)
    {
        Flt->Theta += TWO_PI;
    }

    Flt->PrevAng = MeasAng;

    return Flt->Omega;
}
