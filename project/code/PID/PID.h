#ifndef PID_H_
#define PID_H_

#include "zf_common_headfile.h"
#include "Function/Function.h"

#define LD   (0.031036f)      //mH
#define LQ   (0.035016f)      //mH
#define RS   (0.28659f)       //R
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

/*===========================================================================*/
/*  PID 控制器                                                               */
/*===========================================================================*/
typedef struct
{
    float Kp;
    float Ki;
    float Kd;
    float Ek;           // 当前误差
    float last_Ek;      // 上次误差
    float Ek_sum;       // 误差积分
    float P_Out;
    float I_Out;
    float D_Out;
    float OUT;          // 总输出
} PID_t;

/***********************************************
 * @brief : PID 初始化
 * @param : PID_t *pid, float kp, float ki, float kd
 * @return: void
 * @date  : /
 * @author: PSQ
 ************************************************/
void PID_Init(PID_t *pid, float kp, float ki, float kd);

/***********************************************
 * @brief : PID 状态清零
 * @param : PID_t *pid
 * @return: void
 * @date  : /
 * @author: PSQ
 ************************************************/
void PID_Clear(PID_t *pid);

/***********************************************
 * @brief : 位置式 PID 计算
 * @param : PID_t *pid, float ref, float fbk, float integral_limit
 * @return: PID 输出
 * @date  : /
 * @author: PSQ
 ************************************************/
float PID_Calc(PID_t *pid, float ref, float fbk, float integral_limit);


#endif
