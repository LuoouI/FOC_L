#include "Foc_Protocol.h"
#include "Current_sample/Current_sample.h"
#include "Foc_voice/Foc_voice.h"
#include "Motor_Control/Motor_Control.h"
#include "Motor_Torque/Motor_Torque.h"
#include "SVPWM/SVPWM.h"

static Foc_Protocol_t Protocol;
static void Foc_Protocol_SendLoopParameters(void);
static void Foc_Protocol_SendObserverParameters(void);
static void Foc_Protocol_HandleObserverParameterWrite(const uint8 *Payload);
static void Foc_Protocol_SendObserverWaveform(void);
static uint8 Foc_Protocol_CountObserverStreamFields(uint16 Mask);
static uint16 Foc_Protocol_AppendObserverStreamFloat(uint8 *Payload,
                                                     uint16 Offset,
                                                     uint16 Field_mask,
                                                     float Value);
static uint8 Foc_Protocol_SendObserverStreamBatch(void);
static void Foc_Protocol_HandleObserverStreamConfig(const uint8 *Payload,
                                                     uint16 Payload_length);
static void Foc_Protocol_HandleReset(void);

/***********************************************
 * @brief : 计算实测Iq与SMO估算Iq的差值
 * @param : 无
 * @return: 实测Iq减去SMO估算Iq，单位为A
 * @date  : 2026-09-14
 * @author: L
 ************************************************/
static float Foc_Protocol_CalculateObserverIqError(void)
{
    Clark_t Observer_clark;
    Park_t Actual_park;
    Park_t Observer_park;

    /* 使用同一个编码器电角度，保证实测值与估算值位于相同dq坐标系。 */
    Actual_park = foc_park_calc(
        Current.clark,
        Motor.Encoder.Electrical_angle);

    Observer_clark.Alpha = Motor.SMO.I_alpha_est;
    Observer_clark.Beta = Motor.SMO.I_beta_est;
    Observer_park = foc_park_calc(
        Observer_clark,
        Motor.Encoder.Electrical_angle);

    return Actual_park.Iq - Observer_park.Iq;
}

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
 * @brief : 控制帧超时时快速撤销电压输出并安排完整停机清理
 * @param : 无
 * @return: 无
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
static void Foc_Protocol_ForceTimeoutSafeOutput(void)
{
    /* 该函数在1 ms中断中执行，只修改停机必需状态，完整清理由主循环完成。 */
    Motor.Control_mode = MOTOR_CONTROL_OPEN_LOOP;
    Motor.Open_loop.Uq = 0.0f;
    Motor.Open_loop.Step = 0;
    Motor.Open_loop.Hold_count = 0u;
    Motor.Open_loop.Started = 0u;
    Motor.Current_loop.Id_target = 0.0f;
    Motor.Current_loop.Iq_target = 0.0f;
    Motor.Speed_loop.Command_rpm = 0.0f;
    Motor.Speed_loop.Target_rpm = 0.0f;
    Protocol.Enabled = 0u;
    Protocol.Voice_selected = 0u;
    Protocol.Rearm_required = 1u;
    Protocol.Timeout_cleanup_pending = 1u;
    Protocol.Observer_sample_read = Protocol.Observer_sample_write;
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
    Motor_Control_ResetObserver();
    Protocol.Enabled = 0u;
    Protocol.Voice_selected = 0u;
}

/***********************************************
 * @brief : 停止控制输出并触发CM4系统软件复位
 * @param : 无
 * @return: 无，不返回
 * @date  : 2026-09-14
 * @author: L
 ************************************************/
static void Foc_Protocol_HandleReset(void)
{
    /* 先清除控制目标，避免复位前仍由控制中断输出非零电压。 */
    Foc_Protocol_StopControl();
    NVIC_SystemReset();
}

