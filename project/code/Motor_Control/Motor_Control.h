#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "zf_common_headfile.h"
#include "Foc_config.h"
#include "Foc_transform/Foc_transform.h"
#include "PID/PID.h"
#include "Function/Function.h"

/*===========================================================================*/
/*  电机零点校准参数                                                          */
/*===========================================================================*/
typedef struct
{
    float  Voltage;         /* 零点校准d轴电压 */
    uint16 Ramp_count;      /* 校准锁定电压渐升步数 */
    uint16 Ramp_ms;         /* 校准锁定电压渐升步间隔 */
    uint16 Hold_ms;         /* 校准起始定位保持时间 */
    uint16 Step_count;      /* 零点牵引步数 */
    uint16 Step_ms;         /* 零点牵引步间隔 */
    uint16 Sample_count;    /* 零点位置平均采样次数 */
    uint16 Sample_ms;       /* 零点位置采样间隔 */
    int32  Min_travel;      /* 判定编码器有效的最小累计行程 */
} Motor_ZeroCalib_t;

extern const Motor_ZeroCalib_t Motor_zeroCalib;

/*===========================================================================*/
/*  电机控制模式                                                              */
/*===========================================================================*/
typedef enum
{
    MOTOR_CONTROL_OPEN_LOOP = 0,     /* 开环电压矢量控制 */
    MOTOR_CONTROL_ENCODER_FOC = 1,   /* 有感FOC */
    MOTOR_CONTROL_VOICE = 2,         /* 电机音乐播放控制 */
    MOTOR_CONTROL_SENSORLESS_FOC = 3 /* 无感FOC */
} Motor_control_mode_t;

/*===========================================================================*/
/*  有感FOC子控制模式                                                         */
/*===========================================================================*/
typedef enum
{
    MOTOR_FOC_CURRENT = 1,      /* 电流环控制 */
    MOTOR_FOC_SPEED,            /* 速度环级联电流环 */
    MOTOR_FOC_POSITION          /* 位置环级联速度环和电流环 */
} Motor_foc_mode_t;

/*===========================================================================*/
/*  位置环回正方式                                                            */
/*===========================================================================*/
typedef enum
{
    MOTOR_POSITION_RETURN_SHORTEST = 0,     /* 按最近距离回正 */
    MOTOR_POSITION_RETURN_REVERSE_PATH      /* 沿偏转路径的反方向原路回正 */
} Motor_position_return_mode_t;

/*===========================================================================*/
/*  编码器配置及角度反馈                                                      */
/*===========================================================================*/
typedef struct
{
    menc15a_module_enum Sensor_id;      /* 编码器模块编号 */
    int8 Direction;                     /* 编码器方向，取值为+1或-1 */
    uint16 Zero_offset;                 /* 机械角零偏 */
    uint16 Mechanical_angle;            /* 机械角，范围0~32767 */
    uint16 Electrical_angle;            /* 电角度，范围0~32767 */
    float Spd_rpm;                      /* 滤波后的机械转速，单位为转/分钟 */
} Motor_Encoder_t;

/*===========================================================================*/
/*  电机输出状态                                                              */
/*===========================================================================*/
typedef struct
{
    int16 Duty_target;            /* 输出幅值目标，范围-10000~10000 */
    float Duty_output;            /* 实际输出幅值 */
} Motor_Output_t;

/*===========================================================================*/
/*  开环控制状态                                                              */
/*===========================================================================*/
typedef struct
{
    float Uq;                   /* 开环q轴电压指令，单位为V */
    uint16 Angle;               /* 开环电压矢量电角度 */
    int16 Step;                 /* 单周期电角度增量，负值表示反向 */
    uint16 Align_count;         /* 启动定向所需控制周期数 */
    uint16 Hold_count;          /* 启动定向计数 */
    uint8 Started;              /* 开环启动状态 */
} Motor_OpenLoop_t;

/*===========================================================================*/
/*  FOC电流环控制对象                                                         */
/*===========================================================================*/
typedef struct
{
    float Id_target;            /* d轴电流目标，单位为A */
    float Iq_target;            /* q轴电流目标，单位为A */
    PID_t Id_pid;               /* d轴电流调节器 */
    PID_t Iq_pid;               /* q轴电流调节器 */
    uint16 Bandwidth;           /* 电流环带宽，单位为Hz */
    float Ud_output;            /* d轴电压输出，单位为V */
    float Uq_output;            /* q轴电压输出，单位为V */
} Foc_CurrentLoop_t;

