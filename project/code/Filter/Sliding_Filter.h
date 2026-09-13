#ifndef SLIDING_FILTER_H
#define SLIDING_FILTER_H

#include "zf_common_headfile.h"

/*===========================================================================*/
/*  滑动滤波器数据描述                                                        */
/*===========================================================================*/
typedef struct
{
    float Sum;                  /* 窗口数据总和 */
    float *WindowData;          /* 窗口数据缓存区 */
    uint8 Count;                /* 当前有效数据数量 */
    uint8 Index;                /* 下一个写入位置 */
    uint8 WindowSize;           /* 窗口长度 */
} Sliding_Filter_t;

/*==================================================== 基础函数 ====================================================*/
void    Sliding_Filter_Init                 (Sliding_Filter_t *Filter, float *WindowData, uint8 WindowSize);
void    Sliding_Filter_Update               (Sliding_Filter_t *Filter, float NewData);

float   Sliding_Filter_Get                  (Sliding_Filter_t *Filter);
float   Sliding_Filter_GetTrimmed           (Sliding_Filter_t *Filter);

uint16  Sliding_Filter_GetUint16            (Sliding_Filter_t *Filter);
uint16  Sliding_Filter_GetTrimmedUint16     (Sliding_Filter_t *Filter);
/*==================================================== 基础函数 ====================================================*/

#endif
