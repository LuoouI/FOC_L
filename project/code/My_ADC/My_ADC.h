#ifndef MY_ADC_H
#define MY_ADC_H

#include "zf_common_headfile.h"

#define ADC_1_PIN                         (ADC2_CH00_P18_0) // ADC1输入引脚
#define ADC_2_PIN                         (ADC2_CH01_P18_1) // ADC2输入引脚
#define ADC_V_PIN                         (ADC0_CH18_P07_2) // 母线电压输入引脚

#define ADC_REF_VOLTAGE                   (3.3f)       // ADC参考电压
#define ADC_MAX_VALUE                     (4095.0f)    // 12位ADC最大采样值
#define BATTERY_DIVIDER_RATIO             (11.0f)      // 母线电压分压还原系数
#define BATTERY_VOLTAGE_CALIBRATION       (1.0f)       // 母线电压校准系数

#define ADC_CLOCK_DIVIDER_INDEX           (1u)         // SAR时钟分频器编号
#define ADC_CLOCK_DIVIDER_VALUE           (5u)         // SAR时钟分频寄存器值
#define ADC_SAMPLE_TIME                   (8u)         // ADC采样时间，单位为ADC时钟周期

/*===========================================================================*/
/*  ADC通道硬件描述                                                           */
/*===========================================================================*/
typedef struct
{
    volatile stc_PASS_SAR_t    *Sar;           // SAR模块
    volatile stc_PASS_SAR_CH_t *Channel;       // SAR通道
    cy_en_adc_pin_address_t     PinAddress;    // SAR模拟输入地址
    volatile stc_GPIO_PRT_t    *Port;          // 模拟输入GPIO端口
    uint32                      Pin;           // 模拟输入GPIO引脚编号
} AdcChannel_t;

/*===========================================================================*/
/*  ADC采样数据                                                               */
/*===========================================================================*/
typedef struct
{
    volatile uint16 Adc1Raw;          // ADC1原始采样值
    volatile uint16 Adc2Raw;          // ADC2原始采样值
    volatile uint16 BatteryRaw;       // 母线电压原始采样值
    float BatteryVoltage;             // 母线电压，单位V
    volatile uint8 SampleReady;       // 两路ADC采样完成标志
} AdcData_t;

extern AdcData_t MyAdc;

/***********************************************
 * @brief : 初始化SAR ADC和模拟输入引脚
 * @param : /
 * @return: void
 * @date  : 2026-08-15
 * @author: LYF
 ************************************************/
void My_ADC_Current_Init(void);

/***********************************************
 * @brief : 初始化母线电压检测ADC通道
 * @param : /
 * @return: void
 * @date  : 2026-08-16
 * @author: LYF
 ************************************************/
void My_ADC_Voltage_Init(void);

/***********************************************
 * @brief : 软件触发并更新SAR2的两个ADC通道采样结果
 * @param : /
 * @return: void
 * @date  : 2026-08-15
 * @author: LYF
 ************************************************/
void My_ADC_Sample(void);

/***********************************************
 * @brief : 获取ADC1通道的最近一次原始采样值
 * @param : /
 * @return: ADC原始采样值
 * @date  : 2026-08-15
 * @author: LYF
 ************************************************/
uint16 My_ADC_GetAdc1Value(void);

/***********************************************
 * @brief : 获取ADC2通道的最近一次原始采样值
 * @param : /
 * @return: ADC原始采样值
 * @date  : 2026-08-15
 * @author: LYF
 ************************************************/
uint16 My_ADC_GetAdc2Value(void);

/***********************************************
 * @brief : 采样并获取电压检测通道原始值
 * @param : /
 * @return: ADC原始采样值
 * @date  : 2026-08-15
 * @author: LYF
 ************************************************/
uint16 My_ADC_GetBatteryRawValue(void);

/***********************************************
 * @brief : 采样并获取母线电压
 * @param : /
 * @return: 母线电压，单位V
 * @date  : 2026-08-15
 * @author: LYF
 ************************************************/
float My_ADC_GetBatteryVoltage(void);

#endif
