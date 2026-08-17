#include "My_Key.h"
#include "My_TCPWM\My_TCPWM.h"

void My_Key_Service(void)
{
    static uint16 a = 0;
    if (KEY_SHORT_PRESS == key_get_state(KEY_2))
    {
        key_clear_state(KEY_2);
        a += 1000;
        My_TCPWM_SetDuty(a,a,a);
    }

}