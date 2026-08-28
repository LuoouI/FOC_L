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
