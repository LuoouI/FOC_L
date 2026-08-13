#include "My_ADC.h"
#include "zf_driver_adc.h"

void My_ADC_Init(void)
{
    adc_init(ADC_1_PIN,ADC_12BIT);
    adc_init(ADC_2_PIN,ADC_12BIT);
}

