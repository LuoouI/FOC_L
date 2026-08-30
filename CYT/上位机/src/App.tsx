import { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import {
  Activity,
  AlertTriangle,
  ArrowDownToLine,
  Cable,
  CircleGauge,
  CircleOff,
  Database,
  Download,
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
  Position_return_mode_t,
  Serial_config_t,
  Serial_frame_t,
  Serial_port_info_t,
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
];

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
};

type Theme_mode_t = "dark" | "light";
type App_page_t = "workspace" | "communication";

interface Workspace_layout_t {
  workbenchHeight: number;
  controlWidth: number;
  channelWidth: number;
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
const Workspace_layout_storage_key = "foc-workspace-layout";
const Control_selection_storage_key = "foc-control-selection";
const Zero_calibration_frame_type = 0x15;
const Previous_default_workspace_layout: Workspace_layout_t = {
  workbenchHeight: 560,
  controlWidth: 330,
  channelWidth: 320,
};
const Default_workspace_layout: Workspace_layout_t = {
  workbenchHeight: 560,
  controlWidth: 231,
  channelWidth: 224,
};
const Default_serial_config: Serial_config_t = {
  port: "simulator",
  baudRate: 115200,
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
    return {
      driveMode: Valid_drive_mode.includes(Saved_drive_mode as Drive_mode_t)
        ? Saved_drive_mode as Drive_mode_t
        : Default_control_selection.driveMode,
      mode: Valid_mode.includes(Saved_selection.mode as Control_mode_t)
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
    const Saved_control_width = Number(Saved_layout.controlWidth);
    const Saved_channel_width = Number(Saved_layout.channelWidth);
    return {
      workbenchHeight: Number.isFinite(Saved_layout.workbenchHeight)
        ? Math.min(Math.max(Number(Saved_layout.workbenchHeight), 380), 900)
        : Default_workspace_layout.workbenchHeight,
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

const Format_value = (Value: number, Digits = 1) => Number.isFinite(Value) ? Value.toFixed(Digits) : "--";

/***********************************************
 * @brief : 将仿真遥测打包成与串口协议一致的负载
 * @param : Telemetry 电机遥测数据
 * @return: 56 字节遥测负载
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function buildSimulationTelemetryPayload(Telemetry: Telemetry_t): number[] {
  const Payload_buffer = new ArrayBuffer(56);
  const Payload_view = new DataView(Payload_buffer);
  Payload_view.setUint8(0, 1);
  Payload_view.setUint8(1, 2);
  Payload_view.setUint8(2, 0);
  Payload_view.setUint8(3, 0);
  Payload_view.setUint32(4, Math.round(Telemetry.timestamp * 1000), true);
  Payload_view.setFloat32(8, Telemetry.speedTarget, true);
  Payload_view.setFloat32(12, Telemetry.speedActual, true);
  Payload_view.setFloat32(16, Telemetry.idTarget, true);
  Payload_view.setFloat32(20, Telemetry.idActual, true);
  Payload_view.setFloat32(24, Telemetry.iqTarget, true);
  Payload_view.setFloat32(28, Telemetry.iqActual, true);
  Payload_view.setFloat32(32, Telemetry.busVoltage, true);
  Payload_view.setFloat32(36, Telemetry.zeroOffset, true);
  Payload_view.setFloat32(40, Telemetry.mechanicalAngle, true);
  Payload_view.setFloat32(44, Telemetry.electricalAngle, true);
  Payload_view.setFloat32(48, Telemetry.torque, true);
  Payload_view.setUint16(52, Telemetry.adcRawU, true);
  Payload_view.setUint16(54, Telemetry.adcRawW, true);
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
  onChange: (Value: number) => void;
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
 * @brief : 显示带单位的数值输入控件
 * @param : label 参数名称
 * @param : value 当前数值
 * @param : unit 参数单位
 * @return: 数值输入组件
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function NumericInput({ label, value, unit, minimum, maximum, step, disabled, deferred, fieldClassName, onChange }: Numeric_input_props_t) {
  const [Draft_value, setDraftValue] = useState(() => Format_numeric_input_value(value));
  const Editing_ref = useRef(false);
  const Cancel_commit_ref = useRef(false);

  useEffect(() => {
    if (!Editing_ref.current) setDraftValue(Format_numeric_input_value(value));
  }, [value]);

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

    const Limited_value = Math.min(
      maximum ?? Number.POSITIVE_INFINITY,
      Math.max(minimum ?? Number.NEGATIVE_INFINITY, Number_value));
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

  return (
    <label className={fieldClassName ? `field-block ${fieldClassName}` : "field-block"}>
      <span>{label}</span>
      <div className="input-with-unit">
        <input
          type="number"
          value={deferred ? Draft_value : value}
          min={minimum}
          max={maximum}
          step={step}
          disabled={disabled}
          onChange={Handle_change}
          onFocus={() => { Editing_ref.current = true; }}
          onBlur={deferred ? Commit_value : undefined}
          onKeyDown={Handle_key_down}
        />
        <b>{unit}</b>
      </div>
    </label>
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
  const [Scope_expanded, setScopeExpanded] = useState(false);
  const [Clear_confirm_open, setClearConfirmOpen] = useState(false);
  const [Workspace_layout, setWorkspaceLayout] = useState<Workspace_layout_t>(Get_initial_workspace_layout);
  const Workbench_height = Workspace_layout.workbenchHeight;
  const Control_width = Workspace_layout.controlWidth;
  const Channel_width = Workspace_layout.channelWidth;
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
      : command.mode === "speed"
        ? ["speedTarget", "speedActual", "iqTarget", "iqActual", "busVoltage"]
        : command.mode === "current"
          ? ["idTarget", "idActual", "iqTarget", "iqActual", "ud", "uq"]
          : command.mode === "voltage"
            ? ["idActual", "iqActual", "ud", "uq"]
            : ["mechanicalAngle", "electricalAngle", "speedActual", "iqActual"];
  const Received_key_set = useMemo(() => new Set(receivedChannelKeys), [receivedChannelKeys]);
  const Received_channels = useMemo(() => channels.filter((Channel) => Received_key_set.has(Channel.key)), [Received_key_set, channels]);
  const Recommended_received_channels = Recommended_channels.filter((Key) => Received_key_set.has(Key));
  const Is_foc_drive_mode = command.driveMode === "encoderFoc" || command.driveMode === "sensorlessFoc";

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

  const Handle_clear_scope = () => {
    onClear();
    setClearConfirmOpen(false);
  };

  const Handle_scope_resize_move = (Event: React.PointerEvent<HTMLDivElement>) => {
    const Resize_state = Scope_resize_ref.current;
    if (!Resize_state || Resize_state.pointerId !== Event.pointerId) return;
    const Maximum_height = Math.max(560, Math.min(window.innerHeight - 80, 900));
    const Next_height = Math.min(Math.max(Resize_state.startHeight + Event.clientY - Resize_state.startY, 380), Maximum_height);
    setWorkspaceLayout((Current) => ({ ...Current, workbenchHeight: Math.round(Next_height) }));
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
    setWorkspaceLayout((Current) => ({ ...Current, channelWidth: Math.round(Next_width) }));
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
      setWorkspaceLayout((Current) => ({ ...Current, channelWidth: Math.round(Next_width) }));
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
    const Next_width = Math.min(Math.max(Resize_state.startWidth + Event.clientX - Resize_state.startX, 220), 480);
    setWorkspaceLayout((Current) => ({ ...Current, controlWidth: Math.round(Next_width) }));
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
      const Next_width = Math.min(Math.max(Start_width + Move_event.clientX - Start_x, 220), 480);
      setWorkspaceLayout((Current) => ({ ...Current, controlWidth: Math.round(Next_width) }));
    };
    const Handle_mouse_up = () => {
      window.removeEventListener("mousemove", Handle_mouse_move);
      window.removeEventListener("mouseup", Handle_mouse_up);
    };
    window.addEventListener("mousemove", Handle_mouse_move);
    window.addEventListener("mouseup", Handle_mouse_up);
  };

  return (
    <div className="workspace-view">
      <div className="telemetry-grid">
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
              <p>{command.enabled ? "PWM 输出已使能" : "PWM 输出已关闭"}</p>
            </div>
            <span className={command.enabled ? "run-indicator active" : "run-indicator"}><i />{command.enabled ? "运行" : "待机"}</span>
          </div>

          <div className="drive-mode-layout" role="group" aria-label="驱动模式与零点校准">
            <div className="mode-switch drive-mode-switch">
              {(["openLoop", "foc_voice", "encoderFoc", "sensorlessFoc"] as Drive_mode_t[]).map((Mode) => (
                <button key={Mode} disabled={command.enabled} className={command.driveMode === Mode ? "active" : ""} onClick={() => onCommand({ driveMode: Mode })}>{Drive_mode_labels[Mode]}</button>
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

          {Is_foc_drive_mode && (
            <div className="mode-switch foc-mode-switch" role="tablist" aria-label="FOC 控制模式">
              {Foc_control_modes.map((Mode) => (
                <button key={Mode} disabled={command.enabled} className={command.mode === Mode ? "active" : ""} onClick={() => onCommand({ mode: Mode })}>{Mode_labels[Mode]}</button>
              ))}
            </div>
          )}

          <div className="command-fields">
            {command.driveMode === "openLoop" && <>
              <NumericInput label="Uq" value={command.voltageTarget} unit="V" step={0.1} minimum={-60} maximum={60} onChange={(Value) => onCommand({ voltageTarget: Value })} />
              <NumericInput label="Angle" value={command.angleTarget} unit="0~32767" step={1} minimum={0} maximum={32767} onChange={(Value) => onCommand({ angleTarget: Value })} />
              <NumericInput label="角度步进" value={command.angleStep} unit="计数/周期" step={1} minimum={-1000} maximum={1000} onChange={(Value) => onCommand({ angleStep: Value })} />
            </>}
            {Is_foc_drive_mode && command.mode === "current" && <>
              <NumericInput label="Iq ref" value={command.iqTarget} unit="A" step={0.1} minimum={-100} maximum={100} onChange={(Value) => onCommand({ iqTarget: Value })} />
              <NumericInput label="Id ref" value={command.idTarget} unit="A" step={0.1} minimum={-50} maximum={50} onChange={(Value) => onCommand({ idTarget: Value })} />
              <NumericInput label="带宽" value={loopParameters.currentBandwidth} unit="Hz" step={50} minimum={1} maximum={5000} disabled={!connected || command.enabled} deferred fieldClassName="bandwidth-field" onChange={(Value) => onLoopParameterChange("currentBandwidth", Value)} />
            </>}
            {Is_foc_drive_mode && command.mode === "speed" && <>
              <NumericInput label="Speed ref" value={command.speedTarget} unit="rpm" step={100} minimum={-50000} maximum={50000} onChange={(Value) => onCommand({ speedTarget: Value })} />
              <NumericInput label="速度斜率" value={command.rampRate} unit="rpm/s" step={100} minimum={1} maximum={100000} onChange={(Value) => onCommand({ rampRate: Value })} />
              <div className="loop-parameter-grid speed-loop-parameter-grid">
                <NumericInput label="KP" value={loopParameters.speedKp} unit="" step={0.001} minimum={0} maximum={100} disabled={!connected || command.enabled} deferred onChange={(Value) => onLoopParameterChange("speedKp", Value)} />
                <NumericInput label="KI" value={loopParameters.speedKi} unit="" step={0.001} minimum={0} maximum={100} disabled={!connected || command.enabled} deferred onChange={(Value) => onLoopParameterChange("speedKi", Value)} />
                <NumericInput label="积分项限幅" value={loopParameters.speedIntegralLimit} unit="A" step={0.1} minimum={0} maximum={5} disabled={!connected || command.enabled} deferred onChange={(Value) => onLoopParameterChange("speedIntegralLimit", Value)} />
                <NumericInput label="AB滤波带宽" value={loopParameters.abFilterBandwidth} unit="Hz" step={1} minimum={1} maximum={500} disabled={!connected || command.enabled} deferred fieldClassName="bandwidth-field" onChange={(Value) => onLoopParameterChange("abFilterBandwidth", Value)} />
              </div>
            </>}
            {Is_foc_drive_mode && command.mode === "position" && <>
              <NumericInput label="Position ref" value={command.positionTarget} unit="°" step={1} minimum={0} maximum={360} onChange={(Value) => onCommand({ positionTarget: Value })} />
              <div className="loop-parameter-grid">
                <NumericInput label="KP" value={loopParameters.positionKp} unit="" step={0.001} minimum={0} maximum={100} disabled={!connected || command.enabled} deferred onChange={(Value) => onLoopParameterChange("positionKp", Value)} />
                <NumericInput label="输出限幅" value={loopParameters.positionOutputLimit} unit="rpm" step={100} minimum={0} maximum={30000} disabled={!connected || command.enabled} deferred onChange={(Value) => onLoopParameterChange("positionOutputLimit", Value)} />
                <NumericInput label="角度死区" value={loopParameters.positionDeadband} unit="°" step={0.1} minimum={0} maximum={180} disabled={!connected || command.enabled} deferred onChange={(Value) => onLoopParameterChange("positionDeadband", Value)} />
                <NumericInput label="软化范围" value={loopParameters.positionSoftRange} unit="°" step={0.1} minimum={0} maximum={180} disabled={!connected || command.enabled} deferred onChange={(Value) => onLoopParameterChange("positionSoftRange", Value)} />
                <NumericInput label="到位速度死区" value={loopParameters.positionSpeedDeadband} unit="rpm" step={0.1} minimum={0} maximum={100} disabled={!connected || command.enabled} deferred onChange={(Value) => onLoopParameterChange("positionSpeedDeadband", Value)} />
              </div>
            </>}
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
            title="拖动调整示波器高度，双击恢复默认高度"
            onDoubleClick={() => setWorkspaceLayout((Current) => ({ ...Current, workbenchHeight: Default_workspace_layout.workbenchHeight }))}
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
            <div><h2>观测通道</h2><p>{Received_channels.filter((Channel) => Channel.visible).length} / {Received_channels.length} 已选择</p></div>
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
                <b>{Format_value(Number(telemetry[Channel.key]), Channel.unit === "rpm" ? 0 : 2)} {Channel.unit}</b>
              </label>
            ))}
          </div>
        </section>
      </div>

      <section className="event-strip">
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
  const Type_labels: Record<number, string> = { 0x01: "握手", 0x02: "心跳", 0x10: "控制", 0x11: "读参数", 0x12: "写参数", 0x14: "乐曲列表", 0x15: "零点校准", 0x20: "遥测", 0x21: "波形", 0x30: "故障", 0x31: "日志" };

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
  const Scope_history_ref = useRef<Telemetry_history_t | null>(null);
  if (Scope_history_ref.current === null) Scope_history_ref.current = new Telemetry_history_t(Scope_sample_limit);
  const Scope_history = Scope_history_ref.current;
  const Simulator_ref = useRef(new Motor_simulator_t());
  const Telemetry_ref = useRef<Telemetry_t>(Empty_telemetry);
  const Command_ref = useRef(Command);
  const Control_selection_ref = useRef<Control_selection_t>({
    driveMode: Command.driveMode,
    mode: Command.mode,
    direction: Command.direction,
    positionReturnMode: Command.positionReturnMode,
  });
  const Connection_state_ref = useRef(Connection_state);
  const Music_play_seen_ref = useRef(false);
  const Last_waveform_receive_ref = useRef(0);
  const Pending_frames_ref = useRef<Serial_frame_t[]>([]);
  const Frame_counter_ref = useRef(0);
  const Log_id_ref = useRef(2);

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
  useEffect(() => { Connection_state_ref.current = Connection_state; }, [Connection_state]);

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
    const Incoming_channel_keys = Scope_channels
      .filter((Channel) => Data_list.some((Data) => Object.prototype.hasOwnProperty.call(Data, Channel.key)))
      .map((Channel) => Channel.key);
    setReceivedChannelKeys((Current) => {
      const New_keys = Incoming_channel_keys.filter((Key) => !Current.includes(Key));
      if (New_keys.length === 0) return Current;
      const Received_key_set = new Set([...Current, ...New_keys]);
      return Scope_channels.filter((Channel) => Received_key_set.has(Channel.key)).map((Channel) => Channel.key);
    });

    const Samples = Data_list.map((Data) => {
      const Next = { ...Telemetry_ref.current, ...Data } as Telemetry_t;
      Telemetry_ref.current = Next;
      return Next;
    });
    if (High_speed) {
      Last_waveform_receive_ref.current = performance.now();
      Scope_history.appendBatch(Samples);
      return;
    }
    if (performance.now() - Last_waveform_receive_ref.current >= 200) Scope_history.appendBatch(Samples);
  }, [Scope_history]);

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
    }, 50);
    return () => window.clearInterval(Refresh_timer);
  }, [Scope_history]);

  const Add_log = useCallback((Level: Event_log_t["level"], Source: string, Message: string) => {
    const New_log: Event_log_t = { id: Log_id_ref.current++, timestamp: new Date(), level: Level, source: Source, message: Message };
    setLogs((Current) => [New_log, ...Current].slice(0, 300));
  }, []);

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
        void window.focBridge.sendControl(Next as unknown as Record<string, unknown>).then((Result) => {
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

  const Refresh_serial_ports = useCallback(async () => {
    if (!window.focBridge) return;
    try {
      const Ports = await window.focBridge.listSerialPorts();
      setSerialPorts(Ports);
      Add_log("info", "串口", `检测到 ${Math.max(Ports.length - 1, 0)} 个物理 COM 口`);
    } catch (Caught_error) {
      Add_log("error", "串口", `COM 口扫描失败：${Caught_error instanceof Error ? Caught_error.message : String(Caught_error)}`);
    }
  }, [Add_log]);

  useEffect(() => {
    void Refresh_serial_ports();
  }, [Refresh_serial_ports]);

  useEffect(() => {
    if (!window.focBridge) return;
    const Remove_frame = window.focBridge.onSerialFrame((Frame) => {
      const New_frame: Serial_frame_t = {
        type: Frame.type,
        sequence: Frame.sequence,
        timestamp: (Frame.timestamp % 86400000) / 1000,
        direction: "RX",
        length: Frame.data.length,
        data: Frame.data,
      };
      Pending_frames_ref.current.push(New_frame);
    });
    const Remove_telemetry = window.focBridge.onSerialTelemetry((Data) => {
      const Current_command = Command_ref.current;
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
      const Synced_parameters = Parameters as unknown as Foc_loop_parameters_t & { rampRate: number };
      const { rampRate: Synced_ramp_rate, ...Synced_loop_parameters } = Synced_parameters;
      setLoopParameters(Synced_loop_parameters);
      setCommand((Current) => ({
        ...Current,
        currentBandwidth: Synced_parameters.currentBandwidth,
        rampRate: Synced_ramp_rate,
      }));
      Add_log("info", "参数", "已从下位机同步FOC环路参数和速度斜率");
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

  const Update_command = (Patch: Partial<Motor_command_t>) => {
    Remember_control_selection(Patch);
    setCommand((Current) => {
      const Next = { ...Current, ...Patch };
      if (Connection_state === "online" && Serial_config.port !== "simulator" && window.focBridge) {
        void window.focBridge.sendControl(Next as unknown as Record<string, unknown>);
        if (Patch.rampRate !== undefined && !Next.enabled) {
          void window.focBridge.sendParameters({
            ...Loop_parameters,
            rampRate: Next.rampRate,
          }).then((Result) => {
            if (!Result.ok) Add_log("error", "参数", Result.message);
          });
        }
      }
      return Next;
    });
  };

  /***********************************************
   * @brief : 更新并同步FOC三套控制环和AB滤波器参数
   * @param : Key 参数名称
   * @param : Value 参数值
   * @return: 无
   * @date  : 2026-08-29
   * @author: L
   ************************************************/
  const Update_loop_parameter = (Key: keyof Foc_loop_parameters_t, Value: number) => setLoopParameters((Current) => {
    const Next = { ...Current, [Key]: Value };
    if (Connection_state === "online" && Serial_config.port !== "simulator" && window.focBridge) {
      void window.focBridge.sendParameters({
        ...Next,
        rampRate: Command_ref.current.rampRate,
      }).then((Result) => {
        if (!Result.ok) Add_log("error", "参数", Result.message);
      });
    }
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
      Update_command({ enabled: false });
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
      Add_log("warning", "串口", `${Serial_config.port === "simulator" ? "仿真设备" : Serial_config.port}已断开，PWM 输出已关闭`);
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
        const Synced_ramp_rate = Number(Result.parameters?.rampRate);
        /* 首帧保持失能，仅用于让下位机建立控制心跳和遥测通道。 */
        const Startup_command: Motor_command_t = {
          ...Initial_command,
          ...Control_selection_ref.current,
          focVoiceSongId: Command.focVoiceSongId,
          rampRate: Number.isFinite(Synced_ramp_rate) ? Synced_ramp_rate : Initial_command.rampRate,
        };
        const Startup_result = await window.focBridge.sendControl(
          Startup_command as unknown as Record<string, unknown>);
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

  const Emergency_stop = () => {
    Update_command({ enabled: false, emergencyStopped: true, speedTarget: 0, iqTarget: 0 });
    Add_log("error", "安全", "用户触发紧急停机，所有输出已关闭");
  };

  /* 小键盘回车仅用于触发急停，复位必须重新使用鼠标点按。 */
  useEffect(() => {
    const Handle_emergency_key_down = (Event: KeyboardEvent) => {
      if (Event.code !== "NumpadEnter") return;
      Event.preventDefault();
      if (Event.repeat || Command.emergencyStopped) return;
      Emergency_stop();
    };

    window.addEventListener("keydown", Handle_emergency_key_down);
    return () => window.removeEventListener("keydown", Handle_emergency_key_down);
  }, [Command.emergencyStopped, Emergency_stop]);

  const Reset_emergency = () => {
    Update_command({ emergencyStopped: false });
    Add_log("warning", "安全", "急停状态已人工复位");
  };

  const Start_motor = () => {
    if (Command.driveMode === "foc_voice") {
      Music_play_seen_ref.current = false;
      Update_command({ enabled: true, focVoiceSession: (Command.focVoiceSession + 1) >>> 0 });
    } else {
      Update_command({ enabled: true });
    }
    const Active_mode = (Command.driveMode === "encoderFoc" || Command.driveMode === "sensorlessFoc")
      ? `${Drive_mode_labels[Command.driveMode]} · ${Mode_labels[Command.mode]}`
      : Command.driveMode === "foc_voice"
        ? MusicTracks.find((Track) => Track.id === Command.focVoiceSongId)?.name ?? Drive_mode_labels[Command.driveMode]
        : Drive_mode_labels[Command.driveMode];
    Add_log("success", "电机", `${Active_mode}已启动`);
  };

  /***********************************************
   * @brief : 停止电机输出并保留当前控制模式选择
   * @return: 无
   * @date  : 2026-08-29
   * @author: L
   ************************************************/
  const Stop_motor = () => {
    /* 停止只撤销使能，模式、方向和回正方式继续保留给下一次启动使用。 */
    Music_play_seen_ref.current = false;
    Update_command({
      enabled: false,
      driveMode: Control_selection_ref.current.driveMode,
      mode: Control_selection_ref.current.mode,
      direction: Control_selection_ref.current.direction,
      positionReturnMode: Control_selection_ref.current.positionReturnMode,
    });
    Add_log("info", "电机", "电机已停止，PWM 输出关闭");
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
    const Sequence = Frame_counter_ref.current++;
    const New_frame: Serial_frame_t = { type: Type, sequence: Sequence, timestamp: Telemetry.timestamp, direction: "TX", length: Data.length, data: Data };
    setFrames((Current) => [New_frame, ...Current].slice(0, 500));
    if (window.focBridge) {
      const Result = await window.focBridge.sendSerialFrame(Type, Data);
      Add_log(Result.ok ? "info" : "error", "串口", Result.ok ? `已发送 0x${Type.toString(16).toUpperCase()} 类型帧` : Result.message);
      return Result.ok;
    } else {
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

  const Toggle_channel = (Key: keyof Telemetry_t) => setChannels((Current) => Current.map((Channel) => Channel.key === Key ? { ...Channel, visible: !Channel.visible } : Channel));
  const Set_channel_selection = (Keys: Array<keyof Telemetry_t>) => {
    const Selected_keys = new Set(Keys);
    setChannels((Current) => Current.map((Channel) => ({ ...Channel, visible: Selected_keys.has(Channel.key) })));
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
              <select aria-label="选择串口" value={Serial_config.port} disabled={Connected || Connection_state === "connecting"} onChange={(Event) => setSerialConfig((Current) => ({ ...Current, port: Event.target.value }))}>
                {!Serial_ports.some((Port) => Port.path === Serial_config.port) && <option value={Serial_config.port}>{Serial_config.port} · 未检测到</option>}
                {Serial_ports.map((Port) => <option key={Port.path} value={Port.path}>{Port.path === "simulator" ? "FOC 串口仿真设备" : `${Port.path}${Port.friendlyName ? ` · ${Port.friendlyName}` : Port.manufacturer ? ` · ${Port.manufacturer}` : ""}`}</option>)}
              </select>
            </label>
            <div className="connection-summary"><StatusBadge state={Connection_state} /><div><b>{Connected ? (Serial_config.port === "simulator" ? "FOC 串口仿真设备" : Serial_config.port) : "未连接设备"}</b><span>{Connected ? `${Serial_config.baudRate} bit/s · ${Serial_config.dataBits}-${Serial_config.parity === "none" ? "N" : Serial_config.parity === "even" ? "E" : "O"}-${Serial_config.stopBits}` : Serial_config.port === "simulator" ? "仿真设备已选择" : `${Serial_config.port} 已选择`}</span></div></div>
            <button className={Connected ? "connect-button connected" : "connect-button"} disabled={Connection_state === "connecting"} onClick={Connect}>{Connected ? <Unplug size={17} /> : <PlugZap size={17} />}{Connected ? "断开" : Connection_state === "connecting" ? "连接中" : "连接"}</button>
            {Command.emergencyStopped
              ? <button className="reset-estop-button" onClick={(Event) => { if (Event.detail > 0) Reset_emergency(); }}><RotateCcw size={17} />复位急停</button>
              : <button className="estop-button" onClick={Emergency_stop}><Power size={18} />紧急停机</button>}
          </div>
        </header>

        <div className="content-scroll unified-scroll menu-page-scroll">
          {Active_page === "workspace" && (
            <WorkspaceView telemetry={Telemetry} history={Scope_history} frozenSamples={Frozen_samples} sampleCount={Sample_count} channels={Channels} receivedChannelKeys={Received_channel_keys} command={Command} loopParameters={Loop_parameters} musicTracks={MusicTracks} connected={Connected} paused={Paused} logs={Logs} onCommand={Update_command} onLoopParameterChange={Update_loop_parameter} onChannel={Toggle_channel} onChannelSelection={Set_channel_selection} onPause={Toggle_scope_follow} onClear={Clear_scope_samples} onExport={Export_csv} onStart={Start_motor} onStop={Stop_motor} onZeroCalibration={() => void Zero_calibration()} />
          )}

          {Active_page === "communication" && (
            <section className="menu-page">
              <CommunicationView connected={Connected} frames={Frames} config={Serial_config} serialErrors={Serial_stats} onConfig={(Patch) => setSerialConfig((Current) => ({ ...Current, ...Patch }))} onRefreshPorts={() => void Refresh_serial_ports()} onSend={(Type, Data) => void Send_frame(Type, Data)} onClear={Clear_serial_frames} />
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
