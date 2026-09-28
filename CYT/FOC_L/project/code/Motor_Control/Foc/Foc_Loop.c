#include "Motor_Control/Foc/Foc_Loop.h"
#include "Motor_Control/Motor_Control.h"
#include "Current_sample/Current_sample.h"
#include "Foc_transform/Foc_transform.h"
#include "Function/Function.h"
#include "My_TCPWM/My_TCPWM.h"
#include "SVPWM/SVPWM.h"
#include "float.h"
#include <math.h>

/***********************************************
 * @brief : 按给定变化率将当前转速目标平滑逼近命令转速
 * @param : Command_rpm 上位机下发的原始速度目标，单位为rpm
 * @param : Target_rpm 当前斜坡输出，单位为rpm
 * @param : Ramp_rate 速度斜坡速率，单位为rpm/s
 * @return: 本周期更新后的速度目标，单位为rpm
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static float Speed_Ramp(float Command_rpm,
                        float Target_rpm,
                        float Ramp_rate)
{
    float Ramp_step;
    float Speed_error;

    Ramp_step = Ramp_rate * MOTOR_SPEED_LOOP_TS;
    Speed_error = Command_rpm - Target_rpm;
    if (Speed_error > Ramp_step)
    {
        Target_rpm += Ramp_step;
    }
    else if (Speed_error < -Ramp_step)
    {
        Target_rpm -= Ramp_step;
    }
    else
    {
        Target_rpm = Command_rpm;
    }

    return Target_rpm;
}

/***********************************************
 * @brief : 计算位置环最短有符号角度误差
 * @param : Target_degree 目标机械角度，单位为度
 * @param : Mechanical_degree 当前机械角度，范围0~360度
 * @return: 目标相对当前位置的最短角度误差，范围-180~180度
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
static float Position_GetShortestError(float Target_degree,
                                       float Mechanical_degree)
{
    float Error_degree;

    while (Target_degree >= 360.0f)
    {
        Target_degree -= 360.0f;
    }
    while (Target_degree < 0.0f)
    {
        Target_degree += 360.0f;
    }

    Error_degree = Target_degree - Mechanical_degree;
    while (Error_degree > 180.0f)
    {
        Error_degree -= 360.0f;
    }
    while (Error_degree < -180.0f)
    {
        Error_degree += 360.0f;
    }

    return Error_degree;
}

/***********************************************
 * @brief : 跟踪相对目标位置的连续偏转并计算原路回正误差
 * @param : Target_degree 目标机械角度，单位为度
 * @param : Mechanical_degree 当前机械角度，范围0~360度
 * @return: 与累计偏转方向相反的回正角度误差，单位为度
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
static float Position_GetReversePathError(float Target_degree,
                                          float Mechanical_degree)
{
    float Travel_step;

    if ((Motor.Position_loop.Track_ready == 0u) ||
        (Motor.Position_loop.Last_target_degree != Target_degree))
    {
        Motor.Position_loop.Travel_degree =
            -Position_GetShortestError(Target_degree, Mechanical_degree);
        Motor.Position_loop.Last_degree = Mechanical_degree;
        Motor.Position_loop.Last_target_degree = Target_degree;
        Motor.Position_loop.Track_ready = 1u;
    }
    else
    {
        Travel_step = Position_GetShortestError(
            Mechanical_degree,
            Motor.Position_loop.Last_degree);
        Motor.Position_loop.Travel_degree += Travel_step;
        Motor.Position_loop.Last_degree = Mechanical_degree;
    }

    return -Motor.Position_loop.Travel_degree;
}

/***********************************************
 * @brief : 按指定幅值限制d/q轴电流矢量
 * @param : IdValue d轴电流地址，由调用者保证有效
 * @param : IqValue q轴电流地址，由调用者保证有效
 * @param : Limit d/q轴电流矢量幅值上限，单位为A，由调用者保证大于0
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static void Current_VectorLimit(float *IdValue,
                                float *IqValue,
                                float Limit)
{
    float Current_square;
    float Limit_square;
    float Scale;

    Current_square = (*IdValue * *IdValue) + (*IqValue * *IqValue);
    Limit_square = Limit * Limit;
    if (Current_square > Limit_square)
    {
        Scale = Limit / sqrtf(Current_square);
        *IdValue *= Scale;
        *IqValue *= Scale;
    }
}

/***********************************************
 * @brief : 根据d轴电流目标计算q轴可用电流幅值
 * @param : IdTarget d轴电流目标，单位为A
 * @param : Limit d/q轴电流矢量幅值上限，单位为A，由调用者保证大于0
 * @return: q轴可用电流幅值，单位为A
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static float Current_GetIqLimit(float IdTarget, float Limit)
{
    float Id_abs;

    Id_abs = fabsf(IdTarget);
    if (Id_abs >= Limit)
    {
        return 0.0f;
    }

    return sqrtf((Limit * Limit) - (IdTarget * IdTarget));
}

/***********************************************
 * @brief : 在编码器零点无效时关闭全部有感FOC输出
 * @param : 无
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void Foc_Loop_StopOutput(void)
{
    Motor.Control_mode = MOTOR_CONTROL_OPEN_LOOP;
    Motor.Open_loop.Uq = 0.0f;
    Motor.Open_loop.Step = 0;
    Motor.Current_loop.Id_target = 0.0f;
    Motor.Current_loop.Iq_target = 0.0f;
    Motor.Speed_loop.Command_rpm = 0.0f;
    Motor.Speed_loop.Target_rpm = 0.0f;
    Motor.Speed_loop.Iq_output = 0.0f;
    Motor.Position_loop.Target_degree = 0.0f;
    Motor.Position_loop.Speed_output = 0.0f;
    Motor.Position_loop.Travel_degree = 0.0f;
    Motor.Position_loop.Track_ready = 0u;
    Motor.Position_loop.In_deadband = 0u;
    PID_Clear(&Motor.Current_loop.Id_pid);
    PID_Clear(&Motor.Current_loop.Iq_pid);
    PID_Clear(&Motor.Speed_loop.Pid);
    PID_Clear(&Motor.Position_loop.Pid);
    Open_Loop_Update(0.0f, 0.0f, 0);
}

/***********************************************
 * @brief : 执行d/q轴电流PI控制并将SVPWM饱和误差反算给积分器
 * @param : 无
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void Foc_CurrentLoop_Update(void)
{
    float Ud_request;
    float Uq_request;
    float Voltage_scale;
    float Voltage_limit;
    float Id_target;
    float Iq_target;
    uint16 DutyA;
    uint16 DutyB;
    uint16 DutyC;

    Voltage_limit = SVPWM.DQ_Limit;
    PID_SetIntegralLimit(&Motor.Current_loop.Id_pid, Voltage_limit);
    PID_SetIntegralLimit(&Motor.Current_loop.Iq_pid, Voltage_limit);

    Id_target = Motor.Current_loop.Id_target;
    Iq_target = Motor.Current_loop.Iq_target;
    Current_VectorLimit(
        &Id_target,
        &Iq_target,
        MOTOR_CURRENT_VECTOR_LIMIT_A);
    Motor.Current_loop.Id_target = Id_target;
    Motor.Current_loop.Iq_target = Iq_target;

    Ud_request = PID_Update(
        &Motor.Current_loop.Id_pid,
        Id_target,
        Current.park.Id);
    Uq_request = PID_Update(
        &Motor.Current_loop.Iq_pid,
        Iq_target,
        Current.park.Iq);

    Voltage_scale = foc_voltage_calc_duty(
        Ud_request,
        Uq_request,
        Motor.Encoder.Electrical_angle,
        &DutyA,
        &DutyB,
        &DutyC);

    Motor.Current_loop.Ud_output = Ud_request * Voltage_scale;
    Motor.Current_loop.Uq_output = Uq_request * Voltage_scale;

    PID_BackCalculation(
        &Motor.Current_loop.Id_pid,
        Motor.Current_loop.Ud_output);
    PID_BackCalculation(
        &Motor.Current_loop.Iq_pid,
        Motor.Current_loop.Uq_output);

    My_TCPWM_SetDuty(DutyA, DutyB, DutyC);
}

/***********************************************
 * @brief : 执行速度环并更新电流环Iq目标
 * @param : 无
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void Foc_SpeedLoop_Update(void)
{
    float Iq_limit;
    float Integral_limit;
    float Iq_request;

    if (Motor.Foc_mode == MOTOR_FOC_SPEED)
    {
        Motor.Speed_loop.Target_rpm = Speed_Ramp(
            Motor.Speed_loop.Command_rpm,
            Motor.Speed_loop.Target_rpm,
            Motor.Speed_loop.Ramp_rate);
    }

    /* 位置已到位且转速足够小时关闭交轴电流，避免零速噪声持续激励电机。 */
    if ((Motor.Foc_mode == MOTOR_FOC_POSITION) &&
        (Motor.Position_loop.In_deadband != 0u) &&
        (Motor.Position_loop.Speed_deadband_rpm > 0.0f) &&
        (fabsf(Motor.Encoder.Spd_rpm) <=
         Motor.Position_loop.Speed_deadband_rpm))
    {
        Motor.Speed_loop.Target_rpm = 0.0f;
        Motor.Speed_loop.Iq_output = 0.0f;
        Motor.Current_loop.Iq_target = 0.0f;
        PID_Clear(&Motor.Speed_loop.Pid);
        return;
    }

    Iq_limit = Current_GetIqLimit(
        Motor.Current_loop.Id_target,
        MOTOR_CURRENT_VECTOR_LIMIT_A);

    Integral_limit = Motor.Speed_loop.Integral_limit;
    if (Integral_limit > Iq_limit)
    {
        Integral_limit = Iq_limit;
    }
    PID_SetIntegralLimit(&Motor.Speed_loop.Pid, Integral_limit);

    Iq_request = PID_Update(
        &Motor.Speed_loop.Pid,
        Motor.Speed_loop.Target_rpm,
        Motor.Encoder.Spd_rpm);
    Motor.Speed_loop.Iq_output = Float_Limit(
        Iq_request,
        -Iq_limit,
        Iq_limit);
    PID_BackCalculation(
        &Motor.Speed_loop.Pid,
        Motor.Speed_loop.Iq_output);
    Motor.Current_loop.Iq_target = Motor.Speed_loop.Iq_output;
}

