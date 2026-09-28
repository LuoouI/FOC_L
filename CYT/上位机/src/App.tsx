import { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import {
  Activity,
  AlertTriangle,
  ArrowDownToLine,
  Cable,
  CircleGauge,
  CircleOff,
  Database,
  Dock,
  Download,
  ExternalLink,
  Gauge,
  GripHorizontal,
  GripVertical,
  Maximize2,
  Minimize2,
  Moon,
  Music2,
  Pause,
  Play,
  PlugZap,
  Power,
  Radio,
  RefreshCw,
  RotateCcw,
  Search,
  Send,
  ShieldAlert,
  Square,
  Sun,
  Trash2,
  Unplug,
  Zap,
} from "lucide-react";
import { Empty_telemetry, Scope_channels, Scope_sample_limit } from "./data";
import { Motor_simulator_t } from "./simulator";
import { Default_music_track, Music_tracks } from "./music";
import { ScopeCanvas } from "./components/ScopeCanvas";
import { Telemetry_history_t } from "./telemetryHistory";
import type {
  Channel_definition_t,
  Connection_state_t,
  Control_mode_t,
  Drive_mode_t,
  Event_log_t,
  Foc_loop_parameters_t,
  Music_track_t,
  Motor_command_t,
  Observer_stream_batch_t,
  Position_return_mode_t,
  Serial_config_t,
  Serial_frame_t,
  Serial_port_info_t,
  Sensorless_group_t,
  Telemetry_t,
} from "./types";

const Initial_command: Motor_command_t = {
  enabled: false,
  emergencyStopped: false,
  direction: 1,
  positionReturnMode: "shortest",
  driveMode: "openLoop",
  mode: "speed",
  speedTarget: 0,
  iqTarget: 0,
  idTarget: 0,
  voltageTarget: 0,
  udTarget: 0,
  positionTarget: 0,
  angleTarget: 0,
  angleStep: 0,
  focVoiceSongId: 0,
  focVoiceSession: 0,
  currentBandwidth: 0,
  rampRate: 0,
};

const Simulated_telemetry_channels: Array<keyof Telemetry_t> = [
  "speedTarget",
  "speedActual",
  "idTarget",
  "idActual",
  "iqTarget",
  "iqActual",
  "busVoltage",
  "zeroOffset",
  "mechanicalAngle",
  "mechanicalAngleTarget",
  "electricalAngle",
  "torque",
  "ia",
  "ib",
  "ic",
  "adcRawU",
  "adcRawW",
  "ud",
  "uq",
  "dutyA",
  "dutyB",
  "dutyC",
  "iAlphaActual",
  "iBetaActual",
  "smoIAlphaEst",
  "smoIBetaEst",
  "smoIqError",
  "smoEAlpha",
  "smoEBeta",
  "smoEAlphaFilter",
  "smoEBetaFilter",
  "pllMechanicalAngleEst",
  "pllElectricalAngleEst",
  "pllOmegaEst",
  "pllPhaseError",
];

const Observer_stream_field_bits: Partial<Record<keyof Telemetry_t, number>> = {
  iAlphaActual: 2 ** 0,
  iBetaActual: 2 ** 1,
  smoIAlphaEst: 2 ** 2,
  smoIBetaEst: 2 ** 3,
  smoIqError: 2 ** 4,
  smoEAlpha: 2 ** 5,
  smoEBeta: 2 ** 6,
  smoEAlphaFilter: 2 ** 7,
  smoEBetaFilter: 2 ** 8,
  electricalAngle: 2 ** 9,
  pllElectricalAngleEst: 2 ** 10,
  pllOmegaEst: 2 ** 11,
  pllPhaseError: 2 ** 12,
  pllMechanicalAngleEst: 2 ** 13,
  speedActual: 2 ** 14,
  mechanicalAngle: 2 ** 15,
  speedTarget: 2 ** 16,
  idTarget: 2 ** 17,
  idActual: 2 ** 18,
  iqTarget: 2 ** 19,
  iqActual: 2 ** 20,
  ia: 2 ** 21,
  ib: 2 ** 22,
  ic: 2 ** 23,
  adcRawU: 2 ** 24,
  adcRawW: 2 ** 25,
  ud: 2 ** 26,
  uq: 2 ** 27,
  busVoltage: 2 ** 28,
  dutyA: 2 ** 29,
  dutyB: 2 ** 30,
  dutyC: 2 ** 31,
  zeroOffset: 2 ** 32,
  torque: 2 ** 33,
};

const Observer_stream_fields: Array<keyof Telemetry_t> = [
  "iAlphaActual",
  "iBetaActual",
  "smoIAlphaEst",
  "smoIBetaEst",
  "smoIqError",
  "smoEAlpha",
  "smoEBeta",
  "smoEAlphaFilter",
  "smoEBetaFilter",
  "electricalAngle",
  "pllElectricalAngleEst",
  "pllOmegaEst",
  "pllPhaseError",
  "pllMechanicalAngleEst",
  "speedActual",
  "mechanicalAngle",
  "speedTarget",
  "idTarget",
  "idActual",
  "iqTarget",
  "iqActual",
  "ia",
  "ib",
  "ic",
  "adcRawU",
  "adcRawW",
  "ud",
  "uq",
  "busVoltage",
  "dutyA",
  "dutyB",
  "dutyC",
  "zeroOffset",
  "torque",
];

const Observer_stream_field_mask = (2 ** Observer_stream_fields.length) - 1;

/***********************************************
 * @brief : 判断数字位图中是否选择指定观测字段
 * @param : Field_mask 观测字段位图
 * @param : Bit 字段位号
 * @return: 已选择返回true，否则返回false
 * @date  : 2026-09-23
 * @author: L
 ************************************************/
function Is_observer_stream_field_selected(Field_mask: number, Bit: number) {
  const Bit_value = 2 ** Bit;
  return Math.floor(Field_mask / Bit_value) % 2 === 1;
}

/***********************************************
 * @brief : 将固定步长紧凑观测二进制批次解码为连续遥测样本
 * @param : Batch 主进程传入的紧凑观测批次
 * @return: 仅包含本批次订阅字段的遥测样本
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
function Decode_observer_stream_batch(Batch: Observer_stream_batch_t) {
  const Field_mask = Number(Batch.fieldMask);
  const Timestamp_hz = Number(Batch.timestampHz ?? 1000);
  const Field_count = Observer_stream_fields.reduce(
    (Count, _Field, Bit) => Count + (Is_observer_stream_field_selected(Field_mask, Bit) ? 1 : 0),
    0,
  );
  const Expected_stride = 4 + Field_count * 4;
  if (!Number.isInteger(Field_mask) ||
      Field_mask < 0 ||
      Field_mask > Observer_stream_field_mask ||
      !Number.isFinite(Timestamp_hz) ||
      Timestamp_hz <= 0 ||
      Batch.sampleStride !== Expected_stride ||
      !Number.isSafeInteger(Batch.sampleCount) ||
      Batch.sampleCount < 0) {
    throw new Error("紧凑观测二进制批次格式无效");
  }

  const Bytes = Batch.data instanceof Uint8Array
    ? Batch.data
    : new Uint8Array(Batch.data);
  if (Bytes.byteLength !== Batch.sampleCount * Expected_stride) {
    throw new Error("紧凑观测二进制批次长度不匹配");
  }
  const View = new DataView(Bytes.buffer, Bytes.byteOffset, Bytes.byteLength);
  const Samples: Array<Partial<Telemetry_t>> = [];
  for (let Sample_index = 0; Sample_index < Batch.sampleCount; Sample_index += 1) {
    const Sample_offset = Sample_index * Expected_stride;
    const Sample: Partial<Telemetry_t> = {
      timestamp: View.getUint32(Sample_offset, true) / Timestamp_hz,
    };
    let Value_offset = Sample_offset + 4;
    Observer_stream_fields.forEach((Field, Bit) => {
      if (!Is_observer_stream_field_selected(Field_mask, Bit)) return;
      (Sample as Record<string, number>)[Field] = View.getFloat32(Value_offset, true);
      Value_offset += 4;
    });
    Samples.push(Sample);
  }
  return Samples;
}

const Mode_labels: Record<Control_mode_t, string> = {
  voltage: "电压",
  current: "电流",
  speed: "速度",
  position: "位置",
};

const Drive_mode_labels: Record<Drive_mode_t, string> = {
  openLoop: "开环驱动",
  foc_voice: "音乐模式",
  encoderFoc: "有感 FOC",
  sensorlessFoc: "无感 FOC",
};

const Foc_control_modes: Control_mode_t[] = ["current", "speed", "position"];

const Initial_loop_parameters: Foc_loop_parameters_t = {
  currentBandwidth: 0,
  speedKp: 0,
  speedKi: 0,
  speedIntegralLimit: 0,
  abFilterBandwidth: 0,
  positionKp: 0,
  positionSoftRange: 0,
  positionSpeedDeadband: 0,
  positionOutputLimit: 0,
  positionDeadband: 0,
  smoKSlide: 0,
  smoBoundaryCurrent: 0,
  smoFilterBandwidth: 0,
  pllBandwidth: 0,
};

/* 仿真设备连接后使用的环路参数。 */
const Simulated_loop_parameters: Foc_loop_parameters_t = {
  currentBandwidth: 1000,
  speedKp: 0.01,
  speedKi: 0,
  speedIntegralLimit: 0,
  abFilterBandwidth: 50,
  positionKp: 0,
  positionSoftRange: 0,
  positionSpeedDeadband: 20,
  positionOutputLimit: 0,
  positionDeadband: 0,
  smoKSlide: 10,
  smoBoundaryCurrent: 0.5,
  smoFilterBandwidth: 250,
  pllBandwidth: 20,
};

const Sensorless_group_labels: Record<Sensorless_group_t, string> = {
  hfi: "HFI",
  smo: "SMO",
  pll: "PLL",
};

type Theme_mode_t = "dark" | "light";
type App_page_t = "workspace" | "communication";
type Parameter_group_t = "foc" | "observer";

interface Workspace_layout_t {
  workbenchScale: number;
  controlWidth: number;
  channelWidth: number;
}

interface Workspace_viewport_t {
  width: number;
  height: number;
}

interface Floating_panel_position_t {
  left: number;
  top: number;
}

interface Floating_panel_drag_t {
  pointerId: number;
  offsetX: number;
  offsetY: number;
}

interface Observer_group_drag_t extends Floating_panel_drag_t {
  startX: number;
  startY: number;
  detached: boolean;
}

interface Foc_mode_drag_t extends Floating_panel_drag_t {
  startX: number;
  startY: number;
  detached: boolean;
  mode: Control_mode_t;
}

const Page_titles: Record<App_page_t, { title: string; description: string }> = {
  workspace: { title: "实时工作台", description: "电机控制、实时波形与状态监控" },
  communication: { title: "串口诊断", description: "串口配置、收发统计与协议帧" },
};

const Navigation_items = [
  { id: "workspace", label: "实时工作台", icon: Gauge },
  { id: "communication", label: "串口诊断", icon: Cable },
] as const;

const Theme_storage_key = "foc-theme";
const Serial_config_storage_key = "foc-serial-config";
const Serial_port_poll_interval_ms = 1000;
const Workspace_layout_storage_key = "foc-workspace-layout";
const Control_selection_storage_key = "foc-control-selection";
const Slider_boundary_storage_prefix = "foc-slider-boundary:";
const Parameter_read_frame_type = 0x11;
const Observer_parameter_read_frame_type = 0x13;
const Song_list_frame_type = 0x14;
const Zero_calibration_frame_type = 0x15;
const Board_reset_frame_type = 0x16;
const Observer_stream_config_frame_type = 0x17;
const Observer_stream_period_ms = 1;
const Observer_stream_bandwidth_ratio = 0.8;
const Scope_refresh_interval_ms = 16;
const Status_refresh_interval_ms = 50;

/***********************************************
 * @brief : 在勾选和串口配置入口检查1 ms观测流是否超出发送带宽
 * @param : Channels 待应用的通道选择
 * @param : Config 待应用的串口配置
 * @return: 带宽不足时返回原因，否则返回空字符串
 * @date  : 2026-09-28
 * @author: L
 ************************************************/
function Get_channel_selection_error(Channels: Channel_definition_t[], Config: Serial_config_t) {
  if (Config.port === "simulator") return "";
  const Selected_count = Channels.filter((Channel) =>
    Channel.visible && Observer_stream_field_bits[Channel.key] !== undefined).length;
  const Budget_bps = Math.floor(Config.baudRate * Observer_stream_bandwidth_ratio);
  const Bits_per_byte = 1 + Config.dataBits + Config.stopBits + (Config.parity === "none" ? 0 : 1);
  let Channel_limit = 0;
  for (let Field_count = 1; Field_count <= Observer_stream_fields.length; Field_count += 1) {
    /* 与0x27批次格式一致：256字节负载、14字节批次头、每字段4字节，最多8点。 */
    const Batch_count = Math.min(8, Math.floor((256 - 14) / (Field_count * 4)));
    const Frame_bytes = 10 + 14 + Field_count * 4 * Batch_count;
    if (Frame_bytes * Bits_per_byte * 1000 > Budget_bps * Batch_count * Observer_stream_period_ms) break;
    Channel_limit = Field_count;
  }
  return Selected_count > Channel_limit
    ? `当前 ${Config.baudRate} bit/s 最多支持 ${Channel_limit} 个设备通道按 1 ms 采样，本次选择了 ${Selected_count} 个。请减少通道，或同步提高上下位机波特率。`
    : "";
}

const Previous_default_workspace_layout: Workspace_layout_t = {
  workbenchScale: 1,
  controlWidth: 330,
  channelWidth: 320,
};
const Default_workspace_layout: Workspace_layout_t = {
  workbenchScale: 1,
  controlWidth: 231,
  channelWidth: 224,
};
const Default_workspace_viewport: Workspace_viewport_t = {
  width: 1400,
  height: 560,
};
const Floating_panel_default_width = 280;
const Floating_panel_default_height = 460;
const Default_serial_config: Serial_config_t = {
  port: "simulator",
  baudRate: 460800,
  dataBits: 8,
  stopBits: 1,
  parity: "none",
};

interface Control_selection_t {
  driveMode: Drive_mode_t;
  mode: Control_mode_t;
  direction: 1 | -1;
  positionReturnMode: Position_return_mode_t;
}

const Default_control_selection: Control_selection_t = {
  driveMode: "openLoop",
  mode: "speed",
  direction: 1,
  positionReturnMode: "shortest",
};

/***********************************************
 * @brief : 读取上次选择的控制模式、方向和位置回正方式
 * @return: 可直接使用的控制模式选择
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
function Get_initial_control_selection(): Control_selection_t {
  try {
    const Saved_text = window.localStorage.getItem(Control_selection_storage_key);
    if (!Saved_text) return Default_control_selection;
    const Saved_selection = JSON.parse(Saved_text) as Partial<Control_selection_t>;
    const Valid_drive_mode: Drive_mode_t[] = ["openLoop", "foc_voice", "encoderFoc", "sensorlessFoc"];
    const Valid_mode: Control_mode_t[] = ["voltage", "current", "speed", "position"];
    const Saved_drive_mode = (Saved_selection.driveMode as string | undefined) === "voice"
      ? "foc_voice"
      : Saved_selection.driveMode;
    const Drive_mode = Valid_drive_mode.includes(Saved_drive_mode as Drive_mode_t)
      ? Saved_drive_mode as Drive_mode_t
      : Default_control_selection.driveMode;
    return {
      driveMode: Drive_mode,
      mode: Drive_mode === "sensorlessFoc"
        ? "speed"
        : Valid_mode.includes(Saved_selection.mode as Control_mode_t)
          ? Saved_selection.mode as Control_mode_t
          : Default_control_selection.mode,
      direction: Number(Saved_selection.direction) === -1 ? -1 : 1,
      positionReturnMode: Saved_selection.positionReturnMode === "reversePath"
        ? "reversePath"
        : "shortest",
    };
  } catch {
    return Default_control_selection;
  }
}

/***********************************************
 * @brief : 读取上次保存的界面主题
 * @return: 深色或浅色主题
 * @date  : 2026-07-23
 * @author: LYF
 ************************************************/
function Get_initial_theme(): Theme_mode_t {
  const Current_theme = document.documentElement.dataset.theme;
  if (Current_theme === "light" || Current_theme === "dark") return Current_theme;
  try {
    const Saved_theme = window.localStorage.getItem(Theme_storage_key);
    return Saved_theme === "light" ? "light" : "dark";
  } catch {
    return "dark";
  }
}

/***********************************************
 * @brief : 读取并校验上次保存的工作台尺寸
 * @return: 可直接使用的工作台尺寸
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function Get_initial_workspace_layout(): Workspace_layout_t {
  try {
    const Saved_text = window.localStorage.getItem(Workspace_layout_storage_key);
    if (!Saved_text) return Default_workspace_layout;
    const Saved_layout = JSON.parse(Saved_text) as Partial<Workspace_layout_t>;
    const Saved_workbench_scale = Number(Saved_layout.workbenchScale);
    const Saved_control_width = Number(Saved_layout.controlWidth);
    const Saved_channel_width = Number(Saved_layout.channelWidth);
    return {
      workbenchScale: Number.isFinite(Saved_workbench_scale)
        ? Math.min(Math.max(Saved_workbench_scale, 0.6), 1.6)
        : Default_workspace_layout.workbenchScale,
      controlWidth: Number.isFinite(Saved_control_width)
        ? Math.min(Math.max(Saved_control_width === Previous_default_workspace_layout.controlWidth ? Default_workspace_layout.controlWidth : Saved_control_width, 220), 480)
        : Default_workspace_layout.controlWidth,
      channelWidth: Number.isFinite(Saved_channel_width)
        ? Math.min(Math.max(Saved_channel_width === Previous_default_workspace_layout.channelWidth ? Default_workspace_layout.channelWidth : Saved_channel_width, 180), 480)
        : Default_workspace_layout.channelWidth,
    };
  } catch {
    return Default_workspace_layout;
  }
}

/***********************************************
 * @brief : 读取并校验上次保存的串口配置
 * @return: 可直接使用的串口配置
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function Get_initial_serial_config(): Serial_config_t {
  try {
    const Saved_text = window.localStorage.getItem(Serial_config_storage_key);
    if (!Saved_text) return Default_serial_config;
    const Saved_config = JSON.parse(Saved_text) as Partial<Serial_config_t>;
    const Valid_data_bits = [5, 6, 7, 8].includes(Number(Saved_config.dataBits));
    const Valid_stop_bits = [1, 1.5, 2].includes(Number(Saved_config.stopBits));
    const Valid_parity = ["none", "even", "odd", "mark", "space"].includes(String(Saved_config.parity));
    if (
      typeof Saved_config.port !== "string"
      || Saved_config.port.length === 0
      || !Number.isFinite(Saved_config.baudRate)
      || Number(Saved_config.baudRate) <= 0
      || !Valid_data_bits
      || !Valid_stop_bits
      || !Valid_parity
    ) return Default_serial_config;
    return Saved_config as Serial_config_t;
  } catch {
    return Default_serial_config;
  }
}

/***********************************************
 * @brief : 规范化串口信息中的可变字段
 * @param : Ports 待规范化的串口列表
 * @return: 用于比较的稳定串口信息列表
 * @date  : 2026-09-14
 * @author: L
 ************************************************/
function Normalize_serial_ports(Ports: Serial_port_info_t[]) {
  return Ports
    .map((Port) => ({
      path: Port.path,
      manufacturer: Port.manufacturer ?? "",
      friendlyName: Port.friendlyName ?? "",
      serialNumber: Port.serialNumber ?? "",
      vendorId: Port.vendorId ?? "",
      productId: Port.productId ?? "",
      simulated: Port.simulated === true,
    }))
    .sort((Left, Right) => Left.path.localeCompare(Right.path));
}

/***********************************************
 * @brief : 比较两次扫描得到的串口列表是否一致
 * @param : Previous_ports 上一次扫描得到的串口列表
 * @param : Current_ports 本次扫描得到的串口列表
 * @return: 列表内容一致时返回 true
 * @date  : 2026-09-14
 * @author: L
 ************************************************/
function Are_serial_ports_equal(Previous_ports: Serial_port_info_t[], Current_ports: Serial_port_info_t[]): boolean {
  if (Previous_ports.length !== Current_ports.length) return false;
  return JSON.stringify(Normalize_serial_ports(Previous_ports)) === JSON.stringify(Normalize_serial_ports(Current_ports));
}

const Format_value = (Value: number, Digits = 1) => Number.isFinite(Value) ? Value.toFixed(Digits) : "--";

/***********************************************
 * @brief : 将仿真遥测打包成与串口协议一致的负载
 * @param : Telemetry 电机遥测数据
 * @return: 24 字节基础遥测负载
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function buildSimulationTelemetryPayload(Telemetry: Telemetry_t): number[] {
  const Payload_buffer = new ArrayBuffer(24);
  const Payload_view = new DataView(Payload_buffer);
  Payload_view.setUint8(0, Telemetry.state);
  Payload_view.setUint8(1, Telemetry.mode);
  Payload_view.setUint8(2, Telemetry.fault);
  Payload_view.setUint8(3, Telemetry.flags);
  Payload_view.setUint32(4, Math.round(Telemetry.timestamp * 1000), true);
  Payload_view.setFloat32(8, Telemetry.speedActual, true);
  Payload_view.setFloat32(12, Telemetry.iqActual, true);
  Payload_view.setFloat32(16, Telemetry.busVoltage, true);
  Payload_view.setFloat32(20, Telemetry.mechanicalAngle, true);
  return [...new Uint8Array(Payload_buffer)];
}

interface Numeric_input_props_t {
  label: string;
  value: number;
  unit: string;
  minimum?: number;
  maximum?: number;
  step?: number;
  disabled?: boolean;
  deferred?: boolean;
  fieldClassName?: string;
  adjuster?: "slider" | "dial";
  cyclic?: boolean;
  boundaryStorageKey?: string;
  resetValue?: number;
  resetLabel?: string;
  onChange: (Value: number) => void;
}

interface Slider_boundary_t {
  minimum: number;
  maximum: number;
}

/***********************************************
 * @brief : 将参数数值整理为适合输入框显示的十进制文本
 * @param : Value 当前参数值
 * @return: 最多保留六位小数的文本
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
function Format_numeric_input_value(Value: number) {
  if (!Number.isFinite(Value)) return "0";
  return Number(Value.toFixed(6)).toString();
}

/***********************************************
 * @brief : 读取并校验指定滑块断电保存的左右边界
 * @param : Storage_key 滑块独立存储键
 * @param : Parameter_minimum 参数安全下限
 * @param : Parameter_maximum 参数安全上限
 * @param : Minimum_gap 左右边界最小间隔
 * @return: 可用于当前参数的滑块边界
 * @date  : 2026-09-14
 * @author: L
 ************************************************/
function Get_saved_slider_boundary(Storage_key: string | undefined,
                                   Parameter_minimum: number,
                                   Parameter_maximum: number,
                                   Minimum_gap: number): Slider_boundary_t {
  const Default_boundary = {
    minimum: Parameter_minimum,
    maximum: Parameter_maximum,
  };
  if (!Storage_key || (Parameter_maximum <= Parameter_minimum)) return Default_boundary;

  try {
    const Saved_text = window.localStorage.getItem(`${Slider_boundary_storage_prefix}${Storage_key}`);
    if (!Saved_text) return Default_boundary;
    const Saved_boundary = JSON.parse(Saved_text) as Partial<Slider_boundary_t>;
    if ((typeof Saved_boundary.minimum !== "number") ||
        (typeof Saved_boundary.maximum !== "number") ||
        !Number.isFinite(Saved_boundary.minimum) ||
        !Number.isFinite(Saved_boundary.maximum)) {
      return Default_boundary;
    }

    const Safe_gap = Math.min(
      Math.max(Minimum_gap, Number.EPSILON),
      Parameter_maximum - Parameter_minimum);
    const Saved_minimum = Math.max(
      Parameter_minimum,
      Math.min(Number(Saved_boundary.minimum), Parameter_maximum - Safe_gap));
    const Saved_maximum = Math.min(
      Parameter_maximum,
      Math.max(Number(Saved_boundary.maximum), Parameter_minimum + Safe_gap));
    if ((Saved_maximum - Saved_minimum) < Safe_gap) return Default_boundary;

    return { minimum: Saved_minimum, maximum: Saved_maximum };
  } catch {
    return Default_boundary;
  }
}

/***********************************************
 * @brief : 将指定滑块的左右边界保存到浏览器本地存储
 * @param : Storage_key 滑块独立存储键
 * @param : Boundary 当前滑块左右边界
 * @return: 无
 * @date  : 2026-09-14
 * @author: L
 ************************************************/
function Save_slider_boundary(Storage_key: string | undefined, Boundary: Slider_boundary_t) {
  if (!Storage_key) return;
  try {
    window.localStorage.setItem(
      `${Slider_boundary_storage_prefix}${Storage_key}`,
      JSON.stringify(Boundary));
  } catch {
    /* 本地存储不可用时仍允许当前窗口继续调整边界。 */
  }
}

/***********************************************
 * @brief : 显示带单位的数值输入控件及可选的滑块或圆盘调节器
 * @param : label 参数名称
 * @param : value 当前数值
 * @param : unit 参数单位
 * @return: 数值输入组件
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function NumericInput({ label, value, unit, minimum, maximum, step, disabled, deferred, fieldClassName, adjuster, cyclic, boundaryStorageKey, resetValue, resetLabel, onChange }: Numeric_input_props_t) {
  const [Draft_value, setDraftValue] = useState(() => Format_numeric_input_value(value));
  const Parameter_minimum = minimum ?? 0;
  const Parameter_maximum = maximum ?? 100;
  const Base_adjuster_step = (step !== undefined && step > 0) ? step : 1;
  const [Slider_boundary, setSliderBoundary] = useState(() => Get_saved_slider_boundary(
    boundaryStorageKey,
    Parameter_minimum,
    Parameter_maximum,
    Base_adjuster_step));
  const Slider_minimum = Slider_boundary.minimum;
  const Slider_maximum = Slider_boundary.maximum;
  const Editing_ref = useRef(false);
  const Cancel_commit_ref = useRef(false);
  const Adjuster_minimum = adjuster === "slider" ? Slider_minimum : Parameter_minimum;
  const Adjuster_maximum = adjuster === "slider" ? Slider_maximum : Parameter_maximum;
  const Adjuster_range = Math.max(0, Adjuster_maximum - Adjuster_minimum);
  const Cyclic_adjuster = (adjuster === "dial") && (cyclic === true) && (Adjuster_range > 0);
  const Effective_adjuster_step = adjuster === "slider" && Adjuster_range > 0
    ? Math.max(Base_adjuster_step, Adjuster_range / 1000)
    : Base_adjuster_step;
  const Adjuster_value = Math.min(Adjuster_maximum, Math.max(Adjuster_minimum, value));
  const Dial_value = Cyclic_adjuster && (Adjuster_value >= Adjuster_maximum)
    ? Adjuster_minimum
    : Adjuster_value;
  const Dial_progress = Adjuster_range > 0
    ? (Dial_value - Adjuster_minimum) / Adjuster_range
    : 0;

  useEffect(() => {
    if (!Editing_ref.current) setDraftValue(Format_numeric_input_value(value));
  }, [value]);

  useEffect(() => {
    setSliderBoundary(Get_saved_slider_boundary(
      boundaryStorageKey,
      Parameter_minimum,
      Parameter_maximum,
      Base_adjuster_step));
  }, [boundaryStorageKey, Parameter_minimum, Parameter_maximum, Base_adjuster_step]);

  const Handle_change = (Event: React.ChangeEvent<HTMLInputElement>) => {
    const Input_value = Event.currentTarget.value;
    const Normalized_value = Input_value.replace(/^(-?)0+(?=\d)/, "$1");
    if (Normalized_value !== Input_value) Event.currentTarget.value = Normalized_value;
    if (Normalized_value === "") {
      /* 数值输入框不能保留空值，退格清空个位数时回落到 0。 */
      if (deferred) {
        setDraftValue("0");
      } else {
        onChange(0);
      }
      return;
    }
    if (deferred) {
      setDraftValue(Normalized_value);
      return;
    }
    onChange(Number(Normalized_value));
  };

  const Commit_value = () => {
    Editing_ref.current = false;
    if (Cancel_commit_ref.current) {
      Cancel_commit_ref.current = false;
      setDraftValue(Format_numeric_input_value(value));
      return;
    }

    const Number_value = Number(Draft_value);
    if ((Draft_value.trim() === "") || !Number.isFinite(Number_value)) {
      setDraftValue(Format_numeric_input_value(value));
      return;
    }

    const Bounded_value = Math.min(
      maximum ?? Number.POSITIVE_INFINITY,
      Math.max(minimum ?? Number.NEGATIVE_INFINITY, Number_value));
    const Limited_value = Cyclic_adjuster && (Bounded_value >= Adjuster_maximum)
      ? Adjuster_minimum
      : Bounded_value;
    setDraftValue(Format_numeric_input_value(Limited_value));
    if (Limited_value !== value) onChange(Limited_value);
  };

  const Handle_key_down = (Event: React.KeyboardEvent<HTMLInputElement>) => {
    if (!deferred) return;
    if (Event.key === "Enter") Event.currentTarget.blur();
    if (Event.key === "Escape") {
      Cancel_commit_ref.current = true;
      Event.currentTarget.blur();
    }
  };

  /* 将调节值限制到参数范围，循环圆盘则首尾归一，再按参数步长取整。 */
  const Apply_adjuster_value = (Next_value: number) => {
    const Relative_value = Cyclic_adjuster
      ? ((Next_value - Adjuster_minimum) % Adjuster_range + Adjuster_range) % Adjuster_range
      : Next_value - Adjuster_minimum;
    const Limited_value = Cyclic_adjuster
      ? Adjuster_minimum + Relative_value
      : Math.min(Adjuster_maximum, Math.max(Adjuster_minimum, Next_value));
    const Rounded_source = Number((
      Adjuster_minimum +
      Math.round((Limited_value - Adjuster_minimum) / Effective_adjuster_step) * Effective_adjuster_step
    ).toFixed(6));
    const Rounded_value = Cyclic_adjuster && (Rounded_source >= Adjuster_maximum)
      ? Adjuster_minimum
      : Math.min(Adjuster_maximum, Math.max(Adjuster_minimum, Rounded_source));

    setDraftValue(Format_numeric_input_value(Rounded_value));
    if (Rounded_value !== value) onChange(Rounded_value);
  };

  /* 提交滑块左边界，并将其约束在参数安全范围和右边界以内。 */
  const Commit_slider_minimum = (Event: React.FocusEvent<HTMLInputElement>) => {
    const Input_value = Number(Event.currentTarget.value);
    const Next_minimum = Number.isFinite(Input_value)
      ? Math.min(
          Slider_maximum - Base_adjuster_step,
          Math.max(Parameter_minimum, Input_value))
      : Slider_minimum;

    const Next_boundary = { minimum: Next_minimum, maximum: Slider_maximum };
    setSliderBoundary(Next_boundary);
    Save_slider_boundary(boundaryStorageKey, Next_boundary);
    Event.currentTarget.value = Format_numeric_input_value(Next_minimum);
  };

  /* 提交滑块右边界，并将其约束在左边界和参数安全范围以内。 */
  const Commit_slider_maximum = (Event: React.FocusEvent<HTMLInputElement>) => {
    const Input_value = Number(Event.currentTarget.value);
    const Next_maximum = Number.isFinite(Input_value)
      ? Math.max(
          Slider_minimum + Base_adjuster_step,
          Math.min(Parameter_maximum, Input_value))
      : Slider_maximum;

    const Next_boundary = { minimum: Slider_minimum, maximum: Next_maximum };
    setSliderBoundary(Next_boundary);
    Save_slider_boundary(boundaryStorageKey, Next_boundary);
    Event.currentTarget.value = Format_numeric_input_value(Next_maximum);
  };

  /* 根据指针相对圆心的角度更新圆盘参数，循环圆盘使用完整360度范围。 */
  const Handle_dial_pointer = (Event: React.PointerEvent<HTMLDivElement>) => {
    const Bounds = Event.currentTarget.getBoundingClientRect();
    const Center_x = Bounds.left + Bounds.width / 2;
    const Center_y = Bounds.top + Bounds.height / 2;
    let Angle = Math.atan2(
      Event.clientY - Center_y,
      Event.clientX - Center_x) * 180 / Math.PI + 90;

    if (Cyclic_adjuster) {
      if (Angle < 0) Angle += 360;
      Apply_adjuster_value(
        Adjuster_minimum + (Angle / 360) * Adjuster_range);
      return;
    }

    if (Angle > 180) Angle -= 360;
    Angle = Math.min(135, Math.max(-135, Angle));
    Apply_adjuster_value(
      Adjuster_minimum + ((Angle + 135) / 270) * Adjuster_range);
  };

  /* 使用方向键、翻页键和首尾键精调圆盘参数。 */
  const Handle_dial_key_down = (Event: React.KeyboardEvent<HTMLDivElement>) => {
    let Next_value = value;

    if ((Event.key === "ArrowRight") || (Event.key === "ArrowUp")) Next_value += Effective_adjuster_step;
    else if ((Event.key === "ArrowLeft") || (Event.key === "ArrowDown")) Next_value -= Effective_adjuster_step;
    else if (Event.key === "PageUp") Next_value += Effective_adjuster_step * 10;
    else if (Event.key === "PageDown") Next_value -= Effective_adjuster_step * 10;
    else if (Event.key === "Home") Next_value = Adjuster_minimum;
    else if (Event.key === "End") Next_value = Adjuster_maximum;
    else return;

    Event.preventDefault();
    Apply_adjuster_value(Next_value);
  };

  return (
    <div className={fieldClassName ? `field-block ${fieldClassName}` : "field-block"}>
      <span>{label}</span>
      <div className={resetValue !== undefined ? "input-with-unit has-reset-action" : "input-with-unit"}>
        <input
          type="number"
          value={deferred ? Draft_value : value}
          min={minimum}
          max={maximum}
          step={step}
          disabled={disabled}
          aria-label={`${label}数值`}
          onChange={Handle_change}
          onFocus={() => { Editing_ref.current = true; }}
          onBlur={deferred ? Commit_value : undefined}
          onKeyDown={Handle_key_down}
        />
        <b>{unit}</b>
        {resetValue !== undefined && (
          <button
            type="button"
            className="numeric-reset-button"
            disabled={disabled}
            title={resetLabel ?? `${label}复位`}
            aria-label={resetLabel ?? `${label}复位`}
            onPointerDown={(Event) => Event.preventDefault()}
            onClick={() => {
              Editing_ref.current = false;
              Cancel_commit_ref.current = false;
              setDraftValue(Format_numeric_input_value(resetValue));
              if (resetValue !== value) onChange(resetValue);
            }}
          ><RotateCcw size={13} /></button>
        )}
      </div>
      {adjuster === "slider" && (
        <div className="parameter-slider-control">
          <input
            className="parameter-slider-bound"
            type="number"
            defaultValue={Format_numeric_input_value(Slider_minimum)}
            min={Parameter_minimum}
            max={Slider_maximum - Base_adjuster_step}
            step={Base_adjuster_step}
            disabled={disabled}
            aria-label={`${label}滑块左边界`}
            onBlur={Commit_slider_minimum}
            onKeyDown={(Event) => { if (Event.key === "Enter") Event.currentTarget.blur(); }}
          />
          <input
            className="parameter-slider"
            type="range"
            value={Adjuster_value}
            min={Adjuster_minimum}
            max={Adjuster_maximum}
            step={Effective_adjuster_step}
            disabled={disabled}
            aria-label={`${label}滑块`}
            title={`步长 ${Format_numeric_input_value(Effective_adjuster_step)}`}
            onChange={(Event) => Apply_adjuster_value(Number(Event.currentTarget.value))}
          />
          <input
            className="parameter-slider-bound"
            type="number"
            defaultValue={Format_numeric_input_value(Slider_maximum)}
            min={Slider_minimum + Base_adjuster_step}
            max={Parameter_maximum}
            step={Base_adjuster_step}
            disabled={disabled}
            aria-label={`${label}滑块右边界`}
            onBlur={Commit_slider_maximum}
            onKeyDown={(Event) => { if (Event.key === "Enter") Event.currentTarget.blur(); }}
          />
        </div>
      )}
      {adjuster === "dial" && (
        <div
          className={`parameter-dial${Cyclic_adjuster ? " cyclic" : ""}${disabled ? " disabled" : ""}`}
          role="slider"
          tabIndex={disabled ? -1 : 0}
          aria-label={`${label}圆盘`}
          aria-valuemin={Adjuster_minimum}
          aria-valuemax={Adjuster_maximum}
          aria-valuenow={Adjuster_value}
          aria-disabled={disabled}
          style={{
            "--dial-angle": `${Cyclic_adjuster ? Dial_progress * 360 : -135 + Dial_progress * 270}deg`,
            "--dial-progress-angle": `${Dial_progress * (Cyclic_adjuster ? 360 : 270)}deg`,
          } as React.CSSProperties}
          onPointerDown={(Event) => {
            if (disabled) return;
            Event.preventDefault();
            Event.currentTarget.focus();
            Event.currentTarget.setPointerCapture(Event.pointerId);
            Handle_dial_pointer(Event);
          }}
          onPointerMove={(Event) => {
            if (!disabled && Event.currentTarget.hasPointerCapture(Event.pointerId)) {
              Handle_dial_pointer(Event);
            }
          }}
          onPointerUp={(Event) => {
            if (Event.currentTarget.hasPointerCapture(Event.pointerId)) {
              Event.currentTarget.releasePointerCapture(Event.pointerId);
            }
          }}
          onKeyDown={Handle_dial_key_down}
        >
          <i className="parameter-dial-needle" />
          <span>{Format_numeric_input_value(Adjuster_value)}<b>{unit}</b></span>
        </div>
      )}
    </div>
  );
}

