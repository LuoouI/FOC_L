#include "My_ADC.h"
#include "zf_driver_adc.h"

void My_ADC_Init(void)
{
    adc_init(ADC_1_PIN,ADC_12BIT);
    adc_init(ADC_2_PIN,ADC_12BIT);
    adc_init(ADC_V_PIN,ADC_12BIT);
}

float My_ADC_GetBatteryVoltage(void)
{
    uint16 Adc_value;
    float Adc_voltage;
    float Battery_voltage;

    Adc_value = adc_mean_filter_convert(ADC_V_PIN, 16u);
    Adc_voltage = (float)Adc_value * ADC_REF_VOLTAGE / ADC_MAX_VALUE;
    Battery_voltage = Adc_voltage * BATTERY_DIVIDER_RATIO * BATTERY_VOLTAGE_CALIBRATION;

    return Battery_voltage;
}

