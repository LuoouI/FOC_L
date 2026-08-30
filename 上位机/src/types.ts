export type Control_mode_t = "voltage" | "current" | "speed" | "position";
export type Position_return_mode_t = "shortest" | "reversePath";

export type Drive_mode_t = "openLoop" | "foc_voice" | "encoderFoc" | "sensorlessFoc";

export type Connection_state_t = "offline" | "connecting" | "online";

export interface Music_track_t {
  id: number;
  name: string;
}

export interface Telemetry_t {
  timestamp: number;
  speedTarget: number;
  speedActual: number;
  idTarget: number;
  idActual: number;
  iqTarget: number;
  iqActual: number;
  ia: number;
  ib: number;
  ic: number;
  adcRawU: number;
  adcRawW: number;
  ud: number;
  uq: number;
  busVoltage: number;
  mechanicalAngle: number;
  electricalAngle: number;
  dutyA: number;
  dutyB: number;
  dutyC: number;
  zeroOffset: number;
  torque: number;
}

export interface Motor_command_t {
  enabled: boolean;
  emergencyStopped: boolean;
  direction: 1 | -1;
  positionReturnMode: Position_return_mode_t;
  driveMode: Drive_mode_t;
  mode: Control_mode_t;
  speedTarget: number;
  iqTarget: number;
  idTarget: number;
  voltageTarget: number;
  udTarget: number;
  positionTarget: number;
  angleTarget: number;
  angleStep: number;
  focVoiceSongId: number;
  focVoiceSession: number;
  currentBandwidth: number;
  rampRate: number;
}

export interface Foc_loop_parameters_t {
  currentBandwidth: number;
  speedKp: number;
  speedKi: number;
  speedIntegralLimit: number;
  abFilterBandwidth: number;
  positionKp: number;
  positionSoftRange: number;
  positionSpeedDeadband: number;
  positionOutputLimit: number;
  positionDeadband: number;
}

export interface Channel_definition_t {
  key: keyof Telemetry_t;
  label: string;
  unit: string;
  color: string;
  visible: boolean;
}

export interface Event_log_t {
  id: number;
  timestamp: Date;
  level: "info" | "warning" | "error" | "success";
  source: string;
  message: string;
}

export interface Serial_frame_t {
  type: number;
  sequence: number;
  timestamp: number;
  direction: "RX" | "TX";
  length: number;
  data: number[];
}

export interface Serial_port_info_t {
  path: string;
  manufacturer?: string;
  friendlyName?: string;
  serialNumber?: string;
  vendorId?: string;
  productId?: string;
  simulated?: boolean;
}

export interface Serial_config_t {
  port: string;
  baudRate: number;
  dataBits: 5 | 6 | 7 | 8;
  stopBits: 1 | 1.5 | 2;
  parity: "none" | "even" | "odd" | "mark" | "space";
}
