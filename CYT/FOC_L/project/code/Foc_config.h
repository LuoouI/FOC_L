#ifndef FOC_CONFIG_H_
#define FOC_CONFIG_H_

/*===========================================================================*/
/*  控制环调度参数                                                           */
/*===========================================================================*/
#define MOTOR_CURRENT_LOOP_HZ           (20000u)    /* 电流环执行频率，单位为Hz */
#define MOTOR_SPEED_LOOP_HZ             (1000u)     /* 速度环执行频率，单位为Hz */
#define MOTOR_POSITION_LOOP_HZ          (500u)      /* 位置环执行频率，单位为Hz */
#define MOTOR_VOICE_LOOP_HZ             MOTOR_CURRENT_LOOP_HZ    /* 电机音乐控制频率，单位为Hz */
#define MOTOR_OBSERVER_TIMEBASE_HZ      MOTOR_CURRENT_LOOP_HZ    /* 观测采样时基频率，单位为Hz */
#define MOTOR_OBSERVER_STREAM_MAX_HZ    (4000u)     /* 观测流最高采样频率，单位为Hz */
#define MOTOR_OBSERVER_STREAM_MIN_HZ    (50u)       /* 观测流最低采样频率，单位为Hz */

#define PID_BANDWIDTH_MIN_HZ            (1u)        /* 电流环带宽下限，单位为Hz */
#define PID_BANDWIDTH_MAX_HZ            (5000u)     /* 电流环带宽上限，单位为Hz */

#if ((MOTOR_CURRENT_LOOP_HZ == 0u) || \
     (MOTOR_SPEED_LOOP_HZ == 0u) || \
     (MOTOR_POSITION_LOOP_HZ == 0u) || \
     (MOTOR_OBSERVER_STREAM_MAX_HZ == 0u) || \
     (MOTOR_OBSERVER_STREAM_MIN_HZ == 0u))
#error "控制环及观测流频率必须大于0"
#else
#if ((MOTOR_CURRENT_LOOP_HZ % MOTOR_SPEED_LOOP_HZ) != 0u)
#error "电流环频率必须能被速度环频率整除"
#endif

#if ((MOTOR_CURRENT_LOOP_HZ % MOTOR_POSITION_LOOP_HZ) != 0u)
#error "电流环频率必须能被位置环频率整除"
#endif

#if ((MOTOR_OBSERVER_TIMEBASE_HZ % 1000u) != 0u)
#error "观测采样时基频率必须能被1 kHz整除"
#endif

#if (MOTOR_OBSERVER_STREAM_MIN_HZ > MOTOR_OBSERVER_STREAM_MAX_HZ)
#error "观测流最低采样频率不能高于最高采样频率"
#endif

#if ((MOTOR_OBSERVER_TIMEBASE_HZ % MOTOR_OBSERVER_STREAM_MAX_HZ) != 0u)
#error "观测采样时基频率必须能被观测流最高采样频率整除"
#endif

#if ((MOTOR_OBSERVER_TIMEBASE_HZ % MOTOR_OBSERVER_STREAM_MIN_HZ) != 0u)
#error "观测采样时基频率必须能被观测流最低采样频率整除"
#endif
#endif

#define MOTOR_SPEED_LOOP_DIVIDER        \
    (MOTOR_CURRENT_LOOP_HZ / MOTOR_SPEED_LOOP_HZ)       /* 速度环相对电流环的分频系数 */
#define MOTOR_POSITION_LOOP_DIVIDER     \
    (MOTOR_CURRENT_LOOP_HZ / MOTOR_POSITION_LOOP_HZ)    /* 位置环相对电流环的分频系数 */
#define MOTOR_OBSERVER_TICKS_PER_MS     \
    (MOTOR_OBSERVER_TIMEBASE_HZ / 1000u)                /* 每毫秒包含的观测采样节拍数 */
#define MOTOR_OBSERVER_PERIOD_MIN_TICK  \
    (MOTOR_OBSERVER_TIMEBASE_HZ / MOTOR_OBSERVER_STREAM_MAX_HZ) /* 观测流最短采样周期 */
#define MOTOR_OBSERVER_PERIOD_MAX_TICK  \
    (MOTOR_OBSERVER_TIMEBASE_HZ / MOTOR_OBSERVER_STREAM_MIN_HZ) /* 观测流最长采样周期 */
#define MOTOR_CURRENT_LOOP_TS           \
    (1.0f / (float)MOTOR_CURRENT_LOOP_HZ)               /* 电流环采样周期，单位为秒 */
#define MOTOR_SPEED_LOOP_TS             \
    (1.0f / (float)MOTOR_SPEED_LOOP_HZ)                 /* 速度环采样周期，单位为秒 */
#define MOTOR_POSITION_LOOP_TS          \
    (1.0f / (float)MOTOR_POSITION_LOOP_HZ)              /* 位置环采样周期，单位为秒 */

/*===========================================================================*/
/*  电流控制限幅参数                                                         */
/*===========================================================================*/
#define MOTOR_CURRENT_VECTOR_LIMIT_A    (10.0f)        /* d/q轴电流矢量固定限幅，单位为A */

/*===========================================================================*/
/*  AB滤波器参数                                                             */
/*===========================================================================*/
#define MOTOR_AB_FILTER_BW_MIN_HZ       (1.0f)         /* AB滤波器带宽下限，单位为Hz */
#define MOTOR_AB_FILTER_BW_MAX_HZ       (500.0f)       /* AB滤波器带宽上限，单位为Hz */

/*===========================================================================*/
/*  SMO参数                                                                  */
/*===========================================================================*/
#define MOTOR_SMO_FILTER_BW_MIN_HZ      (1.0f)         /* SMO滤波带宽下限，单位为Hz */
#define MOTOR_SMO_FILTER_BW_MAX_HZ      (500.0f)       /* SMO滤波带宽上限，单位为Hz */

/*===========================================================================*/
/*  PLL参数                                                                  */
/*===========================================================================*/
#define MOTOR_PLL_BW_MIN_HZ             (1.0f)         /* PLL带宽下限，单位为Hz */
#define MOTOR_PLL_BW_MAX_HZ             (500.0f)       /* PLL带宽上限，单位为Hz */
#define MOTOR_PLL_EMF_MIN_V             (0.02f)        /* PLL允许鉴相的最小反电动势幅值，单位为V */
#define MOTOR_PLL_DAMPING_RATIO         (0.70710678f)  /* PLL固定阻尼比 */
#define MOTOR_PLL_OMEGA_LIMIT_RAD_S     (3000.0f)      /* PLL固定电角速度限幅，单位为rad/s */
#define MOTOR_PLL_INTEGRAL_LIMIT_RAD_S  (3000.0f)      /* PLL固定积分项限幅，单位为rad/s */

/*===========================================================================*/
/*  电机与FOC控制参数                                                        */
/*===========================================================================*/
#define LD              (0.031036f)            /* d轴电感，单位为mH */
#define LQ              (0.035016f)            /* q轴电感，单位为mH */

#define LS              (0.033026f)            /* 定子电感，单位为mH */
#define RS              (0.28659f)             /* 定子电阻，单位为欧姆 */

#define FOC_TS          MOTOR_CURRENT_LOOP_TS  /* 电流环采样周期，单位为秒 */

#define DENOMINATOR     \
    (2.0f * LS * 0.001f + RS * FOC_TS)         /* PI增益计算分母 */

#define FLUX            (0.00236)              /* 磁链常数 */

#endif /* FOC_CONFIG_H_ */
