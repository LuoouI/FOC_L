#include "Function.h"

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

uint16 Angle_Wrap(int32 Angle)
{
    Angle %= (int32)ANGLE_PERIOD;
    if (Angle < 0)
    {
        Angle += (int32)ANGLE_PERIOD;
    }

    return (uint16)Angle;
}

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
