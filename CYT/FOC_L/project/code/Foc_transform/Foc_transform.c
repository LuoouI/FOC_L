#include "Foc_transform.h"
#include "Fast_sin/Fast_sin.h"
#include "Function/Function.h"

/***********************************************
 * @brief : 对两相电流进行Clark变换
 * @param : CurrentA A相电流
 * @param : CurrentB B相电流
 * @return: Clark变换结果
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
Clark_t foc_clark_calc(float CurrentA, float CurrentB)
{
    Clark_t Clark;

    Clark.Alpha = CurrentA;
    Clark.Beta = (CurrentA + 2.0f * CurrentB) / SQRT3;

    return Clark;
}

/***********************************************
 * @brief : 对Alpha/Beta分量进行Park变换
 * @param : Clark Clark变换结果
 * @param : ElectricalAngle 电角度，0~32767对应0~2PI
 * @return: Park变换结果
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
Park_t foc_park_calc(Clark_t Clark, uint16 ElectricalAngle)
{
    Park_t Park;

    float SinValue = fast_sinf(ElectricalAngle);
    float CosValue = fast_cosf(ElectricalAngle);

    Park.Id = Clark.Alpha * CosValue + Clark.Beta * SinValue;
    Park.Iq = -Clark.Alpha * SinValue + Clark.Beta * CosValue;

    return Park;
}

/***********************************************
 * @brief : 对d/q轴分量进行逆Park变换
 * @param : InversePark 逆Park变换输入
 * @param : ElectricalAngle 电角度，0~32767对应0~2PI
 * @return: Alpha/Beta轴输出
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
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
