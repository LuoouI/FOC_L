#ifndef FOC_PROTOCOL_H
#define FOC_PROTOCOL_H

#include "zf_common_headfile.h"

#define FOC_PROTOCOL_VERSION              (1u)                          /* FOC-UART协议版本 */
#define FOC_PROTOCOL_FRAME_TYPE_CONTROL   (0x10u)                       /* 电机控制命令帧 */
#define FOC_PROTOCOL_FRAME_TYPE_PARAMETER_READ (0x11u)                  /* FOC环路参数读取帧 */
#define FOC_PROTOCOL_FRAME_TYPE_PARAMETER_WRITE (0x12u)                 /* FOC环路参数写入帧 */
#define FOC_PROTOCOL_FRAME_TYPE_OBSERVER_PARAMETER_READ (0x13u)         /* SMO/PLL参数读取帧 */
#define FOC_PROTOCOL_FRAME_TYPE_SONG_LIST (0x14u)                       /* 内置乐曲列表查询和响应帧 */
#define FOC_PROTOCOL_FRAME_TYPE_ZERO_CAL  (0x15u)                       /* 编码器零点校准命令帧 */
#define FOC_PROTOCOL_FRAME_TYPE_RESET     (0x16u)                       /* 驱动板软件复位命令帧 */
#define FOC_PROTOCOL_FRAME_TYPE_OBSERVER_STREAM_CONFIG (0x17u)          /* 紧凑观测流配置帧 */
#define FOC_PROTOCOL_FRAME_TYPE_TELEMETRY (0x20u)                       /* 基础遥测数据帧 */
#define FOC_PROTOCOL_FRAME_TYPE_WAVEFORM  (0x21u)                       /* 高速波形采样帧 */
#define FOC_PROTOCOL_FRAME_TYPE_OBSERVER_WAVEFORM (0x22u)               /* SMO/PLL观测波形帧 */
#define FOC_PROTOCOL_FRAME_TYPE_OBSERVER_PARAMETER_WRITE (0x23u)        /* SMO/PLL参数写入帧 */
#define FOC_PROTOCOL_FRAME_TYPE_OBSERVER_STREAM (0x24u)                 /* 按通道订阅的紧凑观测帧 */
#define FOC_PROTOCOL_FRAME_TYPE_OBSERVER_STREAM_BATCH (0x25u)           /* 连续浮点观测批次帧 */
#define FOC_PROTOCOL_FRAME_TYPE_OBSERVER_STREAM_ADAPTIVE_BATCH (0x26u)  /* 高分辨率自适应观测批次帧 */
#define FOC_PROTOCOL_CONTROL_LENGTH       (16u)                         /* 控制命令负载长度 */
#define FOC_PROTOCOL_PARAMETER_LENGTH     (44u)                         /* FOC环路参数负载长度 */
#define FOC_PROTOCOL_OBSERVER_PARAMETER_LENGTH (16u)                    /* SMO/PLL参数负载长度 */
#define FOC_PROTOCOL_OBSERVER_STREAM_CONFIG_LEGACY_LENGTH (3u)          /* 旧版字段位图和毫秒周期配置长度 */
#define FOC_PROTOCOL_OBSERVER_STREAM_CONFIG_LENGTH (7u)                 /* 字段位图、毫秒周期和带宽配置长度 */
#define FOC_PROTOCOL_OBSERVER_STREAM_ADAPTIVE_CONFIG_LENGTH (8u)        /* 字段位图、控制节拍周期和带宽配置长度 */
#define FOC_PROTOCOL_TELEMETRY_LENGTH     (56u)                         /* 基础遥测负载长度 */
#define FOC_PROTOCOL_WAVEFORM_LENGTH      (36u)                         /* 高速波形负载长度 */
#define FOC_PROTOCOL_OBSERVER_WAVEFORM_LENGTH (56u)                     /* SMO/PLL观测波形负载长度 */
#define FOC_PROTOCOL_OBSERVER_STREAM_MAX_LENGTH (256u)                  /* 单个紧凑观测批次最大负载长度 */
#define FOC_PROTOCOL_OBSERVER_STREAM_BATCH_MAX (8u)                     /* 单帧最多携带的连续采样数 */
#define FOC_PROTOCOL_OBSERVER_STREAM_RING_CAPACITY (32u)                /* 中断采样环形缓冲容量 */
#define FOC_PROTOCOL_OBSERVER_STREAM_BATCH_HEADER_LENGTH (8u)           /* 毫秒观测批次固定负载长度 */
#define FOC_PROTOCOL_OBSERVER_STREAM_ADAPTIVE_HEADER_LENGTH (9u)        /* 高分辨率观测批次固定负载长度 */
#define FOC_PROTOCOL_OBSERVER_STREAM_TICK_HZ (20000u)                   /* 高分辨率观测采样时基频率 */
#define FOC_PROTOCOL_OBSERVER_STREAM_TICKS_PER_MS (20u)                 /* 每毫秒包含的观测采样节拍数 */
#define FOC_PROTOCOL_OBSERVER_STREAM_PERIOD_MIN_TICK (5u)               /* 最短采样周期，限制最高采样率为4 kHz */
#define FOC_PROTOCOL_OBSERVER_STREAM_PERIOD_MAX_TICK (400u)             /* 最长采样周期，对应20 ms */
#define FOC_PROTOCOL_OBSERVER_STREAM_FLUSH_TICK (160u)                  /* 未满批次的最长等待时间，对应8 ms */
#define FOC_PROTOCOL_FRAME_MAX            (64u)                         /* 接收帧最大字节数 */
#define FOC_PROTOCOL_SONG_NAME_MAX        (48u)                         /* 单个UTF-8乐曲名称最大字节数 */
#define FOC_PROTOCOL_TELEMETRY_PERIOD_MS  (25u)                         /* 基础遥测发送周期 */
#define FOC_PROTOCOL_WAVEFORM_PERIOD_MS   (10u)                         /* 高速波形发送周期 */
#define FOC_PROTOCOL_OBSERVER_WAVEFORM_PERIOD_MS (20u)                  /* SMO/PLL观测波形发送周期 */
#define FOC_PROTOCOL_OBSERVER_STREAM_BPS_MIN (40000u)                   /* 紧凑观测流最低可用串口带宽 */
#define FOC_PROTOCOL_OBSERVER_STREAM_BPS_MAX (1200000u)                 /* 紧凑观测流最高可用串口带宽 */
#define FOC_PROTOCOL_OBSERVER_STREAM_BPS_DEFAULT (200000u)              /* 旧版配置使用的默认串口带宽 */
#define FOC_PROTOCOL_CONTROL_TIMEOUT_MS   (200u)                         /* 合法控制帧接收超时时间 */
#define FOC_PROTOCOL_CONTROL_HZ           (20000.0f)                    /* 电机控制频率 */
#define FOC_PROTOCOL_UQ_LIMIT             (60.0f)                       /* 开环交轴电压限幅 */
#define FOC_PROTOCOL_DRIVE_MODE_OPEN_LOOP (0u)                          /* 开环电压矢量控制模式 */
#define FOC_PROTOCOL_DRIVE_MODE_ENCODER_FOC (1u)                        /* 有感FOC控制模式 */
#define FOC_PROTOCOL_DRIVE_MODE_VOICE     (2u)                          /* 电机音乐播放模式 */
#define FOC_PROTOCOL_DRIVE_MODE_SENSORLESS_FOC (3u)                    /* 无感FOC观测调试模式 */
#define FOC_PROTOCOL_CURRENT_BW_MIN_HZ    (1u)                          /* 电流环带宽下限，单位为Hz */
#define FOC_PROTOCOL_CURRENT_BW_MAX_HZ    (5000u)                       /* 电流环带宽上限，单位为Hz */
#define FOC_PROTOCOL_LOOP_GAIN_MAX        (100.0f)                      /* 速度、位置环增益上限 */
#define FOC_PROTOCOL_SPEED_INTEGRAL_LIMIT_MAX (5.0f)                    /* 速度环积分项限幅上限，单位为A */
#define FOC_PROTOCOL_SPEED_RAMP_MIN       (1.0f)                        /* 速度斜坡速率下限，单位为rpm/s */
#define FOC_PROTOCOL_SPEED_RAMP_MAX       (100000.0f)                   /* 速度斜坡速率上限，单位为rpm/s */
#define FOC_PROTOCOL_POSITION_LIMIT_MAX   (30000.0f)                    /* 位置环限幅上限，单位为rpm */
#define FOC_PROTOCOL_POSITION_DEADBAND_MAX (180.0f)                     /* 位置环角度死区上限，单位为度 */
#define FOC_PROTOCOL_POSITION_SOFT_RANGE_MAX (180.0f)                   /* 位置环软化范围上限，单位为度 */
#define FOC_PROTOCOL_POSITION_SPEED_DEADBAND_MAX (100.0f)               /* 到位速度死区上限，单位为rpm */
#define FOC_PROTOCOL_SMO_GAIN_MAX         (100.0f)                       /* SMO滑模增益上限 */
#define FOC_PROTOCOL_SMO_BOUNDARY_CURRENT_MAX (100.0f)                   /* SMO边界电流上限，单位为A */
#define FOC_PROTOCOL_SMO_FILTER_BW_MIN    (1.0f)                         /* SMO滤波带宽下限，单位为Hz */
#define FOC_PROTOCOL_SMO_FILTER_BW_MAX    (500.0f)                       /* SMO滤波带宽上限，单位为Hz */
#define FOC_PROTOCOL_PLL_BW_MIN          (1.0f)                          /* PLL带宽下限，单位为Hz */
#define FOC_PROTOCOL_PLL_BW_MAX          (500.0f)                        /* PLL带宽上限，单位为Hz */
#define FOC_PROTOCOL_STATUS_MUSIC_PLAYING (0x04u)                       /* 状态标志中的音乐播放位 */

