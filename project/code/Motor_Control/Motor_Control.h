#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "zf_common_headfile.h"
#include "Foc_transform/Foc_transform.h"

/*===========================================================================*/
/*  闭环驱动对象                                                        */
/*===========================================================================*/
typedef struct Foc_motor_init_target
{
    menc15a_module_enum sensor_id;              // 编码器模块编号
    int8   direction;                           // 旋转方向（+1 / -1）
    uint16 zero_offset;                         // 电角度零偏
    uint8  pole_pairs;                          // 极对数
    uint16 mechanical_angle;                    // 机械角（0~32767）
    uint16 electrical_angle;                    // 电角度（0~32767）
    Clark_t clark;                              // Clark 变换结果
    Park_t  park;                               // Park 变换结果

    // Foc_CurrentLoop_t  current_loop;         // 电流环对象
    // Foc_SpeedLoop_t    speed_loop;           // 速度环对象
    // Foc_PositionLoop_t position_loop;        // 位置环对象

    /*开环驱动对象*/
    uint8  enable;                              // 开环使能标志
    uint16 electrical_step;                     // 每步电角度增量
    float  ud;                                  // d 轴电压
    float  uq;                                  // q 轴电压

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

#endif
