#include "My_ADC.h"
#include "zf_driver_adc.h"

void My_ADC_Init(void)
{
    adc_init(ADC2_CH00_P18_0,ADC_12BIT);
    adc_init(ADC2_CH01_P18_1,ADC_12BIT);
}

