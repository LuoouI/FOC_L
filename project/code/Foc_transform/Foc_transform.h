#ifndef FOC_TRANSFORM_H
#define FOC_TRANSFORM_H

#include "zf_common_headfile.h"

/*===========================================================================*/
/*  Clark变换输出                                                             */
/*===========================================================================*/
typedef struct
{
    float Alpha;    // Alpha轴分量
    float Beta;     // Beta轴分量
} Clark_t;

/*===========================================================================*/
/*  Park变换输出                                                              */
/*===========================================================================*/
typedef struct
{
    float Id;       // d轴分量
    float Iq;       // q轴分量
} Park_t;

/*===========================================================================*/
/*  逆Park变换输入                                                            */
/*===========================================================================*/
typedef struct
{
    float Ud;       // d轴分量
    float Uq;       // q轴分量
} InversePark_t;

/*===========================================================================*/
/*  逆Park变换输出                                                            */
/*===========================================================================*/
typedef struct
{
    float Ualpha;   // Alpha轴分量
    float Ubeta;    // Beta轴分量
} AlphaBeta_t;

/***********************************************
 * @brief : 对两相电流进行Clark变换
 * @param : CurrentA A相电流
 * @param : CurrentB B相电流
 * @return: Clark变换结果
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
Clark_t foc_clark_calc(float CurrentA, float CurrentB);

/***********************************************
 * @brief : 对Alpha/Beta分量进行Park变换
 * @param : Clark Clark变换结果
 * @param : ElectricalAngle 电角度，0~32767对应0~2PI
 * @return: Park变换结果
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
Park_t foc_park_calc(Clark_t Clark, uint16 ElectricalAngle);
    
/***********************************************
 * @brief : 对d/q轴分量进行逆Park变换
 * @param : InversePark 逆Park变换输入
 * @param : ElectricalAngle 电角度，0~32767对应0~2PI
 * @return: Alpha/Beta轴输出
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
AlphaBeta_t foc_ipark_calc(InversePark_t InversePark,
                           uint16 ElectricalAngle);

#endif