interface Status_badge_props_t {
  state: Connection_state_t;
}

function StatusBadge({ state }: Status_badge_props_t) {
  const Labels = { offline: "离线", connecting: "连接中", online: "在线" };
  return <span className={`status-badge ${state}`}><i />{Labels[state]}</span>;
}

interface Telemetry_card_props_t {
  label: string;
  value: string;
  unit: string;
  icon: typeof Gauge;
  tone: string;
  secondary: string;
}

function TelemetryCard({ label, value, unit, icon: Icon, tone, secondary }: Telemetry_card_props_t) {
  return (
    <article className="telemetry-card" style={{ "--tone": tone } as React.CSSProperties}>
      <div className="telemetry-label"><Icon size={15} />{label}</div>
      <div className="telemetry-value"><strong>{value}</strong><span>{unit}</span></div>
      <small>{secondary}</small>
    </article>
  );
}

interface Angle_telemetry_card_props_t {
  mechanicalAngle: number;
  electricalAngle: number;
}

/***********************************************
 * @brief : 在右上角状态栏显示机械角和电角度观测
 * @param : mechanicalAngle 当前机械角度
 * @param : electricalAngle 当前电角度
 * @return: 紧凑角度观测卡
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function AngleTelemetryCard({ mechanicalAngle, electricalAngle }: Angle_telemetry_card_props_t) {
  return (
    <article className="telemetry-card angle-telemetry-card" style={{ "--tone": "#d18cff" } as React.CSSProperties}>
      <div>
        <div className="telemetry-label"><CircleGauge size={15} />机械角度</div>
        <div className="telemetry-value"><strong>{Format_value(mechanicalAngle, 1)}</strong><span>°</span></div>
        <small>电角度 {Format_value(electricalAngle, 1)}°</small>
      </div>
      <div className="angle-observer-dial" aria-label={`机械角 ${Format_value(mechanicalAngle, 1)} 度，电角度 ${Format_value(electricalAngle, 1)} 度`}>
        <div className="angle-observer-ring electrical"><i style={{ transform: `translateX(-50%) rotate(${electricalAngle}deg)` }} /></div>
        <div className="angle-observer-ring mechanical"><i style={{ transform: `translateX(-50%) rotate(${mechanicalAngle}deg)` }} /></div>
        <span />
      </div>
    </article>
  );
}

interface Workspace_view_props_t {
  telemetry: Telemetry_t;
  history: Telemetry_history_t;
  frozenSamples: Telemetry_t[] | null;
  sampleCount: number;
  channels: Channel_definition_t[];
  receivedChannelKeys: Array<keyof Telemetry_t>;
  command: Motor_command_t;
  loopParameters: Foc_loop_parameters_t;
  musicTracks: Music_track_t[];
  connected: boolean;
  paused: boolean;
  logs: Event_log_t[];
  onCommand: (Patch: Partial<Motor_command_t>) => void;
  onLoopParameterChange: (Key: keyof Foc_loop_parameters_t, Value: number) => void;
  onChannel: (Key: keyof Telemetry_t) => void;
  onChannelSelection: (Keys: Array<keyof Telemetry_t>) => void;
  onPause: () => void;
  onClear: () => void;
  onExport: () => void;
  onStart: () => void;
  onStop: () => void;
  onZeroCalibration: () => void;
}

/***********************************************
 * @brief : 组合实时波形、电机控制和状态监控区域
 * @param : telemetry 最新遥测数据
 * @param : history 历史遥测缓冲
 * @param : frozenSamples 暂停时保存的波形快照
 * @param : command 电机控制命令
 * @return: 实时工作台页面
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function WorkspaceView({ telemetry, history, frozenSamples, sampleCount, channels, receivedChannelKeys, command, loopParameters, musicTracks, connected, paused, logs, onCommand, onLoopParameterChange, onChannel, onChannelSelection, onPause, onClear, onExport, onStart, onStop, onZeroCalibration }: Workspace_view_props_t) {
  const Device_enabled = connected && telemetry.state !== 0;
  const Enable_pending = command.enabled && !Device_enabled;
  const [Scope_expanded, setScopeExpanded] = useState(false);
  const [Clear_confirm_open, setClearConfirmOpen] = useState(false);
  const [Sensorless_group, setSensorlessGroup] = useState<Sensorless_group_t>("smo");
  const [Observer_panel_floating, setObserverPanelFloating] = useState(false);
  const [Observer_panel_position, setObserverPanelPosition] = useState<Floating_panel_position_t>({ left: 24, top: 96 });
  const [Foc_parameter_panel_floating, setFocParameterPanelFloating] = useState(false);
  const [Foc_parameter_panel_position, setFocParameterPanelPosition] = useState<Floating_panel_position_t>({ left: 24, top: 96 });
  const [Workspace_layout, setWorkspaceLayout] = useState<Workspace_layout_t>(Get_initial_workspace_layout);
  const [Workspace_viewport, setWorkspaceViewport] = useState<Workspace_viewport_t>(() => ({
    width: Math.max(1, window.innerWidth - 96),
    height: Default_workspace_viewport.height,
  }));
  const Workspace_view_ref = useRef<HTMLDivElement>(null);
  const Telemetry_grid_ref = useRef<HTMLDivElement>(null);
  const Event_strip_ref = useRef<HTMLElement>(null);
  const Observer_panel_ref = useRef<HTMLElement>(null);
  const Foc_parameter_panel_ref = useRef<HTMLElement>(null);
  const Observer_panel_drag_ref = useRef<Floating_panel_drag_t | null>(null);
  const Observer_group_drag_ref = useRef<Observer_group_drag_t | null>(null);
  const Observer_group_drag_abort_ref = useRef<AbortController | null>(null);
  const Foc_parameter_panel_drag_ref = useRef<Floating_panel_drag_t | null>(null);
  const Foc_mode_drag_ref = useRef<Foc_mode_drag_t | null>(null);
  const Foc_mode_drag_abort_ref = useRef<AbortController | null>(null);
  const Foc_mode_dragged_ref = useRef(false);
  const Width_scale = Math.min(1.25, Math.max(0.72, Workspace_viewport.width / Default_workspace_viewport.width));
  const Fitted_workbench_height = Math.min(2000, Math.max(280, Workspace_viewport.height));
  const Workbench_height = Math.round(Math.min(2000, Math.max(280, Fitted_workbench_height * Workspace_layout.workbenchScale)));
  const Control_width = Math.round(Math.min(480, Math.max(200, Workspace_layout.controlWidth * Width_scale)));
  const Channel_width = Math.round(Math.min(480, Math.max(180, Workspace_layout.channelWidth * Width_scale)));
  const Scope_resize_ref = useRef<{ pointerId: number; startY: number; startHeight: number } | null>(null);
  const Scope_left_width_resize_ref = useRef<{ pointerId: number; startX: number; startWidth: number } | null>(null);
  const Scope_width_resize_ref = useRef<{ pointerId: number; startX: number; startWidth: number } | null>(null);
  const Clear_confirm_button_ref = useRef<HTMLButtonElement>(null);
  const Is_position_mode =
    (command.driveMode === "encoderFoc" ||
     command.driveMode === "sensorlessFoc") &&
    command.mode === "position";
  const Recommended_channels: Array<keyof Telemetry_t> = command.driveMode === "openLoop"
    ? ["speedActual", "ia", "ib", "ic", "uq"]
    : command.driveMode === "foc_voice"
      ? ["speedActual", "ia", "ib", "ic", "busVoltage"]
      : command.driveMode === "sensorlessFoc"
        ? Sensorless_group === "hfi"
          ? ["ia", "ib", "ic", "speedActual"]
          : Sensorless_group === "smo"
            ? ["iAlphaActual", "iBetaActual", "smoIAlphaEst", "smoIBetaEst", "smoIqError"]
            : ["electricalAngle", "pllElectricalAngleEst", "pllOmegaEst", "pllPhaseError"]
        : command.mode === "speed"
          ? ["speedTarget", "speedActual", "iqTarget", "iqActual", "busVoltage"]
          : command.mode === "current"
            ? ["idTarget", "idActual", "iqTarget", "iqActual", "ud", "uq"]
            : command.mode === "voltage"
              ? ["idActual", "iqActual", "ud", "uq"]
              : ["mechanicalAngleTarget", "mechanicalAngle", "electricalAngle", "speedActual", "iqActual"];
  const Received_key_set = useMemo(() => new Set(receivedChannelKeys), [receivedChannelKeys]);
  const Received_channels = useMemo(
    () => channels.filter((Channel) =>
      Received_key_set.has(Channel.key) ||
      Observer_stream_field_bits[Channel.key] !== undefined),
    [Received_key_set, channels],
  );
  const Recommended_received_channels = Recommended_channels.filter(
    (Key) => Received_key_set.has(Key) || Observer_stream_field_bits[Key] !== undefined,
  );
  const Is_foc_drive_mode = command.driveMode === "encoderFoc" || command.driveMode === "sensorlessFoc";

  useLayoutEffect(() => {
    const Update_viewport = () => {
      const Workspace_element = Workspace_view_ref.current;
      const Scroll_element = Workspace_element?.parentElement;
      const Telemetry_element = Telemetry_grid_ref.current;
      const Event_element = Event_strip_ref.current;
      const View_width = Workspace_element?.getBoundingClientRect().width ?? window.innerWidth;
      let Available_height = Default_workspace_viewport.height;
      if (Scroll_element && Telemetry_element && Event_element) {
        const Scroll_style = window.getComputedStyle(Scroll_element);
        const Telemetry_style = window.getComputedStyle(Telemetry_element);
        const Event_style = window.getComputedStyle(Event_element);
        const Scroll_padding = parseFloat(Scroll_style.paddingTop) + parseFloat(Scroll_style.paddingBottom);
        const Telemetry_height = Telemetry_element.getBoundingClientRect().height
          + parseFloat(Telemetry_style.marginTop)
          + parseFloat(Telemetry_style.marginBottom);
        const Event_height = Event_element.getBoundingClientRect().height
          + parseFloat(Event_style.marginTop)
          + parseFloat(Event_style.marginBottom);
        Available_height = Scroll_element.clientHeight - Scroll_padding - Telemetry_height - Event_height;
      }
      const Next_viewport = {
        width: Math.max(1, Math.round(View_width)),
        height: Math.max(1, Math.round(Available_height)),
      };
      setWorkspaceViewport((Current) => (
        Current.width === Next_viewport.width && Current.height === Next_viewport.height
          ? Current
          : Next_viewport
      ));
    };
    const Resize_observer = new ResizeObserver(Update_viewport);
    if (Workspace_view_ref.current) Resize_observer.observe(Workspace_view_ref.current);
    if (Workspace_view_ref.current?.parentElement) Resize_observer.observe(Workspace_view_ref.current.parentElement);
    if (Telemetry_grid_ref.current) Resize_observer.observe(Telemetry_grid_ref.current);
    if (Event_strip_ref.current) Resize_observer.observe(Event_strip_ref.current);
    window.addEventListener("resize", Update_viewport);
    Update_viewport();
    return () => {
      Resize_observer.disconnect();
      window.removeEventListener("resize", Update_viewport);
    };
  }, []);

  useEffect(() => {
    try {
      window.localStorage.setItem(Workspace_layout_storage_key, JSON.stringify(Workspace_layout));
    } catch {
      // 本地存储不可用时仍允许当前窗口调整工作台尺寸
    }
  }, [Workspace_layout]);

  useEffect(() => {
    if (!Scope_expanded && !Clear_confirm_open) return;
    const Handle_key_down = (Event: KeyboardEvent) => {
      if (Event.key !== "Escape") return;
      if (Clear_confirm_open) {
        setClearConfirmOpen(false);
        return;
      }
      setScopeExpanded(false);
    };
    window.addEventListener("keydown", Handle_key_down);
    return () => window.removeEventListener("keydown", Handle_key_down);
  }, [Clear_confirm_open, Scope_expanded]);

  useEffect(() => {
    if (!Clear_confirm_open) return;
    const Focus_frame = window.requestAnimationFrame(() => Clear_confirm_button_ref.current?.focus());
    return () => window.cancelAnimationFrame(Focus_frame);
  }, [Clear_confirm_open]);

  /* 工作台卸载时终止尚未结束的浮动面板拖动。 */
  useEffect(() => () => {
    Observer_group_drag_abort_ref.current?.abort();
    Observer_group_drag_abort_ref.current = null;
    Foc_mode_drag_abort_ref.current?.abort();
    Foc_mode_drag_abort_ref.current = null;
  }, []);

  const Handle_clear_scope = () => {
    onClear();
    setClearConfirmOpen(false);
  };

  const Handle_scope_resize_move = (Event: React.PointerEvent<HTMLDivElement>) => {
    const Resize_state = Scope_resize_ref.current;
    if (!Resize_state || Resize_state.pointerId !== Event.pointerId) return;
    const Maximum_height = Math.max(560, Math.min(window.innerHeight - 80, 2000));
    const Next_height = Math.min(Math.max(Resize_state.startHeight + Event.clientY - Resize_state.startY, 280), Maximum_height);
    const Workbench_scale = Math.min(1.6, Math.max(0.6, Next_height / Fitted_workbench_height));
    setWorkspaceLayout((Current) => ({ ...Current, workbenchScale: Number(Workbench_scale.toFixed(3)) }));
  };

  const Handle_scope_resize_end = (Event: React.PointerEvent<HTMLDivElement>) => {
    if (Scope_resize_ref.current?.pointerId !== Event.pointerId) return;
    Scope_resize_ref.current = null;
    if (Event.currentTarget.hasPointerCapture(Event.pointerId)) Event.currentTarget.releasePointerCapture(Event.pointerId);
  };

  const Handle_scope_width_resize_move = (Event: React.PointerEvent<HTMLDivElement>) => {
    const Resize_state = Scope_width_resize_ref.current;
    if (!Resize_state || Resize_state.pointerId !== Event.pointerId) return;
    const Next_width = Math.min(Math.max(Resize_state.startWidth - (Event.clientX - Resize_state.startX), 180), 480);
    const Base_width = Math.min(480, Math.max(180, Next_width / Width_scale));
    setWorkspaceLayout((Current) => ({ ...Current, channelWidth: Math.round(Base_width) }));
  };

  const Handle_scope_width_resize_end = (Event: React.PointerEvent<HTMLDivElement>) => {
    if (Scope_width_resize_ref.current?.pointerId !== Event.pointerId) return;
    Scope_width_resize_ref.current = null;
    if (Event.currentTarget.hasPointerCapture(Event.pointerId)) Event.currentTarget.releasePointerCapture(Event.pointerId);
  };

  const Handle_scope_width_mouse_down = (Event: React.MouseEvent<HTMLDivElement>) => {
    Event.preventDefault();
    const Start_x = Event.clientX;
    const Start_width = Channel_width;
    const Handle_mouse_move = (Move_event: MouseEvent) => {
      const Next_width = Math.min(Math.max(Start_width - (Move_event.clientX - Start_x), 180), 480);
      const Base_width = Math.min(480, Math.max(180, Next_width / Width_scale));
      setWorkspaceLayout((Current) => ({ ...Current, channelWidth: Math.round(Base_width) }));
    };
    const Handle_mouse_up = () => {
      window.removeEventListener("mousemove", Handle_mouse_move);
      window.removeEventListener("mouseup", Handle_mouse_up);
    };
    window.addEventListener("mousemove", Handle_mouse_move);
    window.addEventListener("mouseup", Handle_mouse_up);
  };

  const Handle_scope_left_width_resize_move = (Event: React.PointerEvent<HTMLDivElement>) => {
    const Resize_state = Scope_left_width_resize_ref.current;
    if (!Resize_state || Resize_state.pointerId !== Event.pointerId) return;
    const Next_width = Math.min(Math.max(Resize_state.startWidth + Event.clientX - Resize_state.startX, 200), 480);
    const Base_width = Math.min(480, Math.max(220, Next_width / Width_scale));
    setWorkspaceLayout((Current) => ({ ...Current, controlWidth: Math.round(Base_width) }));
  };

  const Handle_scope_left_width_resize_end = (Event: React.PointerEvent<HTMLDivElement>) => {
    if (Scope_left_width_resize_ref.current?.pointerId !== Event.pointerId) return;
    Scope_left_width_resize_ref.current = null;
    if (Event.currentTarget.hasPointerCapture(Event.pointerId)) Event.currentTarget.releasePointerCapture(Event.pointerId);
  };

  const Handle_scope_left_width_mouse_down = (Event: React.MouseEvent<HTMLDivElement>) => {
    Event.preventDefault();
    const Start_x = Event.clientX;
    const Start_width = Control_width;
    const Handle_mouse_move = (Move_event: MouseEvent) => {
      const Next_width = Math.min(Math.max(Start_width + Move_event.clientX - Start_x, 200), 480);
      const Base_width = Math.min(480, Math.max(220, Next_width / Width_scale));
      setWorkspaceLayout((Current) => ({ ...Current, controlWidth: Math.round(Base_width) }));
    };
    const Handle_mouse_up = () => {
      window.removeEventListener("mousemove", Handle_mouse_move);
      window.removeEventListener("mouseup", Handle_mouse_up);
    };
    window.addEventListener("mousemove", Handle_mouse_move);
    window.addEventListener("mouseup", Handle_mouse_up);
  };

  /***********************************************
   * @brief : 将无感观测器参数面板脱出控制区并放到控制区旁边
   * @param : 无
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Detach_observer_panel = () => {
    const Panel_rect = Observer_panel_ref.current?.getBoundingClientRect();
    const Panel_width = Math.min(Floating_panel_default_width, Math.max(240, window.innerWidth - 16));
    const Panel_height = Math.min(Floating_panel_default_height, Math.max(260, window.innerHeight - 16));
    const Viewport_padding = 8;
    let Next_left = Panel_rect ? Panel_rect.right + 12 : 280;
    if ((Next_left + Panel_width) > (window.innerWidth - Viewport_padding)) {
      Next_left = Panel_rect
        ? Panel_rect.left - Panel_width - 12
        : window.innerWidth - Panel_width - Viewport_padding;
    }
    const Next_top = Math.min(
      Math.max(Panel_rect?.top ?? 96, Viewport_padding),
      Math.max(Viewport_padding, window.innerHeight - Panel_height - Viewport_padding));

    Observer_panel_drag_ref.current = null;
    setObserverPanelPosition({
      left: Math.min(Math.max(Next_left, Viewport_padding), Math.max(Viewport_padding, window.innerWidth - Panel_width - Viewport_padding)),
      top: Next_top,
    });
    setObserverPanelFloating(true);
  };

  /***********************************************
   * @brief : 将浮动的无感观测器参数面板收回控制区
   * @param : 无
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Dock_observer_panel = () => {
    Observer_panel_drag_ref.current = null;
    setObserverPanelFloating(false);
  };

  /***********************************************
   * @brief : 记录无感观测器参数面板开始拖动时的指针偏移
   * @param : Event 指针按下事件
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Handle_observer_panel_drag_start = (Event: React.PointerEvent<HTMLDivElement>) => {
    if (!Observer_panel_floating || (Event.target as HTMLElement).closest("button")) return;
    const Panel_rect = Observer_panel_ref.current?.getBoundingClientRect();
    if (!Panel_rect) return;

    Observer_panel_drag_ref.current = {
      pointerId: Event.pointerId,
      offsetX: Event.clientX - Panel_rect.left,
      offsetY: Event.clientY - Panel_rect.top,
    };
    Event.currentTarget.setPointerCapture(Event.pointerId);
  };

  /***********************************************
   * @brief : 根据指针位置移动无感观测器参数面板并保持面板位于窗口内
   * @param : Event 指针移动事件
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Handle_observer_panel_drag_move = (Event: React.PointerEvent<HTMLDivElement>) => {
    const Drag_state = Observer_panel_drag_ref.current;
    const Panel_rect = Observer_panel_ref.current?.getBoundingClientRect();
    if (!Drag_state || !Panel_rect || Drag_state.pointerId !== Event.pointerId) return;

    const Viewport_padding = 8;
    const Maximum_left = Math.max(Viewport_padding, window.innerWidth - Panel_rect.width - Viewport_padding);
    const Maximum_top = Math.max(Viewport_padding, window.innerHeight - Panel_rect.height - Viewport_padding);
    setObserverPanelPosition({
      left: Math.min(Math.max(Event.clientX - Drag_state.offsetX, Viewport_padding), Maximum_left),
      top: Math.min(Math.max(Event.clientY - Drag_state.offsetY, Viewport_padding), Maximum_top),
    });
  };

  /***********************************************
   * @brief : 结束无感观测器参数面板拖动并释放指针捕获
   * @param : Event 指针抬起或取消事件
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Handle_observer_panel_drag_end = (Event: React.PointerEvent<HTMLDivElement>) => {
    if (Observer_panel_drag_ref.current?.pointerId !== Event.pointerId) return;
    Observer_panel_drag_ref.current = null;
    if (Event.currentTarget.hasPointerCapture(Event.pointerId)) {
      Event.currentTarget.releasePointerCapture(Event.pointerId);
    }
  };

  /***********************************************
   * @brief : 拖动观测器参数标签时脱出并持续移动整个参数面板
   * @param : Event 窗口指针移动事件
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Handle_observer_group_drag_move = (Event: PointerEvent) => {
    const Drag_state = Observer_group_drag_ref.current;
    if (!Drag_state || Drag_state.pointerId !== Event.pointerId) return;

    const Is_detaching = !Drag_state.detached;
    if (!Drag_state.detached) {
      const Move_distance = Math.hypot(
        Event.clientX - Drag_state.startX,
        Event.clientY - Drag_state.startY);
      if (Move_distance < 6) return;
      Drag_state.detached = true;
      setObserverPanelFloating(true);
    }

    const Panel_rect = Observer_panel_ref.current?.getBoundingClientRect();
    const Panel_width = Is_detaching
      ? Math.min(Floating_panel_default_width, window.innerWidth - 16)
      : Panel_rect?.width ?? Math.min(Floating_panel_default_width, window.innerWidth - 16);
    const Panel_height = Is_detaching
      ? Math.min(Floating_panel_default_height, window.innerHeight - 16)
      : Panel_rect?.height ?? Math.min(Floating_panel_default_height, window.innerHeight - 16);
    const Viewport_padding = 8;
    const Maximum_left = Math.max(Viewport_padding, window.innerWidth - Panel_width - Viewport_padding);
    const Maximum_top = Math.max(Viewport_padding, window.innerHeight - Panel_height - Viewport_padding);
    setObserverPanelPosition({
      left: Math.min(Math.max(Event.clientX - Drag_state.offsetX, Viewport_padding), Maximum_left),
      top: Math.min(Math.max(Event.clientY - Drag_state.offsetY, Viewport_padding), Maximum_top),
    });
    Event.preventDefault();
  };

  /***********************************************
   * @brief : 结束观测器参数标签拖动并移除窗口级指针监听
   * @param : Event 窗口指针抬起或取消事件
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Handle_observer_group_drag_end = (Event: PointerEvent) => {
    if (Observer_group_drag_ref.current?.pointerId !== Event.pointerId) return;
    Observer_group_drag_ref.current = null;
    Observer_group_drag_abort_ref.current?.abort();
    Observer_group_drag_abort_ref.current = null;
  };

  /***********************************************
   * @brief : 记录观测器参数标签的左键按下位置并准备拖出面板
   * @param : Event 参数标签指针按下事件
   * @param : Group 当前观测器参数分组
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Handle_observer_group_drag_start = (Event: React.PointerEvent<HTMLButtonElement>, Group: Sensorless_group_t) => {
    if (!Event.isPrimary || Event.button !== 0) return;
    setSensorlessGroup(Group);
    if (Observer_panel_floating) return;
    const Panel_rect = Observer_panel_ref.current?.getBoundingClientRect();
    if (!Panel_rect) return;

    Observer_group_drag_abort_ref.current?.abort();
    Observer_group_drag_ref.current = {
      pointerId: Event.pointerId,
      startX: Event.clientX,
      startY: Event.clientY,
      offsetX: Event.clientX - Panel_rect.left,
      offsetY: Event.clientY - Panel_rect.top,
      detached: false,
    };
    const Drag_abort_controller = new AbortController();
    Observer_group_drag_abort_ref.current = Drag_abort_controller;
    window.addEventListener("pointermove", Handle_observer_group_drag_move, {
      signal: Drag_abort_controller.signal,
    });
    window.addEventListener("pointerup", Handle_observer_group_drag_end, {
      signal: Drag_abort_controller.signal,
    });
    window.addEventListener("pointercancel", Handle_observer_group_drag_end, {
      signal: Drag_abort_controller.signal,
    });
  };

  /***********************************************
   * @brief : 将浮动的有感FOC参数面板收回控制区
   * @param : 无
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Dock_foc_parameter_panel = () => {
    Foc_parameter_panel_drag_ref.current = null;
    Foc_mode_drag_ref.current = null;
    Foc_mode_drag_abort_ref.current?.abort();
    Foc_mode_drag_abort_ref.current = null;
    setFocParameterPanelFloating(false);
  };

  /***********************************************
   * @brief : 记录浮动有感FOC参数面板标题栏的拖动偏移
   * @param : Event 标题栏指针按下事件
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Handle_foc_parameter_panel_drag_start = (Event: React.PointerEvent<HTMLDivElement>) => {
    if (!Foc_parameter_panel_floating || (Event.target as HTMLElement).closest("button")) return;
    const Panel_rect = Foc_parameter_panel_ref.current?.getBoundingClientRect();
    if (!Panel_rect) return;

    Foc_parameter_panel_drag_ref.current = {
      pointerId: Event.pointerId,
      offsetX: Event.clientX - Panel_rect.left,
      offsetY: Event.clientY - Panel_rect.top,
    };
    Event.currentTarget.setPointerCapture(Event.pointerId);
  };

  /***********************************************
   * @brief : 根据指针位置移动浮动有感FOC参数面板
   * @param : Event 标题栏指针移动事件
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Handle_foc_parameter_panel_drag_move = (Event: React.PointerEvent<HTMLDivElement>) => {
    const Drag_state = Foc_parameter_panel_drag_ref.current;
    const Panel_rect = Foc_parameter_panel_ref.current?.getBoundingClientRect();
    if (!Drag_state || !Panel_rect || Drag_state.pointerId !== Event.pointerId) return;

    const Viewport_padding = 8;
    const Maximum_left = Math.max(Viewport_padding, window.innerWidth - Panel_rect.width - Viewport_padding);
    const Maximum_top = Math.max(Viewport_padding, window.innerHeight - Panel_rect.height - Viewport_padding);
    setFocParameterPanelPosition({
      left: Math.min(Math.max(Event.clientX - Drag_state.offsetX, Viewport_padding), Maximum_left),
      top: Math.min(Math.max(Event.clientY - Drag_state.offsetY, Viewport_padding), Maximum_top),
    });
  };

  /***********************************************
   * @brief : 结束浮动有感FOC参数面板的标题栏拖动
   * @param : Event 标题栏指针抬起或取消事件
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Handle_foc_parameter_panel_drag_end = (Event: React.PointerEvent<HTMLDivElement>) => {
    if (Foc_parameter_panel_drag_ref.current?.pointerId !== Event.pointerId) return;
    Foc_parameter_panel_drag_ref.current = null;
    if (Event.currentTarget.hasPointerCapture(Event.pointerId)) {
      Event.currentTarget.releasePointerCapture(Event.pointerId);
    }
  };

  /***********************************************
   * @brief : 拖动电流、速度或位置标签时脱出并持续移动对应参数面板
   * @param : Event 窗口指针移动事件
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Handle_foc_mode_drag_move = (Event: PointerEvent) => {
    const Drag_state = Foc_mode_drag_ref.current;
    if (!Drag_state || Drag_state.pointerId !== Event.pointerId) return;

    const Is_detaching = !Drag_state.detached;
    if (!Drag_state.detached) {
      const Move_distance = Math.hypot(
        Event.clientX - Drag_state.startX,
        Event.clientY - Drag_state.startY);
      if (Move_distance < 6) return;
      Drag_state.detached = true;
      Foc_mode_dragged_ref.current = true;
      if (!command.enabled && command.mode !== Drag_state.mode) {
        onCommand({ mode: Drag_state.mode });
      }
      setFocParameterPanelFloating(true);
    }

    const Panel_rect = Foc_parameter_panel_ref.current?.getBoundingClientRect();
    const Panel_width = Is_detaching
      ? Math.min(Floating_panel_default_width, window.innerWidth - 16)
      : Panel_rect?.width ?? Math.min(Floating_panel_default_width, window.innerWidth - 16);
    const Panel_height = Is_detaching
      ? Math.min(Floating_panel_default_height, window.innerHeight - 16)
      : Panel_rect?.height ?? Math.min(Floating_panel_default_height, window.innerHeight - 16);
    const Viewport_padding = 8;
    const Maximum_left = Math.max(Viewport_padding, window.innerWidth - Panel_width - Viewport_padding);
    const Maximum_top = Math.max(Viewport_padding, window.innerHeight - Panel_height - Viewport_padding);
    setFocParameterPanelPosition({
      left: Math.min(Math.max(Event.clientX - Drag_state.offsetX, Viewport_padding), Maximum_left),
      top: Math.min(Math.max(Event.clientY - Drag_state.offsetY, Viewport_padding), Maximum_top),
    });
    Event.preventDefault();
  };

  /***********************************************
   * @brief : 结束有感FOC参数标签拖动并移除窗口级指针监听
   * @param : Event 窗口指针抬起或取消事件
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Handle_foc_mode_drag_end = (Event: PointerEvent) => {
    if (Foc_mode_drag_ref.current?.pointerId !== Event.pointerId) return;
    Foc_mode_drag_ref.current = null;
    Foc_mode_drag_abort_ref.current?.abort();
    Foc_mode_drag_abort_ref.current = null;
    window.setTimeout(() => {
      Foc_mode_dragged_ref.current = false;
    }, 0);
  };

  /***********************************************
   * @brief : 记录有感FOC参数标签的左键按下位置并准备拖出参数面板
   * @param : Event 参数标签指针按下事件
   * @param : Mode 当前有感FOC子模式
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Handle_foc_mode_drag_start = (Event: React.PointerEvent<HTMLButtonElement>, Mode: Control_mode_t) => {
    if (Foc_parameter_panel_floating || !Event.isPrimary || Event.button !== 0) return;
    const Panel_rect = Foc_parameter_panel_ref.current?.getBoundingClientRect();
    if (!Panel_rect) return;

    Foc_mode_dragged_ref.current = false;
    Foc_mode_drag_abort_ref.current?.abort();
    Foc_mode_drag_ref.current = {
      pointerId: Event.pointerId,
      startX: Event.clientX,
      startY: Event.clientY,
      offsetX: Event.clientX - Panel_rect.left,
      offsetY: Event.clientY - Panel_rect.top,
      detached: false,
      mode: Mode,
    };
    const Drag_abort_controller = new AbortController();
    Foc_mode_drag_abort_ref.current = Drag_abort_controller;
    window.addEventListener("pointermove", Handle_foc_mode_drag_move, {
      signal: Drag_abort_controller.signal,
    });
    window.addEventListener("pointerup", Handle_foc_mode_drag_end, {
      signal: Drag_abort_controller.signal,
    });
    window.addEventListener("pointercancel", Handle_foc_mode_drag_end, {
      signal: Drag_abort_controller.signal,
    });
  };

  const Foc_parameter_panel = (
    <section
      ref={Foc_parameter_panel_ref}
      className={Foc_parameter_panel_floating
        ? "tool-panel foc-parameter-tool floating"
        : "foc-parameter-tool docked"}
      style={Foc_parameter_panel_floating
        ? { left: `${Foc_parameter_panel_position.left}px`, top: `${Foc_parameter_panel_position.top}px` }
        : undefined}
      aria-label={`有感FOC${Mode_labels[command.mode]}参数`}
    >
      {Foc_parameter_panel_floating && (
        <div
          className="foc-parameter-heading"
          onPointerDown={Handle_foc_parameter_panel_drag_start}
          onPointerMove={Handle_foc_parameter_panel_drag_move}
          onPointerUp={Handle_foc_parameter_panel_drag_end}
          onPointerCancel={Handle_foc_parameter_panel_drag_end}
        >
          <GripVertical size={17} aria-hidden="true" />
          <div><h3>有感 FOC · {Mode_labels[command.mode]}</h3><p>在线控制参数</p></div>
          <button type="button" className="icon-button" onClick={Dock_foc_parameter_panel} title="收回控制区" aria-label="将有感FOC参数面板收回控制区"><Dock size={16} /></button>
        </div>
      )}
      <div className="foc-parameter-body">
        {command.mode === "current" && <>
          <NumericInput label="Iq ref" value={command.iqTarget} unit="A" step={0.1} minimum={-100} maximum={100} deferred adjuster="slider" boundaryStorageKey="iqTarget" onChange={(Value) => onCommand({ iqTarget: Value })} />
          <NumericInput label="Id ref" value={command.idTarget} unit="A" step={0.1} minimum={-50} maximum={50} deferred adjuster="slider" boundaryStorageKey="idTarget" onChange={(Value) => onCommand({ idTarget: Value })} />
          <NumericInput label="带宽" value={loopParameters.currentBandwidth} unit="Hz" step={50} minimum={1} maximum={5000} disabled={!connected} deferred adjuster="slider" boundaryStorageKey="currentBandwidth" fieldClassName="bandwidth-field" onChange={(Value) => onLoopParameterChange("currentBandwidth", Value)} />
        </>}
        {command.mode === "speed" && <>
          <NumericInput label="Speed ref" value={command.speedTarget} unit="rpm" step={100} minimum={-50000} maximum={50000} deferred adjuster="slider" boundaryStorageKey="speedTarget" resetValue={0} resetLabel="目标转速清零" onChange={(Value) => onCommand({ speedTarget: Value })} />
          <NumericInput label="速度斜率" value={command.rampRate} unit="rpm/s" step={100} minimum={1} maximum={100000} deferred adjuster="slider" boundaryStorageKey="rampRate" onChange={(Value) => onCommand({ rampRate: Value })} />
          <div className="loop-parameter-grid speed-loop-parameter-grid">
            <NumericInput label="KP" value={loopParameters.speedKp} unit="" step={0.001} minimum={0} maximum={100} disabled={!connected} deferred adjuster="slider" boundaryStorageKey="speedKp" onChange={(Value) => onLoopParameterChange("speedKp", Value)} />
            <NumericInput label="KI" value={loopParameters.speedKi} unit="" step={0.001} minimum={0} maximum={100} disabled={!connected} deferred adjuster="slider" boundaryStorageKey="speedKi" onChange={(Value) => onLoopParameterChange("speedKi", Value)} />
            <NumericInput label="积分项限幅" value={loopParameters.speedIntegralLimit} unit="A" step={0.1} minimum={0} maximum={5} disabled={!connected} deferred adjuster="slider" boundaryStorageKey="speedIntegralLimit" onChange={(Value) => onLoopParameterChange("speedIntegralLimit", Value)} />
            <NumericInput label="AB滤波带宽" value={loopParameters.abFilterBandwidth} unit="Hz" step={1} minimum={1} maximum={500} disabled={!connected} deferred adjuster="slider" boundaryStorageKey="abFilterBandwidth" fieldClassName="bandwidth-field" onChange={(Value) => onLoopParameterChange("abFilterBandwidth", Value)} />
          </div>
        </>}
        {command.mode === "position" && <div className="position-command-fields">
          <NumericInput label="Position ref" value={command.positionTarget} unit="°" step={1} minimum={0} maximum={360} deferred adjuster="dial" cyclic onChange={(Value) => onCommand({ positionTarget: Value })} />
          <div className="loop-parameter-grid position-loop-parameter-grid">
            <NumericInput label="KP" value={loopParameters.positionKp} unit="" step={0.001} minimum={0} maximum={100} disabled={!connected} deferred adjuster="slider" boundaryStorageKey="positionKp" onChange={(Value) => onLoopParameterChange("positionKp", Value)} />
            <NumericInput label="输出限幅" value={loopParameters.positionOutputLimit} unit="rpm" step={100} minimum={0} maximum={30000} disabled={!connected} deferred adjuster="slider" boundaryStorageKey="positionOutputLimit" onChange={(Value) => onLoopParameterChange("positionOutputLimit", Value)} />
            <NumericInput label="角度死区" value={loopParameters.positionDeadband} unit="°" step={0.1} minimum={0} maximum={180} disabled={!connected} deferred adjuster="slider" boundaryStorageKey="positionDeadband" onChange={(Value) => onLoopParameterChange("positionDeadband", Value)} />
            <NumericInput label="软化范围" value={loopParameters.positionSoftRange} unit="°" step={0.1} minimum={0} maximum={180} disabled={!connected} deferred adjuster="slider" boundaryStorageKey="positionSoftRange" onChange={(Value) => onLoopParameterChange("positionSoftRange", Value)} />
            <NumericInput label="到位速度死区" value={loopParameters.positionSpeedDeadband} unit="rpm" step={0.1} minimum={0} maximum={100} disabled={!connected} deferred adjuster="slider" boundaryStorageKey="positionSpeedDeadband" onChange={(Value) => onLoopParameterChange("positionSpeedDeadband", Value)} />
          </div>
        </div>}
      </div>
      {Foc_parameter_panel_floating && <span className="observer-panel-resize-mark" aria-hidden="true"><Maximize2 size={13} /></span>}
    </section>
  );

  const Observer_parameter_panel = (
    <section
      ref={Observer_panel_ref}
      className={Observer_panel_floating
        ? "tool-panel observer-parameter-tool floating"
        : "observer-parameter-tool docked"}
      style={Observer_panel_floating
        ? { left: `${Observer_panel_position.left}px`, top: `${Observer_panel_position.top}px` }
        : undefined}
      aria-label="无感观测器在线调参"
    >
      <div
        className="observer-parameter-heading"
        onPointerDown={Handle_observer_panel_drag_start}
        onPointerMove={Handle_observer_panel_drag_move}
        onPointerUp={Handle_observer_panel_drag_end}
        onPointerCancel={Handle_observer_panel_drag_end}
      >
        {Observer_panel_floating && <GripVertical className="observer-drag-icon" size={17} aria-hidden="true" />}
        <div>
          <h3>无感观测器</h3>
          <p>SMO / PLL 在线调参</p>
        </div>
        <button
          type="button"
          className="icon-button"
          onClick={Observer_panel_floating ? Dock_observer_panel : Detach_observer_panel}
          title={Observer_panel_floating ? "收回控制区" : "脱出为浮动面板"}
          aria-label={Observer_panel_floating ? "将无感观测器面板收回控制区" : "将无感观测器面板脱出控制区"}
        >
          {Observer_panel_floating ? <Dock size={16} /> : <ExternalLink size={16} />}
        </button>
      </div>
      <div className="observer-parameter-body">
        <div className="mode-switch sensorless-group-switch" role="tablist" aria-label="无感观测器参数分组">
          {(["hfi", "smo", "pll"] as Sensorless_group_t[]).map((Group) => (
            <button
              key={Group}
              className={Sensorless_group === Group ? "active" : ""}
              onPointerDown={(Event) => Handle_observer_group_drag_start(Event, Group)}
              onClick={() => setSensorlessGroup(Group)}
            >{Sensorless_group_labels[Group]}</button>
          ))}
        </div>
        {Sensorless_group === "hfi" && (
          <div className="inline-notice sensorless-empty-notice"><Activity size={15} />HFI 参数暂留，当前无需配置</div>
        )}
        {Sensorless_group === "smo" && (
          <div className="loop-parameter-grid sensorless-loop-parameter-grid">
            <NumericInput label="滑模增益" value={loopParameters.smoKSlide} unit="V" step={0.01} minimum={0} maximum={100} disabled={!connected} deferred adjuster="slider" boundaryStorageKey="smoKSlide" onChange={(Value) => onLoopParameterChange("smoKSlide", Value)} />
            <NumericInput label="边界电流" value={loopParameters.smoBoundaryCurrent} unit="A" step={0.01} minimum={0} maximum={100} disabled={!connected} deferred adjuster="slider" boundaryStorageKey="smoBoundaryCurrent" onChange={(Value) => onLoopParameterChange("smoBoundaryCurrent", Value)} />
            <NumericInput label="反电动势滤波带宽" value={loopParameters.smoFilterBandwidth} unit="Hz" step={1} minimum={1} maximum={500} disabled={!connected} deferred adjuster="slider" boundaryStorageKey="smoFilterBandwidth" fieldClassName="bandwidth-field" onChange={(Value) => onLoopParameterChange("smoFilterBandwidth", Value)} />
          </div>
        )}
        {Sensorless_group === "pll" && (
          <div className="loop-parameter-grid sensorless-loop-parameter-grid">
            <NumericInput label="PLL 带宽" value={loopParameters.pllBandwidth} unit="Hz" step={1} minimum={1} maximum={500} disabled={!connected} deferred adjuster="slider" boundaryStorageKey="pllBandwidth" fieldClassName="bandwidth-field" onChange={(Value) => onLoopParameterChange("pllBandwidth", Value)} />
          </div>
        )}
      </div>
      {Observer_panel_floating && <span className="observer-panel-resize-mark" aria-hidden="true"><Maximize2 size={13} /></span>}
    </section>
  );

  return (
    <div className="workspace-view" ref={Workspace_view_ref}>
      <div className="telemetry-grid" ref={Telemetry_grid_ref}>
        <TelemetryCard label="机械转速" value={Format_value(telemetry.speedActual, 0)} unit="rpm" icon={Gauge} tone="#42d3a5" secondary={`目标 ${Format_value(telemetry.speedTarget, 0)} rpm`} />
        <TelemetryCard label="Q 轴电流" value={Format_value(telemetry.iqActual, 2)} unit="A" icon={Zap} tone="#7da6ff" secondary={`Id ${Format_value(telemetry.idActual, 2)} A`} />
        <TelemetryCard label="母线电压" value={Format_value(telemetry.busVoltage, 2)} unit="V" icon={Activity} tone="#f4b942" secondary={`Uq ${Format_value(telemetry.uq, 2)} V`} />
        <AngleTelemetryCard mechanicalAngle={telemetry.mechanicalAngle} electricalAngle={telemetry.electricalAngle} />
      </div>

      <div className="unified-workbench-grid" style={{ "--workbench-height": `${Workbench_height}px`, "--control-width": `${Control_width}px`, "--channel-width": `${Channel_width}px` } as React.CSSProperties}>
        <section className="tool-panel control-panel unified-control-panel">
          <div className="panel-heading">
            <div>
              <h2>驱动控制</h2>
              <p>{Device_enabled ? "下位机已确认 PWM 输出" : Enable_pending ? "等待下位机确认使能" : "PWM 输出已关闭"}</p>
            </div>
            <span className={Device_enabled ? "run-indicator active" : Enable_pending ? "run-indicator pending" : "run-indicator"}><i />{Device_enabled ? "运行" : Enable_pending ? "待确认" : "待机"}</span>
          </div>

          <div className="drive-mode-layout" role="group" aria-label="驱动模式与零点校准">
            <div className="mode-switch drive-mode-switch">
              {(["openLoop", "foc_voice", "encoderFoc", "sensorlessFoc"] as Drive_mode_t[]).map((Mode) => (
                <button
                  key={Mode}
                  disabled={command.enabled}
                  className={command.driveMode === Mode ? "active" : ""}
                  onClick={() => onCommand(Mode === "sensorlessFoc" ? { driveMode: Mode, mode: "speed" } : { driveMode: Mode })}
                >{Drive_mode_labels[Mode]}</button>
              ))}
            </div>
            <button
              type="button"
              className="zero-calibration-button"
              disabled={!connected || command.enabled || command.emergencyStopped}
              onClick={onZeroCalibration}
              title="执行编码器零点校准"
              aria-label="零点校准"
            ><RotateCcw size={14} /><span>零点<br />校准</span></button>
          </div>

          {command.driveMode === "encoderFoc" && (
            <div className="mode-switch foc-mode-switch" role="tablist" aria-label="FOC 控制模式">
              {Foc_control_modes.map((Mode) => {
                const Is_drag_target = !Foc_parameter_panel_floating;
                return (
                  <button
                    key={Mode}
                    disabled={command.enabled && Mode !== command.mode}
                    className={`${command.mode === Mode ? "active" : ""}${Is_drag_target ? " foc-mode-drag-target" : ""}`}
                    onPointerDown={Is_drag_target ? (Event) => Handle_foc_mode_drag_start(Event, Mode) : undefined}
                    onClick={() => {
                      if (Foc_mode_dragged_ref.current || command.enabled) return;
                      onCommand({ mode: Mode });
                    }}
                    title={Is_drag_target ? `左键按住并拖出${Mode_labels[Mode]}参数面板` : undefined}
                  >{Mode_labels[Mode]}</button>
                );
              })}
            </div>
          )}

          {/* 数值控制目标统一在失焦或回车时提交，避免逐字符发送中间值。 */}
          <div className="command-fields">
            {command.driveMode === "openLoop" && <>
              <NumericInput label="Uq" value={command.voltageTarget} unit="V" step={0.1} minimum={-60} maximum={60} deferred adjuster="slider" boundaryStorageKey="voltageTarget" onChange={(Value) => onCommand({ voltageTarget: Value })} />
              <NumericInput label="Angle" value={command.angleTarget} unit="0~32767" step={1} minimum={0} maximum={32767} deferred adjuster="slider" boundaryStorageKey="angleTarget" onChange={(Value) => onCommand({ angleTarget: Value })} />
              <NumericInput label="角度步进" value={command.angleStep} unit="计数/周期" step={1} minimum={-1000} maximum={1000} deferred adjuster="slider" boundaryStorageKey="angleStep" onChange={(Value) => onCommand({ angleStep: Value })} />
            </>}
            {command.driveMode === "encoderFoc" && !Foc_parameter_panel_floating && Foc_parameter_panel}
            {command.driveMode === "sensorlessFoc" && !Observer_panel_floating && Observer_parameter_panel}
            {command.driveMode === "foc_voice" && (
              <div className="music-control-panel">
                <label className="field-block">
                  <span>乐曲</span>
                  <select
                    value={command.focVoiceSongId}
                    disabled={command.enabled}
                    onChange={(Event) => onCommand({ focVoiceSongId: Number(Event.target.value) })}
                  >
                    {musicTracks.map((Track) => <option key={Track.id} value={Track.id}>{Track.name}</option>)}
                  </select>
                </label>
                <div className="music-play-row">
                  <span className="music-track-icon"><Music2 size={18} /></span>
                  <div><b>{musicTracks.find((Track) => Track.id === command.focVoiceSongId)?.name ?? "未知乐曲"}</b><span>{command.enabled ? "正在播放" : "等待播放"}</span></div>
                  <button
                    type="button"
                    className={command.enabled ? "music-play-button active" : "music-play-button"}
                    disabled={!connected || command.emergencyStopped}
                    onClick={command.enabled ? onStop : onStart}
                    title={command.enabled ? "停止播放" : "开始播放"}
                    aria-label={command.enabled ? "停止播放" : "开始播放"}
                  >
                    {command.enabled
                      ? <Square size={15} fill="currentColor" />
                      : <Play size={16} fill="currentColor" />}
                  </button>
                </div>
              </div>
            )}
          </div>

          {command.driveMode !== "foc_voice" &&
           (!Is_foc_drive_mode || command.mode === "voltage" || Is_position_mode) &&
           <div className="direction-control">
            <span>{Is_position_mode ? "回正方式" : "旋转方向"}</span>
            <div className="segmented compact">
              {Is_position_mode ? <>
                <button className={command.positionReturnMode === "shortest" ? "active" : ""} onClick={() => onCommand({ positionReturnMode: "shortest" })}>最近距离</button>
                <button className={command.positionReturnMode === "reversePath" ? "active" : ""} onClick={() => onCommand({ positionReturnMode: "reversePath" })}>原路回正</button>
              </> : <>
                <button className={command.direction === 1 ? "active" : ""} onClick={() => onCommand({ direction: 1 })}>正转</button>
                <button className={command.direction === -1 ? "active" : ""} onClick={() => onCommand({ direction: -1 })}>反转</button>
              </>}
            </div>
          </div>}

          <div className="output-bars">
            {[{ label: "PWM A", value: telemetry.dutyA }, { label: "PWM B", value: telemetry.dutyB }, { label: "PWM C", value: telemetry.dutyC }].map((Item) => (
              <div className="output-row" key={Item.label}>
                <span>{Item.label}</span>
                <div><i style={{ width: `${Item.value}%` }} /></div>
                <b>{Format_value(Item.value, 1)}%</b>
              </div>
            ))}
          </div>

          {command.driveMode !== "foc_voice" && <div className="control-actions">
            <button className="primary-command" disabled={!connected || command.emergencyStopped || command.enabled} onClick={onStart}><Play size={18} fill="currentColor" />启动</button>
            <button className="secondary-command" disabled={!command.enabled} onClick={onStop}><Square size={17} fill="currentColor" />停止</button>
          </div>}
          {!connected && <div className="inline-notice"><Unplug size={15} />连接设备后可发送控制指令</div>}
          {command.emergencyStopped && <div className="inline-notice danger"><ShieldAlert size={15} />急停已锁定，请先复位急停</div>}
        </section>

        <section className={Scope_expanded ? "tool-panel scope-panel unified-scope-panel expanded" : "tool-panel scope-panel unified-scope-panel"}>
          <div className="panel-heading">
            <div>
              <h2>实时示波器</h2>
              <p>{sampleCount} 点缓冲 · {paused ? "显示冻结，采集继续" : "设备端时间基准"}</p>
            </div>
            <div className="icon-actions">
              <button className="icon-button" onClick={() => setScopeExpanded((Current) => !Current)} title={Scope_expanded ? "退出全屏" : "全屏查看"} aria-label={Scope_expanded ? "退出示波器全屏" : "示波器全屏"}>{Scope_expanded ? <Minimize2 size={17} /> : <Maximize2 size={17} />}</button>
              <button
                type="button"
                className={paused ? "scope-follow-switch" : "scope-follow-switch active"}
                role="switch"
                aria-checked={!paused}
                onClick={onPause}
                title={paused ? "恢复实时跟随" : "暂停实时跟随"}
              ><span><i /></span><b>实时跟随</b></button>
              <button
                className="icon-button"
                disabled={sampleCount === 0}
                onClick={() => setClearConfirmOpen(true)}
                title="清空波形"
                aria-label="清空示波器波形"
              ><Trash2 size={17} /></button>
              <button className="icon-button" onClick={onExport} title="导出 CSV"><Download size={17} /></button>
            </div>
          </div>
          <ScopeCanvas history={history} frozenSamples={frozenSamples} channels={Received_channels} paused={paused} interactive />
          <div
            className="scope-left-width-resize-handle"
            role="separator"
            aria-label="调整示波器左侧宽度"
            aria-orientation="vertical"
            title="左右拖动调整左侧区域宽度，双击恢复默认宽度"
            onDoubleClick={() => setWorkspaceLayout((Current) => ({ ...Current, controlWidth: Default_workspace_layout.controlWidth }))}
            onMouseDown={Handle_scope_left_width_mouse_down}
            onPointerDown={(Event) => {
              Scope_left_width_resize_ref.current = { pointerId: Event.pointerId, startX: Event.clientX, startWidth: Control_width };
              Event.currentTarget.setPointerCapture(Event.pointerId);
            }}
            onPointerMove={Handle_scope_left_width_resize_move}
            onPointerUp={Handle_scope_left_width_resize_end}
            onPointerCancel={Handle_scope_left_width_resize_end}
          ><GripVertical size={15} /></div>
          <div
            className="scope-width-resize-handle"
            role="separator"
            aria-label="调整示波器宽度"
            aria-orientation="vertical"
            title="左右拖动调整示波器宽度，双击恢复默认宽度"
            onDoubleClick={() => setWorkspaceLayout((Current) => ({ ...Current, channelWidth: Default_workspace_layout.channelWidth }))}
            onMouseDown={Handle_scope_width_mouse_down}
            onPointerDown={(Event) => {
              Scope_width_resize_ref.current = { pointerId: Event.pointerId, startX: Event.clientX, startWidth: Channel_width };
              Event.currentTarget.setPointerCapture(Event.pointerId);
            }}
            onPointerMove={Handle_scope_width_resize_move}
            onPointerUp={Handle_scope_width_resize_end}
            onPointerCancel={Handle_scope_width_resize_end}
          ><GripVertical size={15} /></div>
          <div
            className="scope-resize-handle"
            role="separator"
            aria-label="调整示波器高度"
            aria-orientation="horizontal"
            title="拖动调整示波器高度，双击恢复自适应高度"
            onDoubleClick={() => setWorkspaceLayout((Current) => ({ ...Current, workbenchScale: Default_workspace_layout.workbenchScale }))}
            onPointerDown={(Event) => {
              Scope_resize_ref.current = { pointerId: Event.pointerId, startY: Event.clientY, startHeight: Workbench_height };
              Event.currentTarget.setPointerCapture(Event.pointerId);
            }}
            onPointerMove={Handle_scope_resize_move}
            onPointerUp={Handle_scope_resize_end}
            onPointerCancel={Handle_scope_resize_end}
          ><GripHorizontal size={15} /></div>
        </section>

        <section className="tool-panel channel-panel">
          <div className="panel-heading channel-heading">
            <div><h2>观测通道</h2><p>{Received_channels.filter((Channel) => Channel.visible).length} / {Received_channels.length} 已选择 · 勾选通道固定 1 ms 采样</p></div>
          </div>
          <div className="channel-quick-actions">
            <button disabled={Received_channels.length === 0} onClick={() => onChannelSelection(Recommended_received_channels)}>模式推荐</button>
            <button disabled={Received_channels.length === 0} onClick={() => onChannelSelection(Received_channels.map((Channel) => Channel.key))}>全选</button>
            <button onClick={() => onChannelSelection([])}>清空</button>
          </div>
          <div className="channel-list all-channel-list">
            {Received_channels.length === 0 && <div className="channel-empty">等待设备上报遥测数据</div>}
            {Received_channels.map((Channel) => (
              <label className={Channel.visible ? "channel-item selected" : "channel-item"} key={Channel.key}>
                <input type="checkbox" checked={Channel.visible} onChange={() => onChannel(Channel.key)} />
                <i style={{ backgroundColor: Channel.color }} />
                <span>{Channel.label}</span>
                <b>{Channel.visible
                  ? (Received_key_set.has(Channel.key)
                    ? `${Format_value(Number(telemetry[Channel.key]), Channel.unit === "rpm" ? 0 : 2)} ${Channel.unit}`
                    : "待采集")
                  : "未选择"}</b>
              </label>
            ))}
          </div>
        </section>
      </div>

      <section className="event-strip" ref={Event_strip_ref}>
        <div className="event-strip-title"><Database size={16} /><span>最近事件</span><small>{logs.length} 条</small></div>
        <div className="event-strip-list">
          {logs.slice(0, 4).map((Log) => (
            <div className={`event-line ${Log.level}`} key={Log.id}>
              <time>{Log.timestamp.toLocaleTimeString("zh-CN", { hour12: false })}</time>
              <b>{Log.source}</b>
              <span>{Log.message}</span>
            </div>
          ))}
        </div>
      </section>

      {Clear_confirm_open && (
        <div className="confirm-dialog-overlay" onMouseDown={(Event) => {
          if (Event.target === Event.currentTarget) setClearConfirmOpen(false);
        }}>
          <section className="confirm-dialog" role="alertdialog" aria-modal="true" aria-labelledby="clear-scope-title" aria-describedby="clear-scope-description">
            <div className="confirm-dialog-content">
              <span className="confirm-dialog-icon"><AlertTriangle size={20} /></span>
              <div>
                <h3 id="clear-scope-title">确认清空示波器？</h3>
                <p id="clear-scope-description">当前 {sampleCount} 个历史采样点将被删除，清空后无法恢复。</p>
              </div>
            </div>
            <div className="confirm-dialog-actions">
              <button type="button" className="confirm-cancel-button" onClick={() => setClearConfirmOpen(false)}>取消</button>
              <button ref={Clear_confirm_button_ref} type="button" className="confirm-clear-button" onClick={Handle_clear_scope}><Trash2 size={15} />确认清空</button>
            </div>
          </section>
        </div>
      )}
      {Observer_panel_floating && Observer_parameter_panel}
      {Foc_parameter_panel_floating && Foc_parameter_panel}
    </div>
  );
}

