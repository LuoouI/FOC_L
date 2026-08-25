#include "Motor_Control.h"
#include "Function/Function.h"
#include "Motor_Flash/Motor_Flash.h"
#include "My_TCPWM/My_TCPWM.h"
#include "SVPWM/SVPWM.h"
#include "Current_sample/Current_sample.h"

Foc_motor_t Motor = {
    .sensor_id = menc15a_2_module,
    .direction = 1,
    .zero_offset = 0,
    .pole_pairs = 7,
    .mechanical_angle = 0,
    .electrical_angle = 0,
    .clark = {0.0f, 0.0f},
    .park = {0.0f, 0.0f},
    .motor_duty = 0,
    .duty_output = 0.0f,
    .duty_ramp_step = MOTOR_DUTY_RAMP_DEFAULT_STEP,
    .ud = 0.0f,
    .uq = 2.0f,
    .control_mode = MOTOR_CONTROL_ENCODER_FOC,
    .open_loop_angle = 0u,
    .open_loop_step = MOTOR_OPEN_LOOP_DEFAULT_STEP,
    .open_loop_hold_count = 0u,
    .open_loop_started = 0u,
    .ready = 0u
};

const Motor_ZeroCalib_t Motor_zeroCalib =
{
    .Voltage      = 1.0f,
    .Hold_ms      = 200u,
    .Step_count   = 100u,
    .Step_ms      = 8u,
    .Sample_count = 16u,
    .Sample_ms    = 2u,
    .Min_travel   = 400
};

static volatile uint8 MotorCalibrating = 0u;

/***********************************************
 * @brief : 按设定步长更新电机实际输出幅值
 * @param : Output_duty 当前实际输出幅值
 * @param : Target_duty 目标输出幅值
 * @param : Ramp_step 单控制周期变化量
 * @return: 更新后的实际输出幅值
 * @date  : 2026-08-18
 * @author: LYF
 ************************************************/
static float Motor_Duty_Ramp(float Output_duty,
                             float Target_duty,
                             float Ramp_step)
{
    if (Ramp_step <= 0.0f)
    {
        return Target_duty;
    }

    if (Output_duty < Target_duty)
    {
        Output_duty += Ramp_step;
        if (Output_duty > Target_duty)
        {
            Output_duty = Target_duty;
        }
    }
    else if (Output_duty > Target_duty)
    {
        Output_duty -= Ramp_step;
        if (Output_duty < Target_duty)
        {
            Output_duty = Target_duty;
        }
    }

    return Output_duty;
}

/***********************************************
 * @brief : 按指定电压矢量输出三相PWM
 * @param : motor 电机控制对象
 * @param : Ud d轴电压，单位为V
 * @param : Uq q轴电压，单位为V
 * @param : ElectricalAngle 电压矢量电角度
 * @return: void
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
static void Foc_OutputVoltage(Foc_motor_t *motor,
                              float Ud,
                              float Uq,
                              uint16 ElectricalAngle)
{
    uint16 DutyA;
    uint16 DutyB;
    uint16 DutyC;

    if (motor == NULL)
    {
        return;
    }

    foc_voltage_calc_duty(
        Ud,
        Uq,
        ElectricalAngle,
        &DutyA,
        &DutyB,
        &DutyC);
    My_TCPWM_SetDuty(DutyA, DutyB, DutyC);
}

/***********************************************
 * @brief : 连续读取磁编码器并计算展开角度平均值
 * @param : motor 电机控制对象
 * @param : unwrap 连续角度展开对象
 * @param : sample_count 采样次数
 * @param : sample_ms 采样间隔
 * @return: 连续机械角度平均值
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
static int32 Motor_Read_Average_Angle(Foc_motor_t *motor,
                                      AngleUnwrap_t *unwrap,
                                      uint16 sample_count,
                                      uint16 sample_ms)
{
    int32 Angle_sum;
    int32 Angle_now;
    uint16 Sample_index;

    if ((motor == NULL) || (unwrap == NULL) || (sample_count == 0u))
    {
        return 0;
    }

    Angle_sum = 0;
    for (Sample_index = 0u; Sample_index < sample_count; Sample_index++)
    {
        Angle_now = Angle_Unwrap(
            unwrap,
            menc15a_get_absolute_data(motor->sensor_id));
        Angle_sum += Angle_now;

        if (sample_ms > 0u)
        {
            system_delay_ms(sample_ms);
        }
    }

    if (Angle_sum >= 0)
    {
        Angle_sum += (int32)(sample_count / 2u);
    }
    else
    {
        Angle_sum -= (int32)(sample_count / 2u);
    }

    return Angle_sum / (int32)sample_count;
}

void Angle_Update(Foc_motor_t *motor)
{
    int32 RelativeMechanicalAngle;

    if (motor == NULL)
    {
        return;
    }

    if (MotorCalibrating != 0u)
    {
        return;
    }

    motor->mechanical_angle = menc15a_get_absolute_data(motor->sensor_id);

    /* 零偏保存为编码器机械计数，方向在乘极对数前生效。 */
    RelativeMechanicalAngle =
        (int32)motor->mechanical_angle - (int32)motor->zero_offset;
    motor->electrical_angle = Angle_Wrap(
        RelativeMechanicalAngle *
        (int32)motor->pole_pairs *
        (int32)motor->direction);
}

