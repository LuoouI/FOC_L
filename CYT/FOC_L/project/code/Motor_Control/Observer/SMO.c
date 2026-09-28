#include "Motor_Control/Observer/SMO.h"
#include "Motor_Control/Motor_Control.h"
#include "Motor_Control/Observer/PLL.h"
#include "Current_sample/Current_sample.h"
#include "Fast_sin/Fast_sin.h"
#include "Function/Function.h"
#include "SVPWM/SVPWM.h"
#include <math.h>

/*===========================================================================*/
/*  PLL相位跟踪                                                               */
/*===========================================================================*/

/***********************************************
 * @brief : 根据当前控制状态确定PLL反电动势方向
 * @param : 无
 * @return: 观测方向，返回+1表示正转，返回-1表示反转
 * @date  : 2026-09-15
 * @author: L
 ************************************************/
static int8 SMO_PLL_GetDirection(void)
{
    if (Motor.Control_mode == MOTOR_CONTROL_ENCODER_FOC)
    {
        if (Motor.Encoder.Spd_rpm > 1.0f)
        {
            return 1;
        }
        if (Motor.Encoder.Spd_rpm < -1.0f)
        {
            return -1;
        }
    }

    if (Motor.Foc_mode == MOTOR_FOC_SPEED)
    {
        if (Motor.Speed_loop.Command_rpm > 0.0f)
        {
            return 1;
        }
        if (Motor.Speed_loop.Command_rpm < 0.0f)
        {
            return -1;
        }
    }
    else if (Motor.Foc_mode == MOTOR_FOC_CURRENT)
    {
        if (Motor.Current_loop.Iq_target > 0.0f)
        {
            return 1;
        }
        if (Motor.Current_loop.Iq_target < 0.0f)
        {
            return -1;
        }
    }

    return (Motor.PLL.Direction < 0) ? -1 : 1;
}

/***********************************************
 * @brief : 有感模式下使用编码器选择PLL机械角所属的磁极扇区
 * @param : Mechanical_output_rad PLL输出机械角，单位为rad
 * @return: 扇区对齐后的PLL机械角，单位为rad
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
static float SMO_PLL_AlignMechanicalSector(float Mechanical_output_rad)
{
    float Mechanical_actual_rad;
    float Sector_period_rad;
    float Sector_error_rad;
    int32 Sector_offset;

    if (Motor.Control_mode != MOTOR_CONTROL_ENCODER_FOC)
    {
        return Mechanical_output_rad;
    }

    Mechanical_actual_rad =
        Motor_Control_GetMechanicalDegree() * TWO_PI / 360.0f;
    Sector_period_rad = TWO_PI / (float)Motor.Pole_pairs;
    Sector_error_rad = Mechanical_actual_rad - Mechanical_output_rad;
    if (Sector_error_rad > PI)
    {
        Sector_error_rad -= TWO_PI;
    }
    else if (Sector_error_rad < -PI)
    {
        Sector_error_rad += TWO_PI;
    }

    /* 只补偿整磁极周期，保留PLL自身的连续相位误差用于观测。 */
    Sector_offset = (Sector_error_rad >= 0.0f) ?
                    (int32)(Sector_error_rad / Sector_period_rad + 0.5f) :
                    (int32)(Sector_error_rad / Sector_period_rad - 0.5f);

    return Mechanical_output_rad +
           (float)Sector_offset * Sector_period_rad;
}

/***********************************************
 * @brief : 使用滤波后的Alpha/Beta轴反电动势更新PLL角度和电角速度
 * @param : 无
 * @return: 无，估算结果保存到公共PLL对象
 * @date  : 2026-09-20
 * @author: L
 ************************************************/
