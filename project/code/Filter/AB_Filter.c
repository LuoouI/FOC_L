#include "AB_Filter.h"
#include "Function/Function.h"

ABFilter_t Angle = {0};     // 角度滤波器

/* 初始化AB滤波器 */
void ABFilter_Init(ABFilter_t *Filter, float SampleTime, float ResponseRate)
{
    if ((Filter == NULL) || (SampleTime <= 0.0f))
    {
        return;
    }

    Filter->Ts = SampleTime;
    Filter->A = ResponseRate * SampleTime;
    Filter->B = 0.5f * Filter->A * Filter->A;
    Filter->ThetaFilter = 0.0f;
    Filter->OmegaFilter = 0.0f;
    Filter->PreviousAngle = 0.0f;
    Filter->FirstFlag = 0U;
}

/* 更新AB滤波器并输出角速度 */
float ABFilter_Update(ABFilter_t *Filter, float MeasuredAngle)
{
    float AngleError;
    float ThetaPrediction;

    if ((Filter == NULL) || (Filter->Ts <= 0.0f))
    {
        return 0.0f;
    }

    if (Filter->FirstFlag == 0U)
    {
        Filter->ThetaFilter = MeasuredAngle;
        Filter->OmegaFilter = 0.0f;
        Filter->PreviousAngle = MeasuredAngle;
        Filter->FirstFlag = 1U;
        return 0.0f;
    }

    AngleError = MeasuredAngle - Filter->ThetaFilter;

    if (AngleError > PI)
    {
        AngleError -= TWO_PI;
    }
    else if (AngleError < -PI)
    {
        AngleError += TWO_PI;
    }

    ThetaPrediction = Filter->ThetaFilter +
                      Filter->OmegaFilter * Filter->Ts;

    Filter->ThetaFilter = ThetaPrediction + Filter->A * AngleError;
    Filter->OmegaFilter = Filter->OmegaFilter +
                          (Filter->B / Filter->Ts) * AngleError;

    while (Filter->ThetaFilter >= TWO_PI)
    {
        Filter->ThetaFilter -= TWO_PI;
    }

    while (Filter->ThetaFilter < 0.0f)
    {
        Filter->ThetaFilter += TWO_PI;
    }

    Filter->PreviousAngle = MeasuredAngle;

    return Filter->OmegaFilter;
}
