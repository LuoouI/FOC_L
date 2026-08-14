#ifndef MY_ADC_H
#define MY_ADC_H

#define ADC_1_PIN         (ADC2_CH00_P18_0)
#define ADC_2_PIN         (ADC2_CH01_P18_1)

#define ADC_V_PIN         (ADC0_CH18_P07_2)

#define ADC_REF_VOLTAGE              (3.3f)       // ADC参考电压
#define ADC_MAX_VALUE                (4095.0f)    // 12位ADC最大采样值
#define BATTERY_DIVIDER_RATIO        (11.0f)      // 电池检测电路分压还原系数
#define BATTERY_VOLTAGE_CALIBRATION  (1.0f)       // 电池电压校准系数

/***********************************************
 * @brief : 初始化ADC采样通道
 * @param : 无
 * @return: 无
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
void My_ADC_Init(void);

/***********************************************
 * @brief : 获取电池电压
 * @param : 无
 * @return: 电池电压，单位V
 * @date  : 2026-08-14
 * @author: LYF
 ************************************************/
float My_ADC_GetBatteryVoltage(void);

#endif // MY_ADC_H
