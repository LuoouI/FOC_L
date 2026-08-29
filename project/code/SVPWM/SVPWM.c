#include "SVPWM.h"
#include "My_ADC/My_ADC.h"
#include "My_TCPWM/My_TCPWM.h"
#include "Foc_transform/Foc_transform.h"
#include "Function/Function.h"
#include <float.h>
#include <math.h>

SVPWM_t SVPWM =
{
    .VBUS = 24.0f,
    .V_Margin = 0.95f,
    .DQ_Limit = 24.0f * 0.95f / SQRT3,
    .DutyA = SVPWM_DUTY_MAX / 2u,
    .DutyB = SVPWM_DUTY_MAX / 2u,
    .DutyC = SVPWM_DUTY_MAX / 2u
};

/***********************************************
 * @brief : 限制d/q电压矢量幅值，避免进入过调制区
 * @param : Ud d轴电压指针
 * @param : Uq q轴电压指针
 * @return: 实际电压矢量与请求电压矢量的比例，范围0~1
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
static float SVPWM_DQ_LimitVoltage(float *Ud, float *Uq)
{
    float Limit;
    float VoltageSquare;
    float LimitSquare;
    float Scale;

    Limit = SVPWM.DQ_Limit;
    if ((Limit <= 0.0f) ||
        (*Ud != *Ud) ||
        (*Uq != *Uq) ||
        (*Ud > FLT_MAX) ||
        (*Ud < -FLT_MAX) ||
        (*Uq > FLT_MAX) ||
        (*Uq < -FLT_MAX))
    {
        *Ud = 0.0f;
        *Uq = 0.0f;
        return 0.0f;
    }

    VoltageSquare = (*Ud * *Ud) + (*Uq * *Uq);
    LimitSquare = Limit * Limit;
    if (VoltageSquare > LimitSquare)
    {
        Scale = Limit / sqrtf(VoltageSquare);

        *Ud *= Scale;
        *Uq *= Scale;

        return Scale;
    }

    return 1.0f;
}

/***********************************************
 * @brief : 将相电压换算为中心对齐PWM占空比
 * @param : PhaseVoltage 相电压，单位为V
 * @return: 万分比占空比
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
static uint16 SVPWM_VoltageToDuty(float PhaseVoltage)
{
    float Duty;
    int32 DutyValue;

    if (SVPWM.VBUS <= 0.0f)
    {
        return (uint16)(SVPWM_DUTY_MAX / 2u);
    }

    /* 半桥平均输出电压为(VBUS * Duty - VBUS / 2)。 */
    Duty = 0.5f + PhaseVoltage / SVPWM.VBUS;
    Duty = Float_Limit(Duty, 0.0f, 1.0f);
    DutyValue = (int32)(Duty * (float)SVPWM_DUTY_MAX + 0.5f);

    return (uint16)Int_Limit(
        DutyValue,
        0,
        (int32)TCPWM_DUTY_OUTPUT_LIMIT);
}

void VBUS_Get(void)
{
    SVPWM.VBUS = My_ADC_GetBatteryVoltage();
    if (SVPWM.VBUS < 0.0f)
    {
        SVPWM.VBUS = 0.0f;
    }

    SVPWM_DQ_Limit_Update();
}

void SVPWM_DQ_Limit_Update(void)
{
    SVPWM.V_Margin = Float_Limit(SVPWM.V_Margin, 0.0f, 1.0f);
    if (SVPWM.VBUS <= 0.0f)
    {
        SVPWM.DQ_Limit = 0.0f;
        return;
    }

    /* 线性区最大电压矢量为VBUS/sqrt(3)，再乘以裕量。 */
    SVPWM.DQ_Limit = SVPWM.VBUS * SVPWM.V_Margin / SQRT3;
}

float foc_voltage_calc_duty(float Ud,
                            float Uq,
                            uint16 ElectricalAngle,
                            uint16 *DutyA,
                            uint16 *DutyB,
                            uint16 *DutyC)
{
    InversePark_t InversePark;
    AlphaBeta_t AlphaBeta;
    float VoltageScale;

    InversePark.Ud = Ud;
    InversePark.Uq = Uq;
    VoltageScale =
        SVPWM_DQ_LimitVoltage(&InversePark.Ud, &InversePark.Uq);

    /* d/q电压经过逆Park变换得到静止坐标系电压。 */
    AlphaBeta = foc_ipark_calc(InversePark, ElectricalAngle);
    float Ualpha = AlphaBeta.Ualpha;
    float Ubeta = AlphaBeta.Ubeta;

    /* 逆Clarke变换得到三相相电压。 */
    float Ua = Ualpha;
    float Ub = -0.5f * Ualpha + 0.5f * SQRT3 * Ubeta;
    float Uc = -0.5f * Ualpha - 0.5f * SQRT3 * Ubeta;

    /* 注入-(最大值+最小值)/2，使三相占空比保持在母线范围内。 */
    float Umax = Ua;
    float Umin = Ua;
    if (Ub > Umax) Umax = Ub;
    if (Uc > Umax) Umax = Uc;
    if (Ub < Umin) Umin = Ub;
    if (Uc < Umin) Umin = Uc;

    float Uzero = -0.5f * (Umax + Umin);
    Ua += Uzero;
    Ub += Uzero;
    Uc += Uzero;

    if (DutyA != NULL)
    {
        *DutyA = SVPWM_VoltageToDuty(Ua);
    }
    if (DutyB != NULL)
    {
        *DutyB = SVPWM_VoltageToDuty(Ub);
    }
    if (DutyC != NULL)
    {
        *DutyC = SVPWM_VoltageToDuty(Uc);
    }

    if ((DutyA != NULL) && (DutyB != NULL) && (DutyC != NULL))
    {
        SVPWM_DutyCache_Update(*DutyA, *DutyB, *DutyC);
    }

    return VoltageScale;
}

void SVPWM_DutyCache_Update(uint16 DutyA, uint16 DutyB, uint16 DutyC)
{
    SVPWM.DutyA = (uint16)Int_Limit(
        (int32)DutyA,
        0,
        (int32)TCPWM_DUTY_OUTPUT_LIMIT);
    SVPWM.DutyB = (uint16)Int_Limit(
        (int32)DutyB,
        0,
        (int32)TCPWM_DUTY_OUTPUT_LIMIT);
    SVPWM.DutyC = (uint16)Int_Limit(
        (int32)DutyC,
        0,
        (int32)TCPWM_DUTY_OUTPUT_LIMIT);
}
