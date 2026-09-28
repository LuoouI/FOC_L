#include "Sliding_Filter.h"
#include "Function/Function.h"

/***********************************************
 * @brief : 将浮点滤波结果四舍五入并限制为uint16范围
 * @param : Value 待转换的浮点滤波结果
 * @return: 转换后的uint16滤波结果
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
static uint16 Sliding_Filter_ToUint16(float Value)
{
    Value = Float_Limit(Value + 0.5f, 0.0f, 65535.0f);
    return (uint16)Value;
}

/***********************************************
 * @brief : 初始化滑动滤波器
 * @param : Filter 滑动滤波器
 * @param : WindowData 窗口数据缓存区
 * @param : WindowSize 窗口长度
 * @return: void
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
void Sliding_Filter_Init(Sliding_Filter_t *Filter, float *WindowData, uint8 WindowSize)
{
    uint8 Index;

    Filter->Sum = 0.0F;
    Filter->WindowData = WindowData;
    Filter->Count = 0U;
    Filter->Index = 0U;
    Filter->WindowSize = WindowSize;

    if (WindowData == NULL)
    {
        return;
    }

    for (Index = 0U; Index < WindowSize; Index++)
    {
        WindowData[Index] = 0.0F;
    }
}

/***********************************************
 * @brief : 向滑动滤波器写入一个新数据
 * @param : Filter 滑动滤波器
 * @param : NewData 新数据
 * @return: void
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
void Sliding_Filter_Update(Sliding_Filter_t *Filter, float NewData)
{
    if ((Filter->WindowData == NULL) || (Filter->WindowSize == 0U))
    {
        return;
    }

    Filter->Sum -= Filter->WindowData[Filter->Index];
    Filter->WindowData[Filter->Index] = NewData;
    Filter->Sum += NewData;
    Filter->Index++;

    if (Filter->Index >= Filter->WindowSize)
    {
        Filter->Index = 0U;
    }

    if (Filter->Count < Filter->WindowSize)
    {
        Filter->Count++;
    }
}

/***********************************************
 * @brief : 获取普通滑动平均值
 * @param : Filter 滑动滤波器
 * @return: 普通滑动平均值
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
float Sliding_Filter_Get(Sliding_Filter_t *Filter)
{
    if ((Filter->WindowData == NULL) || (Filter->Count == 0U))
    {
        return 0.0F;
    }

    return Filter->Sum / (float)Filter->Count;
}

/***********************************************
 * @brief : 获取去除一个最大值和一个最小值后的滑动平均值
 * @param : Filter 滑动滤波器
 * @return: 去极值滑动平均值
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
float Sliding_Filter_GetTrimmed(Sliding_Filter_t *Filter)
{
    uint8 Index;
    float MinValue;
    float MaxValue;
    float TrimmedSum;

    if ((Filter->WindowData == NULL) || (Filter->Count == 0U))
    {
        return 0.0F;
    }

    if (Filter->Count <= 2U)
    {
        return Sliding_Filter_Get(Filter);
    }

    MinValue = Filter->WindowData[0];
    MaxValue = Filter->WindowData[0];

    for (Index = 1U; Index < Filter->Count; Index++)
    {
        if (Filter->WindowData[Index] < MinValue)
        {
            MinValue = Filter->WindowData[Index];
        }

        if (Filter->WindowData[Index] > MaxValue)
        {
            MaxValue = Filter->WindowData[Index];
        }
    }

    TrimmedSum = Filter->Sum - MinValue - MaxValue;
    return TrimmedSum / (float)(Filter->Count - 2U);
}

/***********************************************
 * @brief : 获取普通滑动平均值并转换为uint16
 * @param : Filter 滑动滤波器
 * @return: 四舍五入后的普通滑动平均值
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
uint16 Sliding_Filter_GetUint16(Sliding_Filter_t *Filter)
{
    return Sliding_Filter_ToUint16(Sliding_Filter_Get(Filter));
}

/***********************************************
 * @brief : 获取去极值滑动平均值并转换为uint16
 * @param : Filter 滑动滤波器
 * @return: 四舍五入后的去极值滑动平均值
 * @date  : 2026-08-16
 * @author: L
 ************************************************/
uint16 Sliding_Filter_GetTrimmedUint16(Sliding_Filter_t *Filter)
{
    return Sliding_Filter_ToUint16(Sliding_Filter_GetTrimmed(Filter));
}
