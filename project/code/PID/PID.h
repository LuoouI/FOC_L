#ifndef PID_H_
#define PID_H_

#include "zf_common_headfile.h"

#define PI   (3.141592653589793f)
#define LD   (0.032652f)      //mH
#define LQ   (0.036748f)      //mH
#define RS   (0.28255f)       //R
#define FOC_TS (1.0f/20000.0f)  // 采样周期

#define BW_HZ 250.0f  // 电流环带宽
#define OMEGA_C  (2.0f * PI * BW_HZ)  // 角频率 ≈ 1669 rad/s

// 电流环带宽选择
// Kp = ωc × L
// Ki = ωc × R × Ts 

#define KP_D_BASIC  (float)(OMEGA_C * LD / 1000.0f)      
#define KI_D_BASIC  (float)(OMEGA_C * RS * FOC_TS)  
#define KP_Q_BASIC  (float)(OMEGA_C * LQ / 1000.0f)       
#define KI_Q_BASIC  (float)(OMEGA_C * RS * FOC_TS)  

#endif