interface Communication_view_props_t {
  connected: boolean;
  frames: Serial_frame_t[];
  config: Serial_config_t;
  serialErrors: { crcErrors: number; formatErrors: number };
  onConfig: (Patch: Partial<Serial_config_t>) => void;
  onRefreshPorts: () => void;
  onSend: (Type: number, Data: number[]) => void;
  onClear: () => void;
}

/***********************************************
 * @brief : 显示串口配置、收发统计和原始协议帧
 * @param : connected 设备连接状态
 * @param : frames 串口帧记录
 * @return: 通信诊断页面
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function CommunicationView({ connected, frames, config, serialErrors, onConfig, onRefreshPorts, onSend, onClear }: Communication_view_props_t) {
  const [Frame_type_value, setFrameTypeValue] = useState("10");
  const [Frame_data, setFrameData] = useState("00");
  const [Filter_text, setFilterText] = useState("");
  const Rx_count = frames.filter((Frame) => Frame.direction === "RX").length;
  const Tx_count = frames.filter((Frame) => Frame.direction === "TX").length;
  const Rx_bytes = frames.filter((Frame) => Frame.direction === "RX").reduce((Total, Frame) => Total + Frame.length + 10, 0);
  const Tx_bytes = frames.filter((Frame) => Frame.direction === "TX").reduce((Total, Frame) => Total + Frame.length + 10, 0);
  const Filtered_frames = frames.filter((Frame) => Frame.type.toString(16).toUpperCase().includes(Filter_text.toUpperCase()));
  const Type_labels: Record<number, string> = { 0x01: "握手", 0x02: "心跳", 0x10: "控制", 0x11: "读参数", 0x12: "写参数", 0x13: "读 SMO/PLL", 0x14: "乐曲列表", 0x15: "零点校准", 0x16: "驱动板复位", 0x17: "观测流配置", 0x20: "遥测", 0x21: "波形", 0x22: "兼容观测波形", 0x23: "写 SMO/PLL", 0x24: "紧凑观测流", 0x25: "连续观测批次", 0x26: "高分辨率观测批次", 0x27: "扩展观测批次", 0x30: "故障", 0x31: "日志" };

  const Send_frame = () => {
    const Type = Number.parseInt(Frame_type_value, 16);
    const Data = Frame_data.trim().split(/\s+/).filter(Boolean).map((Byte) => Number.parseInt(Byte, 16));
    if (Number.isFinite(Type) && Type >= 0 && Type <= 255 && Data.every((Byte) => Number.isFinite(Byte) && Byte >= 0 && Byte <= 255) && Data.length <= 64) onSend(Type, Data);
  };

  return (
    <div className="communication-layout">
      <section className="tool-panel bus-config-panel">
        <div className="panel-heading"><div><h2>串口配置</h2><p>USB 转串口连接驱动板调试 UART</p></div><button className="icon-button" onClick={onRefreshPorts} disabled={connected} title="刷新 COM 口"><RefreshCw size={16} /></button></div>
        <label className="field-block"><span>波特率</span><select value={config.baudRate} disabled={connected} onChange={(Event) => onConfig({ baudRate: Number(Event.target.value) })}><option value={115200}>115200 bit/s</option><option value={460800}>460800 bit/s</option><option value={921600}>921600 bit/s</option><option value={1000000}>1000000 bit/s</option><option value={2000000}>2000000 bit/s</option></select></label>
        <div className="two-fields">
          <label className="field-block"><span>数据位</span><select value={config.dataBits} disabled={connected} onChange={(Event) => onConfig({ dataBits: Number(Event.target.value) as Serial_config_t["dataBits"] })}><option value={8}>8 bit</option><option value={7}>7 bit</option></select></label>
          <label className="field-block"><span>停止位</span><select value={config.stopBits} disabled={connected} onChange={(Event) => onConfig({ stopBits: Number(Event.target.value) as Serial_config_t["stopBits"] })}><option value={1}>1 bit</option><option value={2}>2 bit</option></select></label>
        </div>
        <label className="field-block"><span>校验位</span><select value={config.parity} disabled={connected} onChange={(Event) => onConfig({ parity: Event.target.value as Serial_config_t["parity"] })}><option value="none">无校验</option><option value="even">偶校验</option><option value="odd">奇校验</option></select></label>

        <div className="bus-health">
          <h3>串口状态</h3>
          <div><span>协议版本</span><b>{connected ? "FOC-UART/1.0" : "--"}</b></div>
          <div><span>数据格式</span><b>{config.dataBits}-{config.parity === "none" ? "N" : config.parity === "even" ? "E" : "O"}-{config.stopBits}</b></div>
          <div><span>接收帧</span><b>{Rx_count}</b></div>
          <div><span>发送帧</span><b>{Tx_count}</b></div>
          <div><span>收发字节</span><b>{Rx_bytes} / {Tx_bytes}</b></div>
          <div><span>CRC / 格式错误</span><b className={serialErrors.crcErrors + serialErrors.formatErrors === 0 ? "good" : "bad"}>{serialErrors.crcErrors} / {serialErrors.formatErrors}</b></div>
        </div>
      </section>

      <section className="tool-panel frame-panel">
        <div className="frame-toolbar">
          <div><h2>协议帧</h2><p>AA 55 帧头 · 小端序 · CRC16-Modbus</p></div>
          <label className="search-box small"><Search size={15} /><input value={Filter_text} onChange={(Event) => setFilterText(Event.target.value)} placeholder="过滤类型" /></label>
          <button className="icon-button" onClick={onClear} title="清空帧"><Trash2 size={16} /></button>
        </div>
        <div className="frame-table-wrap">
          <table className="data-table frame-table">
            <thead><tr><th>时间</th><th>方向</th><th>类型</th><th>长度</th><th>负载数据</th><th>序号</th></tr></thead>
            <tbody>
              {Filtered_frames.slice(0, 120).map((Frame, Index) => (
                <tr key={`${Frame.timestamp}-${Frame.type}-${Frame.sequence}-${Index}`}>
                  <td>{Frame.timestamp.toFixed(3)} s</td>
                  <td><span className={`direction ${Frame.direction.toLowerCase()}`}>{Frame.direction}</span></td>
                  <td>0x{Frame.type.toString(16).toUpperCase().padStart(2, "0")} · {Type_labels[Frame.type] ?? "自定义"}</td>
                  <td>{Frame.length}</td>
                  <td className="frame-data">{Frame.data.map((Byte) => Byte.toString(16).toUpperCase().padStart(2, "0")).join(" ")}</td>
                  <td>{Frame.sequence}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
        <div className="frame-sender">
          <label><span>帧类型</span><input value={Frame_type_value} onChange={(Event) => setFrameTypeValue(Event.target.value)} /></label>
          <label className="data-input"><span>负载字节</span><input value={Frame_data} onChange={(Event) => setFrameData(Event.target.value)} /></label>
          <button disabled={!connected} onClick={Send_frame}><Send size={16} />发送</button>
        </div>
      </section>
    </div>
  );
}

/***********************************************
 * @brief : 管理多页面调试台、设备状态和所有调试业务
 * @param : 无
 * @return: 上位机根组件
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
export default function App() {
  const [Theme, setTheme] = useState<Theme_mode_t>(Get_initial_theme);
  const [Active_page, setActivePage] = useState<App_page_t>("workspace");
  const [Connection_state, setConnectionState] = useState<Connection_state_t>("offline");
  const [Command, setCommand] = useState<Motor_command_t>(() => ({
    ...Initial_command,
    ...Get_initial_control_selection(),
  }));
  const [Loop_parameters, setLoopParameters] = useState<Foc_loop_parameters_t>(Initial_loop_parameters);
  const [MusicTracks, setMusicTracks] = useState<Music_track_t[]>(Music_tracks);
  const [Telemetry, setTelemetry] = useState<Telemetry_t>(Empty_telemetry);
  const [Sample_count, setSampleCount] = useState(0);
  const [Channels, setChannels] = useState<Channel_definition_t[]>(Scope_channels);
  const [Channel_message, setChannelMessage] = useState("");
  const [Received_channel_keys, setReceivedChannelKeys] = useState<Array<keyof Telemetry_t>>([]);
  const [Paused, setPaused] = useState(false);
  const [Frozen_samples, setFrozenSamples] = useState<Telemetry_t[] | null>(null);
  const [Logs, setLogs] = useState<Event_log_t[]>([
    { id: 1, timestamp: new Date(), level: "info", source: "系统", message: "FOC_L 已启动" },
  ]);
  const [Frames, setFrames] = useState<Serial_frame_t[]>([]);
  const [Serial_config, setSerialConfig] = useState<Serial_config_t>(Get_initial_serial_config);
  const [Serial_ports, setSerialPorts] = useState<Serial_port_info_t[]>([{ path: "simulator", manufacturer: "内置仿真", simulated: true }]);
  const [Serial_stats, setSerialStats] = useState({ crcErrors: 0, formatErrors: 0 });
  const [Board_resetting, setBoardResetting] = useState(false);
  const Scope_history_ref = useRef<Telemetry_history_t | null>(null);
  if (Scope_history_ref.current === null) Scope_history_ref.current = new Telemetry_history_t(Scope_sample_limit);
  const Scope_history = Scope_history_ref.current;
  const Simulator_ref = useRef(new Motor_simulator_t());
  const Telemetry_ref = useRef<Telemetry_t>(Empty_telemetry);
  const Command_ref = useRef(Command);
  const Serial_config_ref = useRef(Serial_config);
  const Serial_ports_ref = useRef<Serial_port_info_t[]>(Serial_ports);
  const Serial_scan_in_progress_ref = useRef(false);
  const Control_selection_ref = useRef<Control_selection_t>({
    driveMode: Command.driveMode,
    mode: Command.mode,
    direction: Command.direction,
    positionReturnMode: Command.positionReturnMode,
  });
  const Connection_state_ref = useRef(Connection_state);
  const Music_play_seen_ref = useRef(false);
  const Last_waveform_receive_ref = useRef(0);
  const Observer_stream_ready_mask_ref = useRef(0);
  const Pending_frames_ref = useRef<Serial_frame_t[]>([]);
  const Frame_counter_ref = useRef(0);
  const Log_id_ref = useRef(2);
  const Pending_loop_parameters_ref = useRef<Foc_loop_parameters_t | null>(null);
  const Pending_parameter_groups_ref = useRef<Set<Parameter_group_t>>(new Set());
  const Loop_parameter_timer_ref = useRef<number | null>(null);

  useLayoutEffect(() => {
    document.documentElement.dataset.theme = Theme;
    document.documentElement.style.colorScheme = Theme;
    document.querySelector('meta[name="theme-color"]')?.setAttribute("content", Theme === "light" ? "#edf1f4" : "#101419");
    try {
      window.localStorage.setItem(Theme_storage_key, Theme);
    } catch {
      // 本地存储不可用时仍允许当前窗口完成主题热切换
    }
  }, [Theme]);

  useEffect(() => {
    try {
      window.localStorage.setItem(Serial_config_storage_key, JSON.stringify(Serial_config));
    } catch {
      // 本地存储不可用时仍允许当前窗口正常使用串口配置
    }
  }, [Serial_config]);

  useEffect(() => { Command_ref.current = Command; }, [Command]);
  useEffect(() => { Serial_config_ref.current = Serial_config; }, [Serial_config]);
  useEffect(() => { Connection_state_ref.current = Connection_state; }, [Connection_state]);
  useEffect(() => () => {
    if (Loop_parameter_timer_ref.current !== null) {
      window.clearTimeout(Loop_parameter_timer_ref.current);
    }
  }, []);

  const Observer_stream_mask = useMemo(
    () => Channels.reduce(
      (Mask, Channel) => Channel.visible
        ? Mask + (Observer_stream_field_bits[Channel.key] ?? 0)
        : Mask,
      0,
    ),
    [Channels],
  );

  useEffect(() => {
    Scope_history.setDisplayNotifyInterval(Scope_refresh_interval_ms);
  }, [Scope_history]);

  useEffect(() => {
    /* 切换连接或订阅字段后，必须等待新配置对应的高速样本到达。 */
    Observer_stream_ready_mask_ref.current = 0;
  }, [Connection_state, Observer_stream_mask]);

  /***********************************************
   * @brief : 合并遥测和高速波形并写入采集缓冲
   * @param : Data_list 新到达的遥测或波形样本
   * @param : High_speed 是否为高速波形样本
   * @return: 无
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  const Append_telemetry_samples = useCallback((Data_list: Array<Partial<Telemetry_t>>, High_speed: boolean) => {
    if (Data_list.length === 0) return;
    /* 高速流中断后让状态卡恢复基础遥测，示波器仍等待真正的高速采样。 */
    if (!High_speed && performance.now() - Last_waveform_receive_ref.current >= 200) {
      Observer_stream_ready_mask_ref.current = 0;
    }
    let Incoming_data_list = Data_list;
    if (High_speed) {
      let Ready_mask = Observer_stream_ready_mask_ref.current;
      Data_list.forEach((Data) => {
        Observer_stream_fields.forEach((Field, Bit) => {
          const Field_bit = Observer_stream_field_bits[Field] ?? 0;
          const Field_value = Number(Data[Field]);
          if (Object.prototype.hasOwnProperty.call(Data, Field) &&
              Number.isFinite(Field_value) &&
              Is_observer_stream_field_selected(Observer_stream_mask, Bit) &&
              !Is_observer_stream_field_selected(Ready_mask, Bit)) {
            Ready_mask += Field_bit;
          }
        });
      });
      Observer_stream_ready_mask_ref.current = Ready_mask;
    }
    if (!High_speed && Observer_stream_mask !== 0) {
      /* 状态卡优先使用已收到的高速字段，尚未收到的字段继续由基础遥测更新。 */
      Incoming_data_list = Data_list.map((Data) => {
        const Filtered_data = { ...Data };
        Observer_stream_fields.forEach((Field, Bit) => {
          const Stream_ready = Is_observer_stream_field_selected(Observer_stream_ready_mask_ref.current, Bit);
          if (Is_observer_stream_field_selected(Observer_stream_mask, Bit) && Stream_ready) {
            delete Filtered_data[Field];
          }
        });
        return Filtered_data;
      });
    }
    /* 目标机械角度由上位机控制命令生成，协议遥测只提供实际机械角度。 */
    const Current_command = Command_ref.current;
    const Is_position_control =
      (Current_command.driveMode === "encoderFoc" ||
       Current_command.driveMode === "sensorlessFoc") &&
      Current_command.mode === "position";
    const Position_target = Number(Current_command.positionTarget);
    const Target_mechanical_angle = Is_position_control && Number.isFinite(Position_target)
      ? Math.min(Math.max(Position_target, 0), 360)
      : 0;
    const Incoming_channel_keys = Scope_channels
      .filter((Channel) => Channel.key === "mechanicalAngleTarget" ||
        Incoming_data_list.some((Data) => Object.prototype.hasOwnProperty.call(Data, Channel.key)))
      .map((Channel) => Channel.key);
    setReceivedChannelKeys((Current) => {
      const New_keys = Incoming_channel_keys.filter((Key) => !Current.includes(Key));
      if (New_keys.length === 0) return Current;
      const Received_key_set = new Set([...Current, ...New_keys]);
      return Scope_channels.filter((Channel) => Received_key_set.has(Channel.key)).map((Channel) => Channel.key);
    });

    const Samples = Incoming_data_list.map((Data) => {
      const Next = {
        ...Telemetry_ref.current,
        ...Data,
        mechanicalAngleTarget: Target_mechanical_angle,
      } as Telemetry_t;
      Telemetry_ref.current = Next;
      const History_sample = { ...Next };
      if (!High_speed) {
        /* 基础遥测只更新状态卡，不能冒充已勾选设备通道的1 ms波形。 */
        Observer_stream_fields.forEach((Field) => {
          History_sample[Field] = Number.NaN;
        });
        return History_sample;
      }

      /* 高速帧未携带的可订阅字段不能沿用旧值，否则勾选通道后会把历史数据误画成低速阶梯。 */
      Observer_stream_fields.forEach((Field) => {
        if (!Object.prototype.hasOwnProperty.call(Data, Field)) History_sample[Field] = Number.NaN;
      });
      return History_sample;
    });
    if (High_speed) {
      Last_waveform_receive_ref.current = performance.now();
      Scope_history.appendBatch(Samples);
      return;
    }
    if (Observer_stream_mask === 0 && performance.now() - Last_waveform_receive_ref.current >= 200) {
      Scope_history.appendBatch(Samples);
    }
  }, [Observer_stream_mask, Scope_history]);

  useEffect(() => {
    const Refresh_timer = window.setInterval(() => {
      const Latest_telemetry = Telemetry_ref.current;
      setTelemetry((Current) => Current === Latest_telemetry ? Current : Latest_telemetry);
      const Current_count = Scope_history.getCount();
      setSampleCount((Current) => Current === Current_count ? Current : Current_count);
      if (Pending_frames_ref.current.length > 0) {
        const Pending_frames = Pending_frames_ref.current;
        Pending_frames_ref.current = [];
        setFrames((Current) => [...Pending_frames.reverse(), ...Current].slice(0, 500));
      }
    }, Status_refresh_interval_ms);
    return () => window.clearInterval(Refresh_timer);
  }, [Scope_history]);

  const Add_log = useCallback((Level: Event_log_t["level"], Source: string, Message: string) => {
    const New_log: Event_log_t = { id: Log_id_ref.current++, timestamp: new Date(), level: Level, source: Source, message: Message };
    setLogs((Current) => [New_log, ...Current].slice(0, 300));
  }, []);

  useEffect(() => {
    if (Connection_state !== "online" ||
        Serial_config.port === "simulator" ||
        Board_resetting ||
        !window.focBridge)
    {
      return;
    }

    const Mask_base = 2 ** 32;
    const Payload_bytes = new Uint8Array(13);
    const Payload_view = new DataView(Payload_bytes.buffer);
    Payload_view.setUint32(0, Observer_stream_mask % Mask_base, true);
    Payload_view.setUint32(4, Math.floor(Observer_stream_mask / Mask_base), true);
    Payload_view.setUint8(8, Observer_stream_period_ms);
    Payload_view.setUint32(
      9,
      Math.floor(Serial_config.baudRate * Observer_stream_bandwidth_ratio),
      true,
    );
    /* 所有勾选的设备通道统一订阅1 ms采样，基础遥测只更新状态卡。 */
    void window.focBridge
      .sendSerialFrame(Observer_stream_config_frame_type, Array.from(Payload_bytes))
      .then((Result) => {
        if (!Result.ok) Add_log("error", "串口", `观测流配置失败：${Result.message}`);
      })
      .catch((Caught_error) => {
        Add_log("error", "串口", `观测流配置失败：${Caught_error instanceof Error ? Caught_error.message : String(Caught_error)}`);
      });
  }, [Add_log, Board_resetting, Connection_state, Observer_stream_mask, Serial_config.baudRate, Serial_config.port]);

  /***********************************************
   * @brief : 根据下位机播放完成状态自动关闭音乐使能
   * @return: 无
   * @date  : 2026-08-30
   * @author: L
   ************************************************/
  const Finish_music_playback = useCallback(() => {
    Music_play_seen_ref.current = false;
    setCommand((Current) => {
      if (Current.driveMode !== "foc_voice" || !Current.enabled) return Current;
      const Next = { ...Current, enabled: false };
      Command_ref.current = Next;
      if (window.focBridge) {
        void window.focBridge.sendControl(Next as unknown as Record<string, unknown>, true).then((Result) => {
          if (!Result.ok) Add_log("error", "音乐", `播放完成状态同步失败：${Result.message}`);
        });
      }
      return Next;
    });
    Add_log("info", "音乐", "乐曲播放完成，输出已自动关闭");
  }, [Add_log]);

  const Clear_scope_samples = useCallback(() => {
    Scope_history.clear();
    setFrozenSamples((Current) => Current === null ? null : []);
    setSampleCount(0);
  }, [Scope_history]);

  /***********************************************
   * @brief : 切换示波器实时跟随并保存或释放冻结快照
   * @return: 无
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  const Toggle_scope_follow = useCallback(() => {
    if (Paused) {
      setFrozenSamples(null);
      setPaused(false);
      return;
    }
    setFrozenSamples(Scope_history.toArray());
    setPaused(true);
  }, [Paused, Scope_history]);

  const Clear_serial_frames = useCallback(() => {
    Pending_frames_ref.current = [];
    setFrames([]);
  }, []);

  const Refresh_serial_ports = useCallback(async (Log_scan = true) => {
    if (!window.focBridge || Serial_scan_in_progress_ref.current) return;
    Serial_scan_in_progress_ref.current = true;
    try {
      const Ports = await window.focBridge.listSerialPorts();
      const Previous_ports = Serial_ports_ref.current;
      const Ports_changed = !Are_serial_ports_equal(Previous_ports, Ports);
      let Replacement_port: string | null = null;
      if (Ports_changed) {
        Serial_ports_ref.current = Ports;
        setSerialPorts(Ports);
        const Physical_ports = Ports.filter((Port) => !Port.simulated);
        const Previous_physical_paths = new Set(Previous_ports.filter((Port) => !Port.simulated).map((Port) => Port.path));
        const Added_physical_ports = Physical_ports.filter((Port) => !Previous_physical_paths.has(Port.path));
        const Replacement_candidates = Added_physical_ports.length === 1
          ? Added_physical_ports
          : Physical_ports.length === 1
            ? Physical_ports
            : [];
        const Selected_port = Serial_config_ref.current.port;
        const Selected_port_missing = Selected_port !== "simulator"
          && !Physical_ports.some((Port) => Port.path === Selected_port);
        if (Connection_state_ref.current === "offline" && Selected_port_missing && Replacement_candidates.length === 1) {
          Replacement_port = Replacement_candidates[0].path;
          setSerialConfig((Current) => Current.port === Selected_port
            ? { ...Current, port: Replacement_port as string }
            : Current);
        }
      }
      if (Log_scan || Ports_changed) {
        Add_log("info", "串口", Replacement_port
          ? `检测到原串口已更换，已自动切换到 ${Replacement_port}`
          : `检测到 ${Math.max(Ports.length - 1, 0)} 个物理 COM 口`);
      }
    } catch (Caught_error) {
      Add_log("error", "串口", `COM 口扫描失败：${Caught_error instanceof Error ? Caught_error.message : String(Caught_error)}`);
    } finally {
      Serial_scan_in_progress_ref.current = false;
    }
  }, [Add_log]);

  useEffect(() => {
    void Refresh_serial_ports();
    const Refresh_timer = window.setInterval(() => {
      void Refresh_serial_ports(false);
    }, Serial_port_poll_interval_ms);
    return () => window.clearInterval(Refresh_timer);
  }, [Refresh_serial_ports]);

  useEffect(() => {
    if (!window.focBridge) return;
    const Remove_frame = window.focBridge.onSerialFrame((Frame) => {
      const New_frame: Serial_frame_t = {
        type: Frame.type,
        sequence: Frame.sequence,
        timestamp: (Frame.timestamp % 86400000) / 1000,
        direction: Frame.direction === "TX" ? "TX" : "RX",
        length: Frame.data.length,
        data: Frame.data,
      };
      Pending_frames_ref.current.push(New_frame);
    });
    const Remove_telemetry = window.focBridge.onSerialTelemetry((Data) => {
      const Current_command = Command_ref.current;
      const Previous_device_state = Telemetry_ref.current.state;
      const Previous_flags = Telemetry_ref.current.flags;
      const Control_timed_out = (Data.flags & 0x08) !== 0;
      const Rx_overflowed = (Data.flags & 0x10) !== 0;
      if (Rx_overflowed && (Previous_flags & 0x10) === 0) {
        Add_log("warning", "串口", "下位机 UART 接收缓冲曾溢出，控制帧可能丢失");
      }
      if (Current_command.enabled &&
          Previous_device_state !== 0 &&
          Data.state === 0) {
        /* 下位机因控制超时等原因已撤销输出时，先同步失能状态并完成重新使能握手。 */
        const Safe_command = { ...Current_command, enabled: false };
        Command_ref.current = Safe_command;
        setCommand(Safe_command);
        if (window.focBridge) {
          void window.focBridge.sendControl(Safe_command as unknown as Record<string, unknown>, true);
        }
        Add_log("warning", "电机", Control_timed_out
          ? "下位机因控制帧超时撤销输出，已同步失能状态，请重新启动"
          : "下位机已撤销输出，原因未确认；已同步失能状态，请重新启动");
      }
      const Voice_enabled = Current_command.driveMode === "foc_voice" && Current_command.enabled;
      if (Voice_enabled && Data.musicPlaying !== 0) {
        Music_play_seen_ref.current = true;
      } else if (Voice_enabled && Music_play_seen_ref.current) {
        Finish_music_playback();
      } else if (!Voice_enabled) {
        Music_play_seen_ref.current = false;
      }
      Append_telemetry_samples([Data as Partial<Telemetry_t>], false);
    });
    const Remove_waveform = window.focBridge.onSerialWaveformBatch((Samples) => {
      Append_telemetry_samples(Samples as Array<Partial<Telemetry_t>>, true);
    });
    const Remove_observer_stream = window.focBridge.onSerialObserverStreamBatch((Batch) => {
      try {
        Append_telemetry_samples(Decode_observer_stream_batch(Batch), true);
      } catch (Caught_error) {
        Add_log(
          "error",
          "串口",
          `紧凑观测批次解析失败：${Caught_error instanceof Error ? Caught_error.message : String(Caught_error)}`);
      }
    });
    const Remove_music_tracks = window.focBridge.onSerialMusicTracks((Tracks) => {
      if (Tracks.length === 0) return;
      setMusicTracks(Tracks);
      setCommand((Current) => Tracks.some((Track) => Track.id === Current.focVoiceSongId)
        ? Current
        : { ...Current, enabled: false, focVoiceSongId: Tracks[0].id });
      Add_log("info", "音乐", `已从下位机同步 ${Tracks.length} 首乐曲`);
    });
    const Remove_parameters = window.focBridge.onSerialParameters((Parameters) => {
      if (Connection_state_ref.current === "offline") return;
      const Synced_parameters = Parameters as Partial<Foc_loop_parameters_t> & { rampRate?: number };
      setLoopParameters((Current) => ({ ...Current, ...Synced_parameters }));
      setCommand((Current) => ({
        ...Current,
        ...(typeof Synced_parameters.currentBandwidth === "number" && Number.isFinite(Synced_parameters.currentBandwidth) ? { currentBandwidth: Synced_parameters.currentBandwidth } : {}),
        ...(typeof Synced_parameters.rampRate === "number" && Number.isFinite(Synced_parameters.rampRate) ? { rampRate: Synced_parameters.rampRate } : {}),
      }));
      Add_log("info", "参数", Object.keys(Synced_parameters).some((Key) => Key.startsWith("smo") || Key.startsWith("pll"))
        ? "已从下位机同步 SMO / PLL 参数"
        : "已从下位机同步 FOC 环路参数和速度斜率");
    });
    const Remove_status = window.focBridge.onSerialStatus((Status) => {
      if (Status.state === "closed" || Status.state === "error") {
        Connection_state_ref.current = "offline";
        setConnectionState("offline");
        setCommand((Current) => ({
          ...Initial_command,
          ...Control_selection_ref.current,
          focVoiceSongId: Current.focVoiceSongId,
        }));
        setLoopParameters(Initial_loop_parameters);
        setMusicTracks(Music_tracks);
        setReceivedChannelKeys([]);
      }
      Add_log(Status.state === "error" ? "error" : "warning", "串口", Status.message);
    });
    const Remove_stats = window.focBridge.onSerialStats((Stats) => setSerialStats(Stats));
    return () => {
      Remove_frame();
      Remove_telemetry();
      Remove_waveform();
      Remove_observer_stream();
      Remove_music_tracks();
      Remove_parameters();
      Remove_status();
      Remove_stats();
    };
  }, [Add_log, Append_telemetry_samples, Finish_music_playback]);

  useEffect(() => {
    if (Connection_state !== "online" || Serial_config.port !== "simulator") return;
    let Last_time = performance.now();
    let Frame_tick = 0;
    const Timer = window.setInterval(() => {
      const Now = performance.now();
      const Delta_time = Math.min((Now - Last_time) / 1000, 0.1);
      Last_time = Now;
      const Next = Simulator_ref.current.update(Command_ref.current, Delta_time, Loop_parameters);
      Telemetry_ref.current = Next;
      Scope_history.append(Next);

      Frame_tick += Delta_time;
      if (Frame_tick >= 0.1) {
        Frame_tick = 0;
        Frame_counter_ref.current += 1;
        const Payload = buildSimulationTelemetryPayload(Next);
        const New_frame: Serial_frame_t = {
          type: 0x20,
          sequence: Frame_counter_ref.current,
          timestamp: Next.timestamp,
          direction: "RX",
          length: Payload.length,
          data: Payload,
        };
        Pending_frames_ref.current.push(New_frame);
      }
    }, 5);
    return () => window.clearInterval(Timer);
  }, [Connection_state, Loop_parameters, Scope_history, Serial_config.port]);

  /***********************************************
   * @brief : 记忆用户选择的驱动模式、FOC子模式、方向和回正方式
   * @param : Patch 本次控制状态变化
   * @return: 无
   * @date  : 2026-08-29
   * @author: L
   ************************************************/
  const Remember_control_selection = (Patch: Partial<Motor_command_t>) => {
    if ((Patch.driveMode === undefined) &&
        (Patch.mode === undefined) &&
        (Patch.direction === undefined) &&
        (Patch.positionReturnMode === undefined))
    {
      return;
    }

    Control_selection_ref.current = {
      driveMode: Patch.driveMode ?? Control_selection_ref.current.driveMode,
      mode: Patch.mode ?? Control_selection_ref.current.mode,
      direction: Patch.direction ?? Control_selection_ref.current.direction,
      positionReturnMode: Patch.positionReturnMode ??
                          Control_selection_ref.current.positionReturnMode,
    };
    try {
      window.localStorage.setItem(
        Control_selection_storage_key,
        JSON.stringify(Control_selection_ref.current));
    } catch {
      // 本地存储不可用时仍保持当前窗口内的模式记忆
    }
  };

  /***********************************************
   * @brief : 立即更新控制状态并向下位机发送最新完整控制命令
   * @param : Patch 本次发生变化的控制字段
   * @return: 控制命令提交结果
   * @date  : 2026-09-16
   * @author: L
   ************************************************/
  const Update_command = (Patch: Partial<Motor_command_t>) => {
    Remember_control_selection(Patch);
    const Next = { ...Command_ref.current, ...Patch };
    Command_ref.current = Next;
    setCommand(Next);
    if (Connection_state_ref.current === "online" && Serial_config_ref.current.port !== "simulator" && window.focBridge) {
      const Send_result = window.focBridge.sendControl(
        Next as unknown as Record<string, unknown>, Patch.enabled === false).catch((Caught_error) => ({
          ok: false,
          message: `控制命令提交失败：${Caught_error instanceof Error ? Caught_error.message : String(Caught_error)}`,
        }));
      if (Patch.rampRate !== undefined && !Next.enabled) {
        void window.focBridge.sendParameters({
          ...Loop_parameters,
          rampRate: Next.rampRate,
        }, "foc").then((Result) => {
          if (!Result.ok) Add_log("error", "参数", Result.message);
        });
      }
      return Send_result;
    }
    return Promise.resolve({
      ok: Serial_config_ref.current.port === "simulator",
      message: Serial_config_ref.current.port === "simulator" ? "仿真控制命令已应用" : "串口未连接",
    });
  };

  /***********************************************
   * @brief : 依次发送本轮发生变化的参数分组
   * @param : Parameters 待同步的完整参数
   * @param : Parameter_groups 待发送的参数分组
   * @return: 无
   * @date  : 2026-09-15
   * @author: L
   ************************************************/
  const Send_parameter_groups = async (
    Parameters: Foc_loop_parameters_t,
    Parameter_groups: Parameter_group_t[],
  ) => {
    if (!window.focBridge) return;
    for (const Parameter_group of Parameter_groups) {
      const Result = await window.focBridge.sendParameters({
        ...Parameters,
        rampRate: Command_ref.current.rampRate,
      }, Parameter_group);
      if (!Result.ok) Add_log("error", "参数", Result.message);
    }
  };

  /***********************************************
   * @brief : 合并滑块连续变化并按参数分组发送最新值
   * @param : Parameters 待同步的完整参数
   * @param : Parameter_group 本次变化所属分组
   * @return: 无
   * @date  : 2026-09-15
   * @author: L
   ************************************************/
  const Schedule_loop_parameter_sync = (
    Parameters: Foc_loop_parameters_t,
    Parameter_group: Parameter_group_t,
  ) => {
    Pending_loop_parameters_ref.current = Parameters;
    Pending_parameter_groups_ref.current.add(Parameter_group);
    if (Loop_parameter_timer_ref.current !== null) return;

    Loop_parameter_timer_ref.current = window.setTimeout(() => {
      const Pending_parameters = Pending_loop_parameters_ref.current;
      const Pending_parameter_groups = [...Pending_parameter_groups_ref.current];
      Pending_loop_parameters_ref.current = null;
      Pending_parameter_groups_ref.current.clear();
      Loop_parameter_timer_ref.current = null;
      if (!Pending_parameters ||
          (Pending_parameter_groups.length === 0) ||
          (Connection_state_ref.current !== "online") ||
          (Serial_config_ref.current.port === "simulator") ||
          !window.focBridge) return;

      void Send_parameter_groups(Pending_parameters, Pending_parameter_groups);
    }, 80);
  };

  /***********************************************
   * @brief : 更新并同步FOC及无感观测器参数
   * @param : Key 参数名称
   * @param : Value 参数值
   * @return: 无
   * @date  : 2026-08-29
   * @author: L
   ************************************************/
  const Update_loop_parameter = (Key: keyof Foc_loop_parameters_t, Value: number) => setLoopParameters((Current) => {
    const Next = { ...Current, [Key]: Value };
    const Parameter_group: Parameter_group_t = Key.startsWith("smo") || Key.startsWith("pll")
      ? "observer"
      : "foc";
    Schedule_loop_parameter_sync(Next, Parameter_group);
    if (Key === "currentBandwidth") Update_command({ currentBandwidth: Value });
    return Next;
  });

  /***********************************************
   * @brief : 打开或关闭当前串口连接
   * @param : 无
   * @return: 无
   * @date  : 2026-08-29
   * @author: L
   ************************************************/
  const Connect = async () => {
    if (Connection_state === "online") {
      const Stop_result = await Update_command({
        enabled: false,
        speedTarget: 0,
        iqTarget: 0,
        idTarget: 0,
        voltageTarget: 0,
        udTarget: 0,
        positionTarget: 0,
        angleTarget: 0,
        angleStep: 0,
      });
      Connection_state_ref.current = "offline";
      if (window.focBridge) await window.focBridge.disconnectSerial();
      setConnectionState("offline");
      setCommand((Current) => ({
        ...Initial_command,
        ...Control_selection_ref.current,
        focVoiceSongId: Current.focVoiceSongId,
      }));
      setLoopParameters(Initial_loop_parameters);
      setMusicTracks(Music_tracks);
      setReceivedChannelKeys([]);
      Add_log(
        Stop_result.ok ? "warning" : "error",
        "串口",
        Stop_result.ok
          ? `${Serial_config.port === "simulator" ? "仿真设备" : Serial_config.port}已断开，停止控制帧已发出`
          : `${Serial_config.port}已断开，停止控制帧发送失败：${Stop_result.message}`);
      return;
    }
    Connection_state_ref.current = "connecting";
    setConnectionState("connecting");
    setCommand((Current) => ({
      ...Initial_command,
      ...Control_selection_ref.current,
      focVoiceSongId: Current.focVoiceSongId,
    }));
    setLoopParameters(Initial_loop_parameters);
    setMusicTracks(Music_tracks);
    setSerialStats({ crcErrors: 0, formatErrors: 0 });
    setReceivedChannelKeys([]);
    const Result = window.focBridge
      ? await window.focBridge.connectSerial(Serial_config as unknown as Record<string, unknown>)
      : await new Promise<{ ok: boolean; message: string; parameters?: Record<string, number> }>((Resolve) => window.setTimeout(() => Resolve(Serial_config.port === "simulator" ? { ok: true, message: "浏览器串口仿真设备已连接" } : { ok: false, message: "浏览器预览只能连接仿真设备" }), 450));
    if (Result.ok) {
      Simulator_ref.current.reset();
      Connection_state_ref.current = "online";
      setConnectionState("online");
      if (Serial_config.port === "simulator") {
        setCommand({
          ...Initial_command,
          ...Control_selection_ref.current,
          focVoiceSongId: Default_music_track.id,
        });
        setLoopParameters(Simulated_loop_parameters);
        setReceivedChannelKeys(Simulated_telemetry_channels);
      } else if (window.focBridge) {
        const Synced_current_bandwidth = Number(Result.parameters?.currentBandwidth);
        const Synced_ramp_rate = Number(Result.parameters?.rampRate);
        /* 连接返回值已完成参数回读，首帧必须直接使用该结果，避免异步事件尚未刷新控制引用。 */
        const Startup_command: Motor_command_t = {
          ...Initial_command,
          ...Control_selection_ref.current,
          focVoiceSongId: Command.focVoiceSongId,
          currentBandwidth: Number.isFinite(Synced_current_bandwidth) ? Synced_current_bandwidth : Initial_command.currentBandwidth,
          rampRate: Number.isFinite(Synced_ramp_rate) ? Synced_ramp_rate : Initial_command.rampRate,
        };
        Command_ref.current = Startup_command;
        setCommand(Startup_command);
        const Startup_result = await window.focBridge.sendControl(
          Startup_command as unknown as Record<string, unknown>, true);
        if (!Startup_result.ok) {
          Add_log("error", "串口", `连接后控制状态同步失败：${Startup_result.message}`);
        }
      }
      Add_log("success", "串口", Result.message);
    } else {
      Connection_state_ref.current = "offline";
      setConnectionState("offline");
      Add_log("error", "串口", Result.message);
    }
  };

  /***********************************************
   * @brief : 锁定急停状态并发送全部目标清零的急停控制帧
   * @return: 无
   * @date  : 2026-09-16
   * @author: L
   ************************************************/
  const Emergency_stop = async () => {
    const Result = await Update_command({
      enabled: false,
      emergencyStopped: true,
      speedTarget: 0,
      iqTarget: 0,
      idTarget: 0,
      voltageTarget: 0,
      udTarget: 0,
      positionTarget: 0,
      angleTarget: 0,
      angleStep: 0,
    });
    if (Result.ok) {
      Add_log("error", "安全", "急停控制帧已从串口发出，已请求下位机锁定停机");
    } else {
      Add_log("error", "安全", `界面急停已锁定，但控制帧发送失败：${Result.message}`);
    }
  };

  /* 小键盘回车仅用于触发急停，复位必须重新使用鼠标点按。 */
  useEffect(() => {
    const Handle_emergency_key_down = (Event: KeyboardEvent) => {
      if (Event.code !== "NumpadEnter") return;
      Event.preventDefault();
      if (Event.repeat || Command.emergencyStopped) return;
      void Emergency_stop();
    };

    window.addEventListener("keydown", Handle_emergency_key_down);
    return () => window.removeEventListener("keydown", Handle_emergency_key_down);
  }, [Command.emergencyStopped, Emergency_stop]);

  const Reset_emergency = () => {
    Update_command({ emergencyStopped: false });
    Add_log("warning", "安全", "急停状态已人工复位");
  };

  const Start_motor = async () => {
    let Result: { ok: boolean; message: string };

    /* 启动前先完成一次失能握手，解除下位机复位或控制超时后的重新使能锁。 */
    const Rearm_result = await Update_command({ enabled: false });
    if (!Rearm_result.ok) {
      Add_log("error", "电机", `启动前失能握手失败：${Rearm_result.message}`);
      return;
    }

    if (Command.driveMode === "foc_voice") {
      Music_play_seen_ref.current = false;
      Result = await Update_command({ enabled: true, focVoiceSession: (Command.focVoiceSession + 1) >>> 0 });
    } else {
      Result = await Update_command({ enabled: true });
    }
    const Active_mode = (Command.driveMode === "encoderFoc" || Command.driveMode === "sensorlessFoc")
      ? `${Drive_mode_labels[Command.driveMode]} · ${Mode_labels[Command.mode]}`
      : Command.driveMode === "foc_voice"
        ? MusicTracks.find((Track) => Track.id === Command.focVoiceSongId)?.name ?? Drive_mode_labels[Command.driveMode]
        : Drive_mode_labels[Command.driveMode];
    Add_log(
      Result.ok ? "info" : "error",
      "电机",
      Result.ok
        ? `${Active_mode}控制帧已写入串口，等待下位机确认使能`
        : `${Active_mode}控制帧发送失败：${Result.message}`);
    if (!Result.ok) {
      const Failed_command = { ...Command_ref.current, enabled: false };
      Command_ref.current = Failed_command;
      setCommand(Failed_command);
    }
  };

  /***********************************************
   * @brief : 停止电机输出并保留当前控制模式选择
   * @return: 无
   * @date  : 2026-08-29
   * @author: L
   ************************************************/
  const Stop_motor = async () => {
    /* 停止时保留模式选择，但清空全部运行目标，避免旧目标继续出现在界面和控制帧中。 */
    Music_play_seen_ref.current = false;
    const Result = await Update_command({
      enabled: false,
      driveMode: Control_selection_ref.current.driveMode,
      mode: Control_selection_ref.current.mode,
      direction: Control_selection_ref.current.direction,
      positionReturnMode: Control_selection_ref.current.positionReturnMode,
      speedTarget: 0,
      iqTarget: 0,
      idTarget: 0,
      voltageTarget: 0,
      udTarget: 0,
      positionTarget: 0,
      angleTarget: 0,
      angleStep: 0,
    });
    if (Result.ok) {
      Add_log("info", "电机", "停止控制帧已从串口发出，等待下位机撤销输出");
    } else {
      Add_log("error", "电机", `停止控制帧发送失败：${Result.message}`);
    }
  };

  const Export_csv = async () => {
    const Samples = Scope_history.toArray();
    if (Samples.length === 0) {
      Add_log("warning", "数据", "没有可导出的波形数据");
      return;
    }
    const Keys = Object.keys(Empty_telemetry) as Array<keyof Telemetry_t>;
    const Content = `\uFEFF${Keys.join(",")}\r\n${Samples.map((Sample) => Keys.map((Key) => Sample[Key]).join(",")).join("\r\n")}`;
    let Saved = false;
    if (window.focBridge) Saved = await window.focBridge.saveCsv(Content);
    else {
      const Blob_data = new Blob([Content], { type: "text/csv;charset=utf-8" });
      const Link = document.createElement("a");
      Link.href = URL.createObjectURL(Blob_data);
      Link.download = "foc-wave.csv";
      Link.click();
      URL.revokeObjectURL(Link.href);
      Saved = true;
    }
    if (Saved) Add_log("success", "数据", `已导出 ${Samples.length} 点波形数据`);
  };

  const Send_frame = async (Type: number, Data: number[]): Promise<boolean> => {
    if (window.focBridge) {
      const Result = await window.focBridge.sendSerialFrame(Type, Data);
      if (Result.ok && Serial_config_ref.current.port === "simulator") {
        const Sequence = Frame_counter_ref.current++;
        const New_frame: Serial_frame_t = { type: Type, sequence: Sequence, timestamp: Telemetry.timestamp, direction: "TX", length: Data.length, data: Data };
        setFrames((Current) => [New_frame, ...Current].slice(0, 500));
      }
      Add_log(Result.ok ? "info" : "error", "串口", Result.ok ? `已发送 0x${Type.toString(16).toUpperCase()} 类型帧` : Result.message);
      return Result.ok;
    } else {
      const Sequence = Frame_counter_ref.current++;
      const New_frame: Serial_frame_t = { type: Type, sequence: Sequence, timestamp: Telemetry.timestamp, direction: "TX", length: Data.length, data: Data };
      setFrames((Current) => [New_frame, ...Current].slice(0, 500));
      Add_log("info", "串口", `已发送仿真帧 0x${Type.toString(16).toUpperCase()}`);
      return true;
    }
  };

  /***********************************************
   * @brief : 向驱动板发送一次编码器零点校准命令
   * @return: 无
   * @date  : 2026-08-29
   * @author: L
   ************************************************/
  const Zero_calibration = async () => {
    if (Connection_state !== "online" || Command.enabled || Command.emergencyStopped) return;
    const Sent = await Send_frame(Zero_calibration_frame_type, []);
    if (Sent) Add_log("warning", "校准", "零点校准指令已发送，请勿转动电机");
  };

  /***********************************************
   * @brief : 发送驱动板软件复位命令并预先关闭控制输出
   * @return: 无
   * @date  : 2026-09-14
   * @author: L
   ************************************************/
  const Reset_board = async () => {
    if (Connection_state !== "online" ||
        Serial_config.port === "simulator" ||
        !window.focBridge ||
        Board_resetting)
    {
      return;
    }
    if (!window.confirm("确定要复位驱动板吗？复位会停止电机并重新初始化板端程序。")) return;

    setBoardResetting(true);
    let Reset_sent = false;
    try {
      const Safe_command: Motor_command_t = {
        ...Command_ref.current,
        enabled: false,
        emergencyStopped: false,
        speedTarget: 0,
        iqTarget: 0,
        idTarget: 0,
        voltageTarget: 0,
        udTarget: 0,
        positionTarget: 0,
        angleTarget: 0,
        angleStep: 0,
      };
      const Stop_result = await window.focBridge.sendControl(
        Safe_command as unknown as Record<string, unknown>, true);
      if (!Stop_result.ok) {
        Add_log("error", "设备", `复位前关闭输出失败：${Stop_result.message}`);
        return;
      }

      Command_ref.current = Safe_command;
      setCommand(Safe_command);
      const Sent = await Send_frame(Board_reset_frame_type, []);
      if (Sent) {
        Reset_sent = true;
        Add_log("warning", "设备", "驱动板复位指令已发送，正在重新初始化");
        window.setTimeout(() => {
          if (Connection_state_ref.current !== "online") {
            setBoardResetting(false);
            return;
          }
          void Send_frame(Parameter_read_frame_type, []);
          void Send_frame(Observer_parameter_read_frame_type, []);
          void Send_frame(Song_list_frame_type, []);
          Add_log("info", "设备", "已请求复位后的参数和曲目同步");
          setBoardResetting(false);
        }, 700);
      }
    } catch (Caught_error) {
      Add_log("error", "设备", `驱动板复位失败：${Caught_error instanceof Error ? Caught_error.message : String(Caught_error)}`);
    } finally {
      if (!Reset_sent) setBoardResetting(false);
    }
  };

  /***********************************************
   * @brief : 校验串口带宽后应用整组通道选择，超额时保留原选择并提示
   * @param : Keys 用户选择的通道键
   * @return: 无
   * @date  : 2026-09-28
   * @author: L
   ************************************************/
  const Set_channel_selection = (Keys: Array<keyof Telemetry_t>) => {
    const Selected_keys = new Set(Keys);
    const Next_channels = Channels.map((Channel) => ({ ...Channel, visible: Selected_keys.has(Channel.key) }));
    const Message = Get_channel_selection_error(Next_channels, Serial_config);
    setChannelMessage(Message);
    if (Message) return;
    setChannels(Next_channels);
  };

  /***********************************************
   * @brief : 将单个通道的勾选变化提交到统一选择入口
   * @param : Key 当前切换的通道键
   * @return: 无
   * @date  : 2026-09-28
   * @author: L
   ************************************************/
  const Toggle_channel = (Key: keyof Telemetry_t) => {
    Set_channel_selection(Channels
      .filter((Channel) => Channel.key === Key ? !Channel.visible : Channel.visible)
      .map((Channel) => Channel.key));
  };

  /***********************************************
   * @brief : 校验新串口配置能否承载已选通道，超额时保留原配置并提示
   * @param : Patch 用户修改的串口配置项
   * @return: 无
   * @date  : 2026-09-28
   * @author: L
   ************************************************/
  const Update_serial_config = (Patch: Partial<Serial_config_t>) => {
    const Next_config = { ...Serial_config, ...Patch };
    const Message = Get_channel_selection_error(Channels, Next_config);
    setChannelMessage(Message);
    if (Message) return;
    setSerialConfig(Next_config);
  };
  const Connected = Connection_state === "online";

  return (
    <div className="app-shell single-page-shell">
      <aside className="app-sidebar">
        <div className="brand-block" title="FOC_L 电机调试台">
          <div className="brand-mark"><CircleGauge size={23} strokeWidth={2.2} /></div>
          <div className="brand-name" aria-hidden="true"><strong>FOC_L</strong><span>电机调试台</span></div>
        </div>
        <nav className="main-navigation" aria-label="主菜单">
          {Navigation_items.map((Item) => {
            const Icon = Item.icon;
            return (
              <button
                key={Item.id}
                type="button"
                className={Active_page === Item.id ? "active" : ""}
                aria-current={Active_page === Item.id ? "page" : undefined}
                aria-label={Item.label}
                data-menu-label={Item.label}
                onClick={() => setActivePage(Item.id)}
              >
                <Icon size={22} strokeWidth={2} />
                {Active_page === Item.id && <i />}
              </button>
            );
          })}
        </nav>
      </aside>

      <main className="main-content">
        <header className="topbar single-topbar">
          <div className="page-heading">
            <h1>{Page_titles[Active_page].title}</h1>
            <p>{Page_titles[Active_page].description}</p>
          </div>
          <div className="topbar-actions">
            <button
              className="theme-toggle"
              data-testid="theme-toggle"
              type="button"
              aria-label={Theme === "dark" ? "切换到浅色模式" : "切换到深色模式"}
              title={Theme === "dark" ? "切换到浅色模式" : "切换到深色模式"}
              onClick={() => setTheme((Current) => Current === "dark" ? "light" : "dark")}
            >
              {Theme === "dark" ? <Sun size={17} /> : <Moon size={17} />}
            </button>
            <label className="topbar-port-select" title="选择串口">
              <Cable size={16} />
              <select aria-label="选择串口" value={Serial_config.port} disabled={Connected || Connection_state === "connecting"} onChange={(Event) => Update_serial_config({ port: Event.target.value })}>
                {!Serial_ports.some((Port) => Port.path === Serial_config.port) && <option value={Serial_config.port}>{Serial_config.port} · 未检测到</option>}
                {Serial_ports.map((Port) => <option key={Port.path} value={Port.path}>{Port.path === "simulator" ? "FOC 串口仿真设备" : `${Port.path}${Port.friendlyName ? ` · ${Port.friendlyName}` : Port.manufacturer ? ` · ${Port.manufacturer}` : ""}`}</option>)}
              </select>
            </label>
            <div className="connection-summary"><StatusBadge state={Connection_state} /><div><b>{Connected ? (Serial_config.port === "simulator" ? "FOC 串口仿真设备" : Serial_config.port) : "未连接设备"}</b><span>{Connected ? `${Serial_config.baudRate} bit/s · ${Serial_config.dataBits}-${Serial_config.parity === "none" ? "N" : Serial_config.parity === "even" ? "E" : "O"}-${Serial_config.stopBits}` : Serial_config.port === "simulator" ? "仿真设备已选择" : `${Serial_config.port} 已选择`}</span></div></div>
            <button className={Connected ? "connect-button connected" : "connect-button"} disabled={Connection_state === "connecting"} onClick={Connect}>{Connected ? <Unplug size={17} /> : <PlugZap size={17} />}{Connected ? "断开" : Connection_state === "connecting" ? "连接中" : "连接"}</button>
            {Command.emergencyStopped
              ? <button className="reset-estop-button" onClick={(Event) => { if (Event.detail > 0) Reset_emergency(); }}><RotateCcw size={17} />复位急停</button>
              : <button className="estop-button" onClick={Emergency_stop}><Power size={18} />紧急停机</button>}
            <button
              className="board-reset-button"
              disabled={!Connected || Serial_config.port === "simulator" || Board_resetting}
              onClick={() => void Reset_board()}
              aria-label="复位驱动板"
              title="复位驱动板"
            >
              <RotateCcw size={17} />{Board_resetting ? "复位中" : "复位驱动板"}
            </button>
          </div>
        </header>

        <div className="content-scroll unified-scroll menu-page-scroll">
          {Channel_message && <div className="panel-heading"><p role="alert">{Channel_message}</p></div>}
          {Active_page === "workspace" && (
            <WorkspaceView telemetry={Telemetry} history={Scope_history} frozenSamples={Frozen_samples} sampleCount={Sample_count} channels={Channels} receivedChannelKeys={Received_channel_keys} command={Command} loopParameters={Loop_parameters} musicTracks={MusicTracks} connected={Connected} paused={Paused} logs={Logs} onCommand={Update_command} onLoopParameterChange={Update_loop_parameter} onChannel={Toggle_channel} onChannelSelection={Set_channel_selection} onPause={Toggle_scope_follow} onClear={Clear_scope_samples} onExport={Export_csv} onStart={Start_motor} onStop={Stop_motor} onZeroCalibration={() => void Zero_calibration()} />
          )}

          {Active_page === "communication" && (
            <section className="menu-page">
              <CommunicationView connected={Connected} frames={Frames} config={Serial_config} serialErrors={Serial_stats} onConfig={Update_serial_config} onRefreshPorts={() => void Refresh_serial_ports()} onSend={(Type, Data) => void Send_frame(Type, Data)} onClear={Clear_serial_frames} />
            </section>
          )}
        </div>

        <footer className="statusbar">
          <span><i className={Connected ? "status-light online" : "status-light"} />{Connected ? "遥测 40 Hz" : "无数据"}</span>
          <span><ArrowDownToLine size={13} />RX {Frames.filter((Frame) => Frame.direction === "RX").length}</span>
          <span><Send size={13} />TX {Frames.filter((Frame) => Frame.direction === "TX").length}</span>
          <span className="status-spacer" />
          <span><Database size={13} />缓冲 {Sample_count}/{Scope_sample_limit} 点/通道</span>
          <span><CircleOff size={13} />错误 0</span>
        </footer>
      </main>
    </div>
  );
}
