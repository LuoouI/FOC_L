#include "Motor_Control.h"
#include "float.h"
#include "Current_sample/Current_sample.h"
#include "Filter/AB_Filter.h"
#include "Foc_voice/Foc_voice.h"
#include "Motor_Flash/Motor_Flash.h"
#include "My_TCPWM/My_TCPWM.h"
#include "SVPWM/SVPWM.h"
#include <math.h>

const Motor_ZeroCalib_t Motor_zeroCalib =
{
    .Voltage = 2.0f,
    .Ramp_count = 0u,
    .Ramp_ms = 5u,
    .Hold_ms = 200u,
    .Step_count = 2000u,
    .Step_ms = 1u,
    .Sample_count = 10u,
    .Sample_ms = 5u,
    .Min_travel = 1000
};

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
    .Ab_filter_bandwidth = 50.0f,
    .Pole_pairs = 7u,
    .Control_mode = MOTOR_CONTROL_OPEN_LOOP,
    .Foc_mode = MOTOR_FOC_CURRENT,
    .Foc_direction = 1,
    .Zero_ready = 0u
};

/*===========================================================================*/
/*  前期准备                                                                  */
/*===========================================================================*/

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

    if ((Command_rpm != Command_rpm) ||
        (Target_rpm != Target_rpm))
    {
        return 0.0f;
    }

    if ((Ramp_rate != Ramp_rate) || (Ramp_rate <= 0.0f))
    {
        return Target_rpm;
    }

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

void Angle_Update(void)
{
    int32 MechAng;

    Motor.Encoder.Mechanical_angle =
        menc15a_get_absolute_data(Motor.Encoder.Sensor_id);

    MechAng =
        ((int32)Motor.Encoder.Mechanical_angle -
         (int32)Motor.Encoder.Zero_offset) *
        (int32)Motor.Encoder.Direction;

    Motor.Encoder.Electrical_angle =
        Angle_Wrap(MechAng * (int32)Motor.Pole_pairs);
}

float Motor_Control_GetMechanicalDegree(void)
{
    int32 Mechanical_count;

    Mechanical_count =
        ((int32)Motor.Encoder.Mechanical_angle -
         (int32)Motor.Encoder.Zero_offset) *
        (int32)Motor.Encoder.Direction;
    Mechanical_count = (int32)Angle_Wrap(Mechanical_count);

    return (float)Mechanical_count * 360.0f / (float)ANGLE_PERIOD;
}

void RPM_Cal(void)
{
    static uint8 FltReady = 0u;
    ABFilterParam_t RpmFltCfg;
    int32 MechAng;
    uint16 WrapAng;
    float MeasAng;
    float Omega;

    if ((FltReady == 0u) ||
        (Angle.Param.Bw_hz != Motor.Ab_filter_bandwidth))
    {
        RpmFltCfg.Ts = MOTOR_SPEED_LOOP_TS;
        RpmFltCfg.Bw_hz = Motor.Ab_filter_bandwidth;
        ABFilter_Init(&Angle, &RpmFltCfg);
        FltReady = 1u;
    }

    MechAng =
        ((int32)Motor.Encoder.Mechanical_angle -
         (int32)Motor.Encoder.Zero_offset) *
        (int32)Motor.Encoder.Direction;

    WrapAng = Angle_Wrap(MechAng);
    MeasAng =
        (float)WrapAng * (TWO_PI / (float)ANGLE_PERIOD);

    Omega = ABFilter_Update(&Angle, MeasAng);
    Motor.Encoder.Spd_rpm = Omega * 60.0f / TWO_PI;
}