/***********************************************
 * @brief : 校验并应用上位机下发的FOC环路参数，支持运行中更新
 * @param : Payload 44字节参数写入负载
 * @return: 无
 * @date  : 2026-09-14
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
    uint32 Irq_state;                         /* 参数组更新期间的中断状态 */

    if ((Current_bandwidth < PID_BANDWIDTH_MIN_HZ) ||
        (Current_bandwidth > PID_BANDWIDTH_MAX_HZ) ||
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

    /* 参数组在控制中断看来必须同时生效，避免拖动期间读到新旧混合配置。 */
    Irq_state = interrupt_global_disable();
    Motor_Control_SetCurrentBandwidth(Current_bandwidth);
    Motor.Speed_loop.Ramp_rate = Ramp_rate;
    Motor_Control_SetSpeedPi(Speed_kp, Speed_ki, Speed_integral_limit);
    Motor_Control_SetPositionKp(Position_kp,
                                Position_output_limit,
                                Position_deadband,
                                Position_soft_range,
                                 Position_speed_deadband);
    Motor.Ab_filter_bandwidth = Ab_filter_bandwidth;
    interrupt_global_enable(Irq_state);
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
 * @brief : 回传当前生效的SMO和PLL参数
 * @param : 无
 * @return: 无
 * @date  : 2026-09-14
 * @author: L
 ************************************************/
static void Foc_Protocol_SendObserverParameters(void)
{
    uint8 Frame[FOC_PROTOCOL_OBSERVER_PARAMETER_LENGTH + 10u];
    uint8 *Payload = &Frame[8];
    uint16 Crc;

    memset(Frame, 0, sizeof(Frame));
    Frame[0] = 0xaau;
    Frame[1] = 0x55u;
    Frame[2] = FOC_PROTOCOL_VERSION;
    Frame[3] = FOC_PROTOCOL_FRAME_TYPE_OBSERVER_PARAMETER_WRITE;
    Foc_Protocol_WriteU16(&Frame[4], Protocol.Tx_sequence++);
    Foc_Protocol_WriteU16(
        &Frame[6],
        FOC_PROTOCOL_OBSERVER_PARAMETER_LENGTH);

    Foc_Protocol_WriteFloat(&Payload[0], Motor.SMO.K_slide);
    Foc_Protocol_WriteFloat(
        &Payload[4],
        Motor.SMO.Boundary_current);
    Foc_Protocol_WriteFloat(
        &Payload[8],
        Motor.SMO.Filter_bandwidth);
    Foc_Protocol_WriteFloat(&Payload[12], Motor.PLL.Bandwidth);

    Crc = Foc_Protocol_Crc16(
        &Frame[2],
        (uint16)(6u + FOC_PROTOCOL_OBSERVER_PARAMETER_LENGTH));
    Foc_Protocol_WriteU16(
        &Frame[8u + FOC_PROTOCOL_OBSERVER_PARAMETER_LENGTH],
        Crc);
    (void)debug_send_buffer(Frame, (uint32)sizeof(Frame));
}

/***********************************************
 * @brief : 校验并应用上位机下发的SMO和PLL参数，支持运行中更新
 * @param : Payload 16字节观测器参数写入负载
 * @return: 无
 * @date  : 2026-09-14
 * @author: L
 ************************************************/
static void Foc_Protocol_HandleObserverParameterWrite(const uint8 *Payload)
{
    float Smo_gain = Foc_Protocol_ReadFloat(&Payload[0]);
    float Smo_boundary_current = Foc_Protocol_ReadFloat(&Payload[4]);
    float Smo_filter_bandwidth = Foc_Protocol_ReadFloat(&Payload[8]);
    float Pll_bandwidth = Foc_Protocol_ReadFloat(&Payload[12]);
    uint32 Irq_state;                         /* 参数组更新期间的中断状态 */
    uint8 Observer_was_active;                /* 写入前的SMO有效状态 */
    uint8 Observer_is_active;                 /* 写入后的SMO有效状态 */

    if ((Smo_gain != Smo_gain) ||
        (Smo_boundary_current != Smo_boundary_current) ||
        (Smo_filter_bandwidth != Smo_filter_bandwidth) ||
        (Pll_bandwidth != Pll_bandwidth) ||
        (Smo_gain < 0.0f) ||
        (Smo_gain > FOC_PROTOCOL_SMO_GAIN_MAX) ||
        (Smo_boundary_current < 0.0f) ||
        (Smo_boundary_current > FOC_PROTOCOL_SMO_BOUNDARY_CURRENT_MAX) ||
        (Smo_filter_bandwidth < MOTOR_SMO_FILTER_BW_MIN_HZ) ||
        (Smo_filter_bandwidth > MOTOR_SMO_FILTER_BW_MAX_HZ) ||
        (Pll_bandwidth < MOTOR_PLL_BW_MIN_HZ) ||
        (Pll_bandwidth > MOTOR_PLL_BW_MAX_HZ))
    {
        return;
    }

    Observer_was_active =
        ((Motor.SMO.K_slide > 0.0f) &&
         (Motor.SMO.Boundary_current > 0.0f) &&
         (Motor.SMO.Filter_bandwidth > 0.0f)) ? 1u : 0u;
    Observer_is_active =
        ((Smo_gain > 0.0f) &&
         (Smo_boundary_current > 0.0f) &&
         (Smo_filter_bandwidth > 0.0f)) ? 1u : 0u;

    /* 在线调参时保留估算电流，首次启用或停机调参时重新建立观测器状态。 */
    Irq_state = interrupt_global_disable();
    Motor.SMO.K_slide = Smo_gain;
    Motor.SMO.Boundary_current = Smo_boundary_current;
    Motor.SMO.Filter_bandwidth = Smo_filter_bandwidth;
    PLL_SetBandwidth(&Motor.PLL, Pll_bandwidth);

    if ((Protocol.Enabled == 0u) ||
        (Observer_was_active == 0u) ||
        (Observer_is_active == 0u))
    {
        Motor_Control_ResetObserver();
    }
    interrupt_global_enable(Irq_state);
    Foc_Protocol_SendObserverParameters();
}

/***********************************************
 * @brief : 执行一帧有感或无感FOC控制命令
 * @param : Payload 16字节控制命令负载
 * @param : Control_mode 下位机使用的FOC控制模式
 * @return: 无
 * @date  : 2026-09-13
 * @author: L
 ************************************************/
static void Foc_Protocol_HandleFocCommon(const uint8 *Payload,
                                         Motor_control_mode_t Control_mode)
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
        ((Control_mode == MOTOR_CONTROL_ENCODER_FOC) &&
         (Motor.Zero_ready == 0u)) ||
        ((Foc_mode == (uint8)MOTOR_FOC_CURRENT) &&
         ((Bandwidth < PID_BANDWIDTH_MIN_HZ) ||
          (Bandwidth > PID_BANDWIDTH_MAX_HZ))) ||
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
    Motor.Control_mode = Control_mode;
    Protocol.Enabled = 1u;
    Protocol.Voice_selected = 0u;
}

