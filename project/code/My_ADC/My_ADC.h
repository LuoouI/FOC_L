#ifndef MY_ADC_H
#define MY_ADC_H

#include "zf_common_headfile.h"

#define ADC_1_PIN                         (ADC2_CH00_P18_0) // 电流采样引脚
#define ADC_2_PIN                         (ADC0_CH17_P07_1) // 电流采样引脚

#define ADC_V_PIN                         (ADC0_CH18_P07_2) // 母线电压输入引脚

#define ADC_REF_VOLTAGE                   (3.3f)       // ADC参考电压
#define ADC_MAX_VALUE                     (4095.0f)    // 12位ADC最大采样值
#define BATTERY_DIVIDER_RATIO             (11.0f)      // 母线电压分压还原系数
#define BATTERY_VOLTAGE_CALIBRATION       (1.0f)       // 母线电压校准系数

#define ADC_CLOCK_DIVIDER_INDEX           (1u)         // SAR时钟分频器编号
#define ADC_CLOCK_DIVIDER_VALUE           (5u)         // SAR时钟分频寄存器值
#define ADC_SAMPLE_TIME                   (8u)         // ADC采样时间，单位为ADC时钟周期
#define ADC_FIRST_ISR_DEBUG_PIN           (P23_7)      // 每组首个ADC中断到达时翻转
#define ADC_BOTH_DONE_DEBUG_PIN           (P02_1)      // 首个中断到达时两路已完成才翻转

/*===========================================================================*/
/*  ADC通道硬件描述                                                           */
/*===========================================================================*/
typedef struct
{
    volatile stc_PASS_SAR_t    *Sar;           // SAR模块
    volatile stc_PASS_SAR_CH_t *Channel;       // SAR通道
    cy_en_adc_pin_address_t     PinAddress;    // SAR模拟输入地址
    cy_en_adc_port_address_t    PortAddress;   // SAR模拟端口地址
    volatile stc_GPIO_PRT_t    *Port;          // 模拟输入GPIO端口
    uint32                      Pin;           // 模拟输入GPIO引脚编号
    en_hsiom_sel_t              Hsiom;         // 模拟输入复用功能
    cy_en_intr_t                InterruptSource; // ADC通道系统中断源
} AdcChannel_t;

/***********************************************
 * @brief : 初始化电流采样数据、电流ADC和母线电压ADC
 * @param : /
 * @return: void
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
void My_ADC_Init(void);

/***********************************************
 * @brief : 采样并获取电压检测通道原始值
 * @param : /
 * @return: ADC原始采样值
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
uint16 My_ADC_GetBatteryRawValue(void);

/***********************************************
 * @brief : 采样并获取母线电压
 * @param : /
 * @return: 母线电压，单位V
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
float My_ADC_GetBatteryVoltage(void);

#endif
