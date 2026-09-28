#ifndef MY_LED_H_
#define MY_LED_H_

#include "zf_common_headfile.h"

#define LED_PIN                 (P06_5)         /* 保护指示灯引脚 */
#define LED_BLINK_PERIOD_MS     (500u)          /* 保护指示灯闪烁周期 */
#define VOLTAGE_LED_OFF_VALUE   (24.0f)         /* 指示灯常灭电压阈值 */
#define VOLTAGE_LED_BLINK_VALUE (24.5f)         /* 指示灯闪烁电压阈值 */

/*===========================================================================*/
/*  保护指示灯状态                                                          */
/*===========================================================================*/
typedef enum
{
    LED_ON = 0, /* 常亮 */
    LED_BLINK, /* 闪烁 */
    LED_OFF /* 常灭 */
} LED_State_t;

/*==================================================== 基础函数 ====================================================*/
void            My_LED_Init                 (void);
void            My_LED_SetLedState          (LED_State_t LedState);
void            My_LED_Service              (uint32 ElapsedMs);
LED_State_t     My_LED_GetLedState          (void);
void            My_LED_CheckVoltage         (void);
/*==================================================== 基础函数 ====================================================*/

#endif
