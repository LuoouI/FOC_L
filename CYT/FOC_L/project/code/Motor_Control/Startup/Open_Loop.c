#include "Motor_Control/Startup/Open_Loop.h"
#include "Motor_Control/Motor_Control.h"
#include "Foc_transform/Foc_transform.h"
#include "Function/Function.h"
#include "My_TCPWM/My_TCPWM.h"
#include "SVPWM/SVPWM.h"

/***********************************************
 * @brief : 按给定d/q轴电压和步长执行一次开环角度牵引
 * @param : Uq q轴电压，单位为V
 * @param : Ud d轴电压，单位为V
 * @param : Step 单控制周期电角度增量，负值表示反向
 * @return: 无
 * @date  : 2026-08-27
 * @author: L
 ************************************************/
void Open_Loop_Update(float Uq, float Ud, int16 Step)
{
    uint16 DutyA;
    uint16 DutyB;
    uint16 DutyC;

    if ((Uq == 0.0f) && (Ud == 0.0f))
    {
        Motor.Open_loop.Angle = 0u;
        Motor.Open_loop.Hold_count = 0u;
        Motor.Open_loop.Started = 0u;

        My_TCPWM_SetDuty(
            (uint16)(TCPWM_DUTY_MAX / 2u),
            (uint16)(TCPWM_DUTY_MAX / 2u),
            (uint16)(TCPWM_DUTY_MAX / 2u));
        SVPWM_DutyCache_Update(
            (uint16)(SVPWM_DUTY_MAX / 2u),
            (uint16)(SVPWM_DUTY_MAX / 2u),
            (uint16)(SVPWM_DUTY_MAX / 2u));
        return;
    }

    if (Motor.Open_loop.Started == 0u)
    {
        Motor.Open_loop.Angle = 0u;
        Motor.Open_loop.Hold_count = 0u;
        Motor.Open_loop.Started = 1u;
    }

    if (Motor.Open_loop.Hold_count < Motor.Open_loop.Align_count)
    {
        Motor.Open_loop.Hold_count++;
    }
    else
    {
        Motor.Open_loop.Angle = Angle_Wrap(
            (int32)Motor.Open_loop.Angle + (int32)Step);
    }

    foc_voltage_calc_duty(
        Ud,
        Uq,
        Motor.Open_loop.Angle,
        &DutyA,
        &DutyB,
        &DutyC);
    My_TCPWM_SetDuty(DutyA, DutyB, DutyC);
}
