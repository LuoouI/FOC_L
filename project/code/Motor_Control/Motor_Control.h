#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "zf_common_headfile.h"
#include "Foc_transform/Foc_transform.h"
#include "PID/PID.h"

#define MOTOR_CONTROL_PERIOD_US               (50u)   // 电机控制周期
#define MOTOR_OPEN_LOOP_DEFAULT_STEP          (60u)   // 单周期默认电角度增量，约314转/分钟（7对极、50us周期）
#define MOTOR_OPEN_LOOP_ALIGN_MS              (200u)  // 开环启动定向时间
#define MOTOR_OPEN_LOOP_ALIGN_COUNT           ((MOTOR_OPEN_LOOP_ALIGN_MS * 1000u) / MOTOR_CONTROL_PERIOD_US)
#define MOTOR_DUTY_RAMP_DEFAULT_STEP          (0.05f) // 单控制周期默认占空比斜坡增量

/*===========================================================================*/
/*  电机零点校准参数                                                          */
/*===========================================================================*/
typedef struct
{
    float  Voltage;                      // 零点校准d轴电压
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
    MOTOR_CONTROL_ENCODER_FOC                   // 磁编码器电压矢量控制
} Motor_control_mode_t;

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
} Motor_Encoder_t;

/*===========================================================================*/
/*  电机输出状态                                                              */
/*===========================================================================*/
typedef struct
{
    int16 Duty_target;                          // 输出幅值目标，范围-10000~10000
    float Duty_output;                          // 斜坡处理后的实际输出幅值
    float Duty_ramp_step;                       // 单控制周期输出幅值变化量
} Motor_Output_t;

/*===========================================================================*/
/*  开环控制状态                                                              */
/*===========================================================================*/
typedef struct
{
    uint16 Angle;                               // 开环电压矢量电角度
    int16 Step;                                 // 单周期电角度增量，负值表示反向
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
    Motor_Encoder_t Encoder;                   // 编码器配置及角度反馈
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
 * @brief : 计算电机转速
 * @param : 无
 * @return: 无
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
void RPM_Cal(void);

#endif
