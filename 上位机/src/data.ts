import type { Channel_definition_t, Telemetry_t } from "./types";

/* 每通道最多保留的示波器历史采样点数 */
export const Scope_sample_limit = 50000;

export const Scope_channels: Channel_definition_t[] = [
  { key: "speedTarget", label: "目标转速", unit: "rpm", color: "#f4b942", visible: false },
  { key: "speedActual", label: "实际转速", unit: "rpm", color: "#42d3a5", visible: false },
  { key: "iqTarget", label: "目标 Iq", unit: "A", color: "#7da6ff", visible: false },
  { key: "iqActual", label: "实际 Iq", unit: "A", color: "#d18cff", visible: false },
  { key: "idTarget", label: "目标 Id", unit: "A", color: "#f0a5ff", visible: false },
  { key: "idActual", label: "实际 Id", unit: "A", color: "#ff7c78", visible: false },
  { key: "ia", label: "A 相电流", unit: "A", color: "#44c7f4", visible: false },
  { key: "ib", label: "B 相电流", unit: "A", color: "#f48fb1", visible: false },
  { key: "ic", label: "C 相电流", unit: "A", color: "#b8d66d", visible: false },
  { key: "adcRawU", label: "U 相 ADC 原始值", unit: "计数", color: "#45b8ac", visible: false },
  { key: "adcRawW", label: "W 相 ADC 原始值", unit: "计数", color: "#f28e5b", visible: false },
  { key: "ud", label: "D 轴电压", unit: "V", color: "#72d6c9", visible: false },
  { key: "uq", label: "Q 轴电压", unit: "V", color: "#ffb36b", visible: false },
  { key: "busVoltage", label: "母线电压", unit: "V", color: "#ff9866", visible: false },
  { key: "mechanicalAngle", label: "机械角度", unit: "°", color: "#9f8cff", visible: false },
  { key: "electricalAngle", label: "电角度", unit: "°", color: "#d18cff", visible: false },
  { key: "dutyA", label: "PWM A", unit: "%", color: "#4fd1a6", visible: false },
  { key: "dutyB", label: "PWM B", unit: "%", color: "#6ab8ff", visible: false },
  { key: "dutyC", label: "PWM C", unit: "%", color: "#f2ca61", visible: false },
  { key: "zeroOffset", label: "编码器零点偏移", unit: "计数", color: "#ff6b6b", visible: false },
  { key: "torque", label: "估算转矩", unit: "N·m", color: "#9ad0c2", visible: false },
];

export const Empty_telemetry: Telemetry_t = {
  timestamp: 0,
  speedTarget: 0,
  speedActual: 0,
  idTarget: 0,
  idActual: 0,
  iqTarget: 0,
  iqActual: 0,
  ia: 0,
  ib: 0,
  ic: 0,
  adcRawU: 0,
  adcRawW: 0,
  ud: 0,
  uq: 0,
  busVoltage: 0,
  mechanicalAngle: 0,
  electricalAngle: 0,
  dutyA: 0,
  dutyB: 0,
  dutyC: 0,
  zeroOffset: 0,
  torque: 0,
};
