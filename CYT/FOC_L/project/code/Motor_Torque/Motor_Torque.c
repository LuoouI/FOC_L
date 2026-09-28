#include "Motor_Torque.h"
#include "Function/Function.h"
#include "Current_sample/Current_sample.h"

volatile Torque_t Torque =
{
    .Kv = 420.0f,
    .Kt = 60.0f / (TWO_PI * 420.0f),
    .Gear_ratio = 1.0f,
    .Motor_torque = 0.0f,
    .Ready = 1u
};

/***********************************************
 * @brief : 根据q轴电流估算电机理想输出转矩
 * @param : Iq q轴电流，单位为A
 * @return: /
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
void Motor_Torque_Estimate(void)
{
    Torque.Kt = 60.0f / (TWO_PI * Torque.Kv);
    Torque.Motor_torque = Torque.Kt * Current.park.Iq * Torque.Gear_ratio;
    
    Torque.Ready = 1u;

}