/***********************************************
 * @brief : 使用正弦包络输出一组指定相桥臂的自检音符
 * @param : Phase 主发声相
 * @param : Pitch 音符频率
 * @param : Tone_ms 单次鸣响持续时间，单位为ms
 * @param : Gap_ms 单次鸣响后的间隔时间，单位为ms
 * @param : Beep_count 鸣响次数
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static void PHASE_TestBeep(
    Foc_voicePhase_t Phase,
    Foc_voicePitch_t Pitch,
    uint16 Tone_ms,
    uint16 Gap_ms,
    uint8 Beep_count)
{
    uint8 Beep_index;

    for (Beep_index = 0u; Beep_index < Beep_count; Beep_index++)
    {
        Foc_voice_PlayTone(Phase, Pitch, Tone_ms, Gap_ms);
    }
}

/***********************************************
 * @brief : 依次驱动三相桥臂，通过一声、两声、三声检查MOS及预驱功能
 * @param : 无
 * @return: 无，自检结果由鸣响是否完整进行人工判断
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static void PHASE_Test(void)
{
    /* A相主发声，播放C5音符一声。 */
    PHASE_TestBeep(
        FOC_VOICE_PHASE_A,
        FOC_VOICE_PITCH_C5,
        70u,
        50u,
        1u);
    system_delay_ms(200u);

    /* B相主发声，播放E5音符两声。 */
    PHASE_TestBeep(
        FOC_VOICE_PHASE_B,
        FOC_VOICE_PITCH_E5,
        70u,
        50u,
        2u);
    system_delay_ms(200u);

    /* C相主发声，播放G5音符三声。 */
    PHASE_TestBeep(
        FOC_VOICE_PHASE_C,
        FOC_VOICE_PITCH_G5,
        70u,
        50u,
        3u);
}

