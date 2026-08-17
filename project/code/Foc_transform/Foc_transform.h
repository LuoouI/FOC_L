#ifndef FOC_TRANSFORM_H
#define FOC_TRANSFORM_H

#include "zf_common_headfile.h"

#define FOC_SQRT3    (1.732050807568877f)    // 3的平方根

/*===========================================================================*/
/*  Clark变换输出                                                             */
/*===========================================================================*/
typedef struct
{
    float Alpha;    // Alpha轴分量
    float Beta;     // Beta轴分量
} FocClark_t;

/*===========================================================================*/
/*  Park变换输出                                                              */
/*===========================================================================*/
typedef struct
{
    float Id;       // d轴分量
    float Iq;       // q轴分量
} FocPark_t;

/*===========================================================================*/
/*  反Park变换输入                                                            */
/*===========================================================================*/
typedef struct
{
    float Ud;       // d轴分量
    float Uq;       // q轴分量
} FocInversePark_t;

/*===========================================================================*/
/*  反Park变换输出                                                            */
/*===========================================================================*/
typedef struct
{
    float Ualpha;   // Alpha轴分量
    float Ubeta;    // Beta轴分量
} FocAlphaBeta_t;

/***********************************************
 * @brief : 对两相电流进行Clark变换
 * @param : CurrentA A相电流
 * @param : CurrentB B相电流
 * @return: Clark变换结果
 * @date  : 2026-08-15
 * @author: LYF
 ************************************************/
FocClark_t foc_clark_calc(float CurrentA, float CurrentB);

/***********************************************
 * @brief : 对Alpha/Beta分量进行Park变换
 * @param : Clark Clark变换结果
 * @param : ElectricalAngle 电角度，0~32767对应0~2PI
 * @return: Park变换结果
 * @date  : 2026-08-15
 * @author: LYF
 ************************************************/
FocPark_t foc_park_calc(FocClark_t Clark, uint16 ElectricalAngle);

/***********************************************
 * @brief : 对d/q轴分量进行反Park变换
 * @param : InversePark 反Park变换输入
 * @param : ElectricalAngle 电角度，0~32767对应0~2PI
 * @return: Alpha/Beta轴输出
 * @date  : 2026-08-15
 * @author: LYF
 ************************************************/
FocAlphaBeta_t foc_ipark_calc(FocInversePark_t InversePark,
                              uint16 ElectricalAngle);

#endif
