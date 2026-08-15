#include "Foc_transform.h"
#include "Fast_sin/Fast_sin.h"

FocClark_t foc_clark_calc(float CurrentA, float CurrentB)
{
    FocClark_t Clark;

    Clark.Alpha = CurrentA;
    Clark.Beta = (CurrentA + 2.0f * CurrentB) / FOC_SQRT3;

    return Clark;
}

FocPark_t foc_park_calc(FocClark_t Clark, uint16 ElectricalAngle)
{
    FocPark_t Park;
    float SinValue;
    float CosValue;

    SinValue = fast_sinf(ElectricalAngle);
    CosValue = fast_cosf(ElectricalAngle);

    Park.Id = Clark.Alpha * CosValue + Clark.Beta * SinValue;
    Park.Iq = -Clark.Alpha * SinValue + Clark.Beta * CosValue;

    return Park;
}

FocAlphaBeta_t foc_ipark_calc(FocInversePark_t InversePark,
                              uint16 ElectricalAngle)
{
    FocAlphaBeta_t AlphaBeta;
    float SinValue;
    float CosValue;

    SinValue = fast_sinf(ElectricalAngle);
    CosValue = fast_cosf(ElectricalAngle);

    AlphaBeta.Ualpha = InversePark.Ud * CosValue - InversePark.Uq * SinValue;
    AlphaBeta.Ubeta = InversePark.Ud * SinValue + InversePark.Uq * CosValue;

    return AlphaBeta;
}