/***********************************************
 * @brief : 执行位置环并更新速度环目标
 * @param : 无
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void Foc_PositionLoop_Update(void)
{
    float Mechanical_degree;
    float Position_error;
    float Effective_error;
    float Reverse_path_error;

    /* 位置环使用扣除零偏、修正方向后的机械角，统一映射到0~360度。 */
    Mechanical_degree = Motor_Control_GetMechanicalDegree();
    Reverse_path_error = Position_GetReversePathError(
        Motor.Position_loop.Target_degree,
        Mechanical_degree);
    if (Motor.Position_loop.Return_mode ==
        MOTOR_POSITION_RETURN_REVERSE_PATH)
    {
        Position_error = Reverse_path_error;
    }
    else
    {
        Position_error = Position_GetShortestError(
            Motor.Position_loop.Target_degree,
            Mechanical_degree);
    }
    if (fabsf(Position_error) <= Motor.Position_loop.Deadband_degree)
    {
        Motor.Position_loop.In_deadband = 1u;
        Motor.Position_loop.Speed_output = 0.0f;
        Motor.Speed_loop.Target_rpm = 0.0f;
        if (Motor.Position_loop.Return_mode ==
            MOTOR_POSITION_RETURN_SHORTEST)
        {
            Motor.Position_loop.Travel_degree = 0.0f;
            Motor.Position_loop.Last_degree = Mechanical_degree;
            Motor.Position_loop.Last_target_degree =
                Motor.Position_loop.Target_degree;
            Motor.Position_loop.Track_ready = 1u;
        }
        PID_Clear(&Motor.Position_loop.Pid);
        return;
    }
    Motor.Position_loop.In_deadband = 0u;

    /* 扣除死区宽度，使速度目标在死区边界从零连续增加。 */
    if (Position_error > 0.0f)
    {
        Effective_error =
            Position_error - Motor.Position_loop.Deadband_degree;
    }
    else
    {
        Effective_error =
            Position_error + Motor.Position_loop.Deadband_degree;
    }

    /* 到位软化范围内线性恢复位置Kp比例，超出范围后使用完整增益。 */
    if ((Motor.Position_loop.Soft_range_degree >
         Motor.Position_loop.Deadband_degree) &&
        (fabsf(Position_error) < Motor.Position_loop.Soft_range_degree))
    {
        Effective_error *=
            fabsf(Effective_error) /
            (Motor.Position_loop.Soft_range_degree -
             Motor.Position_loop.Deadband_degree);
    }

    Motor.Position_loop.Speed_output = PID_Update(
        &Motor.Position_loop.Pid,
        Effective_error,
        0.0f);
    Motor.Speed_loop.Target_rpm = Motor.Position_loop.Speed_output;
}

