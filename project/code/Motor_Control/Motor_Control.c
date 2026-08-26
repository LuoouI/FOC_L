#include "Motor_Control.h"
#include "Filter/AB_Filter.h"

static const ABFilterParam_t RpmFltCfg =
{
    .Ts = 0.001f,
    .Bw = 100.0f
};

static const float Rad2Rpm = 60.0f / TWO_PI;

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
        .Duty_ramp_step = MOTOR_DUTY_RAMP_DEFAULT_STEP
    },
    .Open_loop =
    {
        .Angle = 0u,
        .Step = MOTOR_OPEN_LOOP_DEFAULT_STEP,
        .Hold_count = 0u,
        .Started = 0u
    },
    .Pole_pairs = 7u,
    .Control_mode = MOTOR_CONTROL_ENCODER_FOC,
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
    Motor.Encoder.Spd_rpm = Omega * Rad2Rpm;
}
