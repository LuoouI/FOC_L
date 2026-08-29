#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "zf_common_headfile.h"
#include "Foc_transform/Foc_transform.h"
#include "PID/PID.h"
#include "Function/Function.h"

#define MOTOR_CURRENT_LOOP_HZ           (20000u)        // 电流环执行频率，单位为Hz
#define MOTOR_SPEED_LOOP_HZ             (1000u)         // 速度环执行频率，单位为Hz
#define MOTOR_POSITION_LOOP_HZ          (500u)          // 位置环执行频率，单位为Hz

#define MOTOR_SPEED_LOOP_DIVIDER        \
    (MOTOR_CURRENT_LOOP_HZ / MOTOR_SPEED_LOOP_HZ)       // 速度环相对电流环的分频系数
#define MOTOR_POSITION_LOOP_DIVIDER     \
    (MOTOR_CURRENT_LOOP_HZ / MOTOR_POSITION_LOOP_HZ)    // 位置环相对电流环的分频系数
#define MOTOR_CURRENT_LOOP_TS           \
    (1.0f / (float)MOTOR_CURRENT_LOOP_HZ)               // 电流环采样周期，单位为秒
#define MOTOR_SPEED_LOOP_TS             \
    (1.0f / (float)MOTOR_SPEED_LOOP_HZ)                 // 速度环采样周期，单位为秒
#define MOTOR_POSITION_LOOP_TS          \
    (1.0f / (float)MOTOR_POSITION_LOOP_HZ)              // 位置环采样周期，单位为秒

#define MOTOR_CURRENT_VECTOR_LIMIT_A    (10.0f)         // d/q轴电流矢量固定限幅，单位为A
#define MOTOR_AB_FILTER_BW_MIN_HZ        (1.0f)         // AB滤波器带宽下限，单位为Hz
#define MOTOR_AB_FILTER_BW_MAX_HZ        (500.0f)       // AB滤波器带宽上限，单位为Hz

/*===========================================================================*/
/*  电机零点校准参数                                                          */
/*===========================================================================*/
typedef struct
{
    float  Voltage;                            // 零点校准d轴电压
    uint16 Ramp_count;                         // 校准锁定电压渐升步数
    uint16 Ramp_ms;                            // 校准锁定电压渐升步间隔
    uint16 Hold_ms;                            // 校准起始定位保持时间
    uint16 Step_count;                         // 零点牵引步数
    uint16 Step_ms;                            // 零点牵引步间隔
    uint16 Sample_count;                       // 零点位置平均采样次数
    uint16 Sample_ms;                          // 零点位置采样间隔
    int32  Min_travel;                         // 判定编码器有效的最小累计行程
} Motor_ZeroCalib_t;

extern const Motor_ZeroCalib_t Motor_zeroCalib;

/*===========================================================================*/
/*  电机控制模式                                                              */
/*===========================================================================*/
typedef enum
{
    MOTOR_CONTROL_OPEN_LOOP = 0,               // 开环电压矢量控制
    MOTOR_CONTROL_ENCODER_FOC,                 // 有感FOC
    MOTOR_CONTROL_SENSORLESS_FOC,              // 无感FOC
    MOTOR_CONTROL_VOICE                        // 电机音乐播放控制
} Motor_control_mode_t;

/*===========================================================================*/
/*  有感FOC子控制模式                                                         */
/*===========================================================================*/
typedef enum
{
    MOTOR_FOC_CURRENT = 1,                      // 电流环控制
    MOTOR_FOC_SPEED,                            // 速度环级联电流环
    MOTOR_FOC_POSITION                          // 位置环级联速度环和电流环
} Motor_foc_mode_t;

/*===========================================================================*/
/*  位置环回正方式                                                            */
/*===========================================================================*/
typedef enum
{
    MOTOR_POSITION_RETURN_SHORTEST = 0,         // 按最近距离回正
    MOTOR_POSITION_RETURN_REVERSE_PATH          // 沿偏转路径的反方向原路回正
} Motor_position_return_mode_t;

