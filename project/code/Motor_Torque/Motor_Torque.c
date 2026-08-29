#include "Motor_Torque.h"
#include "Function/Function.h"
#include <float.h>

volatile Torque_t Torque =
{
    .Kv = 420.0f,
    .Kt = 60.0f / (TWO_PI * 420.0f),
    .Gear_ratio = 1.0f,
    .Motor_torque = 0.0f,
    .Ready = 1u
};

void Motor_Torque_Estimate(float Iq)
{
    if ((Iq != Iq) ||
        (Torque.Kv != Torque.Kv) ||
        (Torque.Gear_ratio != Torque.Gear_ratio) ||
        (Iq > FLT_MAX) ||
        (Iq < -FLT_MAX) ||
        (Torque.Kv <= 0.0f) ||
        (Torque.Kv > FLT_MAX) ||
        (Torque.Gear_ratio <= 0.0f) ||
        (Torque.Gear_ratio > FLT_MAX))
    {
        Torque.Kt = 0.0f;
        Torque.Motor_torque = 0.0f;
        Torque.Ready = 0u;
    }

    Torque.Kt = 60.0f / (TWO_PI * Torque.Kv);
    Torque.Motor_torque = Torque.Kt * Iq * Torque.Gear_ratio;
    Torque.Ready = 1u;

}