void Foc_Init(Foc_motor_t *motor)
{
    if (motor == NULL)
    {
        return;
    }

    if (motor->direction == 0)
    {
        motor->direction = 1;
    }

    if (motor->pole_pairs == 0u)
    {
        motor->pole_pairs = 1u;
    }

    if (motor->open_loop_step == 0u)
    {
        motor->open_loop_step = MOTOR_OPEN_LOOP_DEFAULT_STEP;
    }

    if (motor->duty_ramp_step <= 0.0f)
    {
        motor->duty_ramp_step = MOTOR_DUTY_RAMP_DEFAULT_STEP;
    }

    if ((motor->control_mode != MOTOR_CONTROL_OPEN_LOOP) &&
        (motor->control_mode != MOTOR_CONTROL_ENCODER_FOC))
    {
        motor->control_mode = MOTOR_CONTROL_OPEN_LOOP;
    }

    motor->motor_duty = 0;
    motor->duty_output = 0.0f;
    motor->open_loop_angle = 0u;
    motor->open_loop_hold_count = 0u;
    motor->open_loop_started = 0u;
}

void Motor_Stop(Foc_motor_t *motor)
{
    if (motor == NULL)
    {
        return;
    }

    motor->motor_duty = 0;
    motor->duty_output = 0.0f;
    motor->open_loop_hold_count = 0u;
    motor->open_loop_started = 0u;
    My_TCPWM_SetDuty(
        (uint16)(TCPWM_DUTY_MAX / 2u),
        (uint16)(TCPWM_DUTY_MAX / 2u),
        (uint16)(TCPWM_DUTY_MAX / 2u));
}

void Foc_Set_Control_Mode(Foc_motor_t *motor, Motor_control_mode_t mode)
{
    if (motor == NULL)
    {
        return;
    }

    if ((mode != MOTOR_CONTROL_OPEN_LOOP) &&
        (mode != MOTOR_CONTROL_ENCODER_FOC))
    {
        return;
    }

    Motor_Stop(motor);
    motor->control_mode = mode;
    motor->open_loop_angle = 0u;
}

void Foc_Run(Foc_motor_t *motor)
{
    int32 TargetDuty;
    float Duty;
    float DutyAbs;
    float DutyScale;
    float Ud;
    float Uq;

    if (motor == NULL)
    {
        return;
    }

    if (MotorCalibrating != 0u)
    {
        return;
    }

    if ((motor->control_mode == MOTOR_CONTROL_ENCODER_FOC) &&
        (motor->ready == 0u))
    {
        Motor_Stop(motor);
        return;
    }

    TargetDuty = Int_Limit(
        (int32)motor->motor_duty,
        -(int32)TCPWM_DUTY_MAX,
        (int32)TCPWM_DUTY_MAX);
    motor->motor_duty = (int16)TargetDuty;

    if (TargetDuty == 0)
    {
        Motor_Stop(motor);
        return;
    }

    Duty = Motor_Duty_Ramp(
        motor->duty_output,
        (float)TargetDuty,
        motor->duty_ramp_step);
    motor->duty_output = Duty;

    DutyAbs = (Duty >= 0) ? Duty : -Duty;
    DutyScale = (float)DutyAbs / (float)TCPWM_DUTY_MAX;

    Ud = motor->ud * DutyScale;
    Uq = motor->uq * DutyScale;

    if (motor->control_mode == MOTOR_CONTROL_OPEN_LOOP)
    {
        if (motor->open_loop_started == 0u)
        {
            motor->open_loop_angle = 0u;
            motor->open_loop_hold_count = 0u;
            motor->open_loop_started = 1u;
        }

        if (motor->open_loop_hold_count < MOTOR_OPEN_LOOP_ALIGN_COUNT)
        {
            motor->open_loop_hold_count++;
        }
        else if (Duty > 0)
        {
            motor->open_loop_angle = Angle_Wrap(
                (int32)motor->open_loop_angle +
                (int32)motor->open_loop_step);
        }
        else
        {
            motor->open_loop_angle = Angle_Wrap(
                (int32)motor->open_loop_angle -
                (int32)motor->open_loop_step);
        }

        /* 开环角度代表转子磁链目标角，使用d轴电压牵引转子跟随。 */
        Foc_OutputVoltage(motor, Uq, 0.0f, motor->open_loop_angle);
        return;
    }

    if (Duty < 0)
    {
        Uq = -Uq;
    }
    Foc_OutputVoltage(motor, Ud, Uq, motor->electrical_angle);
}

