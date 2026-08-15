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

/***********************************************
 * @brief : 初始化SAR ADC和模拟输入引脚
 * @param : /
 * @return: void
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
void My_ADC_Init(void);

/***********************************************
 * @brief : 软件触发并更新SAR2的两个ADC通道采样结果
 * @param : /
 * @return: void
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
void My_ADC_Sample(void);

/***********************************************
 * @brief : 获取ADC1通道的最近一次原始采样值
 * @param : /
 * @return: ADC原始采样值
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
uint16 My_ADC_GetAdc1Value(void);

/***********************************************
 * @brief : 获取ADC2通道的最近一次原始采样值
 * @param : /
 * @return: ADC原始采样值
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
uint16 My_ADC_GetAdc2Value(void);

/***********************************************
 * @brief : 获取电压检测通道的最近一次原始采样值
 * @param : /
 * @return: ADC原始采样值
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
uint16 My_ADC_GetBatteryRawValue(void);

/***********************************************
 * @brief : 获取母线电压
 * @param : /
 * @return: 母线电压，单位为V
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
float My_ADC_GetBatteryVoltage(void);

#endif
