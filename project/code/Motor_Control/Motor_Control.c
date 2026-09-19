#include "Motor_Control.h"
#include "Foc_voice/Foc_voice.h"
#include "SVPWM/SVPWM.h"
#include "float.h"

Foc_motor_t Motor =
{
    .Encoder =
    {
        .Sensor_id = menc15a_2_module,
        .Direction = 1,
        .Zero_offset = 0u,
        .Mechanical_angle = 0u,
        .Electrical_angle = 0u,
        .Spd_rpm = 0.0f
    },
    .Output =
    {
        .Duty_target = 0,
        .Duty_output = 0.0f,
    },
    .Open_loop =
    {
        .Uq = 0.0f,
        .Angle = 0u,
        .Step = 0,
        .Align_count = 4000u,
        .Hold_count = 0u,
        .Started = 0u
    },
    .Current_loop =
    {
        .Bandwidth = 1000u
    },
    .Speed_loop =
    {
        .Ramp_rate = 2000.0f,
        .Pid =
        {
            .Kp = 0.01f,
            .Ki = 0.0044,
        },
        .Integral_limit = 1.0f
    },
    .Position_loop =
    {
        .Pid =
        {
            .Kp = 30.0f,
            .LimMax = 200.0f,
            .LimMin = -200.0f,
        },
        .Deadband_degree = 8.0f,
        .Soft_range_degree = 10.0f,
        .Speed_deadband_rpm = 20.0f,
        .Travel_degree = 0.0f,
        .Last_degree = 0.0f,
        .Last_target_degree = 0.0f,
        .Return_mode = MOTOR_POSITION_RETURN_SHORTEST,
        .Track_ready = 0u,
        .In_deadband = 0u
    },
    .SMO =
    {
        .E_alpha = 0.0f,
        .E_beta = 0.0f,
        .E_beta_filter = 0.0f,
        .E_alpha_filter = 0.0f,
        .K_slide = 1.80f,
        .Boundary_current = 1.5f,
        .Filter_bandwidth = 500.0f,
        .A = (2.0f * LS * 0.001f - RS * FOC_TS) / DENOMINATOR,
        .B = 2.0f * FOC_TS / DENOMINATOR
    },
    .PLL =
    {
        .Bandwidth = 250.0f,
        .Kp = 0.0f,
        .Ki = 0.0f,
        .Integral_sum = 0.0f,
        .Phase_error = 0.0f,
        .Mechanical_angle_est = 0u,
        .Electrical_angle_est = 0u,
        .Omega_est = 0.0f,
        .Mechanical_angle_rad = 0.0f,
        .Electrical_angle_rad = 0.0f,
        .Direction = 1
    },
    .Ab_filter_bandwidth = 50.0f,
    .Pole_pairs = 7u,
    .Control_mode = MOTOR_CONTROL_OPEN_LOOP,
    .Foc_mode = MOTOR_FOC_CURRENT,
    .Foc_direction = 1,
    .Zero_ready = 0u
};

/***********************************************
 * @brief : 使用Motor中的参数初始化FOC环路及无感观测器
 * @param : 无
 * @return: 无
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
void Motor_Control_Init(void)
{
    float Voltage_limit;

    (void)menc15a_init();

    Motor_Control_SetCurrentBandwidth(Motor.Current_loop.Bandwidth);

    Voltage_limit = SVPWM.DQ_Limit;
    PID_Config(
        &Motor.Current_loop.Id_pid,
        MOTOR_CURRENT_LOOP_TS,
        MOTOR_CURRENT_LOOP_TS,
        -FLT_MAX,
        FLT_MAX,
        -Voltage_limit,
        Voltage_limit);
    PID_Config(
        &Motor.Current_loop.Iq_pid,
        MOTOR_CURRENT_LOOP_TS,
        MOTOR_CURRENT_LOOP_TS,
        -FLT_MAX,
        FLT_MAX,
        -Voltage_limit,
        Voltage_limit);

    Motor_Control_SetSpeedPi(
        Motor.Speed_loop.Pid.Kp,
        Motor.Speed_loop.Pid.Ki,
        Motor.Speed_loop.Integral_limit);
    Motor_Control_SetPositionKp(
        Motor.Position_loop.Pid.Kp,
        Motor.Position_loop.Pid.LimMax,
        Motor.Position_loop.Deadband_degree,
        Motor.Position_loop.Soft_range_degree,
        Motor.Position_loop.Speed_deadband_rpm);
    PLL_SetBandwidth(&Motor.PLL, Motor.PLL.Bandwidth);
    Motor_Control_ResetObserver();
}

/***********************************************
 * @brief : 清除SMO和PLL动态状态，等待下一组电流样本重新初始化
 * @param : 无
 * @return: 无
 * @date  : 2026-09-15
 * @author: L
 ************************************************/
