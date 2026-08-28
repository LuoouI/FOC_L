#include "Motor_Control.h"
#include "Filter/AB_Filter.h"
#include "FOC_Voice/FOC_Voice.h"
#include "My_TCPWM/My_TCPWM.h"
#include "SVPWM/SVPWM.h"

static const ABFilterParam_t RpmFltCfg =
{
    .Ts = 0.001f,
    .Bw = 100.0f
};

Foc_motor_t Motor =
{
    .Encoder =
    {
        .Sensor_id = menc15a_2_module,
        .Direction = 1,
        .Zero_offset = 0u,
        .Mechanical_angle = 0u,
        .Electrical_angle = 0u,
        .Spd_rpm = 0.0f
    },
    .Output =
    {
        .Duty_target = 0,
        .Duty_output = 0.0f,
    },
    .Open_loop =
    {
        .Uq = 0.0f,
        .Angle = 0u,
        .Step = 0,
        .Align_count = 4000u,
        .Hold_count = 0u,
        .Started = 0u
    },
    .Pole_pairs = 7u,
    .Control_mode = MOTOR_CONTROL_OPEN_LOOP,
    .Zero_ready = 0u
};

/*===========================================================================*/
/*  前期准备                                                                  */
/*===========================================================================*/

void Angle_Update(void)
{
    int32 MechAng;

    Motor.Encoder.Mechanical_angle =
        menc15a_get_absolute_data(Motor.Encoder.Sensor_id);

    MechAng =
        ((int32)Motor.Encoder.Mechanical_angle -
         (int32)Motor.Encoder.Zero_offset) *
        (int32)Motor.Encoder.Direction;

    Motor.Encoder.Electrical_angle =
        Angle_Wrap(MechAng * (int32)Motor.Pole_pairs);
}

void RPM_Cal(void)
{
    static uint8 FltReady = 0u;
    int32 MechAng;
    uint16 WrapAng;
    float MeasAng;
    float Omega;

    if (FltReady == 0u)
    {
        ABFilter_Init(&Angle, &RpmFltCfg);
        FltReady = 1u;
    }

    MechAng =
        ((int32)Motor.Encoder.Mechanical_angle -
         (int32)Motor.Encoder.Zero_offset) *
        (int32)Motor.Encoder.Direction;

    WrapAng = Angle_Wrap(MechAng);
    MeasAng =
        (float)WrapAng * (TWO_PI / (float)ANGLE_PERIOD);

    Omega = ABFilter_Update(&Angle, MeasAng);
    Motor.Encoder.Spd_rpm = Omega * 60.0f / TWO_PI;
}

/*===========================================================================*/
/*  开环角度牵引                                                              */
/*===========================================================================*/

/***********************************************
 * @brief : 按给定d/q轴电压和步长执行一次开环角度牵引
 * @param : Uq q轴电压，单位为V
 * @param : Ud d轴电压，单位为V
 * @param : Step 单控制周期电角度增量，负值表示反向
 * @return: 无
 * @date  : 2026-08-27
 * @author: L
 ************************************************/
static void Motor_openloop_set(float Uq, float Ud, int16 Step)
{
    uint16 DutyA;
    uint16 DutyB;
    uint16 DutyC;

    if ((Uq == 0.0f) && (Ud == 0.0f))
    {
        Motor.Open_loop.Angle = 0u;
        Motor.Open_loop.Hold_count = 0u;
        Motor.Open_loop.Started = 0u;

        My_TCPWM_SetDuty(
            (uint16)(TCPWM_DUTY_MAX / 2u),
            (uint16)(TCPWM_DUTY_MAX / 2u),
            (uint16)(TCPWM_DUTY_MAX / 2u));
        return;
    }

    if (Motor.Open_loop.Started == 0u)
    {
        Motor.Open_loop.Angle = 0u;
        Motor.Open_loop.Hold_count = 0u;
        Motor.Open_loop.Started = 1u;
    }

    if (Motor.Open_loop.Hold_count < Motor.Open_loop.Align_count)
    {
        Motor.Open_loop.Hold_count++;
    }
    else
    {
        Motor.Open_loop.Angle = Angle_Wrap(
            (int32)Motor.Open_loop.Angle + (int32)Step);
    }

    foc_voltage_calc_duty(
        Ud,
        Uq,
        Motor.Open_loop.Angle,
        &DutyA,
        &DutyB,
        &DutyC);
    My_TCPWM_SetDuty(DutyA, DutyB, DutyC);

}

/*===========================================================================*/
/*  总控制                                                                    */
/*===========================================================================*/

void Motor_Control_Loop(void)
{
    switch (Motor.Control_mode)
    {
        case MOTOR_CONTROL_OPEN_LOOP:
            Motor_openloop_set(
                Motor.Open_loop.Uq,
                0.0f,
                Motor.Open_loop.Step);
            break;

        case MOTOR_CONTROL_ENCODER_FOC:

            break;

        case MOTOR_CONTROL_VOICE:
            FOC_Voice_Loop();
            break;

        default:
            break;
    }
}
