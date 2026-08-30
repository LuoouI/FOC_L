#ifndef FOC_PROTOCOL_H
#define FOC_PROTOCOL_H

#include "zf_common_headfile.h"

#define FOC_PROTOCOL_VERSION              (1u)       // FOC-UART协议版本
#define FOC_PROTOCOL_FRAME_TYPE_CONTROL   (0x10u)    // 电机控制命令帧
#define FOC_PROTOCOL_FRAME_TYPE_PARAMETER_READ (0x11u) // FOC环路参数读取帧
#define FOC_PROTOCOL_FRAME_TYPE_PARAMETER_WRITE (0x12u) // FOC环路参数写入帧
#define FOC_PROTOCOL_FRAME_TYPE_SONG_LIST (0x14u)    // 内置乐曲列表查询和响应帧
#define FOC_PROTOCOL_FRAME_TYPE_ZERO_CAL  (0x15u)    // 编码器零点校准命令帧
#define FOC_PROTOCOL_FRAME_TYPE_TELEMETRY (0x20u)   // 基础遥测数据帧
#define FOC_PROTOCOL_FRAME_TYPE_WAVEFORM  (0x21u)    // 高速波形采样帧
#define FOC_PROTOCOL_CONTROL_LENGTH       (16u)      // 控制命令负载长度
#define FOC_PROTOCOL_PARAMETER_LENGTH     (44u)      // FOC环路参数负载长度
#define FOC_PROTOCOL_TELEMETRY_LENGTH     (56u)      // 基础遥测负载长度
#define FOC_PROTOCOL_WAVEFORM_LENGTH      (36u)      // 高速波形负载长度
#define FOC_PROTOCOL_FRAME_MAX            (64u)      // 接收帧最大字节数
#define FOC_PROTOCOL_SONG_NAME_MAX        (48u)      // 单个UTF-8乐曲名称最大字节数
#define FOC_PROTOCOL_TIMEOUT_MS           (200u)     // 控制心跳超时时间
#define FOC_PROTOCOL_TELEMETRY_PERIOD_MS  (25u)      // 基础遥测发送周期
#define FOC_PROTOCOL_WAVEFORM_PERIOD_MS   (10u)      // 高速波形发送周期
#define FOC_PROTOCOL_CONTROL_HZ           (20000.0f) // 电机控制频率
#define FOC_PROTOCOL_UQ_LIMIT             (60.0f)    // 开环交轴电压限幅
#define FOC_PROTOCOL_DRIVE_MODE_OPEN_LOOP (0u)       // 开环电压矢量控制模式
#define FOC_PROTOCOL_DRIVE_MODE_ENCODER_FOC (1u)     // 有感FOC控制模式
#define FOC_PROTOCOL_DRIVE_MODE_VOICE     (2u)       // 电机音乐播放模式
#define FOC_PROTOCOL_CURRENT_BW_MIN_HZ    (1u)       // 电流环带宽下限，单位为Hz
#define FOC_PROTOCOL_CURRENT_BW_MAX_HZ    (5000u)    // 电流环带宽上限，单位为Hz
#define FOC_PROTOCOL_LOOP_GAIN_MAX        (100.0f)   // 速度、位置环增益上限
#define FOC_PROTOCOL_SPEED_INTEGRAL_LIMIT_MAX (5.0f) // 速度环积分项限幅上限，单位为A
#define FOC_PROTOCOL_SPEED_RAMP_MIN       (1.0f)      // 速度斜坡速率下限，单位为rpm/s
#define FOC_PROTOCOL_SPEED_RAMP_MAX       (100000.0f) // 速度斜坡速率上限，单位为rpm/s
#define FOC_PROTOCOL_POSITION_LIMIT_MAX   (30000.0f) // 位置环限幅上限，单位为rpm
#define FOC_PROTOCOL_POSITION_DEADBAND_MAX (180.0f)  // 位置环角度死区上限，单位为度
#define FOC_PROTOCOL_POSITION_SOFT_RANGE_MAX (180.0f) // 位置环软化范围上限，单位为度
#define FOC_PROTOCOL_POSITION_SPEED_DEADBAND_MAX (100.0f) // 到位速度死区上限，单位为rpm
#define FOC_PROTOCOL_STATUS_MUSIC_PLAYING (0x04u)    // 状态标志中的音乐播放位

/*===========================================================================*/
/*  FOC-UART流式接收状态                                                      */
/*===========================================================================*/
typedef struct
{
    uint8 Data[FOC_PROTOCOL_FRAME_MAX];          // 当前接收帧数据
    uint16 Length;                               // 当前已接收字节数
    uint16 Expected_length;                      // 当前完整帧字节数
} Foc_ProtocolParser_t;

/*===========================================================================*/
/*  FOC-UART协议运行状态                                                      */
/*===========================================================================*/
typedef struct
{
    Foc_ProtocolParser_t Parser;                 // 流式接收状态
    volatile uint32 Time_ms;                     // 协议毫秒时间基准
    uint32 Last_control_ms;                      // 最近控制帧接收时刻
    uint32 Voice_session;                        // 当前音乐播放会话标识
    uint32 Last_telemetry_ms;                    // 最近基础遥测发送时刻
    uint32 Last_waveform_ms;                     // 最近高速波形发送时刻
    uint16 Start_angle;                          // 当前开环起始角度
    uint16 Tx_sequence;                          // 发送帧序号
    uint8 Control_seen;                          // 已接收有效控制帧标志
    uint8 Parameters_seen;                       // 已接收有效环路参数标志
    uint8 Enabled;                               // 控制输出使能标志
    uint8 Song_id;                               // 当前曲目编号
    uint8 Voice_selected;                        // 音乐模式选中标志
} Foc_Protocol_t;

/***********************************************
 * @brief : 初始化调试串口及FOC-UART协议状态
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
void Foc_Protocol_Init(void);

/***********************************************
 * @brief : 处理串口接收、乐曲选择和播放开关
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
void Foc_Protocol_Service(void);

/***********************************************
 * @brief : 更新FOC-UART协议毫秒时间基准，需按1 kHz调用
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
void Foc_Protocol_Tick1ms(void);

#endif
