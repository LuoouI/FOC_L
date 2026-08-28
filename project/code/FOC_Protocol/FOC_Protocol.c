#include "FOC_Protocol.h"
#include "FOC_Voice/FOC_Voice.h"

static FOC_ProtocolParser_t Protocol_parser;
static volatile uint32 Protocol_timeMs = 0u;
static uint32 Protocol_lastControlMs = 0u;
static uint32 Protocol_voiceSession = 0u;
static uint8 Protocol_songId = 0u;
static uint8 Protocol_controlSeen = 0u;
static uint8 Protocol_enabled = 0u;
static uint8 Protocol_voiceSelected = 0u;

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
 * @brief : 执行一帧音乐控制命令
 * @param : Payload 16字节控制命令负载
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void FOC_Protocol_HandleControl(const uint8 *Payload)
{
    uint8 Drive_mode = Payload[0];
    uint8 Flags = Payload[1];
    uint8 Song_id = Payload[3];
    uint8 Enable = (uint8)(Flags & 0x01u);
    uint8 Emergency = (uint8)(Flags & 0x04u);
    uint32 Session = FOC_Protocol_ReadU32(&Payload[4]);
    uint8 Need_start;

    Protocol_controlSeen = 1u;
    Protocol_lastControlMs = Protocol_timeMs;

    if ((Emergency != 0u) || (Enable == 0u))
    {
        if (Protocol_voiceSelected != 0u)
        {
            FOC_Voice_Stop();
        }
        Protocol_enabled = 0u;
        Protocol_voiceSelected = (Drive_mode == 2u) ? 1u : 0u;
        return;
    }

    if ((Drive_mode != 2u) ||
        (Song_id == 0u) ||
        (Song_id > FOC_VOICE_SONG_COUNT))
    {
        if (Protocol_voiceSelected != 0u)
        {
            FOC_Voice_Stop();
        }
        Protocol_enabled = 1u;
        Protocol_voiceSelected = 0u;
        return;
    }

    Need_start = ((Protocol_enabled == 0u) ||
                  (Protocol_voiceSelected == 0u) ||
                  (Protocol_songId != Song_id) ||
                  (Protocol_voiceSession != Session)) ? 1u : 0u;

    Protocol_enabled = 1u;
    Protocol_voiceSelected = 1u;
    Protocol_voiceSession = Session;
    Protocol_songId = Song_id;

    if (Need_start != 0u)
    {
        (void)FOC_Voice_StartSong(Song_id);
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
    Protocol_songId = 0u;
    Protocol_controlSeen = 0u;
    Protocol_enabled = 0u;
    Protocol_voiceSelected = 0u;
}

void FOC_Protocol_Service(void)
{
    uint8 Receive_data[FOC_PROTOCOL_FRAME_MAX];
    uint32 Receive_length;
    uint32 Index;
    uint32 Current_ms = Protocol_timeMs;

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

    if ((Protocol_controlSeen != 0u) &&
        (Protocol_enabled != 0u) &&
        ((uint32)(Current_ms - Protocol_lastControlMs) >
         FOC_PROTOCOL_TIMEOUT_MS))
    {
        FOC_Voice_Stop();
        Protocol_enabled = 0u;
    }
}

void FOC_Protocol_Tick1ms(void)
{
    Protocol_timeMs++;
}
