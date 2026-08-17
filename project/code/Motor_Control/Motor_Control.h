#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "zf_common_headfile.h"
#include "Foc_transform/Foc_transform.h"

#define MOTOR_CONTROL_PERIOD_US               (50u)   // 电机控制周期
#define MOTOR_OPEN_LOOP_DEFAULT_STEP          (3u)    // 单周期默认电角度增量
#define MOTOR_OPEN_LOOP_ALIGN_MS              (200u)  // 开环启动定向时间
#define MOTOR_OPEN_LOOP_ALIGN_COUNT           ((MOTOR_OPEN_LOOP_ALIGN_MS * 1000u) / MOTOR_CONTROL_PERIOD_US)

#define MOTOR_ZERO_CALIBRATION_VOLTAGE        (1.5f)  // 零点校准d轴电压
#define MOTOR_ZERO_CALIBRATION_HOLD_MS        (200u)  // 校准起始定位保持时间
#define MOTOR_ZERO_CALIBRATION_STEP_COUNT     (100u)  // 零点牵引步数
#define MOTOR_ZERO_CALIBRATION_STEP_MS        (8u)    // 零点牵引步间隔
#define MOTOR_ZERO_CALIBRATION_SAMPLE_COUNT   (16u)   // 零点位置平均采样次数
#define MOTOR_ZERO_CALIBRATION_SAMPLE_MS      (2u)    // 零点位置采样间隔
#define MOTOR_ZERO_CALIBRATION_MIN_TRAVEL     (400)   // 判定编码器有效的最小累计行程
#define MOTOR_ZERO_CALIBRATION_MAX_POLE_PAIRS (64u)  // 校准允许的最大极对数

/*===========================================================================*/
/*  电机控制模式                                                              */
/*===========================================================================*/
typedef enum
{
    MOTOR_CONTROL_OPEN_LOOP = 0,                // 开环电压矢量控制
    MOTOR_CONTROL_ENCODER_FOC                    // 磁编码器电压矢量控制
} Motor_control_mode_t;

/*===========================================================================*/
/*  驱动对象                                                                  */
/*===========================================================================*/
typedef struct Foc_motor_init_target
{
    menc15a_module_enum sensor_id;              // 编码器模块编号
    int8   direction;                           // 编码器方向（+1 / -1）
    uint16 zero_offset;                         // 机械角零偏
    uint8  pole_pairs;                          // 极对数
    uint16 mechanical_angle;                    // 机械角（0~32767）
    uint16 electrical_angle;                    // 电角度（0~32767）
    Clark_t clark;                              // Clark 变换结果
    Park_t  park;                               // Park 变换结果

    // Foc_CurrentLoop_t  current_loop;         // 电流环对象
    // Foc_SpeedLoop_t    speed_loop;           // 速度环对象
    // Foc_PositionLoop_t position_loop;        // 位置环对象

    int16  motor_duty;                          // 输出幅值指令，范围-10000~10000
    float  ud;                                  // d轴最大电压，单位为V
    float  uq;                                  // q轴最大电压，单位为V
    Motor_control_mode_t control_mode;          // 当前电机控制模式
    uint16 open_loop_angle;                     // 开环电压矢量电角度
    uint16 open_loop_step;                      // 开环单周期电角度增量
    uint16 open_loop_hold_count;                // 开环启动定向计数
    uint8  open_loop_started;                   // 开环启动状态
    uint8  ready;                               // 零点参数有效标志

} Foc_motor_t;


/*===========================================================================*/
/*  总控制结构体                                                              */
/*===========================================================================*/
typedef struct
{
    Foc_motor_t      motor;              // 电机
    uint8            ready;              // 校准就绪标志
    uint8            calibrating;        // 校准进行中标志

} Motor_Control_t;

extern Foc_motor_t Motor;                // 电机控制对象

/***********************************************
 * @brief : 更新电机机械角度和电角度
 * @param : motor 电机控制对象
 * @return: void
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
void Angle_Update(Foc_motor_t *motor);

/***********************************************
 * @brief : 初始化电机控制状态
 * @param : motor 电机控制对象
 * @return: void
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
void Foc_Init(Foc_motor_t *motor);

/***********************************************
 * @brief : 停止电机电压输出，三相PWM保持运行并输出零线电压
 * @param : motor 电机控制对象
 * @return: void
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
void Foc_Stop(Foc_motor_t *motor);

/***********************************************
 * @brief : 设置电机控制模式并停止当前输出
 * @param : motor 电机控制对象
 * @param : mode 目标控制模式
 * @return: void
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
void Foc_Set_Control_Mode(Foc_motor_t *motor, Motor_control_mode_t mode);

/***********************************************
 * @brief : 根据当前模式执行一次电压矢量控制周期
 * @param : motor 电机控制对象
 * @return: void
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
void Foc_Run(Foc_motor_t *motor);

/***********************************************
 * @brief : 执行电机编码器零点、方向和极对数校准
 * @param : motor 电机控制对象
 * @return: 0校准成功，1校准失败
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
uint8 Motor_Zero_Calibration(Foc_motor_t *motor);

#endif
