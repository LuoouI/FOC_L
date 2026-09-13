#include "Function.h"

/***********************************************
 * @brief : 对浮点数进行双边限幅
 * @param : Value 待限幅数值
 * @param : Min 下限
 * @param : Max 上限
 * @return: 限幅后的数值
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
float Float_Limit(float Value, float Min, float Max)
{
    if (Value < Min)
    {
        return Min;
    }

    if (Value > Max)
    {
        return Max;
    }

    return Value;
}

/***********************************************
 * @brief : 对32位有符号整数进行双边限幅
 * @param : Value 待限幅数值
 * @param : Min 下限
 * @param : Max 上限
 * @return: 限幅后的数值
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
int32 Int_Limit(int32 Value, int32 Min, int32 Max)
{
    if (Value < Min)
    {
        return Min;
    }

    if (Value > Max)
    {
        return Max;
    }

    return Value;
}

/***********************************************
 * @brief : 将角度归一化到一个周期内
 * @param : Angle 待归一化角度
 * @return: 0~32767范围内的单圈角度
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
uint16 Angle_Wrap(int32 Angle)
{
    Angle %= (int32)ANGLE_PERIOD;
    if (Angle < 0)
    {
        Angle += (int32)ANGLE_PERIOD;
    }

    return (uint16)Angle;
}

/***********************************************
 * @brief : 清除角度解缠状态
 * @param : Unwrap 角度解缠状态
 * @return: void
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
void Angle_Unwrap_Clear(AngleUnwrap_t *Unwrap)
{
    if (Unwrap == NULL)
    {
        return;
    }

    Unwrap->Last = 0u;
    Unwrap->Value = 0;
    Unwrap->Ready = 0u;
}

/***********************************************
 * @brief : 将单圈角度转换为连续多圈角度，相邻采样变化需小于半圈
 * @param : Unwrap 角度解缠状态，首次使用前需清除
 * @param : Angle 当前单圈角度，范围0~32767
 * @return: 连续多圈角度
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
int32 Angle_Unwrap(AngleUnwrap_t *Unwrap, uint16 Angle)
{
    int32 Delta;

    Angle = Angle_Wrap((int32)Angle);
    if (Unwrap == NULL)
    {
        return (int32)Angle;
    }

    if (Unwrap->Ready == 0u)
    {
        Unwrap->Last = Angle;
        Unwrap->Value = (int32)Angle;
        Unwrap->Ready = 1u;
        return Unwrap->Value;
    }

    Delta = (int32)Angle - (int32)Unwrap->Last;
    if (Delta > ANGLE_HALF_PERIOD)
    {
        Delta -= (int32)ANGLE_PERIOD;
    }
    else if (Delta < -ANGLE_HALF_PERIOD)
    {
        Delta += (int32)ANGLE_PERIOD;
    }

    Unwrap->Last = Angle;
    Unwrap->Value += Delta;

    return Unwrap->Value;
}