/***********************************************
 * @brief : 按指定电压和电角度输出零点校准用d轴电压
 * @param : Voltage d轴电压，单位为V
 * @param : Electrical_angle 电角度，范围0~32767
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static void Zero_CalibrationOutput(float Voltage, uint16 Electrical_angle)
{
    uint16 DutyA;
    uint16 DutyB;
    uint16 DutyC;

    foc_voltage_calc_duty(
        Voltage,
        0.0f,
        Electrical_angle,
        &DutyA,
        &DutyB,
        &DutyC);
    My_TCPWM_SetDuty(DutyA, DutyB, DutyC);
}

/***********************************************
 * @brief : 对转子静止位置进行解缠平均采样
 * @param : 无
 * @return: 平均后的机械角零偏，范围0~32767
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static uint16 Zero_CalibrationSample(void)
{
    AngleUnwrap_t Sample_angle;
    int64 Sample_sum = 0;
    int64 Sample_average;
    uint16 Sample_index;
    uint16 Raw_angle;

    Angle_Unwrap_Clear(&Sample_angle);

    for (Sample_index = 0u;
         Sample_index < Motor_zeroCalib.Sample_count;
         Sample_index++)
    {
        Raw_angle = menc15a_get_absolute_data(Motor.Encoder.Sensor_id);
        Sample_sum += (int64)Angle_Unwrap(&Sample_angle, Raw_angle);
        system_delay_ms(Motor_zeroCalib.Sample_ms);
    }

    if (Sample_sum >= 0)
    {
        Sample_average =
            (Sample_sum + (int64)(Motor_zeroCalib.Sample_count / 2u)) /
            (int64)Motor_zeroCalib.Sample_count;
    }
    else
    {
        Sample_average =
            (Sample_sum - (int64)(Motor_zeroCalib.Sample_count / 2u)) /
            (int64)Motor_zeroCalib.Sample_count;
    }

    return Angle_Wrap((int32)Sample_average);
}

/***********************************************
 * @brief : 快速播放零点校准成功七音阶
 * @param : 无
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static void Zero_CalibrationSuccessTone(void)
{
    static const Foc_voicePitch_t Tone_pitch[7] =
    {
        FOC_VOICE_PITCH_D5,
        FOC_VOICE_PITCH_E5,
        FOC_VOICE_PITCH_F5,
        FOC_VOICE_PITCH_G5,
        FOC_VOICE_PITCH_A5,
        FOC_VOICE_PITCH_B5,
        FOC_VOICE_PITCH_C6
    };
    uint8 Tone_index;

    for (Tone_index = 0u; Tone_index < 7u; Tone_index++)
    {
        Foc_voice_PlayTone(
            FOC_VOICE_PHASE_A,
            Tone_pitch[Tone_index],
            70u,
            10u);
    }
}

/***********************************************
 * @brief : 播放Flash保存成功的对称七音降调
 * @param : 无
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static void Zero_CalibrationFlashTone(void)
{
    static const Foc_voicePitch_t Tone_pitch[7] =
    {
        FOC_VOICE_PITCH_C6,
        FOC_VOICE_PITCH_B5,
        FOC_VOICE_PITCH_A5,
        FOC_VOICE_PITCH_G5,
        FOC_VOICE_PITCH_F5,
        FOC_VOICE_PITCH_E5,
        FOC_VOICE_PITCH_D5
    };
    uint8 Tone_index;

    for (Tone_index = 0u; Tone_index < 7u; Tone_index++)
    {
        Foc_voice_PlayTone(
            FOC_VOICE_PHASE_A,
            Tone_pitch[Tone_index],
            70u,
            10u);
    }
}

/* 执行桥臂自检及编码器零点校准。 */
void Zero_Calibration(void)
{
    AngleUnwrap_t Travel_angle;
    uint32 Irq_state;
    int32 Start_angle;
    int32 End_angle;
    int32 Travel;
    int32 Abs_travel;
    int32 Field_angle;
    float Align_voltage;
    uint16 Ramp_index;
    uint16 Step_index;
    uint16 Raw_angle;
    uint16 Pole_pairs;
    uint16 Neutral_duty = (uint16)(TCPWM_DUTY_MAX / 2u);
    uint16 Old_zero = Motor.Encoder.Zero_offset;
    uint8 Old_pole_pairs = Motor.Pole_pairs;
    int8 Old_direction = Motor.Encoder.Direction;
    uint8 Calib_ok = 0u;
    uint8 Flash_ok = 0u;

    Foc_voice_Stop();
    Motor.Control_mode = MOTOR_CONTROL_OPEN_LOOP;
    Motor.Open_loop.Uq = 0.0f;
    Motor.Open_loop.Step = 0;
    Motor.Open_loop.Hold_count = 0u;
    Motor.Open_loop.Started = 0u;
    Motor.Zero_ready = 0u;

    My_TCPWM_SetDuty(Neutral_duty, Neutral_duty, Neutral_duty);
    SVPWM_DutyCache_Update(Neutral_duty, Neutral_duty, Neutral_duty);
    system_delay_ms(Motor_zeroCalib.Hold_ms);

    PHASE_Test();

    if ((Motor_zeroCalib.Step_count == 0u) ||
        (Motor_zeroCalib.Sample_count == 0u))
    {
        return;
    }

    Irq_state = interrupt_global_disable();
    Angle_Unwrap_Clear(&Travel_angle);

    /* 缓慢建立锁定电压，减小转子吸合到电角零位时的冲击。 */
    if (Motor_zeroCalib.Ramp_count > 0u)
    {
        for (Ramp_index = 1u;
             Ramp_index <= Motor_zeroCalib.Ramp_count;
             Ramp_index++)
        {
            Align_voltage =
                Motor_zeroCalib.Voltage * (float)Ramp_index /
                (float)Motor_zeroCalib.Ramp_count;
            Zero_CalibrationOutput(Align_voltage, 0u);
            system_delay_ms(Motor_zeroCalib.Ramp_ms);
        }
    }
    else
    {
        Zero_CalibrationOutput(Motor_zeroCalib.Voltage, 0u);
    }

    /* 将转子稳定锁定到电角零位，作为整圈牵引的起点。 */
    system_delay_ms(Motor_zeroCalib.Hold_ms);
    Raw_angle = menc15a_get_absolute_data(Motor.Encoder.Sensor_id);
    Start_angle = Angle_Unwrap(&Travel_angle, Raw_angle);
    End_angle = Start_angle;

    /* 正向牵引一整圈电角度，并对机械角连续解缠。 */
    for (Step_index = 1u;
         Step_index <= Motor_zeroCalib.Step_count;
         Step_index++)
    {
        Field_angle =
            (int32)(((uint32)ANGLE_PERIOD * (uint32)Step_index) /
                    (uint32)Motor_zeroCalib.Step_count);
        Zero_CalibrationOutput(
            Motor_zeroCalib.Voltage,
            Angle_Wrap(Field_angle));
        system_delay_ms(Motor_zeroCalib.Step_ms);

        Raw_angle = menc15a_get_absolute_data(Motor.Encoder.Sensor_id);
        End_angle = Angle_Unwrap(&Travel_angle, Raw_angle);
    }

    /* 在下一个电角零位继续保持，消除转子跟随滞后。 */
    Zero_CalibrationOutput(Motor_zeroCalib.Voltage, 0u);
    system_delay_ms(Motor_zeroCalib.Hold_ms);
    Raw_angle = menc15a_get_absolute_data(Motor.Encoder.Sensor_id);
    End_angle = Angle_Unwrap(&Travel_angle, Raw_angle);

    Travel = End_angle - Start_angle;
    Abs_travel = (Travel >= 0) ? Travel : -Travel;

    if (Abs_travel >= Motor_zeroCalib.Min_travel)
    {
        Pole_pairs = (uint16)
            (((int32)ANGLE_PERIOD + (Abs_travel / 2)) / Abs_travel);

        if ((Pole_pairs > 0u) && (Pole_pairs <= 255u))
        {
            Motor.Encoder.Direction = (Travel > 0) ? 1 : -1;
            Motor.Pole_pairs = (uint8)Pole_pairs;
            Motor.Encoder.Zero_offset = Zero_CalibrationSample();
            Motor.Zero_ready = 1u;
            Calib_ok = 1u;
        }
    }

    if (Calib_ok == 0u)
    {
        Motor.Encoder.Zero_offset = Old_zero;
        Motor.Pole_pairs = Old_pole_pairs;
        Motor.Encoder.Direction = Old_direction;
    }
    My_TCPWM_SetDuty(Neutral_duty, Neutral_duty, Neutral_duty);
    SVPWM_DutyCache_Update(Neutral_duty, Neutral_duty, Neutral_duty);
    Motor.Open_loop.Angle = 0u;
    Motor.Open_loop.Hold_count = 0u;
    Motor.Open_loop.Started = 0u;
    Angle_Update();

    interrupt_global_enable(Irq_state);

    /* 校准成功后保存参数，再依次播放校准和存储成功提示音。 */
    if (Calib_ok != 0u)
    {
        Flash_ok = Motor_Flash_Save();
        Zero_CalibrationSuccessTone();
        if (Flash_ok != 0u)
        {
            Zero_CalibrationFlashTone();
        }
    }
}

