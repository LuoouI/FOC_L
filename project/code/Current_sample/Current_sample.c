#include "Current_sample.h"
#include "Function/Function.h"

volatile motor_current_t Current = {0};

static Sliding_Filter_t Cur_FilterU;
static Sliding_Filter_t Cur_FilterV;
static float Cur_FilterBufU[CURRENT_SAMPLE_FILTER_WINDOW_SIZE];
static float Cur_FilterBufV[CURRENT_SAMPLE_FILTER_WINDOW_SIZE];
static uint32 Cur_CalSumU;
static uint32 Cur_CalSumV;
static uint16 Cur_CalCount;

/***********************************************
 * @brief : 将扣除零偏后的ADC值换算为电流
 * @param : AdcCal 扣除零偏后的ADC值
 * @return: 电流值，单位为安培
 * @date  : 2026-08-17
 * @author: LYF
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
 * @param : FilterU U相滤波后的ADC值
 * @param : FilterV V相滤波后的ADC值
 * @return: void
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
static void Current_Sample_UpdateCal(uint16 FilterU, uint16 FilterV)
{
    int32 CalU;
    int32 CalV;
    int32 CalW;

    CalU = (int32)FilterU - (int32)Current.offset_u;
    CalV = (int32)FilterV - (int32)Current.offset_v;
    CalW = -(CalU + CalV);

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

    Current_Sample_StartCalibration();
}

void Current_Sample_StartCalibration(void)
{
    Sliding_Filter_Init(
        &Cur_FilterU,
        Cur_FilterBufU,
        CURRENT_SAMPLE_FILTER_WINDOW_SIZE);
    Sliding_Filter_Init(
        &Cur_FilterV,
        Cur_FilterBufV,
        CURRENT_SAMPLE_FILTER_WINDOW_SIZE);

    Cur_CalSumU = 0u;
    Cur_CalSumV = 0u;
    Cur_CalCount = 0u;

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

void Current_Sample_Update(uint16 AdcRawU, uint16 AdcRawV)
{
    uint16 FilterU;
    uint16 FilterV;

    Current.adc_raw_u = AdcRawU;
    Current.adc_raw_v = AdcRawV;
    Current.adc_raw_w = 0u;

    Sliding_Filter_Update(&Cur_FilterU, (float)AdcRawU);
    Sliding_Filter_Update(&Cur_FilterV, (float)AdcRawV);

    /* 去掉窗口内一个最大值和一个最小值，避免单次开关尖峰进入电流值 */
    FilterU = Sliding_Filter_GetTrimmedUint16(&Cur_FilterU);
    FilterV = Sliding_Filter_GetTrimmedUint16(&Cur_FilterV);

    if (Current.calibrated == 0u)
    {
        Cur_CalSumU += (uint32)AdcRawU;
        Cur_CalSumV += (uint32)AdcRawV;
        Cur_CalCount++;

        if (Cur_CalCount >=
            CURRENT_SAMPLE_CALIBRATION_COUNT)
        {
            // Current.offset_u = (uint16)
            //     (Cur_CalSumU /       
            //      CURRENT_SAMPLE_CALIBRATION_COUNT);
            // Current.offset_v = (uint16)
            //     (Cur_CalSumV /
            //      CURRENT_SAMPLE_CALIBRATION_COUNT);

            /*实测值*/
            Current.offset_u = 2049;
            Current.offset_v = 2051;

            Current.calibrated = 1u;
        }
    }

    if (Current.calibrated != 0u)
    {
        Current_Sample_UpdateCal(FilterU, FilterV);
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
