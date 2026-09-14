#include "My_Key.h"
#include "Foc_voice/Foc_voice.h"
#include "Motor_Control/Motor_Control.h"
#include "My_TCPWM/My_TCPWM.h"

/***********************************************
 * @brief : 按键服务函数
 * @param : /
 * @return: void
 * @date  : 2026-08-14
 * @author: L
 ************************************************/
void My_Key_Service(void)
{
    if (KEY_SHORT_PRESS == key_get_state(KEY_2))
    {
        Motor.Control_mode = MOTOR_CONTROL_ENCODER_FOC;
        Motor.Encoder.Spd_rpm = 3000;
    }

    if (KEY_SHORT_PRESS == key_get_state(KEY_3))
    {
        key_clear_state(KEY_3);
        Motor.Control_mode = MOTOR_CONTROL_ENCODER_FOC;
        Motor.Foc_mode = MOTOR_FOC_SPEED;
        Motor.Speed_loop.Command_rpm = 3000.0f;

    }

}
