#include "My_Key.h"
#include "FOC_Voice/FOC_Voice.h"
#include "Motor_Control/Motor_Control.h"
#include "My_TCPWM/My_TCPWM.h"

void My_Key_Service(void)
{
    if (KEY_SHORT_PRESS == key_get_state(KEY_2))
    {
        key_clear_state(KEY_2);
        if (FOC_Voice_IsPlaying() != 0u)
        {
            FOC_Voice_Stop();
        }
        else
        {
            FOC_Voice_Start();
        }
    }

    if (KEY_SHORT_PRESS == key_get_state(KEY_3))
    {
        key_clear_state(KEY_3);
        FOC_Voice_Stop();
        Motor.Open_loop.Uq = 0.5f;
        Motor.Open_loop.Step = 10;
        Motor.Control_mode = MOTOR_CONTROL_OPEN_LOOP;
    }

}