/*===========================================================================*/
/*  开环角度牵引                                                              */
/*===========================================================================*/

/***********************************************
 * @brief : 按给定d/q轴电压和步长执行一次开环角度牵引
 * @param : Uq q轴电压，单位为V
 * @param : Ud d轴电压，单位为V
 * @param : Step 单控制周期电角度增量，负值表示反向
 * @return: 无
 * @date  : 2026-08-27
 * @author: L
 ************************************************/
static void Motor_openloop_set(float Uq, float Ud, int16 Step)
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

/*===========================================================================*/
/*  有感FOC                                                                  */
/*===========================================================================*/

/***********************************************
 * @brief : 按指定幅值限制d/q轴电流矢量
 * @param : IdValue d轴电流地址
 * @param : IqValue q轴电流地址
 * @param : Limit 电流矢量幅值上限，单位为A
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

    if ((IdValue == NULL) || (IqValue == NULL))
    {
        return;
    }

    if ((Limit <= 0.0f) ||
        (*IdValue != *IdValue) ||
        (*IqValue != *IqValue) ||
        (*IdValue > FLT_MAX) ||
        (*IdValue < -FLT_MAX) ||
        (*IqValue > FLT_MAX) ||
        (*IqValue < -FLT_MAX))
    {
        *IdValue = 0.0f;
        *IqValue = 0.0f;
        return;
    }

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
 * @param : Limit d/q轴电流矢量幅值上限，单位为A
 * @return: q轴可用电流幅值，单位为A
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static float Current_GetIqLimit(float IdTarget, float Limit)
{
    float Id_abs;

    if ((IdTarget != IdTarget) ||
        (Limit != Limit) ||
        (Limit <= 0.0f))
    {
        return 0.0f;
    }

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
static void EncoderFoc_StopOutput(void)
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
    Motor_openloop_set(0.0f, 0.0f, 0);
}

/***********************************************
 * @brief : 执行d/q轴电流PI控制并将SVPWM饱和误差反算给积分器
 * @param : 无
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static void Current_Loop(void)
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
static void Speed_Loop(void)
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
static void Position_Loop(void)
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

void Motor_Control_SetCurrentBandwidth(uint16 BandwidthHz)
{
    if (BandwidthHz == 0u)
    {
        return;
    }

    if (BandwidthHz < PID_BANDWIDTH_MIN_HZ)
    {
        BandwidthHz = PID_BANDWIDTH_MIN_HZ;
    }
    else if (BandwidthHz > PID_BANDWIDTH_MAX_HZ)
    {
        BandwidthHz = PID_BANDWIDTH_MAX_HZ;
    }
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

void Motor_Control_Init(void)
{
    float Voltage_limit;

    (void)menc15a_init();

    Motor_Control_SetCurrentBandwidth(Motor.Current_loop.Bandwidth);

    Voltage_limit = SVPWM.DQ_Limit;
    if ((Voltage_limit != Voltage_limit) || (Voltage_limit < 0.0f))
    {
        Voltage_limit = 0.0f;
    }
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
}

void Motor_Control_SetSpeedPi(float Kp,
                              float Ki,
                              float IntegralLimit)
{
    if (IntegralLimit < 0.0f)
    {
        IntegralLimit = -IntegralLimit;
    }
    if (IntegralLimit > MOTOR_CURRENT_VECTOR_LIMIT_A)
    {
        IntegralLimit = MOTOR_CURRENT_VECTOR_LIMIT_A;
    }

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

void Motor_Control_SetPositionKp(float Kp,
                                 float OutputLimit,
                                 float Deadband_degree,
                                 float SoftRange_degree,
                                 float SpeedDeadband_rpm)
{
    if (OutputLimit < 0.0f)
    {
        OutputLimit = -OutputLimit;
    }
    if ((Deadband_degree != Deadband_degree) ||
        (Deadband_degree < 0.0f))
    {
        Deadband_degree = 0.0f;
    }
    if (Deadband_degree > 180.0f)
    {
        Deadband_degree = 180.0f;
    }
    if ((SoftRange_degree != SoftRange_degree) ||
        (SoftRange_degree < 0.0f))
    {
        SoftRange_degree = 0.0f;
    }
    if (SoftRange_degree > 180.0f)
    {
        SoftRange_degree = 180.0f;
    }
    if ((SpeedDeadband_rpm != SpeedDeadband_rpm) ||
        (SpeedDeadband_rpm < 0.0f))
    {
        SpeedDeadband_rpm = 0.0f;
    }
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

/*===========================================================================*/
/*  总控制                                                                    */
/*===========================================================================*/

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
    }

    switch (Motor.Control_mode)
    {
        case MOTOR_CONTROL_OPEN_LOOP:
            Motor_openloop_set(
                Motor.Open_loop.Uq,
                0.0f,
                Motor.Open_loop.Step);
            break;

        case MOTOR_CONTROL_ENCODER_FOC:
            if (Motor.Zero_ready == 0u)
            {
                EncoderFoc_StopOutput();
                Speed_count = 0u;
                Position_count = 0u;
                break;
            }

            if (Motor.Foc_mode == MOTOR_FOC_POSITION)
            {
                if (Position_count == 0u)
                {
                    Position_Loop();
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
                    Speed_Loop();
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

            Current_Loop();
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
