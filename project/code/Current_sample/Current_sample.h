#ifndef CURRENT_SAMPLE_H
#define CURRENT_SAMPLE_H

#include "zf_common_headfile.h"
#include "Filter/Sliding_Filter.h"

#define CURRENT_SAMPLE_ADC_REF_VOLTAGE       (3.3f)       // ADC参考电压
#define CURRENT_SAMPLE_ADC_MAX_VALUE         (4095.0f)    // 12位ADC最大采样值
#define CURRENT_SAMPLE_AMPLIFIER_GAIN        (20.0f)      // 电流采样运放增益
#define CURRENT_SAMPLE_SHUNT_RESISTANCE      (0.002f)     // 电流采样电阻，单位为欧姆
#define CURRENT_SAMPLE_FILTER_WINDOW_SIZE    (16u)         // 电流采样滤波窗口长度
#define CURRENT_SAMPLE_CALIBRATION_COUNT     (256u)       // 零电流状态下的偏置校准次数

/*===========================================================================*/
/*  三相电流采样数据                                                         */
/*===========================================================================*/
typedef struct
{
    uint16 adc_raw_u;       // U相ADC原始采样值
    uint16 adc_raw_v;       // V相ADC原始采样值
    uint16 adc_raw_w;       // W相ADC原始采样值，当前两电阻采样硬件无独立通道

    uint16 offset_u;        // U相ADC零偏
    uint16 offset_v;        // V相ADC零偏
    uint16 offset_w;        // W相ADC零偏，当前两电阻采样硬件为零

    int16 adc_cal_u;        // U相滤波后扣除零偏的ADC值
    int16 adc_cal_v;        // V相滤波后扣除零偏的ADC值
    int16 adc_cal_w;        // W相由U、V相电流重构的ADC值

    float current_u;        // U相电流，单位为安培
    float current_v;        // V相电流，单位为安培
    float current_w;        // W相电流，单位为安培

    uint8 calibrated;       // 偏置校准完成标志
    uint8 sample_ready;     // 新的一组三相电流数据准备完成标志
} motor_current_t;

extern volatile motor_current_t CurrentSample;

/***********************************************
 * @brief : 初始化三相电流采样数据和滤波器
 * @param : /
 * @return: void
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
void Current_Sample_Init(void);

/***********************************************
 * @brief : 开始新一轮电流采样零偏校准，调用时应保持电机无电流
 * @param : /
 * @return: void
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
void Current_Sample_StartCalibration(void);

/***********************************************
 * @brief : 更新一组三相电流采样数据
 * @param : AdcRawU U相ADC原始采样值
 * @param : AdcRawV V相ADC原始采样值
 * @return: void
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
void Current_Sample_Update(uint16 AdcRawU, uint16 AdcRawV);

#endif