/***********************************************
 * @brief : 更新FOC电流环带宽并重算PI增益
 * @param : BandwidthHz 电流环带宽，单位为Hz
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void Motor_Control_SetCurrentBandwidth(uint16 BandwidthHz)
{
    if ((Motor.Current_loop.Bandwidth == BandwidthHz) &&
        (Motor.Current_loop.Id_pid.Kp != 0.0f) &&
        (Motor.Current_loop.Iq_pid.Kp != 0.0f))
    {
        return;
    }

    PID_SetBandwidth(
        &Motor.Current_loop.Id_pid,
        BandwidthHz,
        LD,
        RS);
    PID_SetBandwidth(
        &Motor.Current_loop.Iq_pid,
        BandwidthHz,
        LQ,
        RS);
    Motor.Current_loop.Bandwidth = BandwidthHz;
}

/***********************************************
 * @brief : 更新速度环PI参数
 * @param : Kp 比例增益
 * @param : Ki 连续时间积分增益
 * @param : IntegralLimit 积分项输出限幅，单位为A
 * @return: 无
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
void Motor_Control_SetSpeedPi(float Kp,
                              float Ki,
                              float IntegralLimit)
{
    /* 保留速度PI原始输出，由速度环按实时Iq能力限幅后进行反算。 */
    PID_Config(
        &Motor.Speed_loop.Pid,
        MOTOR_SPEED_LOOP_TS,
        MOTOR_SPEED_LOOP_TS,
        -FLT_MAX,
        FLT_MAX,
        -IntegralLimit,
        IntegralLimit);
    Motor.Speed_loop.Pid.Kp = Kp;
    Motor.Speed_loop.Pid.Ki = Ki;
    Motor.Speed_loop.Integral_limit = IntegralLimit;
}

