#include "Foc_Protocol.h"
#include "Current_sample/Current_sample.h"
#include "Foc_voice/Foc_voice.h"
#include "Motor_Control/Motor_Control.h"
#include "Motor_Torque/Motor_Torque.h"
#include "SVPWM/SVPWM.h"

static Foc_Protocol_t Protocol;
static void Foc_Protocol_SendLoopParameters(void);

/***********************************************
 * @brief : 读取小端序16位无符号整数
 * @param : Data 待读取字节地址
 * @return: 16位无符号整数
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static uint16 Foc_Protocol_ReadU16(const uint8 *Data)
{
    return (uint16)Data[0] |
           (uint16)((uint16)Data[1] << 8u);
}

/***********************************************
 * @brief : 读取小端序单精度浮点数
 * @param : Data 待读取字节地址
 * @return: 单精度浮点数
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static float Foc_Protocol_ReadFloat(const uint8 *Data)
{
    float Value;

    memcpy(&Value, Data, sizeof(Value));
    return Value;
}

/***********************************************
 * @brief : 读取小端序32位无符号整数
 * @param : Data 待读取字节地址
 * @return: 32位无符号整数
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static uint32 Foc_Protocol_ReadU32(const uint8 *Data)
{
    return (uint32)Data[0] |
           ((uint32)Data[1] << 8u) |
           ((uint32)Data[2] << 16u) |
           ((uint32)Data[3] << 24u);
}

/***********************************************
 * @brief : 写入小端序16位无符号整数
 * @param : Data 待写入字节地址
 * @param : Value 待写入数值
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void Foc_Protocol_WriteU16(uint8 *Data, uint16 Value)
{
    Data[0] = (uint8)Value;
    Data[1] = (uint8)(Value >> 8u);
}

/***********************************************
 * @brief : 写入小端序32位无符号整数
 * @param : Data 待写入字节地址
 * @param : Value 待写入数值
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void Foc_Protocol_WriteU32(uint8 *Data, uint32 Value)
{
    Data[0] = (uint8)Value;
    Data[1] = (uint8)(Value >> 8u);
    Data[2] = (uint8)(Value >> 16u);
    Data[3] = (uint8)(Value >> 24u);
}

/***********************************************
 * @brief : 写入小端序单精度浮点数
 * @param : Data 待写入字节地址
 * @param : Value 待写入数值
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void Foc_Protocol_WriteFloat(uint8 *Data, float Value)
{
    memcpy(Data, &Value, sizeof(Value));
}

/***********************************************
 * @brief : 计算CRC16-Modbus校验值
 * @param : Data 待校验数据地址
 * @param : Length 待校验数据长度
 * @return: CRC16-Modbus校验值
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static uint16 Foc_Protocol_Crc16(const uint8 *Data, uint16 Length)
{
    uint16 Crc = 0xffffu;
    uint16 Index;
    uint8 Bit_index;

    for (Index = 0u; Index < Length; Index++)
    {
        Crc ^= Data[Index];
        for (Bit_index = 0u; Bit_index < 8u; Bit_index++)
        {
            if ((Crc & 0x0001u) != 0u)
            {
                Crc = (uint16)((Crc >> 1u) ^ 0xa001u);
            }
            else
            {
                Crc >>= 1u;
            }
        }
    }

    return Crc;
}

/***********************************************
 * @brief : 停止当前控制输出并恢复开环停止状态
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void Foc_Protocol_StopControl(void)
{
    Foc_voice_Stop();
    Motor.Control_mode = MOTOR_CONTROL_OPEN_LOOP;
    Motor.Open_loop.Uq = 0.0f;
    Motor.Open_loop.Step = 0;
    Motor.Open_loop.Hold_count = 0u;
    Motor.Open_loop.Started = 0u;
    Motor.Current_loop.Id_target = 0.0f;
    Motor.Current_loop.Iq_target = 0.0f;
    Motor.Speed_loop.Command_rpm = 0.0f;
    Motor.Speed_loop.Target_rpm = 0.0f;
    Motor.Speed_loop.Iq_output = 0.0f;
    Motor.Position_loop.Target_degree = 0.0f;
    Motor.Position_loop.Speed_output = 0.0f;
    Motor.Position_loop.Travel_degree = 0.0f;
    Motor.Position_loop.Return_mode = MOTOR_POSITION_RETURN_SHORTEST;
    Motor.Position_loop.Track_ready = 0u;
    Motor.Position_loop.In_deadband = 0u;
    Motor.Foc_direction = 1;
    PID_Clear(&Motor.Current_loop.Id_pid);
    PID_Clear(&Motor.Current_loop.Iq_pid);
    PID_Clear(&Motor.Speed_loop.Pid);
    PID_Clear(&Motor.Position_loop.Pid);
    Protocol.Enabled = 0u;
    Protocol.Voice_selected = 0u;
}

/***********************************************
 * @brief : 校验并应用上位机下发的FOC环路参数
 * @param : Payload 44字节参数写入负载
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static void Foc_Protocol_HandleParameterWrite(const uint8 *Payload)
{
    uint16 Current_bandwidth = Foc_Protocol_ReadU16(&Payload[0]);
    float Ramp_rate = Foc_Protocol_ReadFloat(&Payload[4]);
    float Speed_kp = Foc_Protocol_ReadFloat(&Payload[8]);
    float Speed_ki = Foc_Protocol_ReadFloat(&Payload[12]);
    float Speed_integral_limit = Foc_Protocol_ReadFloat(&Payload[16]);
    float Ab_filter_bandwidth = Foc_Protocol_ReadFloat(&Payload[20]);
    float Position_kp = Foc_Protocol_ReadFloat(&Payload[24]);
    float Position_soft_range = Foc_Protocol_ReadFloat(&Payload[28]);
    float Position_speed_deadband = Foc_Protocol_ReadFloat(&Payload[32]);
    float Position_output_limit = Foc_Protocol_ReadFloat(&Payload[36]);
    float Position_deadband = Foc_Protocol_ReadFloat(&Payload[40]);

    /* 控制运行期间不改环路参数，避免PID状态和输出限幅突变。 */
    if ((Protocol.Enabled != 0u) ||
        (Current_bandwidth < FOC_PROTOCOL_CURRENT_BW_MIN_HZ) ||
        (Current_bandwidth > FOC_PROTOCOL_CURRENT_BW_MAX_HZ) ||
        (Ramp_rate != Ramp_rate) ||
        (Ramp_rate < 0.0f) ||
        (Ramp_rate > FOC_PROTOCOL_SPEED_RAMP_MAX) ||
        (Speed_kp != Speed_kp) ||
        (Speed_ki != Speed_ki) ||
        (Speed_integral_limit != Speed_integral_limit) ||
        (Ab_filter_bandwidth != Ab_filter_bandwidth) ||
        (Position_kp != Position_kp) ||
        (Position_soft_range != Position_soft_range) ||
        (Position_speed_deadband != Position_speed_deadband) ||
        (Position_output_limit != Position_output_limit) ||
        (Position_deadband != Position_deadband) ||
        (Speed_kp < 0.0f) ||
        (Speed_kp > FOC_PROTOCOL_LOOP_GAIN_MAX) ||
        (Speed_ki < 0.0f) ||
        (Speed_ki > FOC_PROTOCOL_LOOP_GAIN_MAX) ||
        (Speed_integral_limit < 0.0f) ||
        (Speed_integral_limit > FOC_PROTOCOL_SPEED_INTEGRAL_LIMIT_MAX) ||
        (Ab_filter_bandwidth < MOTOR_AB_FILTER_BW_MIN_HZ) ||
        (Ab_filter_bandwidth > MOTOR_AB_FILTER_BW_MAX_HZ) ||
        (Position_kp < 0.0f) ||
        (Position_kp > FOC_PROTOCOL_LOOP_GAIN_MAX) ||
        (Position_soft_range < 0.0f) ||
        (Position_soft_range > FOC_PROTOCOL_POSITION_SOFT_RANGE_MAX) ||
        (Position_speed_deadband < 0.0f) ||
        (Position_speed_deadband >
         FOC_PROTOCOL_POSITION_SPEED_DEADBAND_MAX) ||
        (Position_output_limit < 0.0f) ||
        (Position_output_limit > FOC_PROTOCOL_POSITION_LIMIT_MAX) ||
        (Position_deadband < 0.0f) ||
        (Position_deadband > FOC_PROTOCOL_POSITION_DEADBAND_MAX))
    {
        return;
    }

    Motor_Control_SetCurrentBandwidth(Current_bandwidth);
    Motor.Speed_loop.Ramp_rate = Ramp_rate;
    Motor_Control_SetSpeedPi(Speed_kp, Speed_ki, Speed_integral_limit);
    Motor_Control_SetPositionKp(Position_kp,
                                Position_output_limit,
                                Position_deadband,
                                Position_soft_range,
                                Position_speed_deadband);
    Motor.Ab_filter_bandwidth = Ab_filter_bandwidth;
    Protocol.Parameters_seen = 1u;
    Foc_Protocol_SendLoopParameters();
}

