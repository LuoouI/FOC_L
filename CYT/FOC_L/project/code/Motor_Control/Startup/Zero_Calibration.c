#include "Motor_Control/Startup/Zero_Calibration.h"
#include "Motor_Control/Motor_Control.h"
#include "Foc_transform/Foc_transform.h"
#include "Foc_voice/Foc_voice.h"
#include "Function/Function.h"
#include "Motor_Flash/Motor_Flash.h"
#include "My_TCPWM/My_TCPWM.h"
#include "SVPWM/SVPWM.h"

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

/***********************************************
 * @brief : 在主循环中阻塞执行桥臂自检及编码器零点校准
 * @param : 无
 * @return: 无，校准结果保存到Motor，Zero_ready表示是否成功
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
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
