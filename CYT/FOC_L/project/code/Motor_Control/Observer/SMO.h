#ifndef MOTOR_CONTROL_SMO_H
#define MOTOR_CONTROL_SMO_H

#include "zf_common_headfile.h"

/*===========================================================================*/
/*  滑模观测器状态                                                            */
/*===========================================================================*/
typedef struct
{
    float I_alpha_est;                      /* Alpha轴电流估算值 */
    float I_beta_est;                       /* Beta轴电流估算值 */
    float I_alpha_estpre;                   /* 上一周期Alpha轴电流估算值 */
    float I_beta_estpre;                    /* 上一周期Beta轴电流估算值 */

    float U_alpha_pre;                      /* 上一周期Alpha轴电压 */
    float U_beta_pre;                       /* 上一周期Beta轴电压 */

    float E_alpha;                          /* Alpha轴反电动势 */
    float E_beta;                           /* Beta轴反电动势 */
    float E_alpha_filter;                   /* Alpha轴反电动势滤波值 */
    float E_beta_filter;                    /* Beta轴反电动势滤波值 */

    float K_slide;                          /* 滑模增益 */
    float Boundary_current;                 /* 滑模边界电流，单位为A */
    float Filter_bandwidth;                 /* 反电动势滤波器带宽，单位为Hz */
    uint8 Ready;                            /* SMO就绪标志 */

    float A;                                /* 电流观测器上一周期电流离散系数 */
    float B;                                /* 电流观测器输入电压离散系数 */
} SMO_t;

/*==================================================== 基础函数 ====================================================*/
void    SMO_Reset         (void);
void    SMO_Update        (void);
void    SMO_UpdateVoltage (void);
/*==================================================== 基础函数 ====================================================*/

#endif
