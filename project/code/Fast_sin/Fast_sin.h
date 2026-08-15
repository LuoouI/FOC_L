#ifndef FAST_SIN_H
#define FAST_SIN_H

#include "zf_common_headfile.h"

/***********************************************
 * @brief : 使用四分之一波查表和线性插值计算正弦值
 * @param : ElectricalAngle 电角度，0~32767对应0~2PI
 * @return: 正弦值，范围-1.0~1.0
 * @date  : 2026-08-15
 * @author: LYF
 ************************************************/
float fast_sinf(uint16 ElectricalAngle);

/***********************************************
 * @brief : 使用四分之一波查表和线性插值计算余弦值
 * @param : ElectricalAngle 电角度，0~32767对应0~2PI
 * @return: 余弦值，范围-1.0~1.0
 * @date  : 2026-08-15
 * @author: LYF
 ************************************************/
float fast_cosf(uint16 ElectricalAngle);

#endif

