#include "Motor_Control.h"

/*===========================================================================*/
/*  前期准备                                                                  */
/*===========================================================================*/

void Angle_Update(void)
{
    int32 MechanicalAngle;

    Motor.Encoder.Mechanical_angle =
        menc15a_get_absolute_data(Motor.Encoder.Sensor_id);

    MechanicalAngle =
        ((int32)Motor.Encoder.Mechanical_angle -
         (int32)Motor.Encoder.Zero_offset) *
        (int32)Motor.Encoder.Direction;

    Motor.Encoder.Electrical_angle =
        Angle_Wrap(MechanicalAngle * (int32)Motor.Pole_pairs);
}

void RPM_Cal(void)
{
}