/*===========================================================================*/
/*  编码器配置及角度反馈                                                      */
/*===========================================================================*/
typedef struct
{
    menc15a_module_enum Sensor_id;              // 编码器模块编号
    int8 Direction;                             // 编码器方向，取值为+1或-1
    uint16 Zero_offset;                         // 机械角零偏
    uint16 Mechanical_angle;                    // 机械角，范围0~32767
    uint16 Electrical_angle;                    // 电角度，范围0~32767
    float Spd_rpm;                              // 滤波后的机械转速，单位为转/分钟
} Motor_Encoder_t;

/*===========================================================================*/
/*  电机输出状态                                                              */
/*===========================================================================*/
typedef struct
{
    int16 Duty_target;                          // 输出幅值目标，范围-10000~10000
    float Duty_output;                          // 实际输出幅值
} Motor_Output_t;

/*===========================================================================*/
/*  开环控制状态                                                              */
/*===========================================================================*/
typedef struct
{
    float Uq;                                   // 开环q轴电压指令，单位为V
    uint16 Angle;                               // 开环电压矢量电角度
    int16 Step;                                 // 单周期电角度增量，负值表示反向
    uint16 Align_count;                         // 启动定向所需控制周期数
    uint16 Hold_count;                          // 启动定向计数
    uint8 Started;                              // 开环启动状态
} Motor_OpenLoop_t;

/*===========================================================================*/
/*  FOC电流环控制对象                                                         */
/*===========================================================================*/
typedef struct
{
    float Id_target;                            // d轴电流目标，单位为A
    float Iq_target;                            // q轴电流目标，单位为A
    PID_t Id_pid;                               // d轴电流调节器
    PID_t Iq_pid;                               // q轴电流调节器
    uint16 Bandwidth;                           // 电流环带宽，单位为Hz
    float Ud_output;                            // d轴电压输出，单位为V
    float Uq_output;                            // q轴电压输出，单位为V
} Foc_CurrentLoop_t;

/*===========================================================================*/
/*  FOC速度环控制对象                                                         */
/*===========================================================================*/
typedef struct
{
    float Command_rpm;                          // 上位机下发的原始速度目标，单位为rpm
    float Target_rpm;                           // 斜坡处理后的速度目标，单位为rpm
    float Ramp_rate;                            // 速度斜坡速率，单位为rpm/s
    PID_t Pid;                                  // 速度调节器，原始输出由速度环按Iq能力限幅
    float Integral_limit;                       // 速度环积分项配置限幅，单位为A
    float Iq_output;                            // 速度环输出，单位为A
} Foc_SpeedLoop_t;

/*===========================================================================*/
/*  FOC位置环控制对象                                                         */
/*===========================================================================*/
typedef struct
{
    float Target_degree;                        // 位置目标，单位为度
    PID_t Pid;                                  // 位置纯Kp调节器，输出为速度目标
    float Speed_output;                         // 位置环输出，单位为rpm
    float Deadband_degree;                      // 位置角度死区，单位为度
    float Soft_range_degree;                    // 到位线性软化范围，单位为度
    float Speed_deadband_rpm;                   // 到位速度死区，单位为rpm
    float Travel_degree;                        // 相对目标位置的连续偏转角度，单位为度
    float Last_degree;                          // 上次位置环采样角度，单位为度
    float Last_target_degree;                   // 上次跟踪的位置目标，单位为度
    Motor_position_return_mode_t Return_mode;   // 位置环回正方式
    uint8 Track_ready;                          // 连续偏转角度跟踪有效标志
    uint8 In_deadband;                          // 位置误差已进入角度死区标志
} Foc_PositionLoop_t;

