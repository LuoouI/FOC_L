#include "My_Key.h"
#include "Motor_Control/Motor_Control.h"
#include "My_TCPWM/My_TCPWM.h"

void My_Key_Service(void)
{
    if (KEY_LONG_PRESS == key_get_state(KEY_1))
    {
        key_clear_state(KEY_1);
        Motor_Zero_Calibration(&Motor);
    }

    if (KEY_SHORT_PRESS == key_get_state(KEY_2))
    {
        key_clear_state(KEY_2);

        if (Motor.motor_duty >= (int16)TCPWM_DUTY_MAX)
        {
            Motor.motor_duty = 0;
        }
        else
        {
            Motor.motor_duty += 1000;
        }
    }

    if (KEY_SHORT_PRESS == key_get_state(KEY_3))
    {
        key_clear_state(KEY_3);
        Motor.motor_duty = -Motor.motor_duty;
    }
}
