#include "Get_time.h"

/***********************************************
 * @brief : 初始化固定整数分频计时器，首次调用立即触发
 * @param : Timer 计时器对象，由调用者保证有效
 * @param : Period 分频周期，单位为基准节拍，由调用者保证大于0
 * @return: 无
 * @date  : 2026-09-20
 * @author: L
 ************************************************/
void Get_Time_Init(Time_Divider_t *Timer, uint32 Period)
{
    Timer->Count = 0u;
    Timer->Period = Period;
}

/***********************************************
 * @brief : 复位固定整数分频计时器，使下一次调用立即触发
 * @param : Timer 计时器对象，由调用者保证已初始化
 * @return: 无
 * @date  : 2026-09-20
 * @author: L
 ************************************************/
void Get_Time_Reset(Time_Divider_t *Timer)
{
    Timer->Count = 0u;
}

/***********************************************
 * @brief : 更新固定整数分频计时器并判断当前节拍是否触发
 * @param : Timer 计时器对象，由调用者保证已初始化且周期大于0
 * @return: 1 当前节拍触发，0 当前节拍不触发
 * @date  : 2026-09-20
 * @author: L
 ************************************************/
uint8 Get_Time(Time_Divider_t *Timer)
{
    if (Timer->Count == 0u)
    {
        Timer->Count = Timer->Period - 1u;
        return 1u;
    }

    Timer->Count--;
    return 0u;
}
