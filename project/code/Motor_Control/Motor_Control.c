#include "Motor_Control.h"
#include "Function/Function.h"


Foc_motor_t Motor = {
    .sensor_id = menc15a_1_module,
    .direction = 1,
    .zero_offset = 0,
    .pole_pairs = 7,
    .mechanical_angle = 0,
    .electrical_angle = 0,
    .clark = {0.0f, 0.0f},
    .park = {0.0f, 0.0f},
    .enable = 0,
    .electrical_step = 100,
    .ud = 0.0f,
    .uq = 0.0f
};

void Angle_Update(Foc_motor_t *motor)
{
    // 获取机械角度
    motor->mechanical_angle = menc15a_get_absolute_data(motor->sensor_id);

    // 计算电角度
    motor->electrical_angle = Angle_Wrap(
        (int32)motor->mechanical_angle * (int32)motor->pole_pairs +
        (int32)motor->zero_offset);
}

/*===========================================================================*/
/*  开环驱动部分                                                              */
/*===========================================================================*/

void Motor_Control_OpenLoop_Init(Foc_motor_t *motor, uint8 enable, int8 direction, uint16 electrical_step, float ud, float uq)
{
    motor->enable = enable;
    motor->direction = direction;
    motor->electrical_step = electrical_step;
    motor->ud = ud;
    motor->uq = uq;
}
