#ifndef SLIDING_FILTER_H
#define SLIDING_FILTER_H

#include "zf_common_headfile.h"

/*===========================================================================*/
/*  滑动滤波器数据描述                                                        */
/*===========================================================================*/
typedef struct
{
    float Sum;                         // 窗口数据总和
    float *WindowData;                 // 窗口数据缓存区
    uint8 Count;                       // 当前有效数据数量
    uint8 Index;                       // 下一个写入位置
    uint8 WindowSize;                  // 窗口长度
} Sliding_Filter_t;

/***********************************************
 * @brief : 初始化滑动滤波器
 * @param : Filter 滑动滤波器
 * @param : WindowData 窗口数据缓存区
 * @param : WindowSize 窗口长度
 * @return: void
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
void Sliding_Filter_Init(Sliding_Filter_t *Filter, float *WindowData, uint8 WindowSize);

/***********************************************
 * @brief : 向滑动滤波器写入一个新数据
 * @param : Filter 滑动滤波器
 * @param : NewData 新数据
 * @return: void
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
void Sliding_Filter_Update(Sliding_Filter_t *Filter, float NewData);

/***********************************************
 * @brief : 获取普通滑动平均值
 * @param : Filter 滑动滤波器
 * @return: 普通滑动平均值
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
float Sliding_Filter_Get(Sliding_Filter_t *Filter);

/***********************************************
 * @brief : 获取去除一个最大值和一个最小值后的滑动平均值
 * @param : Filter 滑动滤波器
 * @return: 去极值滑动平均值
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
float Sliding_Filter_GetTrimmed(Sliding_Filter_t *Filter);

/***********************************************
 * @brief : 获取普通滑动平均值并转换为uint16
 * @param : Filter 滑动滤波器
 * @return: 四舍五入后的普通滑动平均值
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
uint16 Sliding_Filter_GetUint16(Sliding_Filter_t *Filter);

/***********************************************
 * @brief : 获取去极值滑动平均值并转换为uint16
 * @param : Filter 滑动滤波器
 * @return: 四舍五入后的去极值滑动平均值
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
uint16 Sliding_Filter_GetTrimmedUint16(Sliding_Filter_t *Filter);

#endif
