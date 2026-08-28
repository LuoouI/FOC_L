#include "FOC_Protocol.h"
#include "Current_sample/Current_sample.h"
#include "FOC_Voice/FOC_Voice.h"
#include "Motor_Control/Motor_Control.h"
#include "SVPWM/SVPWM.h"

static FOC_ProtocolParser_t Protocol_parser;
static volatile uint32 Protocol_timeMs = 0u;
static uint32 Protocol_lastControlMs = 0u;
static uint32 Protocol_voiceSession = 0u;
static uint8 Protocol_controlSeen = 0u;
static uint8 Protocol_enabled = 0u;
static uint8 Protocol_songId = 0u;
static uint8 Protocol_voiceSelected = 0u;
static uint16 Protocol_startAngle = 0u;
static uint32 Protocol_lastTelemetryMs = 0u;
static uint32 Protocol_lastWaveformMs = 0u;
static uint16 Protocol_txSequence = 0u;

static const float Protocol_uqLimit = 60.0f;
static const uint32 Protocol_telemetryPeriodMs = 25u;
static const uint32 Protocol_waveformPeriodMs = 10u;
static const float Protocol_controlHz = 20000.0f;

/***********************************************
 * @brief : 读取小端序16位无符号整数
 * @param : Data 待读取字节地址
 * @return: 16位无符号整数
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static uint16 FOC_Protocol_ReadU16(const uint8 *Data)
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
static float FOC_Protocol_ReadFloat(const uint8 *Data)
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
static uint32 FOC_Protocol_ReadU32(const uint8 *Data)
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
static void FOC_Protocol_WriteU16(uint8 *Data, uint16 Value)
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
static void FOC_Protocol_WriteU32(uint8 *Data, uint32 Value)
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
static void FOC_Protocol_WriteFloat(uint8 *Data, float Value)
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
static uint16 FOC_Protocol_Crc16(const uint8 *Data, uint16 Length)
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
static void FOC_Protocol_StopControl(void)
{
    FOC_Voice_Stop();
    Motor.Control_mode = MOTOR_CONTROL_OPEN_LOOP;
    Motor.Open_loop.Uq = 0.0f;
    Motor.Open_loop.Step = 0;
    Motor.Open_loop.Hold_count = 0u;
    Motor.Open_loop.Started = 0u;
    Protocol_enabled = 0u;
    Protocol_voiceSelected = 0u;
}

/***********************************************
 * @brief : 执行一帧开环控制命令中的目标参数
 * @param : Payload 16字节控制命令负载
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void FOC_Protocol_HandleOpenLoop(const uint8 *Payload)
{
    uint8 Flags = Payload[1];
    int8 Direction = ((Flags & 0x02u) != 0u) ? -1 : 1;
    float Uq_target = FOC_Protocol_ReadFloat(&Payload[4]);
    float Start_angle_target = FOC_Protocol_ReadFloat(&Payload[8]);
    float Step_target = FOC_Protocol_ReadFloat(&Payload[12]);
    int32 Start_angle_value;
    int32 Step_value;
    uint16 Start_angle;
    uint8 Need_start;

    /* NaN不等于自身，用于拒绝异常浮点参数。 */
    if ((Uq_target != Uq_target) ||
        (Start_angle_target != Start_angle_target) ||
        (Step_target != Step_target))
    {
        FOC_Protocol_StopControl();
        return;
    }

    if (Protocol_voiceSelected != 0u)
    {
        FOC_Voice_Stop();
    }

    Uq_target = Float_Limit(Uq_target, -Protocol_uqLimit, Protocol_uqLimit);
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

    Need_start = ((Protocol_enabled == 0u) ||
                  (Protocol_startAngle != Start_angle) ||
                  (Motor.Open_loop.Started == 0u)) ? 1u : 0u;

    Protocol_enabled = 1u;
    Protocol_voiceSelected = 0u;
    Protocol_startAngle = Start_angle;
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
static void FOC_Protocol_HandleVoice(const uint8 *Payload)
{
    uint8 Song_id = Payload[3];
    uint32 Session = FOC_Protocol_ReadU32(&Payload[4]);
    uint8 Need_start;

    if ((Song_id == 0u) || (Song_id > FOC_VOICE_SONG_COUNT))
    {
        FOC_Protocol_StopControl();
        return;
    }

    Need_start = ((Protocol_enabled == 0u) ||
                  (Protocol_voiceSelected == 0u) ||
                  (Protocol_songId != Song_id) ||
                  (Protocol_voiceSession != Session)) ? 1u : 0u;

    Protocol_enabled = 1u;
    Protocol_voiceSelected = 1u;
    Protocol_songId = Song_id;
    Protocol_voiceSession = Session;

    if ((Need_start != 0u) && (FOC_Voice_StartSong(Song_id) == 0u))
    {
        FOC_Protocol_StopControl();
    }
}

