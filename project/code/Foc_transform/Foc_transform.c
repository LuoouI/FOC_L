#include "Foc_transform.h"
#include "Fast_sin/Fast_sin.h"
#include "Function/Function.h"

Clark_t foc_clark_calc(float CurrentA, float CurrentB)
{
    Clark_t Clark;

    Clark.Alpha = CurrentA;
    Clark.Beta = (CurrentA + 2.0f * CurrentB) / SQRT3;

    return Clark;
}

Park_t foc_park_calc(Clark_t Clark, uint16 ElectricalAngle)
{
    Park_t Park;

    float SinValue = fast_sinf(ElectricalAngle);
    float CosValue = fast_cosf(ElectricalAngle);

    Park.Id = Clark.Alpha * CosValue + Clark.Beta * SinValue;
    Park.Iq = -Clark.Alpha * SinValue + Clark.Beta * CosValue;

    return Park;
}

AlphaBeta_t foc_ipark_calc(InversePark_t InversePark,
                           uint16 ElectricalAngle)
{
    AlphaBeta_t AlphaBeta;

    float SinValue = fast_sinf(ElectricalAngle);
    float CosValue = fast_cosf(ElectricalAngle);

    AlphaBeta.Ualpha = InversePark.Ud * CosValue - InversePark.Uq * SinValue;
    AlphaBeta.Ubeta = InversePark.Ud * SinValue + InversePark.Uq * CosValue;

    return AlphaBeta;
}