uint8 Motor_Zero_Calibration(Foc_motor_t *motor)
{
    AngleUnwrap_t EncoderUnwrap;
    int32 StartContinuousAngle;
    int32 StopContinuousAngle;
    int32 EncoderTravel;
    int32 EncoderTravelAbs;
    uint16 StartMechanicalAngle;
    uint16 StopMechanicalAngle;
    uint16 SampleMechanicalAngle;
    uint16 ElectricalAngle;
    uint16 PolePairs;
    uint32 Step;
    uint32 InterruptState;
    int8 PreviousDirection;
    uint16 PreviousZeroOffset;
    uint8 PreviousPolePairs;
    Motor_control_mode_t PreviousControlMode;
    uint8 PreviousReady;
    uint8 CalibrationResult;
    uint16 CalibrationDutyA;
    uint16 CalibrationDutyB;
    uint16 CalibrationDutyC;

    printf("开始零点校准\r\n");

    if (motor == NULL)
    {
        printf("零点校准失败：电机对象为空\r\n");
        return 1u;
    }

    if (Motor_zeroCalib.Voltage <= 0.0f)
    {
        printf("零点校准失败：校准电压必须大于0\r\n");
        return 1u;
    }

    /* 校准前同步采样母线电压，避免使用中断尚未更新的旧值。 */
    InterruptState = interrupt_global_disable();
    VBUS_Get();
    interrupt_global_enable(InterruptState);
    if ((SVPWM.VBUS <= 0.0f) ||
        (SVPWM.DQ_Limit < Motor_zeroCalib.Voltage))
    {
        Motor_Stop(motor);
        printf("零点校准失败：母线电压=%.2fV，允许矢量电压=%.2fV\r\n",
               SVPWM.VBUS,
               SVPWM.DQ_Limit);
        return 1u;
    }

    foc_voltage_calc_duty(
        Motor_zeroCalib.Voltage,
        0.0f,
        0u,
        &CalibrationDutyA,
        &CalibrationDutyB,
        &CalibrationDutyC);
    printf("零点校准输出：母线=%.2fV，矢量=%.2fV，占空比=%u,%u,%u\r\n",
           SVPWM.VBUS,
           Motor_zeroCalib.Voltage,
           (uint32)CalibrationDutyA,
           (uint32)CalibrationDutyB,
           (uint32)CalibrationDutyC);

    PreviousDirection = motor->direction;
    PreviousZeroOffset = motor->zero_offset;
    PreviousPolePairs = motor->pole_pairs;
    PreviousControlMode = motor->control_mode;
    PreviousReady = motor->ready;

    if ((PreviousControlMode == MOTOR_CONTROL_ENCODER_FOC) &&
        (PreviousReady == 0u))
    {
        PreviousControlMode = MOTOR_CONTROL_OPEN_LOOP;
    }

    CalibrationResult = 1u;
    motor->ready = 0u;
    MotorCalibrating = 1u;

    /* 校准标志已屏蔽异步FOC，保持中断以持续更新母线电压。 */
    Motor_Stop(motor);

    /* 使用已验证工程的定向电压和保持时间，使转子先稳定吸合。 */
    Foc_OutputVoltage(
        motor,
        Motor_zeroCalib.Voltage,
        0.0f,
        0u);
    system_delay_ms(Motor_zeroCalib.Hold_ms);
    printf("零点校准定向：编码器=%u，电流校准=%u，相电流=%.2f,%.2f,%.2fA\r\n",
           (uint32)menc15a_get_absolute_data(motor->sensor_id),
           (uint32)Current.calibrated,
           Current.current_u,
           Current.current_v,
           Current.current_w);

    Angle_Unwrap_Clear(&EncoderUnwrap);
    StartContinuousAngle = Motor_Read_Average_Angle(
        motor,
        &EncoderUnwrap,
        Motor_zeroCalib.Sample_count,
        Motor_zeroCalib.Sample_ms);
    StartMechanicalAngle = Angle_Wrap(StartContinuousAngle);

    for (Step = 0u;
         Step <= Motor_zeroCalib.Step_count;
         Step++)
    {
        ElectricalAngle = (uint16)(
            (uint32)ANGLE_MAX * Step /
            Motor_zeroCalib.Step_count);
        Foc_OutputVoltage(
            motor,
            Motor_zeroCalib.Voltage,
            0.0f,
            ElectricalAngle);
        system_delay_ms(Motor_zeroCalib.Step_ms);

        SampleMechanicalAngle =
            menc15a_get_absolute_data(motor->sensor_id);
        (void)Angle_Unwrap(&EncoderUnwrap, SampleMechanicalAngle);

        if ((Step == (Motor_zeroCalib.Step_count / 4u)) ||
            (Step == (Motor_zeroCalib.Step_count / 2u)) ||
            (Step == (Motor_zeroCalib.Step_count * 3u / 4u)) ||
            (Step == Motor_zeroCalib.Step_count))
        {
            foc_voltage_calc_duty(
                Motor_zeroCalib.Voltage,
                0.0f,
                ElectricalAngle,
                &CalibrationDutyA,
                &CalibrationDutyB,
                &CalibrationDutyC);
            printf("零点校准扫角：电角=%u，占空比=%u,%u,%u，编码器=%u，相电流=%.2f,%.2f,%.2fA\r\n",
                   (uint32)ElectricalAngle,
                   (uint32)CalibrationDutyA,
                   (uint32)CalibrationDutyB,
                   (uint32)CalibrationDutyC,
                   (uint32)SampleMechanicalAngle,
                   Current.current_u,
                   Current.current_v,
                   Current.current_w);
        }
    }

    StopContinuousAngle = Motor_Read_Average_Angle(
        motor,
        &EncoderUnwrap,
        Motor_zeroCalib.Sample_count,
        Motor_zeroCalib.Sample_ms);
    StopMechanicalAngle = Angle_Wrap(StopContinuousAngle);
    EncoderTravel = StopContinuousAngle - StartContinuousAngle;
    EncoderTravelAbs = (EncoderTravel >= 0) ?
                       EncoderTravel : -EncoderTravel;

    Motor_Stop(motor);

    if (EncoderTravelAbs >= Motor_zeroCalib.Min_travel)
    {
        PolePairs = (uint16)(
            ((uint32)ANGLE_PERIOD + (uint32)(EncoderTravelAbs / 2)) /
            (uint32)EncoderTravelAbs);

        if (PolePairs > 0u)
        {
            motor->direction = (EncoderTravel >= 0) ? 1 : -1;
            motor->pole_pairs = (uint8)PolePairs;
            motor->zero_offset = StopMechanicalAngle;
            /* 校准成功后使用磁编码器角度进行FOC换相。 */
            motor->control_mode = MOTOR_CONTROL_ENCODER_FOC;
            motor->open_loop_angle = 0u;
            motor->open_loop_hold_count = 0u;
            motor->open_loop_started = 0u;
            CalibrationResult = 0u;
        }
    }

    if (CalibrationResult == 0u)
    {
        /* Flash写入期间短暂关闭中断，避免参数页访问被异步流程打断。 */
        InterruptState = interrupt_global_disable();
        CalibrationResult = Motor_Flash_Write();
        interrupt_global_enable(InterruptState);
    }

    if (CalibrationResult != 0u)
    {
        motor->direction = PreviousDirection;
        motor->zero_offset = PreviousZeroOffset;
        motor->pole_pairs = PreviousPolePairs;
        motor->control_mode = PreviousControlMode;
        motor->ready = PreviousReady;
    }

    MotorCalibrating = 0u;
    Angle_Update(motor);

    if (CalibrationResult == 0u)
    {
        printf("零点校准成功：起始角=%u，结束角=%u，累计行程=%ld，方向=%d，极对数=%u，电角度=%u\r\n",
               (uint32)StartMechanicalAngle,
               (uint32)StopMechanicalAngle,
               (long)EncoderTravel,
               (int32)motor->direction,
               (uint32)motor->pole_pairs,
               (uint32)motor->electrical_angle);
    }
    else
    {
        printf("零点校准失败：起始角=%u，结束角=%u，累计行程=%ld\r\n",
               (uint32)StartMechanicalAngle,
               (uint32)StopMechanicalAngle,
               (long)EncoderTravel);
    }

    return CalibrationResult;
}