#define FOC_PROTOCOL_OBSERVER_FIELD_I_ALPHA_ACTUAL       (0x0001u)      /* 实际Alpha轴电流 */
#define FOC_PROTOCOL_OBSERVER_FIELD_I_BETA_ACTUAL        (0x0002u)      /* 实际Beta轴电流 */
#define FOC_PROTOCOL_OBSERVER_FIELD_I_ALPHA_EST          (0x0004u)      /* SMO估算Alpha轴电流 */
#define FOC_PROTOCOL_OBSERVER_FIELD_I_BETA_EST           (0x0008u)      /* SMO估算Beta轴电流 */
#define FOC_PROTOCOL_OBSERVER_FIELD_IQ_ERROR             (0x0010u)      /* SMO估算Iq误差 */
#define FOC_PROTOCOL_OBSERVER_FIELD_E_ALPHA              (0x0020u)      /* 未滤波Alpha轴反电动势 */
#define FOC_PROTOCOL_OBSERVER_FIELD_E_BETA               (0x0040u)      /* 未滤波Beta轴反电动势 */
#define FOC_PROTOCOL_OBSERVER_FIELD_E_ALPHA_FILTER       (0x0080u)      /* 滤波Alpha轴反电动势 */
#define FOC_PROTOCOL_OBSERVER_FIELD_E_BETA_FILTER        (0x0100u)      /* 滤波Beta轴反电动势 */
#define FOC_PROTOCOL_OBSERVER_FIELD_ELECTRICAL_ANGLE     (0x0200u)      /* 编码器实际电角度 */
#define FOC_PROTOCOL_OBSERVER_FIELD_PLL_ELECTRICAL_ANGLE (0x0400u)      /* PLL估算电角度 */
#define FOC_PROTOCOL_OBSERVER_FIELD_PLL_OMEGA            (0x0800u)      /* PLL估算电角速度 */
#define FOC_PROTOCOL_OBSERVER_FIELD_PLL_PHASE_ERROR      (0x1000u)      /* PLL鉴相误差 */
#define FOC_PROTOCOL_OBSERVER_FIELD_PLL_MECHANICAL_ANGLE (0x2000u)      /* PLL估算机械角度 */
#define FOC_PROTOCOL_OBSERVER_FIELD_SPEED_ACTUAL         (0x4000u)      /* 编码器实际机械转速 */
#define FOC_PROTOCOL_OBSERVER_FIELD_MECHANICAL_ANGLE     (0x8000u)      /* 编码器实际机械角度 */
#define FOC_PROTOCOL_OBSERVER_FIELD_ALL                  (0xffffu)      /* 全部紧凑观测字段 */

