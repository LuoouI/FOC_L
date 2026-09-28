#ifndef GET_TIME_H_
#define GET_TIME_H_

#include "zf_common_headfile.h"

/*===========================================================================*/
/*  固定整数分频计时器                                                       */
/*===========================================================================*/
typedef struct
{
    uint32 Count;     /* 距离下次触发剩余的基准节拍数 */
    uint32 Period;    /* 相邻两次触发之间的基准节拍数 */
} Time_Divider_t;

/*==================================================== 基础函数 ====================================================*/
void    Get_Time_Init  (Time_Divider_t *Timer, uint32 Period);
void    Get_Time_Reset (Time_Divider_t *Timer);
uint8   Get_Time       (Time_Divider_t *Timer);
/*==================================================== 基础函数 ====================================================*/

#endif /* GET_TIME_H_ */