/***********************************************
 * @brief : 按驱动模式执行一帧电机控制命令
 * @param : Payload 16字节控制命令负载
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void FOC_Protocol_HandleControl(const uint8 *Payload)
{
    uint8 Drive_mode = Payload[0];
    uint8 Flags = Payload[1];
    uint8 Enable = (uint8)(Flags & 0x01u);
    uint8 Emergency = (uint8)(Flags & 0x04u);

    Protocol_controlSeen = 1u;
    Protocol_lastControlMs = Protocol_timeMs;

    if ((Emergency != 0u) || (Enable == 0u))
    {
        FOC_Protocol_StopControl();
        return;
    }

    if (Drive_mode == FOC_PROTOCOL_DRIVE_MODE_OPEN_LOOP)
    {
        FOC_Protocol_HandleOpenLoop(Payload);
    }
    else if (Drive_mode == FOC_PROTOCOL_DRIVE_MODE_VOICE)
    {
        FOC_Protocol_HandleVoice(Payload);
    }
    else
    {
        FOC_Protocol_StopControl();
    }
}

/***********************************************
 * @brief : 打包并发送一帧基础电机遥测
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void FOC_Protocol_SendTelemetry(void)
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
    FOC_Protocol_WriteU16(&Frame[4], Protocol_txSequence++);
    FOC_Protocol_WriteU16(&Frame[6], FOC_PROTOCOL_TELEMETRY_LENGTH);

    if (Motor.Pole_pairs == 0u)
    {
        Speed_target = 0.0f;
    }
    else
    {
        Speed_target = (float)Motor.Open_loop.Step *
                       Protocol_controlHz * 60.0f /
                       ((float)ANGLE_PERIOD * (float)Motor.Pole_pairs);
    }
    Mechanical_angle = (float)Motor.Encoder.Mechanical_angle *
                       360.0f / (float)ANGLE_PERIOD;
    Electrical_angle = (float)Motor.Encoder.Electrical_angle *
                       360.0f / (float)ANGLE_PERIOD;

    Payload[0] = (Protocol_enabled != 0u) ? 1u : 0u;
    Payload[1] = (uint8)Motor.Control_mode;
    Payload[2] = 0u;
    Payload[3] = (Current.calibrated != 0u) ? 0x02u : 0u;
    if (Protocol_enabled != 0u)
    {
        Payload[3] |= 0x01u;
    }
    FOC_Protocol_WriteU32(&Payload[4], Protocol_timeMs);
    FOC_Protocol_WriteFloat(&Payload[8], Speed_target);
    FOC_Protocol_WriteFloat(&Payload[12], Motor.Encoder.Spd_rpm);
    FOC_Protocol_WriteFloat(&Payload[16], 0.0f);
    FOC_Protocol_WriteFloat(&Payload[20], Current.park.Id);
    FOC_Protocol_WriteFloat(&Payload[24], 0.0f);
    FOC_Protocol_WriteFloat(&Payload[28], Current.park.Iq);
    FOC_Protocol_WriteFloat(&Payload[32], SVPWM.VBUS);
    FOC_Protocol_WriteFloat(&Payload[36], 0.0f);
    FOC_Protocol_WriteFloat(&Payload[40], Mechanical_angle);
    FOC_Protocol_WriteFloat(&Payload[44], Electrical_angle);
    FOC_Protocol_WriteFloat(&Payload[48], 0.0f);

    Crc = FOC_Protocol_Crc16(&Frame[2], (uint16)(6u + FOC_PROTOCOL_TELEMETRY_LENGTH));
    FOC_Protocol_WriteU16(&Frame[8u + FOC_PROTOCOL_TELEMETRY_LENGTH], Crc);
    (void)debug_send_buffer(Frame, (uint32)sizeof(Frame));
}

/***********************************************
 * @brief : 打包并发送一帧高速电流和PWM波形
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void FOC_Protocol_SendWaveform(void)
{
    uint8 Frame[FOC_PROTOCOL_WAVEFORM_LENGTH + 10u];
    uint8 *Payload = &Frame[8];
    uint16 Crc;

    memset(Frame, 0, sizeof(Frame));
    Frame[0] = 0xaau;
    Frame[1] = 0x55u;
    Frame[2] = FOC_PROTOCOL_VERSION;
    Frame[3] = FOC_PROTOCOL_FRAME_TYPE_WAVEFORM;
    FOC_Protocol_WriteU16(&Frame[4], Protocol_txSequence++);
    FOC_Protocol_WriteU16(&Frame[6], FOC_PROTOCOL_WAVEFORM_LENGTH);

    FOC_Protocol_WriteU32(&Payload[0], Protocol_timeMs);
    FOC_Protocol_WriteFloat(&Payload[4], Current.current_u);
    FOC_Protocol_WriteFloat(&Payload[8], Current.current_v);
    FOC_Protocol_WriteFloat(&Payload[12], Current.current_w);
    FOC_Protocol_WriteFloat(&Payload[16], Motor.Current_loop.Ud_output);
    FOC_Protocol_WriteFloat(&Payload[20], Motor.Current_loop.Uq_output);
    FOC_Protocol_WriteFloat(&Payload[24], (float)SVPWM.DutyA * 100.0f / (float)SVPWM_DUTY_MAX);
    FOC_Protocol_WriteFloat(&Payload[28], (float)SVPWM.DutyB * 100.0f / (float)SVPWM_DUTY_MAX);
    FOC_Protocol_WriteFloat(&Payload[32], (float)SVPWM.DutyC * 100.0f / (float)SVPWM_DUTY_MAX);

    Crc = FOC_Protocol_Crc16(&Frame[2], (uint16)(6u + FOC_PROTOCOL_WAVEFORM_LENGTH));
    FOC_Protocol_WriteU16(&Frame[8u + FOC_PROTOCOL_WAVEFORM_LENGTH], Crc);
    (void)debug_send_buffer(Frame, (uint32)sizeof(Frame));
}

/***********************************************
 * @brief : 校验并分发一帧FOC-UART报文
 * @param : Frame 完整帧地址
 * @param : Length 完整帧长度
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void FOC_Protocol_HandleFrame(const uint8 *Frame, uint16 Length)
{
    uint16 Payload_length = FOC_Protocol_ReadU16(&Frame[6]);
    uint16 Received_crc;
    uint16 Calculated_crc;

    if ((Length != (uint16)(Payload_length + 10u)) ||
        (Frame[2] != FOC_PROTOCOL_VERSION))
    {
        return;
    }

    Received_crc = FOC_Protocol_ReadU16(&Frame[8u + Payload_length]);
    Calculated_crc = FOC_Protocol_Crc16(&Frame[2],
                                        (uint16)(6u + Payload_length));
    if (Received_crc != Calculated_crc)
    {
        return;
    }

    if ((Frame[3] == FOC_PROTOCOL_FRAME_TYPE_CONTROL) &&
        (Payload_length == FOC_PROTOCOL_CONTROL_LENGTH))
    {
        FOC_Protocol_HandleControl(&Frame[8]);
    }
}

/***********************************************
 * @brief : 向流式解析器输入一个串口字节
 * @param : Data 新收到的串口字节
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void FOC_Protocol_ParseByte(uint8 Data)
{
    uint16 Payload_length;

    if (Protocol_parser.Length == 0u)
    {
        if (Data == 0xaau)
        {
            Protocol_parser.Data[0] = Data;
            Protocol_parser.Length = 1u;
        }
        return;
    }

    if (Protocol_parser.Length == 1u)
    {
        if (Data == 0x55u)
        {
            Protocol_parser.Data[1] = Data;
            Protocol_parser.Length = 2u;
        }
        else if (Data != 0xaau)
        {
            Protocol_parser.Length = 0u;
        }
        return;
    }

    Protocol_parser.Data[Protocol_parser.Length] = Data;
    Protocol_parser.Length++;

    if (Protocol_parser.Length == 8u)
    {
        Payload_length = FOC_Protocol_ReadU16(&Protocol_parser.Data[6]);
        Protocol_parser.Expected_length = (uint16)(Payload_length + 10u);
        if (Protocol_parser.Expected_length > FOC_PROTOCOL_FRAME_MAX)
        {
            Protocol_parser.Length = 0u;
            Protocol_parser.Expected_length = 0u;
        }
    }

    if ((Protocol_parser.Expected_length != 0u) &&
        (Protocol_parser.Length >= Protocol_parser.Expected_length))
    {
        FOC_Protocol_HandleFrame(Protocol_parser.Data,
                                 Protocol_parser.Expected_length);
        Protocol_parser.Length = 0u;
        Protocol_parser.Expected_length = 0u;
    }
}

void FOC_Protocol_Init(void)
{
    Protocol_parser.Length = 0u;
    Protocol_parser.Expected_length = 0u;
    Protocol_timeMs = 0u;
    Protocol_lastControlMs = 0u;
    Protocol_voiceSession = 0u;
    Protocol_controlSeen = 0u;
    Protocol_enabled = 0u;
    Protocol_songId = 0u;
    Protocol_voiceSelected = 0u;
    Protocol_startAngle = 0u;
    Protocol_lastTelemetryMs = 0u;
    Protocol_lastWaveformMs = 0u;
    Protocol_txSequence = 0u;
}

void FOC_Protocol_Service(void)
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
            FOC_Protocol_ParseByte(Receive_data[Index]);
        }
    }
    while (Receive_length != 0u);

    Current_ms = Protocol_timeMs;

    if ((Protocol_controlSeen != 0u) &&
        ((uint32)(Current_ms - Protocol_lastControlMs) >
         FOC_PROTOCOL_TIMEOUT_MS))
    {
        FOC_Protocol_StopControl();
        Protocol_controlSeen = 0u;
    }

    if ((Protocol_controlSeen != 0u) &&
        ((uint32)(Current_ms - Protocol_lastTelemetryMs) >=
         Protocol_telemetryPeriodMs))
    {
        Protocol_lastTelemetryMs = Current_ms;
        FOC_Protocol_SendTelemetry();
    }

    if ((Protocol_controlSeen != 0u) &&
        ((uint32)(Current_ms - Protocol_lastWaveformMs) >=
         Protocol_waveformPeriodMs))
    {
        Protocol_lastWaveformMs = Current_ms;
        FOC_Protocol_SendWaveform();
    }
}

void FOC_Protocol_Tick1ms(void)
{
    Protocol_timeMs++;
}