void Motor_Control_ResetObserver(void)
{
    SMO_Reset();
    PLL_Reset(&Motor.PLL, Motor.Foc_direction);
}

/***********************************************
 * @brief : 按20 kHz时基执行总控，并分频运行1 kHz速度环和500 Hz位置环
 * @param : 无
 * @return: 无
 * @date  : 2026-08-27
 * @author: L
 ************************************************/
void Motor_Control_Loop(void)
{
    static uint16 Speed_count = 0u;
    static uint16 Position_count = 0u;
    static Motor_control_mode_t Last_control_mode = MOTOR_CONTROL_OPEN_LOOP;
    static Motor_foc_mode_t Last_foc_mode = MOTOR_FOC_CURRENT;

    if ((Motor.Control_mode != Last_control_mode) ||
        ((Motor.Control_mode == MOTOR_CONTROL_ENCODER_FOC) &&
         (Motor.Foc_mode != Last_foc_mode)))
    {
        Speed_count = 0u;
        Position_count = 0u;
        Motor.Position_loop.Track_ready = 0u;
        Motor.Position_loop.In_deadband = 0u;
        if ((Motor.Control_mode == MOTOR_CONTROL_ENCODER_FOC) &&
            (Motor.Foc_mode == MOTOR_FOC_SPEED))
        {
            Motor.Speed_loop.Target_rpm = Motor.Encoder.Spd_rpm;
            PID_Clear(&Motor.Speed_loop.Pid);
        }

        if ((Motor.Control_mode == MOTOR_CONTROL_SENSORLESS_FOC) &&
            (Last_control_mode != MOTOR_CONTROL_SENSORLESS_FOC))
        {
            /* 无感观测模式切入时先清除旧控制输出，避免沿用有感PWM。 */
            Motor.Current_loop.Ud_output = 0.0f;
            Motor.Current_loop.Uq_output = 0.0f;
            PID_Clear(&Motor.Current_loop.Id_pid);
            PID_Clear(&Motor.Current_loop.Iq_pid);
            Motor_Control_ResetObserver();
            Open_Loop_Update(0.0f, 0.0f, 0);
        }
    }

    switch (Motor.Control_mode)
    {
        case MOTOR_CONTROL_OPEN_LOOP:
            Open_Loop_Update(
                Motor.Open_loop.Uq,
                0.0f,
                Motor.Open_loop.Step);
            break;

        case MOTOR_CONTROL_SENSORLESS_FOC:
            /* 无感模式只运行SMO观测，不使用编码器角度或编码器速度反馈。 */
            SMO_Update();
            SMO_UpdateVoltage();
            break;

        case MOTOR_CONTROL_ENCODER_FOC:
            if (Motor.Zero_ready == 0u)
            {
                Foc_Loop_StopOutput();
                Speed_count = 0u;
                Position_count = 0u;
                break;
            }

            if (Motor.Foc_mode == MOTOR_FOC_POSITION)
            {
                if (Position_count == 0u)
                {
                    Foc_PositionLoop_Update();
                    Position_count = MOTOR_POSITION_LOOP_DIVIDER - 1u;
                }
                else
                {
                    Position_count--;
                }
            }
            else
            {
                Position_count = 0u;
            }

            if ((Motor.Foc_mode == MOTOR_FOC_SPEED) ||
                (Motor.Foc_mode == MOTOR_FOC_POSITION))
            {
                if (Speed_count == 0u)
                {
                    Foc_SpeedLoop_Update();
                    Speed_count = MOTOR_SPEED_LOOP_DIVIDER - 1u;
                }
                else
                {
                    Speed_count--;
                }
            }
            else
            {
                Speed_count = 0u;
            }

            SMO_Update();
            Foc_CurrentLoop_Update();
            SMO_UpdateVoltage();
            break;

        case MOTOR_CONTROL_VOICE:
            Foc_voice_Loop();
            break;

        default:
            break;
    }

    Last_control_mode = Motor.Control_mode;
    Last_foc_mode = Motor.Foc_mode;
}
