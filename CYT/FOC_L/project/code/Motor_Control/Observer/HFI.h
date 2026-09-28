#ifndef MOTOR_CONTROL_HFI_H
#define MOTOR_CONTROL_HFI_H

#include "zf_common_headfile.h"

/*===========================================================================*/
/*  高频注入位置估算状态                                                      */
/*===========================================================================*/
typedef struct
{
    float Injection_voltage;          /* 高频注入电压幅值，单位为V */
    float Injection_frequency;        /* 高频注入目标频率，单位为Hz */
    float Demod_bandwidth;            /* 解调低通带宽，单位为Hz */
    float Demod_coefficient;          /* 解调低通离散系数 */
    float Demod_amplitude;            /* 解调归一化幅值，单位为A */

    float Iq_previous;                /* 上一周期HFI坐标系q轴电流 */
    float Iq_delta;                   /* 相邻周期q轴电流变化量 */
    float Demod_raw;                  /* 同步解调原始值 */
    float Demod_filter;               /* 同步解调滤波值 */
    float Injection_voltage_applied;  /* 实际生效的注入电压，单位为V */

    uint16 Carrier_half_count;        /* 载波半周期控制节拍数 */
    uint16 Carrier_count;             /* 当前载波计数 */
    uint16 Polarity_offset;           /* 磁极极性补偿，取值为0或16384 */

    int8 Command_sign;                /* 即将输出的注入极性 */
    int8 Applied_sign;                /* 当前电流差分对应的注入极性 */
    int8 Error_direction;             /* 解调误差方向，取值为+1或-1 */

    uint8 Enabled;                    /* 高频注入使能标志 */
    uint8 Ready;                      /* 位置估算稳定标志 */
    uint8 Polarity_ready;             /* 磁极极性识别完成标志 */
} HFI_t;

#endif
