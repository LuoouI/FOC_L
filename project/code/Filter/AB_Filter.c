#include "AB_Filter.h"
#include "Function/Function.h"
#include <math.h>

ABFilter_t Angle = {0};     // 角度滤波器

void ABFilter_Init(ABFilter_t *Flt, const ABFilterParam_t *Param)
{
    if ((Flt == NULL) ||
        (Param == NULL) ||
        (Param->Ts <= 0.0f) ||
        (Param->Bw_hz <= 0.0f))
    {
        return;
    }

    Flt->Param = *Param;
    /* 使用精确离散映射，保证高带宽下的位置修正系数小于1。 */
    Flt->A = 1.0f - expf(-TWO_PI * Param->Bw_hz * Param->Ts);
    Flt->B = 0.5f * Flt->A * Flt->A;
    Flt->Theta = 0.0f;
    Flt->Omega = 0.0f;
    Flt->PrevAng = 0.0f;
    Flt->First = 0U;
}

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

    ThetaPred = Flt->Theta +
                Flt->Omega * Flt->Param.Ts;

    while (ThetaPred >= TWO_PI)
    {
        ThetaPred -= TWO_PI;
    }

    while (ThetaPred < 0.0f)
    {
        ThetaPred += TWO_PI;
    }

    AngErr = MeasAng - ThetaPred;

    if (AngErr > PI)
    {
        AngErr -= TWO_PI;
    }
    else if (AngErr < -PI)
    {
        AngErr += TWO_PI;
    }

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
