#ifndef MOTOR_CONTROL_FOC_LOOP_H
#define MOTOR_CONTROL_FOC_LOOP_H

#include "zf_common_headfile.h"
#include "PID/PID.h"

/*===========================================================================*/
/*  FOC子控制模式                                                             */
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

/*==================================================== 参数配置 ====================================================*/
void    Motor_Control_SetCurrentBandwidth (uint16 BandwidthHz);
void    Motor_Control_SetSpeedPi          (float Kp,
                                           float Ki,
                                           float IntegralLimit);
void    Motor_Control_SetPositionKp       (float Kp,
                                           float OutputLimit,
                                           float Deadband_degree,
                                           float SoftRange_degree,
                                           float SpeedDeadband_rpm);
/*==================================================== 参数配置 ====================================================*/

/*==================================================== 环路控制 ====================================================*/
void    Foc_Loop_StopOutput     (void);
void    Foc_CurrentLoop_Update  (void);
void    Foc_SpeedLoop_Update    (void);
void    Foc_PositionLoop_Update (void);
/*==================================================== 环路控制 ====================================================*/

#endif