/*===========================================================================*/
/*  FOC电机控制对象                                                           */
/*===========================================================================*/
typedef struct
{
    Motor_Encoder_t Encoder;                    // 编码器配置及角度反馈
    Motor_Output_t Output;                      // 电机输出状态
    Motor_OpenLoop_t Open_loop;                 // 开环控制状态
    Foc_CurrentLoop_t Current_loop;             // 电流环对象
    Foc_SpeedLoop_t Speed_loop;                 // 速度环对象
    Foc_PositionLoop_t Position_loop;           // 位置环对象
    float Ab_filter_bandwidth;                  // 当前生效的AB滤波器带宽，单位为Hz

    uint8 Pole_pairs;                           // 电机极对数
    Motor_control_mode_t Control_mode;          // 当前电机控制模式
    Motor_foc_mode_t Foc_mode;                  // 当前有感FOC子模式
    int8 Foc_direction;                         // 有感FOC目标方向，取值为+1或-1
    uint8 Zero_ready;                           // 编码器零点参数有效标志
} Foc_motor_t;


/*===========================================================================*/
/*  总控制结构体                                                              */
/*===========================================================================*/
typedef struct
{
    Foc_motor_t motor;                           // 电机控制对象
    uint8 ready;                                 // 校准就绪标志
    uint8 calibrating;                           // 校准进行中标志

} Motor_Control_t;

extern Foc_motor_t Motor;                        // 电机控制对象

/***********************************************
 * @brief : 使用Motor中的参数初始化FOC电流环、速度环和位置环
 * @param : 无
 * @return: 无
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
void Motor_Control_Init(void);

/***********************************************
 * @brief : 更新有感FOC电流环带宽并重算PI增益
 * @param : BandwidthHz 电流环带宽，单位为Hz
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void Motor_Control_SetCurrentBandwidth(uint16 BandwidthHz);

/***********************************************
 * @brief : 更新速度环PI参数
 * @param : Kp 比例增益
 * @param : Ki 连续时间积分增益
 * @param : IntegralLimit 积分项输出限幅，单位为A
 * @return: 无
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
void Motor_Control_SetSpeedPi(float Kp,
                              float Ki,
                              float IntegralLimit);

/***********************************************
 * @brief : 更新位置环纯Kp、限幅、死区及到位软化参数
 * @param : Kp 比例增益
 * @param : OutputLimit 输出限幅，单位为rpm
 * @param : Deadband_degree 角度死区，单位为度
 * @param : SoftRange_degree 到位线性软化范围，单位为度，不大于角度死区时关闭
 * @param : SpeedDeadband_rpm 到位速度死区，单位为rpm，填0时关闭
 * @return: 无
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
void Motor_Control_SetPositionKp(float Kp,
                                 float OutputLimit,
                                 float Deadband_degree,
                                 float SoftRange_degree,
                                 float SpeedDeadband_rpm);

/***********************************************
 * @brief : 更新电机机械角度和电角度
 * @param : 无
 * @return: 无
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
void Angle_Update(void);

/***********************************************
 * @brief : 读取扣除零偏并修正方向后的机械角度
 * @param : 无
 * @return: 机械角度，范围0~360度
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
float Motor_Control_GetMechanicalDegree(void);

/***********************************************
 * @brief : 使用AB滤波器计算电机机械转速，需按1 kHz周期调用
 * @param : 无
 * @return: 无，结果保存到Motor.Encoder.Spd_rpm
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
void RPM_Cal(void);

/***********************************************
 * @brief : 在主循环中阻塞执行桥臂自检及编码器零点校准
 * @param : 无
 * @return: 无，校准结果保存到Motor，Zero_ready表示是否成功
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void Zero_Calibration(void);

/***********************************************
 * @brief : 按20 kHz时基执行总控，并分频运行1 kHz速度环和500 Hz位置环
 * @param : 无
 * @return: 无
 * @date  : 2026-08-27
 * @author: L
 ************************************************/
void Motor_Control_Loop(void);

#endif
