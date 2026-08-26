#ifndef MY_LED_H_
#define MY_LED_H_

#include "zf_common_headfile.h"

#define LED_PIN                 (P06_5)       // 保护指示灯引脚
#define LED_BLINK_PERIOD_MS     (500u)        // 保护指示灯闪烁周期
#define VOLTAGE_LED_OFF_VALUE   (24.0f)       // 指示灯常灭电压阈值
#define VOLTAGE_LED_BLINK_VALUE (24.5f)       // 指示灯闪烁电压阈值

/*===========================================================================*/
/*  保护指示灯状态                                                          */
/*===========================================================================*/
typedef enum
{
    LED_ON = 0,                                 // 常亮
    LED_BLINK,                                  // 闪烁
    LED_OFF                                     // 常灭
} LED_State_t;

/***********************************************
 * @brief : 初始化保护指示灯
 * @param : /
 * @return: void
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
void My_LED_Init(void);

/***********************************************
 * @brief : 设置保护指示灯状态
 * @param : LedState 指示灯状态
 * @return: void
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
void My_LED_SetLedState(LED_State_t LedState);

/***********************************************
 * @brief : 更新保护指示灯输出，支持常亮、闪烁和常灭三种状态
 * @param : ElapsedMs 距离上次调用经过的毫秒数
 * @return: void
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
void My_LED_Service(uint32 ElapsedMs);

/***********************************************
 * @brief : 获取当前保护指示灯状态
 * @param : /
 * @return: LED_State_t 当前指示灯状态
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
LED_State_t My_LED_GetLedState(void);

/***********************************************
 * @brief : 根据母线电压更新保护指示灯状态
 * @param : /
 * @return: void
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
void My_LED_CheckVoltage(void);

#endif