/***********************************************
 * @brief : 回传当前生效的FOC环路参数
 * @param : 无
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static void Foc_Protocol_SendLoopParameters(void)
{
    uint8 Frame[FOC_PROTOCOL_PARAMETER_LENGTH + 10u];
    uint8 *Payload = &Frame[8];
    uint16 Crc;

    /* 参数读取表示上位机已完成连接同步，可使用当前参数启动有感FOC。 */
    Protocol.Parameters_seen = 1u;
    memset(Frame, 0, sizeof(Frame));
    Frame[0] = 0xaau;
    Frame[1] = 0x55u;
    Frame[2] = FOC_PROTOCOL_VERSION;
    Frame[3] = FOC_PROTOCOL_FRAME_TYPE_PARAMETER_WRITE;
    Foc_Protocol_WriteU16(&Frame[4], Protocol.Tx_sequence++);
    Foc_Protocol_WriteU16(&Frame[6], FOC_PROTOCOL_PARAMETER_LENGTH);
    Foc_Protocol_WriteU16(&Payload[0], Motor.Current_loop.Bandwidth);
    Foc_Protocol_WriteFloat(&Payload[4], Motor.Speed_loop.Ramp_rate);
    Foc_Protocol_WriteFloat(&Payload[8], Motor.Speed_loop.Pid.Kp);
    Foc_Protocol_WriteFloat(&Payload[12], Motor.Speed_loop.Pid.Ki);
    Foc_Protocol_WriteFloat(&Payload[16], Motor.Speed_loop.Integral_limit);
    Foc_Protocol_WriteFloat(&Payload[20], Motor.Ab_filter_bandwidth);
    Foc_Protocol_WriteFloat(&Payload[24], Motor.Position_loop.Pid.Kp);
    Foc_Protocol_WriteFloat(
        &Payload[28],
        Motor.Position_loop.Soft_range_degree);
    Foc_Protocol_WriteFloat(
        &Payload[32],
        Motor.Position_loop.Speed_deadband_rpm);
    Foc_Protocol_WriteFloat(&Payload[36], Motor.Position_loop.Pid.LimMax);
    Foc_Protocol_WriteFloat(&Payload[40], Motor.Position_loop.Deadband_degree);
    Crc = Foc_Protocol_Crc16(&Frame[2],
                             (uint16)(6u + FOC_PROTOCOL_PARAMETER_LENGTH));
    Foc_Protocol_WriteU16(
        &Frame[8u + FOC_PROTOCOL_PARAMETER_LENGTH],
        Crc);
    (void)debug_send_buffer(Frame, (uint32)sizeof(Frame));
}

