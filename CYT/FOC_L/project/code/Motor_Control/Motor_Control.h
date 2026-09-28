#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "zf_common_headfile.h"
#include "Foc_config.h"
#include "Foc_transform/Foc_transform.h"
#include "PID/PID.h"
#include "Function/Function.h"
#include "Motor_Control/Foc/Foc_Loop.h"
#include "Motor_Control/Observer/PLL.h"
#include "Motor_Control/Observer/SMO.h"
#include "Motor_Control/Observer/HFI.h"
#include "Motor_Control/Feedback/Encoder_Feedback.h"
#include "Motor_Control/Startup/Open_Loop.h"
#include "Motor_Control/Startup/Zero_Calibration.h"

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
/*  电机输出状态                                                              */
/*===========================================================================*/
typedef struct
{
    int16 Duty_target;            /* 输出幅值目标，范围-10000~10000 */
    float Duty_output;            /* 实际输出幅值 */
} Motor_Output_t;

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
void    Motor_Control_Init          (void);
void    Motor_Control_ResetObserver (void);
void    Motor_Control_Loop          (void);
/*==================================================== 基础函数 ====================================================*/

#endif
