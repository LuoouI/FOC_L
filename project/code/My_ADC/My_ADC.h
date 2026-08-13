#ifndef MY_ADC_H
#define MY_ADC_H

#define ADC_1_PIN         (ADC2_CH00_P18_0)     // ADC引脚
#define ADC_2_PIN         (ADC2_CH01_P18_1)     // ADC引脚

#define ADC_REF_VOLTAGE 3.3f                //ADC基准电压 3.3V


void My_ADC_Init(void);

#endif // MY_ADC_H