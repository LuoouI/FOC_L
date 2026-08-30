#include "My_Key.h"
#include "Foc_voice/Foc_voice.h"
#include "Motor_Control/Motor_Control.h"
#include "My_TCPWM/My_TCPWM.h"

void My_Key_Service(void)
{
    if (KEY_SHORT_PRESS == key_get_state(KEY_2))
    {
        key_clear_state(KEY_2);
        if (Foc_voice_IsPlaying() != 0u)
        {
            Foc_voice_Stop();
        }
        else
        {
            Foc_voice_Start();
        }
    }

    if (KEY_SHORT_PRESS == key_get_state(KEY_3))
    {
        key_clear_state(KEY_3);
        Foc_voice_Stop();
        Motor.Open_loop.Uq = 0.5f;
        Motor.Open_loop.Step = 10;
        Motor.Control_mode = MOTOR_CONTROL_OPEN_LOOP;
    }

}