static void SMO_PLL_Update(void)
{
    PLL_t *Pll = &Motor.PLL;
    float E_alpha = Motor.SMO.E_alpha_filter;
    float E_beta = Motor.SMO.E_beta_filter;
    float Emf_amplitude;
    float Sin_theta;
    float Cos_theta;
    float Phase_error;
    float Filter_omega;
    float Phase_compensation;
    float Electrical_output_rad;
    float Mechanical_output_rad;
    int32 Electrical_angle_count;
    int32 Mechanical_angle_count;
    uint16 Electrical_angle_raw;

    Emf_amplitude = sqrtf(E_alpha * E_alpha + E_beta * E_beta);
    if (!(Emf_amplitude >= MOTOR_PLL_EMF_MIN_V))
    {
        Pll->Integral_sum = 0.0f;
        Pll->Phase_error = 0.0f;
        Pll->Omega_est = 0.0f;
        return;
    }

    Pll->Direction = SMO_PLL_GetDirection();
    Electrical_angle_count = (int32)(
        Pll->Electrical_angle_rad * (float)ANGLE_PERIOD / TWO_PI);
    Electrical_angle_raw = Angle_Wrap(Electrical_angle_count);
    Sin_theta = fast_sinf(Electrical_angle_raw);
    Cos_theta = fast_cosf(Electrical_angle_raw);

    /* 方向补偿后，归一化q轴反电动势等于sin(实际角度-估算角度)。 */
    Phase_error =
        (float)Pll->Direction *
        (-E_alpha * Cos_theta - E_beta * Sin_theta) /
        Emf_amplitude;
    Phase_error = asinf(Float_Limit(Phase_error, -1.0f, 1.0f));
    PLL_Update(Pll, Phase_error, FOC_TS, Motor.Pole_pairs);

    /* 补偿反电动势一阶低通产生的随转速变化的相位滞后。 */
    Filter_omega = TWO_PI * Motor.SMO.Filter_bandwidth;
    Phase_compensation = atan2f(Pll->Omega_est, Filter_omega);
    Electrical_output_rad = Pll->Electrical_angle_rad + Phase_compensation;
    Mechanical_output_rad =
        Pll->Mechanical_angle_rad +
        Phase_compensation / (float)Motor.Pole_pairs;
    Mechanical_output_rad =
        SMO_PLL_AlignMechanicalSector(Mechanical_output_rad);
    Electrical_angle_count = (int32)(
        Electrical_output_rad * (float)ANGLE_PERIOD / TWO_PI);
    Mechanical_angle_count = (int32)(
        Mechanical_output_rad * (float)ANGLE_PERIOD / TWO_PI);
    Pll->Electrical_angle_est = Angle_Wrap(Electrical_angle_count);
    Pll->Mechanical_angle_est = Angle_Wrap(Mechanical_angle_count);
}

/*===========================================================================*/
/*  SMO观测                                                                  */
/*===========================================================================*/

/***********************************************
 * @brief : 清除SMO动态状态，等待下一组电流样本重新初始化
 * @param : 无
 * @return: 无
 * @date  : 2026-09-20
 * @author: L
 ************************************************/
void SMO_Reset(void)
{
    Motor.SMO.I_alpha_est = 0.0f;
    Motor.SMO.I_beta_est = 0.0f;
    Motor.SMO.I_alpha_estpre = 0.0f;
    Motor.SMO.I_beta_estpre = 0.0f;
    Motor.SMO.U_alpha_pre = 0.0f;
    Motor.SMO.U_beta_pre = 0.0f;
    Motor.SMO.E_alpha = 0.0f;
    Motor.SMO.E_beta = 0.0f;
    Motor.SMO.E_alpha_filter = 0.0f;
    Motor.SMO.E_beta_filter = 0.0f;
    Motor.SMO.Ready = 0u;
}

/***********************************************
 * @brief : 根据实际PWM占空比重构Alpha/Beta轴电压
 * @param : 无
 * @return: 无，结果保存到SMO上一周期电压
 * @date  : 2026-09-13
 * @author: L
 ************************************************/
