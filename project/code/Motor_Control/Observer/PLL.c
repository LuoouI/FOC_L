#include "Motor_Control/Observer/PLL.h"
#include "Foc_config.h"
#include "Function/Function.h"

/***********************************************
 * @brief : 根据自然频率带宽和固定阻尼比更新指定PLL的PI增益
 * @param : Pll PLL对象地址，由调用者保证有效
 * @param : Bandwidth_hz PLL自然频率带宽，单位为Hz，需在输入边界完成校验
 * @return: 无
 * @date  : 2026-09-20
 * @author: L
 ************************************************/
void PLL_SetBandwidth(PLL_t *Pll, float Bandwidth_hz)
{
    float Natural_omega;

    Natural_omega = TWO_PI * Bandwidth_hz;
    Pll->Bandwidth = Bandwidth_hz;
    Pll->Kp = 2.0f * MOTOR_PLL_DAMPING_RATIO * Natural_omega;
    Pll->Ki = Natural_omega * Natural_omega;
    Pll->Integral_sum = Float_Limit(
        Pll->Integral_sum,
        -MOTOR_PLL_INTEGRAL_LIMIT_RAD_S,
        MOTOR_PLL_INTEGRAL_LIMIT_RAD_S);
    Pll->Omega_est = Float_Limit(
        Pll->Omega_est,
        -MOTOR_PLL_OMEGA_LIMIT_RAD_S,
        MOTOR_PLL_OMEGA_LIMIT_RAD_S);
}

/***********************************************
 * @brief : 清除指定PLL的动态状态并设置初始方向
 * @param : Pll PLL对象地址，由调用者保证有效
 * @param : Direction 初始方向，负值表示反向，其他值表示正向
 * @return: 无
 * @date  : 2026-09-20
 * @author: L
 ************************************************/
void PLL_Reset(PLL_t *Pll, int8 Direction)
{
    Pll->Integral_sum = 0.0f;
    Pll->Phase_error = 0.0f;
    Pll->Mechanical_angle_est = 0u;
    Pll->Electrical_angle_est = 0u;
    Pll->Omega_est = 0.0f;
    Pll->Mechanical_angle_rad = 0.0f;
    Pll->Electrical_angle_rad = 0.0f;
    Pll->Direction = (Direction < 0) ? -1 : 1;
}

/***********************************************
 * @brief : 根据外部鉴相误差更新指定PLL的角速度和角度
 * @param : Pll PLL对象地址，由调用者保证有效
 * @param : Phase_error 已换算为弧度的鉴相误差
 * @param : Sample_time PLL更新周期，单位为秒，由调用者保证大于0
 * @param : Pole_pairs 电机极对数，由调用者保证大于0
 * @return: 无
 * @date  : 2026-09-20
 * @author: L
 ************************************************/
void PLL_Update(PLL_t *Pll,
                float Phase_error,
                float Sample_time,
                uint8 Pole_pairs)
{
    float Integral_next;
    float Omega_unsaturated;
    int32 Electrical_angle_count;
    int32 Mechanical_angle_count;

    Pll->Phase_error = Phase_error;
    Integral_next =
        Pll->Integral_sum + Pll->Ki * Pll->Phase_error * Sample_time;
    Integral_next = Float_Limit(
        Integral_next,
        -MOTOR_PLL_INTEGRAL_LIMIT_RAD_S,
        MOTOR_PLL_INTEGRAL_LIMIT_RAD_S);
    Omega_unsaturated = Pll->Kp * Pll->Phase_error + Integral_next;
    if (!(((Omega_unsaturated > MOTOR_PLL_OMEGA_LIMIT_RAD_S) &&
           (Pll->Phase_error > 0.0f)) ||
          ((Omega_unsaturated < -MOTOR_PLL_OMEGA_LIMIT_RAD_S) &&
           (Pll->Phase_error < 0.0f))))
    {
        Pll->Integral_sum = Integral_next;
    }
    Pll->Omega_est = Float_Limit(
        Pll->Kp * Pll->Phase_error + Pll->Integral_sum,
        -MOTOR_PLL_OMEGA_LIMIT_RAD_S,
        MOTOR_PLL_OMEGA_LIMIT_RAD_S);

    Pll->Electrical_angle_rad += Pll->Omega_est * Sample_time;
    Pll->Mechanical_angle_rad +=
        Pll->Omega_est * Sample_time / (float)Pole_pairs;
    while (Pll->Electrical_angle_rad >= TWO_PI)
    {
        Pll->Electrical_angle_rad -= TWO_PI;
    }
    while (Pll->Electrical_angle_rad < 0.0f)
    {
        Pll->Electrical_angle_rad += TWO_PI;
    }
    while (Pll->Mechanical_angle_rad >= TWO_PI)
    {
        Pll->Mechanical_angle_rad -= TWO_PI;
    }
    while (Pll->Mechanical_angle_rad < 0.0f)
    {
        Pll->Mechanical_angle_rad += TWO_PI;
    }

    Electrical_angle_count = (int32)(
        Pll->Electrical_angle_rad * (float)ANGLE_PERIOD / TWO_PI);
    Mechanical_angle_count = (int32)(
        Pll->Mechanical_angle_rad * (float)ANGLE_PERIOD / TWO_PI);
    Pll->Electrical_angle_est = Angle_Wrap(Electrical_angle_count);
    Pll->Mechanical_angle_est = Angle_Wrap(Mechanical_angle_count);
}