/*===========================================================================*/
/*  紧凑观测采样字段索引                                                     */
/*===========================================================================*/
typedef enum
{
    FOC_PROTOCOL_OBSERVER_VALUE_I_ALPHA_ACTUAL = 0u,      /* 实际Alpha轴电流 */
    FOC_PROTOCOL_OBSERVER_VALUE_I_BETA_ACTUAL,            /* 实际Beta轴电流 */
    FOC_PROTOCOL_OBSERVER_VALUE_I_ALPHA_EST,              /* SMO估算Alpha轴电流 */
    FOC_PROTOCOL_OBSERVER_VALUE_I_BETA_EST,               /* SMO估算Beta轴电流 */
    FOC_PROTOCOL_OBSERVER_VALUE_IQ_ERROR,                  /* SMO估算Iq误差 */
    FOC_PROTOCOL_OBSERVER_VALUE_E_ALPHA,                   /* 未滤波Alpha轴反电动势 */
    FOC_PROTOCOL_OBSERVER_VALUE_E_BETA,                    /* 未滤波Beta轴反电动势 */
    FOC_PROTOCOL_OBSERVER_VALUE_E_ALPHA_FILTER,            /* 滤波Alpha轴反电动势 */
    FOC_PROTOCOL_OBSERVER_VALUE_E_BETA_FILTER,             /* 滤波Beta轴反电动势 */
    FOC_PROTOCOL_OBSERVER_VALUE_ELECTRICAL_ANGLE,          /* 编码器实际电角度 */
    FOC_PROTOCOL_OBSERVER_VALUE_PLL_ELECTRICAL_ANGLE,      /* PLL估算电角度 */
    FOC_PROTOCOL_OBSERVER_VALUE_PLL_OMEGA,                 /* PLL估算电角速度 */
    FOC_PROTOCOL_OBSERVER_VALUE_PLL_PHASE_ERROR,           /* PLL鉴相误差 */
    FOC_PROTOCOL_OBSERVER_VALUE_PLL_MECHANICAL_ANGLE,      /* PLL估算机械角度 */
    FOC_PROTOCOL_OBSERVER_VALUE_SPEED_ACTUAL,              /* 编码器实际机械转速 */
    FOC_PROTOCOL_OBSERVER_VALUE_MECHANICAL_ANGLE,          /* 编码器实际机械角度 */
    FOC_PROTOCOL_OBSERVER_VALUE_COUNT                      /* 观测采样字段总数 */
} Foc_ProtocolObserverValue_t;

