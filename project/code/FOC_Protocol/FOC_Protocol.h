#ifndef FOC_PROTOCOL_H
#define FOC_PROTOCOL_H

#include "zf_common_headfile.h"

#define FOC_PROTOCOL_VERSION              (1u)       // FOC-UART协议版本
#define FOC_PROTOCOL_FRAME_TYPE_CONTROL   (0x10u)    // 电机控制命令帧
#define FOC_PROTOCOL_FRAME_TYPE_TELEMETRY (0x20u)   // 基础遥测数据帧
#define FOC_PROTOCOL_FRAME_TYPE_WAVEFORM  (0x21u)    // 高速波形采样帧
#define FOC_PROTOCOL_CONTROL_LENGTH       (16u)      // 控制命令负载长度
#define FOC_PROTOCOL_TELEMETRY_LENGTH     (52u)      // 基础遥测负载长度
#define FOC_PROTOCOL_WAVEFORM_LENGTH      (36u)      // 高速波形负载长度
#define FOC_PROTOCOL_FRAME_MAX            (64u)      // 接收帧最大字节数
#define FOC_PROTOCOL_TIMEOUT_MS           (200u)     // 控制心跳超时时间
#define FOC_PROTOCOL_TELEMETRY_PERIOD_MS  (25u)      // 基础遥测发送周期
#define FOC_PROTOCOL_WAVEFORM_PERIOD_MS   (10u)      // 高速波形发送周期
#define FOC_PROTOCOL_CONTROL_HZ           (20000.0f) // 电机控制频率
#define FOC_PROTOCOL_UQ_LIMIT             (60.0f)    // 开环交轴电压限幅
#define FOC_PROTOCOL_DRIVE_MODE_OPEN_LOOP (0u)       // 开环电压矢量控制模式
#define FOC_PROTOCOL_DRIVE_MODE_VOICE     (2u)       // 电机音乐播放模式

/*===========================================================================*/
/*  FOC-UART流式接收状态                                                      */
/*===========================================================================*/
typedef struct
{
    uint8 Data[FOC_PROTOCOL_FRAME_MAX];          // 当前接收帧数据
    uint16 Length;                               // 当前已接收字节数
    uint16 Expected_length;                      // 当前完整帧字节数
} FOC_ProtocolParser_t;

/*===========================================================================*/
/*  FOC-UART协议运行状态                                                      */
/*===========================================================================*/
typedef struct
{
    FOC_ProtocolParser_t Parser;                 // 流式接收状态
    volatile uint32 Time_ms;                     // 协议毫秒时间基准
    uint32 Last_control_ms;                      // 最近控制帧接收时刻
    uint32 Voice_session;                        // 当前音乐播放会话标识
    uint32 Last_telemetry_ms;                    // 最近基础遥测发送时刻
    uint32 Last_waveform_ms;                     // 最近高速波形发送时刻
    uint16 Start_angle;                          // 当前开环起始角度
    uint16 Tx_sequence;                          // 发送帧序号
    uint8 Control_seen;                          // 已接收有效控制帧标志
    uint8 Enabled;                               // 控制输出使能标志
    uint8 Song_id;                               // 当前曲目编号
    uint8 Voice_selected;                        // 音乐模式选中标志
} FOC_Protocol_t;

/***********************************************
 * @brief : 初始化FOC-UART协议状态，复用调试串口115200 bit/s
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
void FOC_Protocol_Init(void);

/***********************************************
 * @brief : 处理串口接收、乐曲选择和播放开关
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
void FOC_Protocol_Service(void);

/***********************************************
 * @brief : 更新FOC-UART协议毫秒时间基准，需按1 kHz调用
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
void FOC_Protocol_Tick1ms(void);

#endif
