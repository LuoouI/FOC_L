#include "Motor_Control/Feedback/Encoder_Feedback.h"
#include "Motor_Control/Motor_Control.h"
#include "Filter/AB_Filter.h"
#include "Function/Function.h"

/***********************************************
 * @brief : 更新电机机械角度和电角度
 * @param : 无
 * @return: 无
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
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

/***********************************************
 * @brief : 读取扣除零偏并修正方向后的机械角度
 * @param : 无
 * @return: 机械角度，范围0~360度
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
float Motor_Control_GetMechanicalDegree(void)
{
    int32 Mechanical_count;

    Mechanical_count =
        ((int32)Motor.Encoder.Mechanical_angle -
         (int32)Motor.Encoder.Zero_offset) *
        (int32)Motor.Encoder.Direction;
    Mechanical_count = (int32)Angle_Wrap(Mechanical_count);

    return (float)Mechanical_count * 360.0f / (float)ANGLE_PERIOD;
}

/***********************************************
 * @brief : 使用AB滤波器计算电机机械转速，需按1 kHz周期调用
 * @param : 无
 * @return: 无，结果保存到Motor.Encoder.Spd_rpm
 * @date  : 2026-08-26
 * @author: L
 ************************************************/
void RPM_Cal(void)
{
    static uint8 FltReady = 0u;
    ABFilterParam_t RpmFltCfg;
    int32 MechAng;
    uint16 WrapAng;
    float MeasAng;
    float Omega;

    if ((FltReady == 0u) ||
        (Angle.Param.Bw_hz != Motor.Ab_filter_bandwidth))
    {
        RpmFltCfg.Ts = MOTOR_SPEED_LOOP_TS;
        RpmFltCfg.Bw_hz = Motor.Ab_filter_bandwidth;
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