/***********************************************
 * @brief : 执行一帧有感FOC电流环控制命令
 * @param : Payload 16字节控制命令负载
 * @return: 无
 * @date  : 2026-09-13
 * @author: L
 ************************************************/
static void Foc_Protocol_HandleEncoderFoc(const uint8 *Payload)
{
    Foc_Protocol_HandleFocCommon(Payload, MOTOR_CONTROL_ENCODER_FOC);
}

/***********************************************
 * @brief : 执行一帧无感FOC观测调试控制命令
 * @param : Payload 16字节控制命令负载
 * @return: 无
 * @date  : 2026-09-13
 * @author: L
 ************************************************/
static void Foc_Protocol_HandleSensorlessFoc(const uint8 *Payload)
{
    Foc_Protocol_HandleFocCommon(Payload, MOTOR_CONTROL_SENSORLESS_FOC);
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

    Protocol.Last_control_ms = Protocol.Time_ms;
    Protocol.Control_seen = 1u;
    Protocol.Control_timed_out = 0u;

    if (Emergency != 0u)
    {
        /* 急停命令在板端锁存，复位前禁止后续使能帧重新启动电机。 */
        Protocol.Rearm_required = 1u;
        Foc_Protocol_StopControl();
        return;
    }

    if (Enable == 0u)
    {
        /* 收到明确的失能命令后，允许后续控制帧重新使能电机。 */
        Protocol.Rearm_required = 0u;
        Foc_Protocol_StopControl();
        return;
    }

    if (Protocol.Rearm_required != 0u)
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
    else if (Drive_mode == FOC_PROTOCOL_DRIVE_MODE_SENSORLESS_FOC)
    {
        Foc_Protocol_HandleSensorlessFoc(Payload);
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

    if ((Motor.Control_mode == MOTOR_CONTROL_ENCODER_FOC) ||
        (Motor.Control_mode == MOTOR_CONTROL_SENSORLESS_FOC))
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
                       (float)MOTOR_CURRENT_LOOP_HZ * 60.0f /
                       ((float)ANGLE_PERIOD * (float)Motor.Pole_pairs);
    }
    /* 无感模式仍回传编码器实测角度，便于与PLL估算角度直接对照。 */
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
    /* 无感模式保留编码器速度，仅供上位机与估算转速对照。 */
    Foc_Protocol_WriteFloat(
        &Payload[12],
        Motor.Encoder.Spd_rpm);
    Foc_Protocol_WriteFloat(
        &Payload[16],
        Motor.Current_loop.Id_target);
    Foc_Protocol_WriteFloat(
        &Payload[20],
        (Motor.Control_mode == MOTOR_CONTROL_SENSORLESS_FOC) ?
        0.0f : Current.park.Id);
    Foc_Protocol_WriteFloat(
        &Payload[24],
        Motor.Current_loop.Iq_target);
    Foc_Protocol_WriteFloat(
        &Payload[28],
        (Motor.Control_mode == MOTOR_CONTROL_SENSORLESS_FOC) ?
        0.0f : Current.park.Iq);
    Foc_Protocol_WriteFloat(&Payload[32], SVPWM.VBUS);
    Foc_Protocol_WriteFloat(
        &Payload[36],
        (Motor.Control_mode == MOTOR_CONTROL_SENSORLESS_FOC) ?
        0.0f : (float)Motor.Encoder.Zero_offset);
    Foc_Protocol_WriteFloat(&Payload[40], Mechanical_angle);
    Foc_Protocol_WriteFloat(&Payload[44], Electrical_angle);
    Foc_Protocol_WriteFloat(
        &Payload[48],
        (Motor.Control_mode == MOTOR_CONTROL_SENSORLESS_FOC) ?
        0.0f : Torque.Motor_torque);
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
 * @brief : 打包并发送SMO和PLL观测器波形及Iq误差
 * @param : 无
 * @return: 无
 * @date  : 2026-09-14
 * @author: L
 ************************************************/
static void Foc_Protocol_SendObserverWaveform(void)
{
    uint8 Frame[FOC_PROTOCOL_OBSERVER_WAVEFORM_LENGTH + 10u];
    uint8 *Payload = &Frame[8];
    uint16 Crc;
    float Mechanical_angle;
    float Electrical_angle;
    float Phase_error_degree;
    float Iq_error;

    memset(Frame, 0, sizeof(Frame));
    Frame[0] = 0xaau;
    Frame[1] = 0x55u;
    Frame[2] = FOC_PROTOCOL_VERSION;
    Frame[3] = FOC_PROTOCOL_FRAME_TYPE_OBSERVER_WAVEFORM;
    Foc_Protocol_WriteU16(&Frame[4], Protocol.Tx_sequence++);
    Foc_Protocol_WriteU16(
        &Frame[6],
        FOC_PROTOCOL_OBSERVER_WAVEFORM_LENGTH);

    Mechanical_angle = (float)Motor.PLL.Mechanical_angle_est *
                       360.0f / (float)ANGLE_PERIOD;
    Electrical_angle = (float)Motor.PLL.Electrical_angle_est *
                       360.0f / (float)ANGLE_PERIOD;
    Phase_error_degree = Motor.PLL.Phase_error *
                         360.0f / TWO_PI;
    Iq_error = Foc_Protocol_CalculateObserverIqError();

    Foc_Protocol_WriteU32(&Payload[0], Protocol.Time_ms);
    Foc_Protocol_WriteFloat(&Payload[4], Motor.SMO.I_alpha_est);
    Foc_Protocol_WriteFloat(&Payload[8], Motor.SMO.I_beta_est);
    Foc_Protocol_WriteFloat(&Payload[12], Motor.SMO.E_alpha);
    Foc_Protocol_WriteFloat(&Payload[16], Motor.SMO.E_beta);
    Foc_Protocol_WriteFloat(
        &Payload[20],
        Motor.SMO.E_alpha_filter);
    Foc_Protocol_WriteFloat(
        &Payload[24],
        Motor.SMO.E_beta_filter);
    Foc_Protocol_WriteFloat(&Payload[28], Mechanical_angle);
    Foc_Protocol_WriteFloat(&Payload[32], Electrical_angle);
    Foc_Protocol_WriteFloat(&Payload[36], Motor.PLL.Omega_est);
    Foc_Protocol_WriteFloat(&Payload[40], Phase_error_degree);
    /* 将实际Clarke电流和Iq误差追加到负载末尾，保持既有字段偏移不变。 */
    Foc_Protocol_WriteFloat(&Payload[44], Current.clark.Alpha);
    Foc_Protocol_WriteFloat(&Payload[48], Current.clark.Beta);
    Foc_Protocol_WriteFloat(&Payload[52], Iq_error);

    Crc = Foc_Protocol_Crc16(
        &Frame[2],
        (uint16)(6u + FOC_PROTOCOL_OBSERVER_WAVEFORM_LENGTH));
    Foc_Protocol_WriteU16(
        &Frame[8u + FOC_PROTOCOL_OBSERVER_WAVEFORM_LENGTH],
        Crc);
    (void)debug_send_buffer(Frame, (uint32)sizeof(Frame));
}

/***********************************************
 * @brief : 统计紧凑观测流位图中已选择的字段数量
 * @param : Mask 观测字段位图
 * @return: 已选择的字段数量
 * @date  : 2026-09-15
 * @author: L
 ************************************************/
static uint8 Foc_Protocol_CountObserverStreamFields(uint16 Mask)
{
    uint8 Field_count = 0u;

    while (Mask != 0u)
    {
        Field_count += (uint8)(Mask & 0x0001u);
        Mask >>= 1u;
    }

    return Field_count;
}

/***********************************************
 * @brief : 按订阅位图向紧凑观测负载追加一个浮点字段
 * @param : Payload 紧凑观测负载地址
 * @param : Offset 当前写入偏移
 * @param : Field_mask 当前字段对应的位图掩码
 * @param : Value 当前字段数值
 * @return: 追加字段后的写入偏移
 * @date  : 2026-09-15
 * @author: L
 ************************************************/
static uint16 Foc_Protocol_AppendObserverStreamFloat(uint8 *Payload,
                                                     uint16 Offset,
                                                     uint16 Field_mask,
                                                     float Value)
{
    if ((Protocol.Observer_stream_mask & Field_mask) != 0u)
    {
        Foc_Protocol_WriteFloat(&Payload[Offset], Value);
        Offset += (uint16)sizeof(float);
    }

    return Offset;
}

/***********************************************
 * @brief : 校验紧凑观测流配置并按字段数和串口带宽确定实际采样周期
 * @param : Payload 字段位图、请求周期和可用带宽负载
 * @param : Payload_length 配置负载长度
 * @return: 无
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
static void Foc_Protocol_HandleObserverStreamConfig(const uint8 *Payload,
                                                     uint16 Payload_length)
{
    uint16 Observer_mask =
        Foc_Protocol_ReadU16(&Payload[0]) & FOC_PROTOCOL_OBSERVER_FIELD_ALL;
    uint16 Requested_period_tick;
    uint16 Minimum_requested_tick;
    uint16 Batch_header_length;
    uint8 Field_count;
    uint8 Batch_count;
    uint8 Adaptive_format;
    uint32 Bytes_per_sample;
    uint32 Frame_length;
    uint32 Minimum_period_tick;
    uint32 Stream_bps;

    Adaptive_format =
        (Payload_length == FOC_PROTOCOL_OBSERVER_STREAM_ADAPTIVE_CONFIG_LENGTH) ?
        1u : 0u;
    if (Adaptive_format != 0u)
    {
        Requested_period_tick = Foc_Protocol_ReadU16(&Payload[2]);
        Minimum_requested_tick =
            MOTOR_OBSERVER_PERIOD_MIN_TICK;
        Stream_bps = Foc_Protocol_ReadU32(&Payload[4]);
        Batch_header_length =
            FOC_PROTOCOL_OBSERVER_STREAM_ADAPTIVE_HEADER_LENGTH;
    }
    else
    {
        Requested_period_tick =
            (uint16)Payload[2] * MOTOR_OBSERVER_TICKS_PER_MS;
        Minimum_requested_tick =
            MOTOR_OBSERVER_TICKS_PER_MS;
        Stream_bps =
            (Payload_length == FOC_PROTOCOL_OBSERVER_STREAM_CONFIG_LENGTH) ?
            Foc_Protocol_ReadU32(&Payload[3]) :
            FOC_PROTOCOL_OBSERVER_STREAM_BPS_DEFAULT;
        Batch_header_length =
            FOC_PROTOCOL_OBSERVER_STREAM_BATCH_HEADER_LENGTH;
    }

    if (Requested_period_tick < Minimum_requested_tick)
    {
        Requested_period_tick = Minimum_requested_tick;
    }
    else if (Requested_period_tick >
             MOTOR_OBSERVER_PERIOD_MAX_TICK)
    {
        Requested_period_tick =
            MOTOR_OBSERVER_PERIOD_MAX_TICK;
    }

    if (Stream_bps < FOC_PROTOCOL_OBSERVER_STREAM_BPS_MIN)
    {
        Stream_bps = FOC_PROTOCOL_OBSERVER_STREAM_BPS_MIN;
    }
    else if (Stream_bps > FOC_PROTOCOL_OBSERVER_STREAM_BPS_MAX)
    {
        Stream_bps = FOC_PROTOCOL_OBSERVER_STREAM_BPS_MAX;
    }

    Field_count = Foc_Protocol_CountObserverStreamFields(Observer_mask);
    Bytes_per_sample = (uint32)Field_count * (uint32)sizeof(float);
    Batch_count = 1u;
    if (Bytes_per_sample != 0u)
    {
        Batch_count = (uint8)((FOC_PROTOCOL_OBSERVER_STREAM_MAX_LENGTH -
                               Batch_header_length) /
                              Bytes_per_sample);
        if (Batch_count == 0u)
        {
            Batch_count = 1u;
        }
        else if (Batch_count > FOC_PROTOCOL_OBSERVER_STREAM_BATCH_MAX)
        {
            Batch_count = FOC_PROTOCOL_OBSERVER_STREAM_BATCH_MAX;
        }
    }
    Frame_length = 10u + (uint32)Batch_header_length +
                   (Bytes_per_sample * (uint32)Batch_count);
    Minimum_period_tick =
        ((Frame_length * 10u * MOTOR_OBSERVER_TIMEBASE_HZ) +
         (Stream_bps * (uint32)Batch_count) - 1u) /
        (Stream_bps * (uint32)Batch_count);
    if (Adaptive_format == 0u)
    {
        Minimum_period_tick =
            ((Minimum_period_tick +
              MOTOR_OBSERVER_TICKS_PER_MS - 1u) /
             MOTOR_OBSERVER_TICKS_PER_MS) *
            MOTOR_OBSERVER_TICKS_PER_MS;
    }
    if ((uint32)Requested_period_tick < Minimum_period_tick)
    {
        Requested_period_tick = (uint16)Minimum_period_tick;
    }

    Protocol.Observer_stream_mask = 0u;
    Protocol.Observer_sample_read = 0u;
    Protocol.Observer_sample_write = 0u;
    Protocol.Observer_stream_period_tick = Requested_period_tick;
    Protocol.Last_observer_sample_tick = Protocol.Observer_stream_tick;
    Protocol.Observer_stream_adaptive = Adaptive_format;
    Protocol.Observer_stream_configured = 1u;
    Protocol.Observer_stream_mask = Observer_mask;
}

/***********************************************
 * @brief : 在20 kHz控制中断中按自适应周期采集已订阅的观测字段
 * @param : 无
 * @return: 无
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
void Foc_Protocol_CaptureObserverStream(void)
{
    Foc_ProtocolObserverSample_t *Sample;
    uint32 Current_tick;
    uint16 Observer_mask;
    uint8 Write_index;
    uint8 Next_index;

    if (Protocol.Observer_stream_tick_started == 0u)
    {
        Protocol.Observer_stream_tick =
            Protocol.Time_ms * MOTOR_OBSERVER_TICKS_PER_MS;
        Protocol.Observer_stream_tick_started = 1u;
    }
    else
    {
        Protocol.Observer_stream_tick++;
    }

    Observer_mask = Protocol.Observer_stream_mask;
    if ((Protocol.Control_seen == 0u) ||
        (Protocol.Observer_stream_configured == 0u) ||
        (Observer_mask == 0u))
    {
        return;
    }

    Current_tick = Protocol.Observer_stream_tick;
    if ((uint32)(Current_tick - Protocol.Last_observer_sample_tick) <
        (uint32)Protocol.Observer_stream_period_tick)
    {
        return;
    }
    Protocol.Last_observer_sample_tick = Current_tick;

    Write_index = Protocol.Observer_sample_write;
    Next_index = (uint8)((Write_index + 1u) %
                         FOC_PROTOCOL_OBSERVER_STREAM_RING_CAPACITY);
    if (Next_index == Protocol.Observer_sample_read)
    {
        /* 主循环来不及发送时丢弃最新波形点，禁止历史波形继续挤占控制通信。 */
        return;
    }

    Sample = &Protocol.Observer_samples[Write_index];
    Sample->Timestamp_tick = Current_tick;
    if ((Observer_mask & FOC_PROTOCOL_OBSERVER_FIELD_I_ALPHA_ACTUAL) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_I_ALPHA_ACTUAL] =
            Current.clark.Alpha;
    }
    if ((Observer_mask & FOC_PROTOCOL_OBSERVER_FIELD_I_BETA_ACTUAL) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_I_BETA_ACTUAL] =
            Current.clark.Beta;
    }
    if ((Observer_mask & FOC_PROTOCOL_OBSERVER_FIELD_I_ALPHA_EST) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_I_ALPHA_EST] =
            Motor.SMO.I_alpha_est;
    }
    if ((Observer_mask & FOC_PROTOCOL_OBSERVER_FIELD_I_BETA_EST) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_I_BETA_EST] =
            Motor.SMO.I_beta_est;
    }
    if ((Observer_mask & FOC_PROTOCOL_OBSERVER_FIELD_IQ_ERROR) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_IQ_ERROR] =
            Foc_Protocol_CalculateObserverIqError();
    }
    if ((Observer_mask & FOC_PROTOCOL_OBSERVER_FIELD_E_ALPHA) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_E_ALPHA] =
            Motor.SMO.E_alpha;
    }
    if ((Observer_mask & FOC_PROTOCOL_OBSERVER_FIELD_E_BETA) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_E_BETA] =
            Motor.SMO.E_beta;
    }
    if ((Observer_mask & FOC_PROTOCOL_OBSERVER_FIELD_E_ALPHA_FILTER) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_E_ALPHA_FILTER] =
            Motor.SMO.E_alpha_filter;
    }
    if ((Observer_mask & FOC_PROTOCOL_OBSERVER_FIELD_E_BETA_FILTER) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_E_BETA_FILTER] =
            Motor.SMO.E_beta_filter;
    }
    if ((Observer_mask & FOC_PROTOCOL_OBSERVER_FIELD_ELECTRICAL_ANGLE) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_ELECTRICAL_ANGLE] =
            (float)Motor.Encoder.Electrical_angle *
            360.0f / (float)ANGLE_PERIOD;
    }
    if ((Observer_mask &
         FOC_PROTOCOL_OBSERVER_FIELD_PLL_ELECTRICAL_ANGLE) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_PLL_ELECTRICAL_ANGLE] =
            (float)Motor.PLL.Electrical_angle_est *
            360.0f / (float)ANGLE_PERIOD;
    }
    if ((Observer_mask & FOC_PROTOCOL_OBSERVER_FIELD_PLL_OMEGA) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_PLL_OMEGA] =
            Motor.PLL.Omega_est;
    }
    if ((Observer_mask & FOC_PROTOCOL_OBSERVER_FIELD_PLL_PHASE_ERROR) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_PLL_PHASE_ERROR] =
            Motor.PLL.Phase_error * 360.0f / TWO_PI;
    }
    if ((Observer_mask &
         FOC_PROTOCOL_OBSERVER_FIELD_PLL_MECHANICAL_ANGLE) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_PLL_MECHANICAL_ANGLE] =
            (float)Motor.PLL.Mechanical_angle_est *
            360.0f / (float)ANGLE_PERIOD;
    }
    if ((Observer_mask & FOC_PROTOCOL_OBSERVER_FIELD_SPEED_ACTUAL) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_SPEED_ACTUAL] =
            Motor.Encoder.Spd_rpm;
    }
    if ((Observer_mask & FOC_PROTOCOL_OBSERVER_FIELD_MECHANICAL_ANGLE) != 0u)
    {
        Sample->Values[FOC_PROTOCOL_OBSERVER_VALUE_MECHANICAL_ANGLE] =
            Motor_Control_GetMechanicalDegree();
    }
    Protocol.Observer_sample_write = Next_index;
}