/*===========================================================================*/
/*  FOC速度环控制对象                                                         */
/*===========================================================================*/
typedef struct
{
    float Command_rpm;          /* 上位机下发的原始速度目标，单位为rpm */
    float Target_rpm;           /* 斜坡处理后的速度目标，单位为rpm */
    float Ramp_rate;            /* 速度斜坡速率，单位为rpm/s */
    PID_t Pid;                  /* 速度调节器，原始输出由速度环按Iq能力限幅 */
    float Integral_limit;       /* 速度环积分项配置限幅，单位为A */
    float Iq_output;            /* 速度环输出，单位为A */
} Foc_SpeedLoop_t;

/*===========================================================================*/
/*  FOC位置环控制对象                                                         */
/*===========================================================================*/
typedef struct
{
    float Target_degree;                        /* 位置目标，单位为度 */
    PID_t Pid;                                  /* 位置纯Kp调节器，输出为速度目标 */
    float Speed_output;                         /* 位置环输出，单位为rpm */
    float Deadband_degree;                      /* 位置角度死区，单位为度 */
    float Soft_range_degree;                    /* 到位线性软化范围，单位为度 */
    float Speed_deadband_rpm;                   /* 到位速度死区，单位为rpm */
    float Travel_degree;                        /* 相对目标位置的连续偏转角度，单位为度 */
    float Last_degree;                          /* 上次位置环采样角度，单位为度 */
    float Last_target_degree;                   /* 上次跟踪的位置目标，单位为度 */
    Motor_position_return_mode_t Return_mode;   /* 位置环回正方式 */
    uint8 Track_ready;                          /* 连续偏转角度跟踪有效标志 */
    uint8 In_deadband;                          /* 位置误差已进入角度死区标志 */
} Foc_PositionLoop_t;

/*===========================================================================*/
/*  HFI                                                                      */
/*===========================================================================*/
typedef struct
{
    float Injection_voltage;          /* 高频注入电压幅值，单位为V */
    float Injection_frequency;        /* 高频注入目标频率，单位为Hz */
    float Demod_bandwidth;            /* 解调低通带宽，单位为Hz */
    float Demod_coefficient;          /* 解调低通离散系数 */
    float Demod_amplitude;            /* 解调归一化幅值，单位为A */

    float Iq_previous;                /* 上一周期HFI坐标系q轴电流 */
    float Iq_delta;                   /* 相邻周期q轴电流变化量 */
    float Demod_raw;                  /* 同步解调原始值 */
    float Demod_filter;               /* 同步解调滤波值 */
    float Injection_voltage_applied;  /* 实际生效的注入电压，单位为V */

    uint16 Carrier_half_count;        /* 载波半周期控制节拍数 */
    uint16 Carrier_count;             /* 当前载波计数 */
    uint16 Polarity_offset;           /* 磁极极性补偿，取值为0或16384 */

    int8 Command_sign;                /* 即将输出的注入极性 */
    int8 Applied_sign;                /* 当前电流差分对应的注入极性 */
    int8 Error_direction;             /* 解调误差方向，取值为+1或-1 */

    uint8 Enabled;                    /* 高频注入使能标志 */
    uint8 Ready;                      /* 位置估算稳定标志 */
    uint8 Polarity_ready;             /* 磁极极性识别完成标志 */
} HFI_t;

/*===========================================================================*/
/*  PLL                                                                      */
/*===========================================================================*/
typedef struct
{
    float Bandwidth;                        /* PLL自然频率带宽，单位为Hz */
    float Kp;                               /* 由带宽计算的PLL比例增益，单位为rad/s */
    float Ki;                               /* 由带宽计算的PLL积分增益，单位为rad/s^2 */
    float Integral_sum;                     /* PLL积分项，单位为rad/s */
    float Phase_error;                      /* PLL鉴相误差，单位为rad */

    uint16 Mechanical_angle_est;            /* 估算机械角度，范围0~32767 */
    uint16 Electrical_angle_est;            /* 估算电角度，范围0~32767 */

    float Omega_est;                        /* 估算电角速度，单位为rad/s */
    float Mechanical_angle_rad;             /* PLL内部机械角度，单位为rad */
    float Electrical_angle_rad;             /* PLL内部未补偿电角度，单位为rad */
    int8 Direction;                         /* 当前观测方向，取值为+1或-1 */

} PLL_t;