void SMO_UpdateVoltage(void)
{
    float Phase_voltage_a;
    float Phase_voltage_b;
    float Phase_voltage_c;
    float Common_voltage;

    Phase_voltage_a =
        (float)SVPWM.DutyA * SVPWM.VBUS /
        (float)SVPWM_DUTY_MAX;
    Phase_voltage_b =
        (float)SVPWM.DutyB * SVPWM.VBUS /
        (float)SVPWM_DUTY_MAX;
    Phase_voltage_c =
        (float)SVPWM.DutyC * SVPWM.VBUS /
        (float)SVPWM_DUTY_MAX;

    Common_voltage =
        (Phase_voltage_a + Phase_voltage_b + Phase_voltage_c) / 3.0f;

    Phase_voltage_a -= Common_voltage;
    Phase_voltage_b -= Common_voltage;

    /* 三相电压和为零时，Alpha/Beta轴电压由A、B相直接换算。 */
    Motor.SMO.U_alpha_pre = Phase_voltage_a;
    Motor.SMO.U_beta_pre =
        (Phase_voltage_a + 2.0f * Phase_voltage_b) / SQRT3;
}

/***********************************************
 * @brief : 根据采样电流和上一周期电压更新滑模反电动势估算
 * @param : 无
 * @return: 无，估算结果保存到Motor.SMO并同步更新公共PLL
 * @date  : 2026-08-11
 * @author: L
 ************************************************/
void SMO_Update(void)
{
    float Error_alpha = 0.0f;
    float Error_beta = 0.0f;
    float Filter_omega;
    float Filter_coefficient;

    if ((Motor.SMO.K_slide <= 0.0f) ||
        (Motor.SMO.Boundary_current <= 0.0f) ||
        (Motor.SMO.Filter_bandwidth <= 0.0f))
    {
        return;
    }

    if (Motor.SMO.Ready == 0u)
    {
        Motor.SMO.I_alpha_est = Current.clark.Alpha;
        Motor.SMO.I_beta_est = Current.clark.Beta;
        Motor.SMO.I_alpha_estpre = Current.clark.Alpha;
        Motor.SMO.I_beta_estpre = Current.clark.Beta;
        Motor.SMO.E_alpha = 0.0f;
        Motor.SMO.E_beta = 0.0f;
        Motor.SMO.E_alpha_filter = 0.0f;
        Motor.SMO.E_beta_filter = 0.0f;
        Motor.SMO.Ready = 1u;
        return;
    }

    Motor.SMO.I_alpha_estpre = Motor.SMO.I_alpha_est;
    Motor.SMO.I_beta_estpre = Motor.SMO.I_beta_est;

    Motor.SMO.I_alpha_est =
        Motor.SMO.A * Motor.SMO.I_alpha_estpre +
        Motor.SMO.B * (Motor.SMO.U_alpha_pre - Motor.SMO.E_alpha);
    Motor.SMO.I_beta_est =
        Motor.SMO.A * Motor.SMO.I_beta_estpre +
        Motor.SMO.B * (Motor.SMO.U_beta_pre - Motor.SMO.E_beta);

    Error_alpha = Motor.SMO.I_alpha_est - Current.clark.Alpha;
    Error_beta = Motor.SMO.I_beta_est - Current.clark.Beta;

    Motor.SMO.E_alpha =
        Motor.SMO.K_slide *
        Float_Limit(
            Error_alpha / Motor.SMO.Boundary_current,
            -1.0f,
            1.0f);
    Motor.SMO.E_beta =
        Motor.SMO.K_slide *
        Float_Limit(
            Error_beta / Motor.SMO.Boundary_current,
            -1.0f,
            1.0f);

    Filter_omega = TWO_PI * Motor.SMO.Filter_bandwidth;
    Filter_coefficient =
        Filter_omega * FOC_TS /
        (1.0f + Filter_omega * FOC_TS);

    Motor.SMO.E_alpha_filter +=
        Filter_coefficient *
        (Motor.SMO.E_alpha - Motor.SMO.E_alpha_filter);
    Motor.SMO.E_beta_filter +=
        Filter_coefficient *
        (Motor.SMO.E_beta - Motor.SMO.E_beta_filter);

    SMO_PLL_Update();
}