/***********************************************
 * @brief : 将环形缓冲中的连续观测点合并成一个浮点批次帧发送
 * @param : 无
 * @return: 已发送返回1，否则返回0
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
static uint8 Foc_Protocol_SendObserverStreamBatch(void)
{
    uint8 Frame[FOC_PROTOCOL_OBSERVER_STREAM_MAX_LENGTH + 10u];
    uint8 *Payload = &Frame[8];
    uint16 Payload_length;
    uint16 Observer_mask = Protocol.Observer_stream_mask;
    uint16 Field_mask;
    uint16 Crc;
    uint16 Batch_header_length;
    uint32 Base_timestamp_tick;
    uint32 Current_timestamp_tick;
    uint32 Expected_timestamp_tick;
    uint32 Bytes_per_sample;
    uint8 Field_count;
    uint8 Batch_limit;
    uint8 Available_count;
    uint8 Batch_count;
    uint8 Read_index;
    uint8 Write_index;
    uint8 Sample_index;
    uint8 Value_index;
    uint8 Gap_found = 0u;
    uint8 Adaptive_format = Protocol.Observer_stream_adaptive;
    Foc_ProtocolObserverSample_t *Sample;

    if (Observer_mask == 0u)
    {
        return 0u;
    }

    Read_index = Protocol.Observer_sample_read;
    Write_index = Protocol.Observer_sample_write;
    if (Read_index == Write_index)
    {
        return 0u;
    }

    if (Write_index >= Read_index)
    {
        Available_count = (uint8)(Write_index - Read_index);
    }
    else
    {
        Available_count = (uint8)(FOC_PROTOCOL_OBSERVER_STREAM_RING_CAPACITY -
                                  Read_index + Write_index);
    }

    Field_count = Foc_Protocol_CountObserverStreamFields(Observer_mask);
    Bytes_per_sample = (uint32)Field_count * (uint32)sizeof(float);
    if (Bytes_per_sample == 0u)
    {
        return 0u;
    }
    Batch_header_length = (Adaptive_format != 0u) ?
        FOC_PROTOCOL_OBSERVER_STREAM_ADAPTIVE_HEADER_LENGTH :
        FOC_PROTOCOL_OBSERVER_STREAM_BATCH_HEADER_LENGTH;
    Payload_length = Batch_header_length;
    Batch_limit = (uint8)((FOC_PROTOCOL_OBSERVER_STREAM_MAX_LENGTH -
                           Batch_header_length) /
                          Bytes_per_sample);
    if (Batch_limit > FOC_PROTOCOL_OBSERVER_STREAM_BATCH_MAX)
    {
        Batch_limit = FOC_PROTOCOL_OBSERVER_STREAM_BATCH_MAX;
    }
    if (Batch_limit == 0u)
    {
        Batch_limit = 1u;
    }

    Batch_count = (Available_count < Batch_limit) ?
                  Available_count : Batch_limit;
    Base_timestamp_tick =
        Protocol.Observer_samples[Read_index].Timestamp_tick;
    for (Sample_index = 1u; Sample_index < Batch_count; Sample_index++)
    {
        Sample = &Protocol.Observer_samples[
            (uint8)((Read_index + Sample_index) %
                    FOC_PROTOCOL_OBSERVER_STREAM_RING_CAPACITY)];
        Expected_timestamp_tick = Base_timestamp_tick +
                                  ((uint32)Sample_index *
                                   (uint32)Protocol.Observer_stream_period_tick);
        if (Sample->Timestamp_tick != Expected_timestamp_tick)
        {
            Batch_count = Sample_index;
            Gap_found = 1u;
            break;
        }
    }

    Current_timestamp_tick = Protocol.Observer_stream_tick;
    if ((Gap_found == 0u) &&
        (Batch_count < Batch_limit) &&
        ((uint32)(Current_timestamp_tick - Base_timestamp_tick) <
         FOC_PROTOCOL_OBSERVER_STREAM_FLUSH_TICK))
    {
        return 0u;
    }

    memset(Frame, 0, sizeof(Frame));
    Foc_Protocol_WriteU16(&Payload[4], Observer_mask);
    if (Adaptive_format != 0u)
    {
        Foc_Protocol_WriteU32(&Payload[0], Base_timestamp_tick);
        Foc_Protocol_WriteU16(
            &Payload[6],
            Protocol.Observer_stream_period_tick);
        Payload[8] = Batch_count;
    }
    else
    {
        Foc_Protocol_WriteU32(
            &Payload[0],
            Base_timestamp_tick /
            MOTOR_OBSERVER_TICKS_PER_MS);
        Payload[6] = (uint8)(Protocol.Observer_stream_period_tick /
                             MOTOR_OBSERVER_TICKS_PER_MS);
        Payload[7] = Batch_count;
    }

    for (Sample_index = 0u; Sample_index < Batch_count; Sample_index++)
    {
        Sample = &Protocol.Observer_samples[Read_index];
        for (Value_index = 0u;
             Value_index < (uint8)FOC_PROTOCOL_OBSERVER_VALUE_COUNT;
             Value_index++)
        {
            Field_mask = (uint16)(1u << Value_index);
            Payload_length = Foc_Protocol_AppendObserverStreamFloat(
                Payload,
                Payload_length,
                Field_mask,
                Sample->Values[Value_index]);
        }
        Read_index = (uint8)((Read_index + 1u) %
                             FOC_PROTOCOL_OBSERVER_STREAM_RING_CAPACITY);
    }
    Protocol.Observer_sample_read = Read_index;

    Frame[0] = 0xaau;
    Frame[1] = 0x55u;
    Frame[2] = FOC_PROTOCOL_VERSION;
    Frame[3] = (Adaptive_format != 0u) ?
        FOC_PROTOCOL_FRAME_TYPE_OBSERVER_STREAM_ADAPTIVE_BATCH :
        FOC_PROTOCOL_FRAME_TYPE_OBSERVER_STREAM_BATCH;
    Foc_Protocol_WriteU16(&Frame[4], Protocol.Tx_sequence++);
    Foc_Protocol_WriteU16(&Frame[6], Payload_length);
    Crc = Foc_Protocol_Crc16(&Frame[2], (uint16)(6u + Payload_length));
    Foc_Protocol_WriteU16(&Frame[8u + Payload_length], Crc);
    (void)debug_send_buffer(Frame, (uint32)(Payload_length + 10u));
    return 1u;
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
    else if ((Frame[3] == FOC_PROTOCOL_FRAME_TYPE_OBSERVER_PARAMETER_READ) &&
             (Payload_length == 0u))
    {
        Foc_Protocol_SendObserverParameters();
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
    else if ((Frame[3] == FOC_PROTOCOL_FRAME_TYPE_OBSERVER_PARAMETER_WRITE) &&
             (Payload_length == FOC_PROTOCOL_OBSERVER_PARAMETER_LENGTH))
    {
        Foc_Protocol_HandleObserverParameterWrite(&Frame[8]);
    }
    else if ((Frame[3] == FOC_PROTOCOL_FRAME_TYPE_OBSERVER_STREAM_CONFIG) &&
             ((Payload_length ==
               FOC_PROTOCOL_OBSERVER_STREAM_CONFIG_LEGACY_LENGTH) ||
              (Payload_length == FOC_PROTOCOL_OBSERVER_STREAM_CONFIG_LENGTH) ||
              (Payload_length ==
               FOC_PROTOCOL_OBSERVER_STREAM_ADAPTIVE_CONFIG_LENGTH)))
    {
        Foc_Protocol_HandleObserverStreamConfig(&Frame[8], Payload_length);
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
    else if ((Frame[3] == FOC_PROTOCOL_FRAME_TYPE_RESET) &&
             (Payload_length == 0u))
    {
        Foc_Protocol_HandleReset();
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

/***********************************************
 * @brief : 初始化调试串口及FOC-UART协议状态
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
void Foc_Protocol_Init(void)
{
    debug_init();
    memset(&Protocol, 0, sizeof(Protocol));
    /* 上电或复位后先等待失能帧，禁止残留的旧使能心跳直接启动电机。 */
    Protocol.Rearm_required = 1u;
}

