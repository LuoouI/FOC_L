#include "SVPWM.h"
#include "My_ADC/My_ADC.h"
#include "Foc_transform/Foc_transform.h"
#include "Function/Function.h"
#include <math.h>

SVPWM_t SVPWM =
{
    .VBUS = 24.0f,
    .V_Margin = 0.95f,
    .DQ_Limit = 24.0f * 0.95f / SQRT3
};

/***********************************************
 * @brief : 限制d/q电压矢量幅值，避免进入过调制区
 * @param : Ud d轴电压指针
 * @param : Uq q轴电压指针
 * @return: void
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
static void SVPWM_DQ_LimitVoltage(float *Ud, float *Uq)
{
    float Limit;
    float VoltageSquare;
    float LimitSquare;
    float Scale;

    Limit = SVPWM.DQ_Limit;
    if (Limit <= 0.0f)
    {
        *Ud = 0.0f;
        *Uq = 0.0f;
        return;
    }

    VoltageSquare = (*Ud * *Ud) + (*Uq * *Uq);
    LimitSquare = Limit * Limit;
    if (VoltageSquare > LimitSquare)
    {
        Scale = Limit / sqrtf(VoltageSquare);

        *Ud *= Scale;
        *Uq *= Scale;
    }
}

/***********************************************
 * @brief : 将相电压换算为中心对齐PWM占空比
 * @param : PhaseVoltage 相电压，单位为V
 * @return: 万分比占空比
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
static uint16 SVPWM_VoltageToDuty(float PhaseVoltage)
{
    float Duty;

    if (SVPWM.VBUS <= 0.0f)
    {
        return (uint16)(SVPWM_DUTY_MAX / 2u);
    }

    /* 半桥平均输出电压为(VBUS * Duty - VBUS / 2)。 */
    Duty = 0.5f + PhaseVoltage / SVPWM.VBUS;
    Duty = Float_Limit(Duty, 0.0f, 1.0f);

    return (uint16)(Duty * (float)SVPWM_DUTY_MAX + 0.5f);
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

void foc_voltage_calc_duty(float Ud,
                           float Uq,
                           uint16 ElectricalAngle,
                           uint16 *DutyA,
                           uint16 *DutyB,
                           uint16 *DutyC)
{
    InversePark_t InversePark;
    AlphaBeta_t AlphaBeta;

    InversePark.Ud = Ud;
    InversePark.Uq = Uq;
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
}