/*===========================================================================*/
/*  SMO数据结构                                                               */
/*===========================================================================*/
typedef struct
{
    float I_alpha_est;                      /* Alpha轴电流估算值 */ 
    float I_beta_est;                       /* Beta轴电流估算值 */
    float I_alpha_estpre;                   /* 上一周期Alpha轴电流估算值 */
    float I_beta_estpre;                    /* 上一周期Beta轴电流估算值 */
 
    float U_alpha_pre;                      /* 上一周期Alpha轴电压 */
    float U_beta_pre;                       /* 上一周期Beta轴电压 */

    float E_alpha;                          /* Alpha轴反电动势 */
    float E_beta;                           /* Beta轴反电动势 */
    float E_alpha_filter;                   /* Alpha轴反电动势滤波值 */
    float E_beta_filter;                    /* Beta轴反电动势滤波值 */

    float K_slide;                          /* 滑模增益 */  

    float Boundary_current;                 /* 滑模边界电流，单位为A */
    float Filter_bandwidth;                 /* 反电动势滤波器带宽，单位为Hz */
    uint8 Ready;                            /* SMO就绪标志 */

    float A;                                /* 电流观测器上一周期电流离散系数 */
    float B;                                /* 电流观测器输入电压离散系数 */
} SMO_t;

/*===========================================================================*/
/*  FOC电机控制对象                                                           */
/*===========================================================================*/
typedef struct
{
    Motor_Encoder_t Encoder;                /* 编码器配置及角度反馈 */
    Motor_Output_t Output;                  /* 电机输出状态 */
    Motor_OpenLoop_t Open_loop;             /* 开环控制状态 */
    Foc_CurrentLoop_t Current_loop;         /* 电流环对象 */
    Foc_SpeedLoop_t Speed_loop;             /* 速度环对象 */
    Foc_PositionLoop_t Position_loop;       /* 位置环对象 */
    HFI_t HFI;                              /* 高频注入位置估算对象 */
    SMO_t SMO;                              /* SMO对象 */
    PLL_t PLL;                              /* 公共角度跟踪PLL */
    float Ab_filter_bandwidth;              /* 当前生效的AB滤波器带宽，单位为Hz */

    uint8 Pole_pairs;                       /* 电机极对数 */
    Motor_control_mode_t Control_mode;      /* 当前电机控制模式 */
    Motor_foc_mode_t Foc_mode;              /* 当前有感FOC子模式 */
    int8 Foc_direction;                     /* 有感FOC目标方向，取值为+1或-1 */
    uint8 Zero_ready;                       /* 编码器零点参数有效标志 */

} Foc_motor_t;

/*===========================================================================*/
/*  总控制结构体                                                              */
/*===========================================================================*/
typedef struct
{
    Foc_motor_t motor;        /* 电机控制对象 */
    uint8 ready;              /* 校准就绪标志 */
    uint8 calibrating;        /* 校准进行中标志 */

} Motor_Control_t;

extern Foc_motor_t Motor;     /* 电机控制对象 */


/*==================================================== 基础函数 ====================================================*/
void    Motor_Control_Init                          (void);
void    Motor_Control_SetCurrentBandwidth           (uint16 BandwidthHz);
void    Motor_Control_SetSpeedPi                    (float Kp,
                                                     float Ki,
                                                     float IntegralLimit);
void    Motor_Control_SetPositionKp                 (float Kp,
                                                     float OutputLimit,
                                                     float Deadband_degree,
                                                     float SoftRange_degree,
                                                     float SpeedDeadband_rpm);
void    PLL_SetBandwidth                             (PLL_t *Pll,
                                                     float Bandwidth_hz);
void    PLL_Reset                                    (PLL_t *Pll,
                                                     int8 Direction);
void    PLL_Update                                   (PLL_t *Pll,
                                                     float Phase_error,
                                                     float Sample_time,
                                                     uint8 Pole_pairs);
void    Angle_Update                                (void);
float   Motor_Control_GetMechanicalDegree           (void);
void    RPM_Cal                                     (void);
void    Zero_Calibration                            (void);
void    Motor_Control_ResetObserver                 (void);
void    Motor_Control_Loop                          (void);
/*==================================================== 基础函数 ====================================================*/

#endif