/***********************************************
 * @brief : 执行一帧有感FOC电流环控制命令
 * @param : Payload 16字节控制命令负载
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static void Foc_Protocol_HandleEncoderFoc(const uint8 *Payload)
{
    uint8 Flags = Payload[1];
    uint8 Foc_mode = Payload[2];
    int8 Direction = ((Flags & 0x02u) != 0u) ? -1 : 1;
    float Primary_target = Foc_Protocol_ReadFloat(&Payload[4]);
    float Id_target = Foc_Protocol_ReadFloat(&Payload[8]);
    float Ramp_rate = Motor.Speed_loop.Ramp_rate;
    uint16 Bandwidth = Motor.Current_loop.Bandwidth;

    if (Foc_mode == (uint8)MOTOR_FOC_CURRENT)
    {
        Bandwidth = Foc_Protocol_ReadU16(&Payload[12]);
    }
    else if (Foc_mode == (uint8)MOTOR_FOC_SPEED)
    {
        Ramp_rate = Foc_Protocol_ReadFloat(&Payload[12]);
    }

    if ((Foc_mode < (uint8)MOTOR_FOC_CURRENT) ||
        (Foc_mode > (uint8)MOTOR_FOC_POSITION) ||
        (Primary_target != Primary_target) ||
        (Id_target != Id_target) ||
        ((Foc_mode == (uint8)MOTOR_FOC_SPEED) &&
         ((Ramp_rate != Ramp_rate) ||
          (Ramp_rate < FOC_PROTOCOL_SPEED_RAMP_MIN) ||
          (Ramp_rate > FOC_PROTOCOL_SPEED_RAMP_MAX))) ||
        (Motor.Zero_ready == 0u) ||
        ((Foc_mode == (uint8)MOTOR_FOC_CURRENT) &&
         ((Bandwidth < FOC_PROTOCOL_CURRENT_BW_MIN_HZ) ||
          (Bandwidth > FOC_PROTOCOL_CURRENT_BW_MAX_HZ))) ||
        (Protocol.Parameters_seen == 0u))
    {
        Foc_Protocol_StopControl();
        return;
    }

    if (Protocol.Voice_selected != 0u)
    {
        Foc_voice_Stop();
    }

    Id_target = Float_Limit(Id_target, -50.0f, 50.0f);
    if (Foc_mode == (uint8)MOTOR_FOC_CURRENT)
    {
        Motor_Control_SetCurrentBandwidth(Bandwidth);
    }
    Motor.Current_loop.Id_target = Id_target;
    Motor.Foc_direction = Direction;
    if (Foc_mode == (uint8)MOTOR_FOC_CURRENT)
    {
        Motor.Current_loop.Iq_target =
            Float_Limit(Primary_target, -100.0f, 100.0f) *
            (float)Direction;
    }
    else if (Foc_mode == (uint8)MOTOR_FOC_SPEED)
    {
        Motor.Speed_loop.Command_rpm =
            Float_Limit(Primary_target, -50000.0f, 50000.0f) *
            (float)Direction;
        Motor.Speed_loop.Ramp_rate = Ramp_rate;
        Motor.Current_loop.Iq_target = 0.0f;
    }
    else
    {
        Motor.Position_loop.Target_degree =
            Float_Limit(Primary_target, 0.0f, 360.0f);
        Motor.Position_loop.Return_mode =
            ((Flags & 0x02u) != 0u) ?
            MOTOR_POSITION_RETURN_REVERSE_PATH :
            MOTOR_POSITION_RETURN_SHORTEST;
        Motor.Current_loop.Iq_target = 0.0f;
    }
    Motor.Foc_mode = (Motor_foc_mode_t)Foc_mode;
    Motor.Control_mode = MOTOR_CONTROL_ENCODER_FOC;
    Protocol.Enabled = 1u;
    Protocol.Voice_selected = 0u;
}

/***********************************************
 * @brief : 执行一帧开环控制命令中的目标参数
 * @param : Payload 16字节控制命令负载
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void Foc_Protocol_HandleOpenLoop(const uint8 *Payload)
{
    uint8 Flags = Payload[1];
    int8 Direction = ((Flags & 0x02u) != 0u) ? -1 : 1;
    float Uq_target = Foc_Protocol_ReadFloat(&Payload[4]);
    float Start_angle_target = Foc_Protocol_ReadFloat(&Payload[8]);
    float Step_target = Foc_Protocol_ReadFloat(&Payload[12]);
    int32 Start_angle_value;
    int32 Step_value;
    uint16 Start_angle;
    uint8 Need_start;

    /* NaN不等于自身，用于拒绝异常浮点参数。 */
    if ((Uq_target != Uq_target) ||
        (Start_angle_target != Start_angle_target) ||
        (Step_target != Step_target))
    {
        Foc_Protocol_StopControl();
        return;
    }

    if (Protocol.Voice_selected != 0u)
    {
        Foc_voice_Stop();
    }

    Uq_target = Float_Limit(Uq_target,
                            -FOC_PROTOCOL_UQ_LIMIT,
                            FOC_PROTOCOL_UQ_LIMIT);
    Start_angle_value = (Start_angle_target >= 0.0f) ?
                        (int32)(Start_angle_target + 0.5f) :
                        (int32)(Start_angle_target - 0.5f);
    Start_angle_value = Int_Limit(Start_angle_value, -32768, 32767);
    Start_angle = Angle_Wrap(Start_angle_value);

    Step_target = Float_Limit(Step_target, -32768.0f, 32767.0f);
    Step_value = (Step_target >= 0.0f) ?
                 (int32)(Step_target + 0.5f) :
                 (int32)(Step_target - 0.5f);
    Step_value *= (int32)Direction;
    Step_value = Int_Limit(Step_value, -32768, 32767);

    Need_start = ((Protocol.Enabled == 0u) ||
                  (Protocol.Start_angle != Start_angle) ||
                  (Motor.Open_loop.Started == 0u)) ? 1u : 0u;

    Protocol.Enabled = 1u;
    Protocol.Voice_selected = 0u;
    Protocol.Start_angle = Start_angle;
    Motor.Control_mode = MOTOR_CONTROL_OPEN_LOOP;
    Motor.Open_loop.Uq = Uq_target;
    Motor.Open_loop.Step = (int16)Step_value;

    if (Need_start != 0u)
    {
        Motor.Open_loop.Angle = Start_angle;
        Motor.Open_loop.Hold_count = 0u;
        Motor.Open_loop.Started = 1u;
    }
}

