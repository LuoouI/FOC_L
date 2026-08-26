#include "Motor_Control.h"

/*===========================================================================*/
/*  前期准备                                                                  */
/*===========================================================================*/
void Angle_Update(void)
{
    Motor.mechanical_angle = menc15a_get_absolute_data(Motor.sensor_id);


}