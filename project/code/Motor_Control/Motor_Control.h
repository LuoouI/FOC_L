#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "zf_common_headfile.h"
#include "Foc_transform/Foc_transform.h"
#include "PID/PID.h"
#include "Function/Function.h"

/*===========================================================================*/
/*  电机零点校准参数                                                          */
/*===========================================================================*/
typedef struct
{
    float  Voltage;                      // 零点校准d轴电压
    uint16 Ramp_count;                   // 校准锁定电压渐升步数
    uint16 Ramp_ms;                      // 校准锁定电压渐升步间隔
    uint16 Hold_ms;                      // 校准起始定位保持时间
    uint16 Step_count;                   // 零点牵引步数
    uint16 Step_ms;                      // 零点牵引步间隔
    uint16 Sample_count;                 // 零点位置平均采样次数
    uint16 Sample_ms;                    // 零点位置采样间隔
    int32  Min_travel;                   // 判定编码器有效的最小累计行程
} Motor_ZeroCalib_t;

extern const Motor_ZeroCalib_t Motor_zeroCalib;

/*===========================================================================*/
/*  电机控制模式                                                              */
/*===========================================================================*/
typedef enum
{
    MOTOR_CONTROL_OPEN_LOOP = 0,                // 开环电压矢量控制
    MOTOR_CONTROL_ENCODER_FOC,                  // 磁编码器电压矢量控制
    MOTOR_CONTROL_VOICE                         // 电机音乐播放控制
} Motor_control_mode_t;

/*===========================================================================*/
/*  编码器配置及角度反馈                                                      */
/*===========================================================================*/
typedef struct
{
    menc15a_module_enum Sensor_id;              // 编码器模块编号
    int8 Direction;                             // 编码器方向，取值为+1或-1
    uint16 Zero_offset;                         // 机械角零偏
    uint16 Mechanical_angle;           // 机械角，范围0~32767
    uint16 Electrical_angle;                    // 电角度，范围0~32767
    float Spd_rpm;                     // 滤波后的机械转速，单位为转/分钟
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
    float Ud_output;                            // d轴电压输出，单位为V
    float Uq_output;                            // q轴电压输出，单位为V
} Foc_CurrentLoop_t;

/*===========================================================================*/
/*  FOC电机控制对象                                                           */
/*===========================================================================*/
typedef struct
{
    Motor_Encoder_t Encoder;                    // 编码器配置及角度反馈
    Motor_Output_t Output;                      // 电机输出状态
    Motor_OpenLoop_t Open_loop;                 // 开环控制状态
    Foc_CurrentLoop_t Current_loop;             // 电流环对象

    uint8 Pole_pairs;                           // 电机极对数
    Motor_control_mode_t Control_mode;          // 当前电机控制模式
    uint8 Zero_ready;                           // 编码器零点参数有效标志
} Foc_motor_t;


/*===========================================================================*/
/*  总控制结构体                                                              */
/*===========================================================================*/
typedef struct
{
    Foc_motor_t motor;                            // 电机控制对象
    uint8 ready;                                  // 校准就绪标志
    uint8 calibrating;                            // 校准进行中标志

} Motor_Control_t;

extern Foc_motor_t Motor;                // 电机控制对象

/***********************************************
 * @brief : 更新电机机械角度和电角度
 * @param : 无
 * @return: 无
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
void Angle_Update(void);

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
 * @brief : 根据当前控制模式执行一次电机控制周期
 * @param : 无
 * @return: 无
 * @date  : 2026-08-27
 * @author: L
 ************************************************/
void Motor_Control_Loop(void);

#endif