/***********************************************
 * @brief : 执行一帧音乐播放命令中的曲目参数
 * @param : Payload 16字节控制命令负载
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void Foc_Protocol_HandleVoice(const uint8 *Payload)
{
    uint8 Song_id = Payload[3];
    uint32 Session = Foc_Protocol_ReadU32(&Payload[4]);
    uint8 Need_start;

    if ((Song_id == 0u) || (Song_id > Foc_voice_GetSongCount()))
    {
        Foc_Protocol_StopControl();
        return;
    }

    Need_start = ((Protocol.Enabled == 0u) ||
                  (Protocol.Voice_selected == 0u) ||
                  (Protocol.Song_id != Song_id) ||
                  (Protocol.Voice_session != Session)) ? 1u : 0u;

    Protocol.Enabled = 1u;
    Protocol.Voice_selected = 1u;
    Protocol.Song_id = Song_id;
    Protocol.Voice_session = Session;

    if ((Need_start != 0u) && (Foc_voice_StartSong(Song_id) == 0u))
    {
        Foc_Protocol_StopControl();
    }
}

/***********************************************
 * @brief : 按驱动模式执行一帧电机控制命令
 * @param : Payload 16字节控制命令负载
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void Foc_Protocol_HandleControl(const uint8 *Payload)
{
    uint8 Drive_mode = Payload[0];
    uint8 Flags = Payload[1];
    uint8 Enable = (uint8)(Flags & 0x01u);
    uint8 Emergency = (uint8)(Flags & 0x04u);

    Protocol.Control_seen = 1u;
    Protocol.Last_control_ms = Protocol.Time_ms;

    if ((Emergency != 0u) || (Enable == 0u))
    {
        Foc_Protocol_StopControl();
        return;
    }

    if (Drive_mode == FOC_PROTOCOL_DRIVE_MODE_OPEN_LOOP)
    {
        Foc_Protocol_HandleOpenLoop(Payload);
    }
    else if (Drive_mode == FOC_PROTOCOL_DRIVE_MODE_ENCODER_FOC)
    {
        Foc_Protocol_HandleEncoderFoc(Payload);
    }
    else if (Drive_mode == FOC_PROTOCOL_DRIVE_MODE_VOICE)
    {
        Foc_Protocol_HandleVoice(Payload);
    }
    else
    {
        Foc_Protocol_StopControl();
    }
}

/***********************************************
 * @brief : 打包并发送一帧基础电机遥测
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void Foc_Protocol_SendTelemetry(void)
{
    uint8 Frame[FOC_PROTOCOL_TELEMETRY_LENGTH + 10u];
    uint8 *Payload = &Frame[8];
    uint16 Crc;
    float Speed_target;
    float Mechanical_angle;
    float Electrical_angle;

    memset(Frame, 0, sizeof(Frame));
    Frame[0] = 0xaau;
    Frame[1] = 0x55u;
    Frame[2] = FOC_PROTOCOL_VERSION;
    Frame[3] = FOC_PROTOCOL_FRAME_TYPE_TELEMETRY;
    Foc_Protocol_WriteU16(&Frame[4], Protocol.Tx_sequence++);
    Foc_Protocol_WriteU16(&Frame[6], FOC_PROTOCOL_TELEMETRY_LENGTH);

    if (Motor.Control_mode == MOTOR_CONTROL_ENCODER_FOC)
    {
        if (Motor.Foc_mode == MOTOR_FOC_POSITION)
        {
            Speed_target = Motor.Speed_loop.Target_rpm;
        }
        else if (Motor.Foc_mode == MOTOR_FOC_SPEED)
        {
            Speed_target = Motor.Speed_loop.Target_rpm;
        }
        else
        {
            Speed_target = 0.0f;
        }
    }
    else if (Motor.Pole_pairs == 0u)
    {
        Speed_target = 0.0f;
    }
    else
    {
        Speed_target = (float)Motor.Open_loop.Step *
                       FOC_PROTOCOL_CONTROL_HZ * 60.0f /
                       ((float)ANGLE_PERIOD * (float)Motor.Pole_pairs);
    }
    Mechanical_angle = Motor_Control_GetMechanicalDegree();
    Electrical_angle = (float)Motor.Encoder.Electrical_angle *
                       360.0f / (float)ANGLE_PERIOD;

    Payload[0] = (Protocol.Enabled != 0u) ? 1u : 0u;
    Payload[1] = (uint8)Motor.Control_mode;
    Payload[2] = 0u;
    Payload[3] = (Current.calibrated != 0u) ? 0x02u : 0u;
    if (Protocol.Enabled != 0u)
    {
        Payload[3] |= 0x01u;
    }
    if (Foc_voice_IsPlaying() != 0u)
    {
        Payload[3] |= FOC_PROTOCOL_STATUS_MUSIC_PLAYING;
    }
    Foc_Protocol_WriteU32(&Payload[4], Protocol.Time_ms);
    Foc_Protocol_WriteFloat(&Payload[8], Speed_target);
    Foc_Protocol_WriteFloat(&Payload[12], Motor.Encoder.Spd_rpm);
    Foc_Protocol_WriteFloat(
        &Payload[16],
        Motor.Current_loop.Id_target);
    Foc_Protocol_WriteFloat(&Payload[20], Current.park.Id);
    Foc_Protocol_WriteFloat(
        &Payload[24],
        Motor.Current_loop.Iq_target);
    Foc_Protocol_WriteFloat(&Payload[28], Current.park.Iq);
    Foc_Protocol_WriteFloat(&Payload[32], SVPWM.VBUS);
    Foc_Protocol_WriteFloat(&Payload[36], (float)Motor.Encoder.Zero_offset);
    Foc_Protocol_WriteFloat(&Payload[40], Mechanical_angle);
    Foc_Protocol_WriteFloat(&Payload[44], Electrical_angle);
    Foc_Protocol_WriteFloat(&Payload[48], Torque.Motor_torque);
    Foc_Protocol_WriteU16(&Payload[52], Current.adc_raw_u);
    Foc_Protocol_WriteU16(&Payload[54], Current.adc_raw_w);

    Crc = Foc_Protocol_Crc16(&Frame[2], (uint16)(6u + FOC_PROTOCOL_TELEMETRY_LENGTH));
    Foc_Protocol_WriteU16(&Frame[8u + FOC_PROTOCOL_TELEMETRY_LENGTH], Crc);
    (void)debug_send_buffer(Frame, (uint32)sizeof(Frame));
}

/***********************************************
 * @brief : 打包并发送一帧高速电流和PWM波形
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void Foc_Protocol_SendWaveform(void)
{
    uint8 Frame[FOC_PROTOCOL_WAVEFORM_LENGTH + 10u];
    uint8 *Payload = &Frame[8];
    uint16 Crc;

    memset(Frame, 0, sizeof(Frame));
    Frame[0] = 0xaau;
    Frame[1] = 0x55u;
    Frame[2] = FOC_PROTOCOL_VERSION;
    Frame[3] = FOC_PROTOCOL_FRAME_TYPE_WAVEFORM;
    Foc_Protocol_WriteU16(&Frame[4], Protocol.Tx_sequence++);
    Foc_Protocol_WriteU16(&Frame[6], FOC_PROTOCOL_WAVEFORM_LENGTH);

    Foc_Protocol_WriteU32(&Payload[0], Protocol.Time_ms);
    Foc_Protocol_WriteFloat(&Payload[4], Current.current_u);
    Foc_Protocol_WriteFloat(&Payload[8], Current.current_v);
    Foc_Protocol_WriteFloat(&Payload[12], Current.current_w);
    Foc_Protocol_WriteFloat(&Payload[16], Motor.Current_loop.Ud_output);
    Foc_Protocol_WriteFloat(&Payload[20], Motor.Current_loop.Uq_output);
    Foc_Protocol_WriteFloat(&Payload[24], (float)SVPWM.DutyA * 100.0f / (float)SVPWM_DUTY_MAX);
    Foc_Protocol_WriteFloat(&Payload[28], (float)SVPWM.DutyB * 100.0f / (float)SVPWM_DUTY_MAX);
    Foc_Protocol_WriteFloat(&Payload[32], (float)SVPWM.DutyC * 100.0f / (float)SVPWM_DUTY_MAX);

    Crc = Foc_Protocol_Crc16(&Frame[2], (uint16)(6u + FOC_PROTOCOL_WAVEFORM_LENGTH));
    Foc_Protocol_WriteU16(&Frame[8u + FOC_PROTOCOL_WAVEFORM_LENGTH], Crc);
    (void)debug_send_buffer(Frame, (uint32)sizeof(Frame));
}

/***********************************************
 * @brief : 逐首发送下位机内置乐曲编号和UTF-8名称
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void Foc_Protocol_SendSongList(void)
{
    uint8 Frame[FOC_PROTOCOL_FRAME_MAX];
    uint8 *Payload = &Frame[8];
    const char *Song_name;
    uint32 Name_length;
    uint16 Payload_length;
    uint16 Crc;
    uint8 Song_count = Foc_voice_GetSongCount();
    uint8 Song_id;

    for (Song_id = 1u; Song_id <= Song_count; Song_id++)
    {
        Song_name = Foc_voice_GetSongName(Song_id);
        if (Song_name == NULL)
        {
            continue;
        }

        Name_length = (uint32)strlen(Song_name);
        if (Name_length > FOC_PROTOCOL_SONG_NAME_MAX)
        {
            Name_length = FOC_PROTOCOL_SONG_NAME_MAX;
            while ((Name_length > 0u) &&
                   (((uint8)Song_name[Name_length] & 0xc0u) == 0x80u))
            {
                Name_length--;
            }
        }
        Payload_length = (uint16)(3u + Name_length);

        memset(Frame, 0, sizeof(Frame));
        Frame[0] = 0xaau;
        Frame[1] = 0x55u;
        Frame[2] = FOC_PROTOCOL_VERSION;
        Frame[3] = FOC_PROTOCOL_FRAME_TYPE_SONG_LIST;
        Foc_Protocol_WriteU16(&Frame[4], Protocol.Tx_sequence++);
        Foc_Protocol_WriteU16(&Frame[6], Payload_length);

        Payload[0] = Song_id;
        Payload[1] = Song_count;
        Payload[2] = (uint8)Name_length;
        memcpy(&Payload[3], Song_name, Name_length);

        Crc = Foc_Protocol_Crc16(&Frame[2],
                                 (uint16)(6u + Payload_length));
        Foc_Protocol_WriteU16(&Frame[8u + Payload_length], Crc);
        (void)debug_send_buffer(Frame, (uint32)(Payload_length + 10u));
    }
}

/***********************************************
 * @brief : 校验并分发一帧FOC-UART报文
 * @param : Frame 完整帧地址
 * @param : Length 完整帧长度
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void Foc_Protocol_HandleFrame(const uint8 *Frame, uint16 Length)
{
    uint16 Payload_length = Foc_Protocol_ReadU16(&Frame[6]);
    uint16 Received_crc;
    uint16 Calculated_crc;

    if ((Length != (uint16)(Payload_length + 10u)) ||
        (Frame[2] != FOC_PROTOCOL_VERSION))
    {
        return;
    }

    Received_crc = Foc_Protocol_ReadU16(&Frame[8u + Payload_length]);
    Calculated_crc = Foc_Protocol_Crc16(&Frame[2],
                                        (uint16)(6u + Payload_length));
    if (Received_crc != Calculated_crc)
    {
        return;
    }

    if ((Frame[3] == FOC_PROTOCOL_FRAME_TYPE_PARAMETER_READ) &&
        (Payload_length == 0u))
    {
        Foc_Protocol_SendLoopParameters();
    }
    else if ((Frame[3] == FOC_PROTOCOL_FRAME_TYPE_CONTROL) &&
             (Payload_length == FOC_PROTOCOL_CONTROL_LENGTH))
    {
        Foc_Protocol_HandleControl(&Frame[8]);
    }
    else if ((Frame[3] == FOC_PROTOCOL_FRAME_TYPE_PARAMETER_WRITE) &&
             (Payload_length == FOC_PROTOCOL_PARAMETER_LENGTH))
    {
        Foc_Protocol_HandleParameterWrite(&Frame[8]);
    }
    else if ((Frame[3] == FOC_PROTOCOL_FRAME_TYPE_SONG_LIST) &&
             (Payload_length == 0u))
    {
        Foc_Protocol_SendSongList();
    }
    else if ((Frame[3] == FOC_PROTOCOL_FRAME_TYPE_ZERO_CAL) &&
             (Payload_length == 0u))
    {
        Foc_Protocol_StopControl();
        Zero_Calibration();
    }
}

/***********************************************
 * @brief : 向流式解析器输入一个串口字节
 * @param : Data 新收到的串口字节
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void Foc_Protocol_ParseByte(uint8 Data)
{
    uint16 Payload_length;

    if (Protocol.Parser.Length == 0u)
    {
        if (Data == 0xaau)
        {
            Protocol.Parser.Data[0] = Data;
            Protocol.Parser.Length = 1u;
        }
        return;
    }

    if (Protocol.Parser.Length == 1u)
    {
        if (Data == 0x55u)
        {
            Protocol.Parser.Data[1] = Data;
            Protocol.Parser.Length = 2u;
        }
        else if (Data != 0xaau)
        {
            Protocol.Parser.Length = 0u;
        }
        return;
    }

    Protocol.Parser.Data[Protocol.Parser.Length] = Data;
    Protocol.Parser.Length++;

    if (Protocol.Parser.Length == 8u)
    {
        Payload_length = Foc_Protocol_ReadU16(&Protocol.Parser.Data[6]);
        Protocol.Parser.Expected_length = (uint16)(Payload_length + 10u);
        if (Protocol.Parser.Expected_length > FOC_PROTOCOL_FRAME_MAX)
        {
            Protocol.Parser.Length = 0u;
            Protocol.Parser.Expected_length = 0u;
        }
    }

    if ((Protocol.Parser.Expected_length != 0u) &&
        (Protocol.Parser.Length >= Protocol.Parser.Expected_length))
    {
        Foc_Protocol_HandleFrame(Protocol.Parser.Data,
                                 Protocol.Parser.Expected_length);
        Protocol.Parser.Length = 0u;
        Protocol.Parser.Expected_length = 0u;
    }
}

void Foc_Protocol_Init(void)
{
    debug_init();
    memset(&Protocol, 0, sizeof(Protocol));
}

void Foc_Protocol_Service(void)
{
    uint8 Receive_data[FOC_PROTOCOL_FRAME_MAX];
    uint32 Receive_length;
    uint32 Index;
    uint32 Current_ms;

    do
    {
        Receive_length = debug_read_ring_buffer(
            Receive_data,
            (uint32)sizeof(Receive_data));
        for (Index = 0u; Index < Receive_length; Index++)
        {
            Foc_Protocol_ParseByte(Receive_data[Index]);
        }
    }
    while (Receive_length != 0u);

    Current_ms = Protocol.Time_ms;

    if ((Protocol.Control_seen != 0u) &&
        ((uint32)(Current_ms - Protocol.Last_control_ms) >
         FOC_PROTOCOL_TIMEOUT_MS))
    {
        Foc_Protocol_StopControl();
        Protocol.Control_seen = 0u;
    }

    if ((Protocol.Control_seen != 0u) &&
        ((uint32)(Current_ms - Protocol.Last_telemetry_ms) >=
         FOC_PROTOCOL_TELEMETRY_PERIOD_MS))
    {
        Protocol.Last_telemetry_ms = Current_ms;
        Foc_Protocol_SendTelemetry();
    }

    if ((Protocol.Control_seen != 0u) &&
        ((uint32)(Current_ms - Protocol.Last_waveform_ms) >=
         FOC_PROTOCOL_WAVEFORM_PERIOD_MS))
    {
        Protocol.Last_waveform_ms = Current_ms;
        Foc_Protocol_SendWaveform();
    }
}

void Foc_Protocol_Tick1ms(void)
{
    Protocol.Time_ms++;
}