/***********************************************
 * @brief : 处理串口接收、乐曲选择和播放开关
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
void Foc_Protocol_Service(void)
{
    uint8 Receive_data[FOC_PROTOCOL_FRAME_MAX];
    uint32 Receive_length;
    uint32 Index;
    uint32 Current_ms;

    if (Protocol.Timeout_cleanup_pending != 0u)
    {
        Protocol.Timeout_cleanup_pending = 0u;
        Foc_Protocol_StopControl();
    }

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
        ((uint32)(Current_ms - Protocol.Last_telemetry_ms) >=
         FOC_PROTOCOL_TELEMETRY_PERIOD_MS))
    {
        /* 发送接口为阻塞式，每次只发送一帧，给接收环形缓冲留出处理机会。 */
        Protocol.Last_telemetry_ms = Current_ms;
        Foc_Protocol_SendTelemetry();
        return;
    }

    if ((Protocol.Control_seen != 0u) &&
        (Protocol.Observer_stream_configured != 0u) &&
        (Protocol.Observer_stream_mask != 0u))
    {
        if (Foc_Protocol_SendObserverStreamBatch() != 0u)
        {
            return;
        }
    }

    if ((Protocol.Control_seen != 0u) &&
        ((Motor.Control_mode == MOTOR_CONTROL_OPEN_LOOP) ||
         (Motor.Control_mode == MOTOR_CONTROL_VOICE)) &&
        ((uint32)(Current_ms - Protocol.Last_waveform_ms) >=
         FOC_PROTOCOL_WAVEFORM_PERIOD_MS))
    {
        Protocol.Last_waveform_ms = Current_ms;
        Foc_Protocol_SendWaveform();
        return;
    }

    if ((Protocol.Control_seen != 0u) &&
        ((Motor.Control_mode == MOTOR_CONTROL_ENCODER_FOC) ||
         (Motor.Control_mode == MOTOR_CONTROL_SENSORLESS_FOC)) &&
        (Protocol.Observer_stream_configured == 0u) &&
        ((uint32)(Current_ms - Protocol.Last_observer_waveform_ms) >=
         FOC_PROTOCOL_OBSERVER_WAVEFORM_PERIOD_MS))
    {
        Protocol.Last_observer_waveform_ms = Current_ms;
        Foc_Protocol_SendObserverWaveform();
    }
}

/***********************************************
 * @brief : 更新协议时间并在控制帧超时后强制撤销电机输出
 * @param : 无
 * @return: 无
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
void Foc_Protocol_Tick1ms(void)
{
    Protocol.Time_ms++;
    if ((Protocol.Control_seen != 0u) &&
        (Protocol.Control_timed_out == 0u) &&
        ((uint32)(Protocol.Time_ms - Protocol.Last_control_ms) >=
         FOC_PROTOCOL_CONTROL_TIMEOUT_MS))
    {
        Protocol.Control_timed_out = 1u;
        Foc_Protocol_ForceTimeoutSafeOutput();
    }
}