/***********************************************
 * @brief : 更新位置环纯Kp、限幅、死区及到位软化参数
 * @param : Kp 比例增益
 * @param : OutputLimit 输出限幅，单位为rpm
 * @param : Deadband_degree 角度死区，单位为度
 * @param : SoftRange_degree 到位线性软化范围，单位为度，不大于角度死区时关闭
 * @param : SpeedDeadband_rpm 到位速度死区，单位为rpm，填0时关闭
 * @return: 无
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
void Motor_Control_SetPositionKp(float Kp,
                                 float OutputLimit,
                                 float Deadband_degree,
                                 float SoftRange_degree,
                                 float SpeedDeadband_rpm)
{
    PID_Config(
        &Motor.Position_loop.Pid,
        MOTOR_POSITION_LOOP_TS,
        MOTOR_POSITION_LOOP_TS,
        -OutputLimit,
        OutputLimit,
        0.0f,
        0.0f);
    /* 位置环只保留比例项，积分器和积分限幅始终清零。 */
    Motor.Position_loop.Pid.Kp = Kp;
    Motor.Position_loop.Pid.Ki = 0.0f;
    Motor.Position_loop.Deadband_degree = Deadband_degree;
    Motor.Position_loop.Soft_range_degree = SoftRange_degree;
    Motor.Position_loop.Speed_deadband_rpm = SpeedDeadband_rpm;
}
