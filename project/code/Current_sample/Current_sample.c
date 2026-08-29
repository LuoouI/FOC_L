#include "Current_sample.h"
#include "Function/Function.h"

volatile motor_current_t Current = {0};

/***********************************************
 * @brief : 将扣除零偏后的ADC值换算为电流
 * @param : AdcCal 扣除零偏后的ADC值
 * @return: 电流值，单位为安培
 * @date  : 2026-08-17
 * @author: L
 ************************************************/
static float Current_Sample_AdcToCurrent(int16 AdcCal)
{
    return ((float)AdcCal * CURRENT_SAMPLE_ADC_REF_VOLTAGE) /
           (CURRENT_SAMPLE_ADC_MAX_VALUE *
            CURRENT_SAMPLE_AMPLIFIER_GAIN *
            CURRENT_SAMPLE_SHUNT_RESISTANCE);
}

/***********************************************
 * @brief : 根据两电阻采样结果更新校准后的三相电流
 * @param : AdcRawU U相ADC原始采样值
 * @param : AdcRawW W相ADC原始采样值
 * @return: void
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static void Current_Sample_UpdateCal(uint16 AdcRawU, uint16 AdcRawW)
{
    int32 CalU;
    int32 CalV;
    int32 CalW;

    CalU = (int32)AdcRawU - (int32)Current.offset_u;
    CalW = (int32)AdcRawW - (int32)Current.offset_w;
    CalV = -(CalU + CalW);

    Current.adc_cal_u = (int16)Int_Limit(CalU, -32768, 32767);
    Current.adc_cal_v = (int16)Int_Limit(CalV, -32768, 32767);
    Current.adc_cal_w = (int16)Int_Limit(CalW, -32768, 32767);

    Current.current_u =
        Current_Sample_AdcToCurrent(Current.adc_cal_u);
    Current.current_v =
        Current_Sample_AdcToCurrent(Current.adc_cal_v);
    Current.current_w =
        Current_Sample_AdcToCurrent(Current.adc_cal_w);
}

void Current_Sample_Init(void)
{
    Current.adc_raw_u = 0u;
    Current.adc_raw_v = 0u;
    Current.adc_raw_w = 0u;

    Current_Sample_StartCalibration();
}

void Current_Sample_StartCalibration(void)
{
    Current.offset_u = 0u;
    Current.offset_v = 0u;
    Current.offset_w = 0u;
    Current.adc_cal_u = 0;
    Current.adc_cal_v = 0;
    Current.adc_cal_w = 0;
    Current.current_u = 0.0f;
    Current.current_v = 0.0f;
    Current.current_w = 0.0f;
    Current.clark = (Clark_t){0.0f, 0.0f};
    Current.park = (Park_t){0.0f, 0.0f};
    Current.calibrated = 0u;
    Current.sample_ready = 0u;
}

void Current_Sample_Update(uint16 AdcRawU, uint16 AdcRawW)
{
    // uint32 Cur_CalSumU = 0;
    // uint32 Cur_CalSumW = 0;
    // uint32 Cur_CalCount = 0;

    Current.adc_raw_u = AdcRawU;
    Current.adc_raw_v = 0u;
    Current.adc_raw_w = AdcRawW;

    if (Current.calibrated == 0u)
    {
        // Cur_CalSumU += (uint32)AdcRawU;
        // Cur_CalSumW += (uint32)AdcRawW;
        // Cur_CalCount++;

        // if (Cur_CalCount >=
        //     CURRENT_SAMPLE_CALIBRATION_COUNT)
        // {
            // Current.offset_u = (uint16)
            //     (Cur_CalSumU /       
            //      CURRENT_SAMPLE_CALIBRATION_COUNT);
            // Current.offset_w = (uint16)
            //     (Cur_CalSumW /
            //      CURRENT_SAMPLE_CALIBRATION_COUNT);
            // }

        /*实测值*/
        Current.offset_u = 2051u;
        Current.offset_w = 2049u;
        Current.calibrated = 1u;
        
    }

    if (Current.calibrated != 0u)
    {
        Current_Sample_UpdateCal(AdcRawU, AdcRawW);
    }
    else
    {
        Current.adc_cal_u = 0;
        Current.adc_cal_v = 0;
        Current.adc_cal_w = 0;
        Current.current_u = 0.0f;
        Current.current_v = 0.0f;
        Current.current_w = 0.0f;
        Current.clark = (Clark_t){0.0f, 0.0f};
        Current.park = (Park_t){0.0f, 0.0f};
    }

    Current.sample_ready = 1u;
}

void Current_Sample_Transform(uint16 ElectricalAngle)
{
    Current.clark = foc_clark_calc(
        Current.current_u,
        Current.current_v);
    Current.park = foc_park_calc(
        Current.clark,
        ElectricalAngle);
}
