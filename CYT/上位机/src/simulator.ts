import { Empty_telemetry } from "./data";
import type { Foc_loop_parameters_t, Motor_command_t, Telemetry_t } from "./types";

const Clamp = (Value: number, Minimum: number, Maximum: number) => Math.min(Maximum, Math.max(Minimum, Value));
const Motor_kv = 420;
const Motor_kt = 60 / (2 * Math.PI * Motor_kv);
const Current_adc_counts_per_amp = (4095 * 20 * 0.002) / 3.3;

export class Motor_simulator_t {
  private Telemetry: Telemetry_t = { ...Empty_telemetry };
  private Start_time = performance.now();
  private Mechanical_angle = 0;
  private Speed_setpoint = 0;
  private Position_travel_degree = 0;
  private Last_position_degree = 0;
  private Last_position_target = 0;
  private Position_track_ready = false;

  /***********************************************
   * @brief : 推进仿真电机状态并生成一帧遥测数据
   * @param : Command 电机控制命令
   * @param : Delta_time 仿真步长
   * @return: 电机遥测数据
   * @date  : 2026-07-22
   * @author: LYF
   ************************************************/
  public update(Command: Motor_command_t, Delta_time: number, Loop_parameters: Foc_loop_parameters_t): Telemetry_t {
    const Time = (performance.now() - this.Start_time) / 1000;
    const Running = Command.enabled && !Command.emergencyStopped;
    const Position_control = Running &&
      Command.driveMode !== "openLoop" &&
      Command.driveMode !== "foc_voice" &&
      Command.mode === "position";
    let Position_in_deadband = false;
    let Requested_speed = 0;
    let Requested_iq = 0;

    if (!Position_control) this.Position_track_ready = false;

    if (Running) {
      if (Command.driveMode === "openLoop") {
        Requested_iq = Command.voltageTarget * 0.42 * Command.direction;
        Requested_speed = Clamp(Command.angleStep * 54 * Command.direction, -12000, 12000);
      } else if (Command.driveMode === "foc_voice") {
        Requested_iq = Math.sin(Time * Math.PI * 2 * 440) * 0.04;
        Requested_speed = 0;
      } else if (Command.mode === "speed") {
        Requested_speed = Command.speedTarget;
        Requested_iq = Clamp((Requested_speed - this.Telemetry.speedActual) * 0.004, -12, 12);
      } else if (Command.mode === "current") {
        Requested_iq = Command.iqTarget;
        Requested_speed = Clamp(Requested_iq * 920, -12000, 12000);
      } else if (Command.mode === "voltage") {
        Requested_iq = Command.voltageTarget * 0.42 * Command.direction;
        Requested_speed = Clamp(Command.voltageTarget * 540 * Command.direction, -12000, 12000);
      } else {
        let Position_target = Command.positionTarget;
        while (Position_target >= 360) Position_target -= 360;
        while (Position_target < 0) Position_target += 360;
        let Position_error = Position_target - this.Mechanical_angle;
        while (Position_error > 180) Position_error -= 360;
        while (Position_error < -180) Position_error += 360;
        if (!this.Position_track_ready || this.Last_position_target !== Position_target) {
          this.Position_travel_degree = -Position_error;
          this.Last_position_degree = this.Mechanical_angle;
          this.Last_position_target = Position_target;
          this.Position_track_ready = true;
        } else {
          let Travel_step = this.Mechanical_angle - this.Last_position_degree;
          while (Travel_step > 180) Travel_step -= 360;
          while (Travel_step < -180) Travel_step += 360;
          this.Position_travel_degree += Travel_step;
          this.Last_position_degree = this.Mechanical_angle;
        }
        if (Command.positionReturnMode === "reversePath") {
          Position_error = -this.Position_travel_degree;
        }
        const Position_deadband = Clamp(Loop_parameters.positionDeadband, 0, 180);
        const Position_soft_range = Clamp(Loop_parameters.positionSoftRange, 0, 180);
        const Position_speed_deadband = Clamp(Loop_parameters.positionSpeedDeadband, 0, 100);
        const Position_limit = Clamp(Loop_parameters.positionOutputLimit, 0, 30000);
        if (Math.abs(Position_error) <= Position_deadband) {
          Position_in_deadband = true;
          Requested_speed = 0;
          if (Command.positionReturnMode === "shortest") {
            this.Position_travel_degree = 0;
            this.Last_position_degree = this.Mechanical_angle;
            this.Last_position_target = Position_target;
          }
        } else {
          const Effective_error = Math.sign(Position_error) * (Math.abs(Position_error) - Position_deadband);
          const Kp_scale = Position_soft_range > Position_deadband
            ? Clamp(Math.abs(Effective_error) / (Position_soft_range - Position_deadband), 0, 1)
            : 1;
          Requested_speed = Clamp(Effective_error * Kp_scale * Loop_parameters.positionKp, -Position_limit, Position_limit);
        }
        Requested_iq = Position_in_deadband &&
          Position_speed_deadband > 0 &&
          Math.abs(this.Telemetry.speedActual) <= Position_speed_deadband
            ? 0
            : Clamp((Requested_speed - this.Telemetry.speedActual) * 0.004, -8, 8);
      }
    }

    const Ramp_step = Math.max(1, Command.rampRate) * Delta_time;
    const Speed_error = Requested_speed - this.Speed_setpoint;
    this.Speed_setpoint += Clamp(Speed_error, -Ramp_step, Ramp_step);

    const Mechanical_noise = Running ? Math.sin(Time * 13.1) * 5.5 + Math.sin(Time * 41) * 1.3 : 0;
    const Speed_response = 1 - Math.exp(-Delta_time / 0.18);
    this.Telemetry.speedActual += (this.Speed_setpoint - this.Telemetry.speedActual) * Speed_response;
    const Display_speed = Math.abs(this.Telemetry.speedActual) < 0.2 ? 0 : this.Telemetry.speedActual + Mechanical_noise;

    const Iq_response = 1 - Math.exp(-Delta_time / 0.025);
    this.Telemetry.iqActual += (Requested_iq - this.Telemetry.iqActual) * Iq_response;
    this.Telemetry.idActual += ((Running ? Command.idTarget : 0) - this.Telemetry.idActual) * Iq_response;

    this.Mechanical_angle = (this.Mechanical_angle + this.Telemetry.speedActual * 6 * Delta_time + 360) % 360;
    const Electrical_angle = (this.Mechanical_angle * 7 + 2.4 + 360) % 360;
    const Electrical_radian = (Electrical_angle * Math.PI) / 180;
    const Current_peak = Math.hypot(this.Telemetry.iqActual, this.Telemetry.idActual);
    const Current_noise = Running ? Math.sin(Time * 173) * 0.035 : Math.sin(Time * 11) * 0.008;
    const Phase_a = Current_peak * Math.sin(Electrical_radian) + Current_noise;
    const Phase_b = Current_peak * Math.sin(Electrical_radian - (2 * Math.PI) / 3) + Current_noise;
    const Phase_c = -(Phase_a + Phase_b);
    const Voltage_q = Command.driveMode === "openLoop"
      ? Clamp(Command.voltageTarget, -22, 22)
      : Clamp(this.Telemetry.iqActual * 0.6 + Math.abs(Display_speed) * 0.0014, -22, 22);
    const Duty_span = Clamp((Voltage_q / 48) * 100, -45, 45);

    this.Telemetry = {
      timestamp: Time,
      speedTarget: Running ? this.Speed_setpoint : 0,
      speedActual: Display_speed,
      idTarget: Running ? Command.idTarget : 0,
      idActual: this.Telemetry.idActual,
      iqTarget: Running ? Requested_iq : 0,
      iqActual: this.Telemetry.iqActual + Math.sin(Time * 73) * 0.018,
      ia: Phase_a,
      ib: Phase_b,
      ic: Phase_c,
      adcRawU: Clamp(Math.round(2051 + Phase_a * Current_adc_counts_per_amp), 0, 4095),
      adcRawW: Clamp(Math.round(2049 + Phase_c * Current_adc_counts_per_amp), 0, 4095),
      ud: this.Telemetry.idActual * 0.5,
      uq: Voltage_q,
      busVoltage: 48.1 + Math.sin(Time * 0.8) * 0.08 - Current_peak * 0.018,
      mechanicalAngle: this.Mechanical_angle,
      electricalAngle: Electrical_angle,
      dutyA: 50 + Duty_span * Math.sin(Electrical_radian),
      dutyB: 50 + Duty_span * Math.sin(Electrical_radian - (2 * Math.PI) / 3),
      dutyC: 50 + Duty_span * Math.sin(Electrical_radian + (2 * Math.PI) / 3),
      zeroOffset: 2048,
      torque: this.Telemetry.iqActual * Motor_kt,
    };

    return { ...this.Telemetry };
  }

  /***********************************************
   * @brief : 清除仿真电机的动态状态
   * @param : 无
   * @return: 无
   * @date  : 2026-07-22
   * @author: LYF
   ************************************************/
  public reset(): void {
    this.Telemetry = { ...Empty_telemetry };
    this.Start_time = performance.now();
    this.Mechanical_angle = 0;
    this.Speed_setpoint = 0;
    this.Position_travel_degree = 0;
    this.Last_position_degree = 0;
    this.Last_position_target = 0;
    this.Position_track_ready = false;
  }
}