/*===========================================================================*/
/*  20 kHz控制时基下采集的一组观测数据                                       */
/*===========================================================================*/
typedef struct
{
    uint32 Timestamp_tick;                          /* 设备端20 kHz采样时刻 */
    float Values[FOC_PROTOCOL_OBSERVER_VALUE_COUNT];/* 按字段索引排列的连续浮点值 */
} Foc_ProtocolObserverSample_t;

/*===========================================================================*/
/*  FOC-UART流式接收状态                                                      */
/*===========================================================================*/
typedef struct
{
    uint8 Data[FOC_PROTOCOL_FRAME_MAX]; /* 当前接收帧数据 */
    uint16 Length; /* 当前已接收字节数 */
    uint16 Expected_length; /* 当前完整帧字节数 */
} Foc_ProtocolParser_t;

/*===========================================================================*/
/*  FOC-UART协议运行状态                                                      */
/*===========================================================================*/
typedef struct
{
    Foc_ProtocolParser_t Parser;            /* 流式接收状态 */
    volatile uint32 Time_ms;                /* 协议毫秒时间基准 */
    volatile uint32 Last_control_ms;        /* 最近合法控制帧接收时刻 */
    uint32 Voice_session;                   /* 当前音乐播放会话标识 */
    uint32 Last_telemetry_ms;               /* 最近基础遥测发送时刻 */
    uint32 Last_waveform_ms;                /* 最近高速波形发送时刻 */
    uint32 Last_observer_waveform_ms;       /* 最近兼容观测波形发送时刻 */
    volatile uint32 Observer_stream_tick;   /* 20 kHz观测采样时间基准 */
    volatile uint32 Last_observer_sample_tick; /* 最近紧凑观测采样时刻 */
    Foc_ProtocolObserverSample_t Observer_samples[FOC_PROTOCOL_OBSERVER_STREAM_RING_CAPACITY]; /* 观测采样环形缓冲 */
    volatile uint16 Observer_stream_mask;   /* 紧凑观测流字段位图 */
    volatile uint16 Observer_stream_period_tick; /* 紧凑观测流实际采样周期 */
    uint16 Start_angle;                     /* 当前开环起始角度 */
    uint16 Tx_sequence;                     /* 发送帧序号 */
    volatile uint8 Observer_stream_configured;/* 上位机已配置紧凑观测流标志 */
    volatile uint8 Observer_stream_adaptive;/* 使用高分辨率自适应批次格式 */
    volatile uint8 Observer_stream_tick_started; /* 观测采样时基已与毫秒时间同步 */
    volatile uint8 Observer_sample_write;   /* 观测采样写入索引 */
    volatile uint8 Observer_sample_read;    /* 观测采样读取索引 */
    volatile uint8 Control_seen;            /* 已接收有效控制帧标志 */
    volatile uint8 Control_timed_out;       /* 控制帧接收已经超时标志 */
    volatile uint8 Timeout_cleanup_pending; /* 主循环待完成超时停机清理标志 */
    volatile uint8 Rearm_required;          /* 重新使能前需确认失能的标志 */
    uint8 Parameters_seen;                  /* 已接收有效环路参数标志 */
    volatile uint8 Enabled;                 /* 控制输出使能标志 */
    uint8 Song_id;                          /* 当前曲目编号 */
    volatile uint8 Voice_selected;          /* 音乐模式选中标志 */
} Foc_Protocol_t;

/*==================================================== 基础函数 ====================================================*/
void    Foc_Protocol_Init                  (void);
void    Foc_Protocol_Service               (void);
void    Foc_Protocol_Tick1ms               (void);
void    Foc_Protocol_CaptureObserverStream (void);
/*==================================================== 基础函数 ====================================================*/

#endif
