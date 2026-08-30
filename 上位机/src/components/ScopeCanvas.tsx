import { memo, useCallback, useEffect, useMemo, useRef, useState, useSyncExternalStore } from "react";
import { ChevronLeft, ChevronRight, Minus, MoveHorizontal, MoveLeft, MoveVertical, Plus, RotateCcw } from "lucide-react";
import type { Telemetry_history_t } from "../telemetryHistory";
import type { Channel_definition_t, Telemetry_t } from "../types";

interface Scope_canvas_props_t {
  history: Telemetry_history_t;
  frozenSamples?: Telemetry_t[] | null;
  channels: Channel_definition_t[];
  paused: boolean;
  interactive?: boolean;
}

interface Scale_range_t {
  minimum: number;
  maximum: number;
}

interface Unit_plot_area_t {
  topRatio: number;
  heightRatio: number;
}

interface Scope_view_t {
  timeSpan: number;
  endTime: number | null;
  followPosition: number;
  verticalZoom: number;
  verticalOffset: number;
  follow: boolean;
}

interface Cursor_point_t {
  x: number;
  y: number;
}

interface Drag_state_t {
  pointerId: number;
  startX: number;
  startY: number;
  startEndTime: number;
  startLatestTime: number;
  startFollow: boolean;
  startVerticalOffset: number;
}

interface Draw_point_t {
  index: number;
  timestamp: number;
  value: number;
}

interface Draw_bucket_t {
  minimumIndex: number;
  maximumIndex: number;
  minimumValue: number;
  maximumValue: number;
}

interface Pending_pointer_t {
  pointerId: number;
  clientX: number;
  clientY: number;
  x: number;
  y: number;
  width: number;
  height: number;
}

const Initial_scope_view: Scope_view_t = {
  timeSpan: 5,
  endTime: null,
  followPosition: 1,
  verticalZoom: 1,
  verticalOffset: 0,
  follow: true,
};

const Minimum_auto_span = 0.001;
const Minimum_follow_position = 0.05;
const Minimum_time_span = 0.02;
const Default_maximum_time_span = 300;
const Minimum_vertical_zoom = 0.001;
const Maximum_vertical_zoom = 160;
const Scope_view_storage_key = "foc-scope-view";
const Empty_frozen_samples: Telemetry_t[] = [];

/***********************************************
 * @brief : 将数值限制在指定范围内
 * @param : Value 当前值
 * @param : Minimum 最小值
 * @param : Maximum 最大值
 * @return: 限制后的数值
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function Clamp(Value: number, Minimum: number, Maximum: number) {
  return Math.min(Math.max(Value, Minimum), Maximum);
}

/***********************************************
 * @brief : 计算纵向缩放后保持鼠标锚点所需的偏移量
 * @param : Current_zoom 当前纵向倍率
 * @param : Current_offset 当前纵向偏移量
 * @param : New_zoom 缩放后的纵向倍率
 * @param : Cursor_ratio 鼠标在绘图区中的纵向比例
 * @return: 缩放后的纵向偏移量
 * @date  : 2026-08-30
 * @author: L
 ************************************************/
function Calculate_vertical_offset(
  Current_zoom: number,
  Current_offset: number,
  New_zoom: number,
  Cursor_ratio: number,
) {
  const Current_span_ratio = 1 / Math.max(Current_zoom, Minimum_vertical_zoom);
  const New_span_ratio = 1 / Math.max(New_zoom, Minimum_vertical_zoom);
  return Current_offset + (0.5 - Cursor_ratio) * (Current_span_ratio - New_span_ratio);
}

/***********************************************
 * @brief : 读取并校验上次保存的示波器视图
 * @return: 可直接使用的示波器视图
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function Get_initial_scope_view(): Scope_view_t {
  try {
    const Saved_text = window.localStorage.getItem(Scope_view_storage_key);
    if (!Saved_text) return { ...Initial_scope_view };
    const Saved_view = JSON.parse(Saved_text) as Partial<Scope_view_t>;
    return {
      timeSpan: Number.isFinite(Saved_view.timeSpan)
        ? Clamp(Number(Saved_view.timeSpan), Minimum_time_span, 86400)
        : Initial_scope_view.timeSpan,
      endTime: null,
      followPosition: Number.isFinite(Saved_view.followPosition)
        ? Clamp(Number(Saved_view.followPosition), Minimum_follow_position, 1)
        : Initial_scope_view.followPosition,
      verticalZoom: Number.isFinite(Saved_view.verticalZoom)
        ? Clamp(Number(Saved_view.verticalZoom), Minimum_vertical_zoom, Maximum_vertical_zoom)
        : Initial_scope_view.verticalZoom,
      verticalOffset: Number.isFinite(Saved_view.verticalOffset)
        ? Clamp(Number(Saved_view.verticalOffset), -8, 8)
        : Initial_scope_view.verticalOffset,
      follow: true,
    };
  } catch {
    return { ...Initial_scope_view };
  }
}

/***********************************************
 * @brief : 根据当前可见波形生成一次性纵向量程
 * @param : Samples 当前视窗内的采样数据
 * @param : Channels 可见通道
 * @return: 按单位分组的量程
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function Calculate_ranges(Samples: Telemetry_t[], Channels: Channel_definition_t[]) {
  const Unit_ranges = new Map<string, Scale_range_t>();
  Channels.forEach((Channel) => {
    Samples.forEach((Sample) => {
      const Value = Number(Sample[Channel.key]);
      if (!Number.isFinite(Value)) return;
      const Existing = Unit_ranges.get(Channel.unit);
      Unit_ranges.set(Channel.unit, {
        minimum: Math.min(Value, Existing?.minimum ?? Number.POSITIVE_INFINITY),
        maximum: Math.max(Value, Existing?.maximum ?? Number.NEGATIVE_INFINITY),
      });
    });
  });
  Unit_ranges.forEach((Range, Unit) => {
    const Absolute_value = Math.max(Math.abs(Range.minimum), Math.abs(Range.maximum), Minimum_auto_span);
    const Span = Math.max(Range.maximum - Range.minimum, Absolute_value * 0.08, Minimum_auto_span);
    const Margin = Span * 0.12;
    Unit_ranges.set(Unit, { minimum: Range.minimum - Margin, maximum: Range.maximum + Margin });
  });
  return Unit_ranges;
}

/***********************************************
 * @brief : 为不同单位的波形分配纵向显示区域
 * @param : Units 按开启先后排列的可见单位
 * @param : Primary_unit 主Y轴单位
 * @return: 各单位对应的纵向显示区域
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
function Build_unit_plot_areas(Units: string[], Primary_unit: string | undefined) {
  const Plot_areas = new Map<string, Unit_plot_area_t>();
  if (!Primary_unit || Units.length <= 1) {
    Units.forEach((Unit) => Plot_areas.set(Unit, { topRatio: 0, heightRatio: 1 }));
    return Plot_areas;
  }

  Plot_areas.set(Primary_unit, { topRatio: 0, heightRatio: 1 });
  const Secondary_units = Units.filter((Unit) => Unit !== Primary_unit);
  const Lane_gap = 0.02;
  const Lane_height = Math.max(0.045, Math.min(0.2, (0.46 - Lane_gap * Math.max(0, Secondary_units.length - 1)) / Secondary_units.length));
  Secondary_units.forEach((Unit, Index) => {
    Plot_areas.set(Unit, {
      topRatio: 0.035 + Index * (Lane_height + Lane_gap),
      heightRatio: Lane_height,
    });
  });
  return Plot_areas;
}

/***********************************************
 * @brief : 按画布像素合并密集采样并保留每列峰谷
 * @param : Samples 当前视窗内的采样数据
 * @param : Channel 当前绘制通道
 * @param : View_start_time 视窗起始时间
 * @param : Time_span 视窗时间跨度
 * @param : Plot_width 绘图区宽度
 * @return: 用于连线的抽稀采样点
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function Build_draw_points(
  Samples: Telemetry_t[],
  Channel: Channel_definition_t,
  View_start_time: number,
  Time_span: number,
  Plot_width: number,
) {
  const Maximum_points = Math.max(2, Math.floor(Plot_width) * 2);
  if (Samples.length <= Maximum_points) {
    const Draw_points: Draw_point_t[] = [];
    Samples.forEach((Sample, Index) => {
      const Value = Number(Sample[Channel.key]);
      if (Number.isFinite(Value)) Draw_points.push({ index: Index, timestamp: Sample.timestamp, value: Value });
    });
    return Draw_points;
  }

  const Bucket_count = Math.max(1, Math.floor(Plot_width));
  const Buckets = new Array<Draw_bucket_t | undefined>(Bucket_count);
  Samples.forEach((Sample, Index) => {
    const Value = Number(Sample[Channel.key]);
    if (!Number.isFinite(Value)) return;
    const Ratio = (Sample.timestamp - View_start_time) / Math.max(Time_span, 0.0001);
    const Bucket_index = Clamp(Math.floor(Ratio * Bucket_count), 0, Bucket_count - 1);
    const Bucket = Buckets[Bucket_index];
    if (!Bucket) {
      Buckets[Bucket_index] = {
        minimumIndex: Index,
        maximumIndex: Index,
        minimumValue: Value,
        maximumValue: Value,
      };
      return;
    }
    if (Value < Bucket.minimumValue) {
      Bucket.minimumValue = Value;
      Bucket.minimumIndex = Index;
    }
    if (Value > Bucket.maximumValue) {
      Bucket.maximumValue = Value;
      Bucket.maximumIndex = Index;
    }
  });

  const Draw_points: Draw_point_t[] = [];
  const Append_point = (Index: number) => {
    if (Draw_points[Draw_points.length - 1]?.index === Index) return;
    const Sample = Samples[Index];
    const Value = Number(Sample[Channel.key]);
    if (Number.isFinite(Value)) Draw_points.push({ index: Index, timestamp: Sample.timestamp, value: Value });
  };
  Append_point(0);
  Buckets.forEach((Bucket) => {
    if (!Bucket) return;
    Append_point(Math.min(Bucket.minimumIndex, Bucket.maximumIndex));
    Append_point(Math.max(Bucket.minimumIndex, Bucket.maximumIndex));
  });
  Append_point(Samples.length - 1);
  return Draw_points;
}

/***********************************************
 * @brief : 从冻结采样中提取指定时间范围
 * @param : Samples 冻结采样数组
 * @param : Start_time 起始时间
 * @param : End_time 结束时间
 * @return: 时间范围内的采样引用数组
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function Get_frozen_range(Samples: Telemetry_t[], Start_time: number, End_time: number) {
  const Find_boundary = (Timestamp: number, Include_equal: boolean) => {
    let Left = 0;
    let Right = Samples.length;
    while (Left < Right) {
      const Middle = Math.floor((Left + Right) / 2);
      const Move_right = Include_equal ? Samples[Middle].timestamp <= Timestamp : Samples[Middle].timestamp < Timestamp;
      if (Move_right) Left = Middle + 1;
      else Right = Middle;
    }
    return Left;
  };
  return Samples.slice(Find_boundary(Start_time, false), Find_boundary(End_time, true));
}

/***********************************************
 * @brief : 在有序采样中查找最接近目标时间的采样点
 * @param : Samples 当前视窗内的采样数据
 * @param : Timestamp 目标时间
 * @return: 最接近目标时间的采样点
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function Find_nearest_sample(Samples: Telemetry_t[], Timestamp: number) {
  if (Samples.length === 0) return undefined;
  let Left = 0;
  let Right = Samples.length - 1;
  while (Left < Right) {
    const Middle = Math.floor((Left + Right) / 2);
    if (Samples[Middle].timestamp < Timestamp) Left = Middle + 1;
    else Right = Middle;
  }
  const Current = Samples[Left];
  const Previous = Samples[Math.max(0, Left - 1)];
  return Math.abs(Previous.timestamp - Timestamp) <= Math.abs(Current.timestamp - Timestamp) ? Previous : Current;
}

/***********************************************
 * @brief : 格式化示波器坐标轴数值
 * @param : Value 坐标轴数值
 * @return: 适合紧凑显示的字符串
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function Format_axis_value(Value: number) {
  const Absolute_value = Math.abs(Value);
  if (Absolute_value >= 1000) return Value.toFixed(0);
  if (Absolute_value >= 10) return Value.toFixed(1);
  if (Absolute_value >= 1) return Value.toFixed(2);
  if (Absolute_value > 0 && Absolute_value < 0.001) return Value.toExponential(2);
  return Value.toFixed(3);
}

/***********************************************
 * @brief : 格式化每格时间
 * @param : Seconds 每格秒数
 * @return: 带时间单位的字符串
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function Format_time_base(Seconds: number) {
  if (Seconds < 1) return `${Math.round(Seconds * 1000)} ms`;
  return `${Seconds.toFixed(Seconds >= 10 ? 0 : 1)} s`;
}

/***********************************************
 * @brief : 格式化纵向放大倍率
 * @param : Zoom 当前纵向倍率
 * @return: 适合工具栏显示的倍率字符串
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function Format_vertical_zoom(Zoom: number) {
  if (Zoom < 0.01) return Zoom.toFixed(3);
  if (Zoom < 1) return Zoom.toFixed(2);
  if (Zoom < 10) return Zoom.toFixed(1);
  return Zoom.toFixed(0);
}

/***********************************************
 * @brief : 格式化相对时间坐标
 * @param : Seconds 相对波形止点的秒数
 * @param : Time_span 当前视窗总时长
 * @return: 带时间单位的字符串
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function Format_relative_time(Seconds: number, Time_span: number) {
  if (Time_span < 1) return `${Math.round(Seconds * 1000)} ms`;
  return `${Seconds.toFixed(Time_span < 10 ? 1 : 0)} s`;
}

/***********************************************
 * @brief : 将十六进制颜色转换为带透明度的颜色
 * @param : Color 通道颜色
 * @param : Alpha 透明度
 * @return: CSS rgba颜色字符串
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function Color_with_alpha(Color: string, Alpha: number) {
  const Match = Color.trim().match(/^#([0-9a-f]{6})$/i);
  if (!Match) return Color;
  const Red = Number.parseInt(Match[1].slice(0, 2), 16);
  const Green = Number.parseInt(Match[1].slice(2, 4), 16);
  const Blue = Number.parseInt(Match[1].slice(4, 6), 16);
  return `rgba(${Red}, ${Green}, ${Blue}, ${Alpha})`;
}

/***********************************************
 * @brief : 绘制可缩放、可平移的实时多通道电机波形
 * @param : history 遥测历史缓冲
 * @param : frozenSamples 暂停时保存的历史快照
 * @param : channels 通道定义
 * @param : paused 波形暂停状态
 * @param : interactive 是否显示交互控制栏
 * @return: 示波器画布组件
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
export const ScopeCanvas = memo(function ScopeCanvas({ history, frozenSamples = null, channels, paused, interactive = false }: Scope_canvas_props_t) {
  const Canvas_ref = useRef<HTMLCanvasElement>(null);
  const Drag_ref = useRef<Drag_state_t | null>(null);
  const Draw_canvas_ref = useRef<() => void>(() => undefined);
  const Draw_frame_ref = useRef<number | null>(null);
  const Pending_pointer_ref = useRef<Pending_pointer_t | null>(null);
  const Pointer_frame_ref = useRef<number | null>(null);
  const Manual_ranges_ref = useRef(new Map<string, Scale_range_t>());
  const Time_span_ref = useRef(Initial_scope_view.timeSpan);
  const Maximum_time_span_ref = useRef(Default_maximum_time_span);
  const View_end_time_ref = useRef(0);
  const Follow_position_ref = useRef(Initial_scope_view.followPosition);
  const Earliest_time_ref = useRef(0);
  const Latest_time_ref = useRef(0);
  const [View, setView] = useState<Scope_view_t>(() => interactive ? Get_initial_scope_view() : { ...Initial_scope_view });
  const [Cursor_point, setCursorPoint] = useState<Cursor_point_t | null>(null);
  const [Dragging, setDragging] = useState(false);
  const [Canvas_visible, setCanvasVisible] = useState(true);
  const [Visible_unit_order, setVisibleUnitOrder] = useState<string[]>([]);
  const Subscribe_history = useCallback((Listener: () => void) => paused ? () => undefined : history.subscribe(Listener), [history, paused]);
  const Get_history_revision = useCallback(() => paused ? 0 : history.getRevision(), [history, paused]);
  const History_revision = useSyncExternalStore(Subscribe_history, Get_history_revision, Get_history_revision);
  const Frozen_source = paused ? (frozenSamples ?? Empty_frozen_samples) : null;

  /***********************************************
   * @brief : 将多次绘制请求合并到同一个浏览器动画帧
   * @return: 无
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  const Request_draw = useCallback(() => {
    if (Draw_frame_ref.current !== null) return;
    Draw_frame_ref.current = window.requestAnimationFrame(() => {
      Draw_frame_ref.current = null;
      Draw_canvas_ref.current();
    });
  }, []);

  const Visible_channels = useMemo(() => channels.filter((Channel) => Channel.visible), [channels]);
  const Current_visible_units = useMemo(
    () => Array.from(new Set(Visible_channels.map((Channel) => Channel.unit))),
    [Visible_channels],
  );
  useEffect(() => {
    setVisibleUnitOrder((Previous_order) => {
      const Retained_units = Previous_order.filter((Unit) => Current_visible_units.includes(Unit));
      const Added_units = Current_visible_units.filter((Unit) => !Retained_units.includes(Unit));
      const Next_order = [...Retained_units, ...Added_units];
      const Order_unchanged = Next_order.length === Previous_order.length
        && Next_order.every((Unit, Index) => Unit === Previous_order[Index]);
      return Order_unchanged ? Previous_order : Next_order;
    });
  }, [Current_visible_units]);
  const Ordered_visible_units = useMemo(() => {
    const Retained_units = Visible_unit_order.filter((Unit) => Current_visible_units.includes(Unit));
    const Added_units = Current_visible_units.filter((Unit) => !Retained_units.includes(Unit));
    return [...Retained_units, ...Added_units];
  }, [Current_visible_units, Visible_unit_order]);
  const Primary_unit = Ordered_visible_units[0];
  const Latest_time = Frozen_source
    ? Frozen_source[Frozen_source.length - 1]?.timestamp ?? 0
    : history.getLast()?.timestamp ?? 0;
  const Earliest_time = Frozen_source
    ? Frozen_source[0]?.timestamp ?? Latest_time
    : history.getFirst()?.timestamp ?? Latest_time;
  const Sample_time_span = Math.max(Latest_time - Earliest_time, 0);
  const Maximum_time_span = Math.max(Default_maximum_time_span, Sample_time_span);
  const Effective_time_span = Math.min(View.timeSpan, Maximum_time_span);
  const Minimum_view_end_time = Math.min(Latest_time, Earliest_time + Effective_time_span);
  const Maximum_view_end_time = Latest_time + Effective_time_span * (1 - Minimum_follow_position);
  const Follow_end_time = Latest_time + Effective_time_span * (1 - View.followPosition);
  const View_end_time = paused
    ? Clamp(View.endTime ?? Follow_end_time, Minimum_view_end_time, Maximum_view_end_time)
    : View.follow
      ? Follow_end_time
      : Clamp(View.endTime ?? Latest_time, Minimum_view_end_time, Maximum_view_end_time);
  const View_start_time = View_end_time - Effective_time_span;
  const Visible_samples = useMemo(
    () => {
      if (!Canvas_visible) return [];
      if (Frozen_source) return Get_frozen_range(Frozen_source, View_start_time, View_end_time);
      return history.getRange(View_start_time, View_end_time);
    },
    [Canvas_visible, Frozen_source, History_revision, View_end_time, View_start_time, history],
  );
  Time_span_ref.current = Effective_time_span;
  Maximum_time_span_ref.current = Maximum_time_span;
  View_end_time_ref.current = View_end_time;
  Follow_position_ref.current = View.followPosition;
  Earliest_time_ref.current = Earliest_time;
  Latest_time_ref.current = Latest_time;

  useEffect(() => {
    if (!interactive) return;
    try {
      window.localStorage.setItem(Scope_view_storage_key, JSON.stringify({ ...View, endTime: null, follow: true }));
    } catch {
      // 本地存储不可用时仍允许当前窗口调整示波器视图
    }
  }, [View, interactive]);

  useEffect(() => {
    const Canvas = Canvas_ref.current;
    if (!Canvas || typeof IntersectionObserver === "undefined") return;
    const Visibility_observer = new IntersectionObserver(([Entry]) => setCanvasVisible(Entry.isIntersecting), { rootMargin: "120px" });
    Visibility_observer.observe(Canvas);
    return () => Visibility_observer.disconnect();
  }, []);

  useEffect(() => {
    const Canvas = Canvas_ref.current;
    if (!Canvas || !Canvas_visible) {
      Draw_canvas_ref.current = () => undefined;
      return;
    }
    const Context = Canvas.getContext("2d");
    if (!Context) {
      Draw_canvas_ref.current = () => undefined;
      return;
    }

    const Draw = () => {
      const Bounds = Canvas.getBoundingClientRect();
      const Pixel_ratio = window.devicePixelRatio || 1;
      const Width = Math.max(1, Math.floor(Bounds.width));
      const Height = Math.max(1, Math.floor(Bounds.height));
      if (Canvas.width !== Math.floor(Width * Pixel_ratio) || Canvas.height !== Math.floor(Height * Pixel_ratio)) {
        Canvas.width = Math.floor(Width * Pixel_ratio);
        Canvas.height = Math.floor(Height * Pixel_ratio);
      }

      Context.setTransform(Pixel_ratio, 0, 0, Pixel_ratio, 0, 0);
      Context.clearRect(0, 0, Width, Height);
      const Canvas_style = getComputedStyle(Canvas);
      const Scope_background = Canvas_style.getPropertyValue("--scope-background").trim() || "#0d1218";
      const Scope_background_edge = Canvas_style.getPropertyValue("--scope-background-edge").trim() || "#090d12";
      const Scope_grid = Canvas_style.getPropertyValue("--scope-grid").trim() || "rgba(133, 150, 170, 0.13)";
      const Scope_grid_major = Canvas_style.getPropertyValue("--scope-grid-major").trim() || "rgba(155, 177, 196, 0.24)";
      const Scope_label = Canvas_style.getPropertyValue("--scope-label").trim() || "#768394";
      const Scope_axis = Canvas_style.getPropertyValue("--scope-axis").trim() || "#778596";
      const Scope_cursor = Canvas_style.getPropertyValue("--scope-cursor").trim() || "rgba(222, 232, 240, 0.62)";
      const Scope_start_marker = Canvas_style.getPropertyValue("--scope-start-marker").trim() || "#4ecaa0";
      const Scope_stop_marker = Canvas_style.getPropertyValue("--scope-stop-marker").trim() || "#f0b35c";
      Context.fillStyle = Scope_background;
      Context.fillRect(0, 0, Width, Height);

      const Left = interactive ? 62 : 48;
      const Right = 14;
      const Top = 13;
      const Bottom = 29;
      const Plot_width = Math.max(1, Width - Left - Right);
      const Plot_height = Math.max(1, Height - Top - Bottom);

      const Background_gradient = Context.createLinearGradient(0, 0, 0, Height);
      Background_gradient.addColorStop(0, Scope_background);
      Background_gradient.addColorStop(1, Scope_background_edge);
      Context.fillStyle = Background_gradient;
      Context.fillRect(0, 0, Width, Height);

      for (let Index = 0; Index <= 10; Index += 1) {
        const X = Left + (Plot_width * Index) / 10;
        const Is_major = Index % 5 === 0;
        Context.beginPath();
        Context.strokeStyle = Is_major ? Scope_grid_major : Scope_grid;
        Context.lineWidth = Is_major ? 1.15 : 0.75;
        Context.setLineDash(Is_major ? [] : [2, 4]);
        Context.moveTo(X, Top);
        Context.lineTo(X, Top + Plot_height);
        Context.stroke();
      }
      for (let Index = 0; Index <= 8; Index += 1) {
        const Y = Top + (Plot_height * Index) / 8;
        const Is_major = Index % 4 === 0;
        Context.beginPath();
        Context.strokeStyle = Is_major ? Scope_grid_major : Scope_grid;
        Context.lineWidth = Is_major ? 1.15 : 0.75;
        Context.setLineDash(Is_major ? [] : [2, 4]);
        Context.moveTo(Left, Y);
        Context.lineTo(Left + Plot_width, Y);
        Context.stroke();
      }
      Context.setLineDash([]);

      Context.strokeStyle = Scope_grid_major;
      Context.lineWidth = 1;
      Context.strokeRect(Left + 0.5, Top + 0.5, Plot_width - 1, Plot_height - 1);

      const Missing_range_channels = Visible_channels.filter((Channel) => !Manual_ranges_ref.current.has(Channel.unit));
      const Initial_ranges = Missing_range_channels.length > 0
        ? Calculate_ranges(Visible_samples, Missing_range_channels)
        : new Map<string, Scale_range_t>();
      Initial_ranges.forEach((Range, Unit) => {
        if (!Manual_ranges_ref.current.has(Unit)) Manual_ranges_ref.current.set(Unit, Range);
      });
      const Display_ranges = new Map<string, Scale_range_t>();
      Visible_channels.forEach((Channel) => {
        const Base_range = Manual_ranges_ref.current.get(Channel.unit) ?? Initial_ranges.get(Channel.unit) ?? { minimum: -1, maximum: 1 };
        const Base_span = Math.max(Base_range.maximum - Base_range.minimum, 0.0001);
        const Display_span = Base_span / View.verticalZoom;
        const Center = (Base_range.maximum + Base_range.minimum) / 2 + View.verticalOffset * Base_span;
        Display_ranges.set(Channel.unit, {
          minimum: Center - Display_span / 2,
          maximum: Center + Display_span / 2,
        });
      });

      const Primary_channel = Visible_channels.find((Channel) => Channel.unit === Primary_unit) ?? Visible_channels[0];
      const Unit_plot_areas = Build_unit_plot_areas(Ordered_visible_units, Primary_channel?.unit);

      const Zero_units = new Set<string>();
      Visible_channels.forEach((Channel) => {
        if (Zero_units.has(Channel.unit)) return;
        const Range = Display_ranges.get(Channel.unit);
        const Plot_area = Unit_plot_areas.get(Channel.unit);
        if (!Range || !Plot_area || Range.minimum > 0 || Range.maximum < 0) return;
        Zero_units.add(Channel.unit);
        const Zero_ratio = (Range.maximum) / Math.max(Range.maximum - Range.minimum, 0.0001);
        const Zero_y = Top + Plot_height * (Plot_area.topRatio + Zero_ratio * Plot_area.heightRatio);
        Context.save();
        Context.strokeStyle = Color_with_alpha(Channel.color, 0.22);
        Context.lineWidth = 1;
        Context.setLineDash([3, 5]);
        Context.beginPath();
        Context.moveTo(Left, Zero_y);
        Context.lineTo(Left + Plot_width, Zero_y);
        Context.stroke();
        Context.restore();
      });

      const Primary_range = Primary_channel ? Display_ranges.get(Primary_channel.unit) : undefined;
      if (Primary_channel && Primary_range) {
        Context.fillStyle = Scope_axis;
        Context.font = "10px 'Microsoft YaHei', sans-serif";
        Context.textAlign = "right";
        Context.textBaseline = "middle";
        for (let Index = 0; Index <= 4; Index += 1) {
          const Ratio = Index / 4;
          const Y = Top + Plot_height * Ratio;
          const Value = Primary_range.maximum - (Primary_range.maximum - Primary_range.minimum) * Ratio;
          Context.fillText(Format_axis_value(Value), Left - 7, Y);
          Context.strokeStyle = Scope_axis;
          Context.lineWidth = 1;
          Context.beginPath();
          Context.moveTo(Left - 4, Y);
          Context.lineTo(Left, Y);
          Context.stroke();
        }
      }

      if (Visible_samples.length >= 2 && Visible_channels.length > 0) {
        Context.save();
        Context.beginPath();
        Context.rect(Left, Top, Plot_width, Plot_height);
        Context.clip();
        Visible_channels.forEach((Channel) => {
          const Range = Display_ranges.get(Channel.unit);
          const Plot_area = Unit_plot_areas.get(Channel.unit);
          if (!Range || !Plot_area) return;
          const Span = Math.max(Range.maximum - Range.minimum, 0.0001);
          const Draw_points = Build_draw_points(Visible_samples, Channel, View_start_time, Effective_time_span, Plot_width);
          if (Draw_points.length === 0) return;
          Context.beginPath();
          Draw_points.forEach((Point, Index) => {
            const X = Left + ((Point.timestamp - View_start_time) / Effective_time_span) * Plot_width;
            const Value_ratio = (Point.value - Range.minimum) / Span;
            const Y = Top + Plot_height * (Plot_area.topRatio + (1 - Value_ratio) * Plot_area.heightRatio);
            if (Index === 0) Context.moveTo(X, Y);
            else Context.lineTo(X, Y);
          });
          Context.strokeStyle = Channel.color;
          Context.lineWidth = 1.15;
          Context.lineJoin = "bevel";
          Context.lineCap = "butt";
          Context.stroke();
        });

        /* 用竖线标记当前可见波形的首点与末点，末点位置可随实时跟随线移动。 */
        const First_visible_sample = Visible_samples[0];
        const Last_visible_sample = Visible_samples[Visible_samples.length - 1];
        const Start_marker_x = Left + ((First_visible_sample.timestamp - View_start_time) / Effective_time_span) * Plot_width;
        const Stop_marker_x = Left + ((Last_visible_sample.timestamp - View_start_time) / Effective_time_span) * Plot_width;
        const Markers = [
          { x: Start_marker_x, label: "起点", color: Scope_start_marker, labelY: Top + Plot_height - 10 },
          {
            x: Stop_marker_x,
            label: "止点",
            color: Scope_stop_marker,
            labelY: Math.abs(Stop_marker_x - Start_marker_x) < 44 ? Top + Plot_height - 26 : Top + Plot_height - 10,
          },
        ];
        Markers.forEach((Marker) => {
          if (Marker.x < Left - 1 || Marker.x > Left + Plot_width + 1) return;
          Context.save();
          Context.strokeStyle = Marker.color;
          Context.lineWidth = 1.6;
          Context.setLineDash([]);
          Context.beginPath();
          Context.moveTo(Marker.x, Top);
          Context.lineTo(Marker.x, Top + Plot_height);
          Context.stroke();
          Context.fillStyle = Marker.color;
          Context.font = "600 9px 'Microsoft YaHei', sans-serif";
          Context.textBaseline = "middle";
          Context.textAlign = Marker.x < Left + 34 ? "left" : Marker.x > Left + Plot_width - 34 ? "right" : "center";
          const Marker_label_x = Clamp(Marker.x, Left + 4, Left + Plot_width - 4);
          Context.fillText(Marker.label, Marker_label_x, Marker.labelY);
          Context.restore();
        });
        Context.restore();
        const Legend_channels = Visible_channels.slice(0, 6);
        if (Legend_channels.length > 0) {
          Context.save();
          const Legend_width = Math.min(238, Math.max(158, Plot_width * 0.34));
          const Legend_x = Left + Plot_width - Legend_width - 8;
          const Legend_top = Top + 8;
          const Legend_height = 24 + Legend_channels.length * 19 + (Visible_channels.length > Legend_channels.length ? 16 : 0);
          Context.fillStyle = Color_with_alpha(Scope_background_edge, 0.86);
          Context.fillRect(Legend_x, Legend_top, Legend_width, Legend_height);
          Context.strokeStyle = Color_with_alpha(Scope_grid_major, 0.9);
          Context.lineWidth = 1;
          Context.strokeRect(Legend_x + 0.5, Legend_top + 0.5, Legend_width - 1, Legend_height - 1);
          Context.fillStyle = Scope_label;
          Context.font = "600 9px 'Microsoft YaHei', sans-serif";
          Context.textAlign = "left";
          Context.textBaseline = "middle";
          Context.fillText("实时值", Legend_x + 10, Legend_top + 12);
          let Legend_y = Legend_top + 31;
          Legend_channels.forEach((Channel) => {
            const Value = Number(Visible_samples[Visible_samples.length - 1][Channel.key]);
            const Text = `${Format_axis_value(Value)} ${Channel.unit}`;
            Context.strokeStyle = Channel.color;
            Context.lineWidth = 2;
            Context.beginPath();
            Context.moveTo(Legend_x + 10, Legend_y);
            Context.lineTo(Legend_x + 21, Legend_y);
            Context.stroke();
            Context.fillStyle = Scope_axis;
            Context.font = "600 10px 'Microsoft YaHei', sans-serif";
            Context.textAlign = "left";
            Context.fillText(Channel.label, Legend_x + 28, Legend_y, Math.max(34, Legend_width - 112));
            Context.textAlign = "right";
            Context.fillText(Text, Legend_x + Legend_width - 10, Legend_y);
            Legend_y += 19;
          });
          if (Visible_channels.length > Legend_channels.length) {
            Context.fillStyle = Scope_label;
            Context.textAlign = "left";
            Context.font = "9px 'Microsoft YaHei', sans-serif";
            Context.fillText(`+${Visible_channels.length - Legend_channels.length} 通道`, Legend_x + 28, Legend_y + 1);
          }
          Context.restore();
        }
      } else {
        Context.fillStyle = Scope_label;
        Context.font = "13px 'Microsoft YaHei', sans-serif";
        Context.textAlign = "center";
        Context.textBaseline = "middle";
        const Message = Visible_channels.length === 0 ? "请选择至少一个观测通道" : "等待遥测数据";
        Context.fillText(Message, Width / 2, Height / 2);
      }

      Context.fillStyle = Scope_axis;
      Context.font = "10px 'Microsoft YaHei', sans-serif";
      Context.textBaseline = "alphabetic";
      const Timeline_time = Visible_samples[Visible_samples.length - 1]?.timestamp ?? Latest_time;
      for (let Index = 0; Index <= 5; Index += 1) {
        const Ratio = Index / 5;
        const X = Left + Plot_width * Ratio;
        const Axis_time = View_start_time + Effective_time_span * Ratio;
        Context.textAlign = Index === 0 ? "left" : Index === 5 ? "right" : "center";
        Context.fillText(Format_relative_time(Axis_time - Timeline_time, Effective_time_span), X, Height - 8);
        Context.strokeStyle = Scope_axis;
        Context.lineWidth = 1;
        Context.beginPath();
        Context.moveTo(X, Top + Plot_height);
        Context.lineTo(X, Top + Plot_height + 4);
        Context.stroke();
      }
      Context.fillStyle = Scope_label;
      Context.textAlign = "right";
      Context.font = "9px 'Microsoft YaHei', sans-serif";
      Context.fillText("时间", Width - 8, Height - 22);

      if (interactive && Cursor_point && Cursor_point.x >= Left && Cursor_point.x <= Left + Plot_width && Cursor_point.y >= Top && Cursor_point.y <= Top + Plot_height && Visible_samples.length > 0) {
        const Cursor_time = View_start_time + ((Cursor_point.x - Left) / Plot_width) * Effective_time_span;
        const Cursor_sample = Find_nearest_sample(Visible_samples, Cursor_time);
        if (!Cursor_sample) return;
        const Cursor_x = Left + ((Cursor_sample.timestamp - View_start_time) / Effective_time_span) * Plot_width;
        Context.save();
        Context.strokeStyle = Scope_cursor;
        Context.lineWidth = 1;
        Context.setLineDash([4, 4]);
        Context.beginPath();
        Context.moveTo(Cursor_x, Top);
        Context.lineTo(Cursor_x, Top + Plot_height);
        Context.moveTo(Left, Cursor_point.y);
        Context.lineTo(Left + Plot_width, Cursor_point.y);
        Context.stroke();
        Context.restore();

        const Tooltip_channels = Visible_channels.slice(0, 6);
        const Tooltip_width = 196;
        const Tooltip_height = 29 + Tooltip_channels.length * 17 + (Visible_channels.length > 6 ? 16 : 0);
        const Tooltip_x = Clamp(Cursor_x + 12, Left + 4, Left + Plot_width - Tooltip_width - 4);
        const Tooltip_y = Clamp(Cursor_point.y + 12, Top + 4, Top + Plot_height - Tooltip_height - 4);
        Context.fillStyle = Canvas_style.getPropertyValue("--scope-tooltip").trim() || "rgba(20, 26, 32, 0.94)";
        Context.fillRect(Tooltip_x, Tooltip_y, Tooltip_width, Tooltip_height);
        Context.strokeStyle = Canvas_style.getPropertyValue("--scope-tooltip-border").trim() || "#3b4651";
        Context.strokeRect(Tooltip_x + 0.5, Tooltip_y + 0.5, Tooltip_width - 1, Tooltip_height - 1);
        Context.fillStyle = Scope_cursor;
        Context.font = "600 10px 'Microsoft YaHei', sans-serif";
        Context.textAlign = "left";
        Context.textBaseline = "middle";
        Context.fillText(`t = ${Cursor_sample.timestamp.toFixed(3)} s`, Tooltip_x + 10, Tooltip_y + 15);
        Tooltip_channels.forEach((Channel, Index) => {
          const Y = Tooltip_y + 34 + Index * 17;
          Context.fillStyle = Channel.color;
          Context.fillRect(Tooltip_x + 10, Y - 3, 7, 7);
          Context.fillStyle = Scope_cursor;
          Context.font = "10px 'Microsoft YaHei', sans-serif";
          Context.fillText(`${Channel.label}  ${Format_axis_value(Number(Cursor_sample[Channel.key]))} ${Channel.unit}`, Tooltip_x + 24, Y);
        });
        if (Visible_channels.length > 6) {
          Context.fillStyle = Scope_label;
          Context.fillText(`另有 ${Visible_channels.length - 6} 个通道`, Tooltip_x + 24, Tooltip_y + Tooltip_height - 10);
        }
      }

      if (paused) {
        Context.fillStyle = Scope_label;
        Context.font = "600 11px 'Microsoft YaHei', sans-serif";
        Context.textAlign = "right";
        Context.textBaseline = "top";
        Context.fillText("已暂停", Width - 20, 21);
      }
    };

    Draw_canvas_ref.current = Draw;
    Request_draw();
  }, [Canvas_visible, Cursor_point, Effective_time_span, Ordered_visible_units, Primary_unit, View, Visible_channels, Visible_samples, View_start_time, interactive, paused, Request_draw]);

  useEffect(() => {
    const Canvas = Canvas_ref.current;
    if (!Canvas) return;
    const Resize_observer = new ResizeObserver(Request_draw);
    const Theme_observer = new MutationObserver(Request_draw);
    Resize_observer.observe(Canvas);
    Theme_observer.observe(document.documentElement, { attributes: true, attributeFilter: ["data-theme"] });
    Request_draw();
    return () => {
      Resize_observer.disconnect();
      Theme_observer.disconnect();
      if (Draw_frame_ref.current !== null) window.cancelAnimationFrame(Draw_frame_ref.current);
      if (Pointer_frame_ref.current !== null) window.cancelAnimationFrame(Pointer_frame_ref.current);
      Draw_frame_ref.current = null;
      Pointer_frame_ref.current = null;
    };
  }, [Request_draw]);

  const Adjust_time_span = (Factor: number) => {
    setView((Current) => ({
      ...Current,
      timeSpan: Clamp(Number((Effective_time_span * Factor).toPrecision(3)), Minimum_time_span, Maximum_time_span),
    }));
  };

  const Adjust_vertical_zoom = (Factor: number) => {
    setView((Current) => ({ ...Current, verticalZoom: Clamp(Number((Current.verticalZoom * Factor).toPrecision(3)), Minimum_vertical_zoom, Maximum_vertical_zoom) }));
  };

  /***********************************************
   * @brief : 按当前可见波形重建并锁定纵向量程
   * @return: 无
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  const Fit_vertical_range = () => {
    Manual_ranges_ref.current.clear();
    setView((Current) => ({ ...Current, verticalZoom: 1, verticalOffset: 0 }));
  };

  /***********************************************
   * @brief : 恢复并锁定默认时间范围
   * @return: 无
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  const Fit_time_range = () => {
    setView((Current) => ({
      ...Current,
      timeSpan: Initial_scope_view.timeSpan,
      endTime: null,
      followPosition: 1,
      follow: true,
    }));
  };

  /***********************************************
   * @brief : 按当前波形重建并锁定全部坐标轴
   * @return: 无
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  const Fit_all_ranges = () => {
    Manual_ranges_ref.current.clear();
    setView({ ...Initial_scope_view });
  };

  const Reset_view = () => {
    Manual_ranges_ref.current.clear();
    setView(Initial_scope_view);
  };

  useEffect(() => {
    const Canvas = Canvas_ref.current;
    if (!Canvas || !interactive) return;
    const Handle_wheel = (Event: WheelEvent) => {
      Event.preventDefault();
      Event.stopPropagation();
      if (Event.shiftKey) {
        const Bounds = Canvas.getBoundingClientRect();
        const Plot_top = 13;
        const Plot_height = Math.max(1, Bounds.height - 42);
        const Cursor_ratio = Clamp((Event.clientY - Bounds.top - Plot_top) / Plot_height, 0, 1);
        const Zoom_factor = Event.deltaY > 0 ? 0.8 : 1.25;
        setView((Current) => {
          const New_zoom = Clamp(Number((Current.verticalZoom * Zoom_factor).toPrecision(3)), Minimum_vertical_zoom, Maximum_vertical_zoom);
          /* 缩放前后保持鼠标所指的纵向数值位于同一屏幕位置。 */
          return {
            ...Current,
            verticalZoom: New_zoom,
            verticalOffset: Clamp(
              Calculate_vertical_offset(Current.verticalZoom, Current.verticalOffset, New_zoom, Cursor_ratio),
              -8,
              8,
            ),
          };
        });
      } else {
        const Bounds = Canvas.getBoundingClientRect();
        const Plot_left = 62;
        const Plot_width = Math.max(1, Bounds.width - 76);
        const Cursor_ratio = Clamp((Event.clientX - Bounds.left - Plot_left) / Plot_width, 0, 1);
        const Current_span = Time_span_ref.current;
        const New_span = Clamp(Number((Current_span * (Event.deltaY > 0 ? 1.25 : 0.8)).toPrecision(3)), Minimum_time_span, Maximum_time_span_ref.current);
        /* 缩放前后保持鼠标所指的采样时刻位于同一横向位置。 */
        const Anchor_time = View_end_time_ref.current - Current_span + Cursor_ratio * Current_span;
        const Desired_end_time = Anchor_time + (1 - Cursor_ratio) * New_span;
        const Latest = Latest_time_ref.current;
        const Minimum_end_time = Math.min(Latest, Earliest_time_ref.current + New_span);
        const Maximum_end_time = Latest + New_span * (1 - Minimum_follow_position);
        const New_end_time = Clamp(Desired_end_time, Minimum_end_time, Maximum_end_time);
        const Follow = New_end_time >= Latest - Math.max(New_span * 0.005, 0.001);
        const Follow_position = Follow
          ? Clamp(1 - (New_end_time - Latest) / New_span, Minimum_follow_position, 1)
          : Follow_position_ref.current;
        setView((Current) => ({
          ...Current,
          timeSpan: New_span,
          endTime: Follow ? null : New_end_time,
          followPosition: Follow_position,
          follow: Follow,
        }));
      }
    };
    Canvas.addEventListener("wheel", Handle_wheel, { passive: false });
    return () => Canvas.removeEventListener("wheel", Handle_wheel);
  }, [interactive]);

  useEffect(() => {
    if (paused) {
      setView((Current) => {
        if (!Current.follow) return Current;
        return {
          ...Current,
          follow: false,
          endTime: Latest_time_ref.current + Time_span_ref.current * (1 - Current.followPosition),
        };
      });
      return;
    }
    setView((Current) => Current.follow ? Current : { ...Current, follow: true, endTime: null });
  }, [paused]);

  const Handle_pointer_down = (Event: React.PointerEvent<HTMLCanvasElement>) => {
    if (!interactive || Event.button !== 0) return;
    const Bounds = Event.currentTarget.getBoundingClientRect();
    Drag_ref.current = {
      pointerId: Event.pointerId,
      startX: Event.clientX,
      startY: Event.clientY,
      startEndTime: View_end_time,
      startLatestTime: Latest_time,
      startFollow: View.follow,
      startVerticalOffset: View.verticalOffset,
    };
    Event.currentTarget.setPointerCapture(Event.pointerId);
    setCursorPoint({ x: Event.clientX - Bounds.left, y: Event.clientY - Bounds.top });
    setDragging(true);
  };

  const Handle_pointer_move = (Event: React.PointerEvent<HTMLCanvasElement>) => {
    if (!interactive) return;
    const Bounds = Event.currentTarget.getBoundingClientRect();
    Pending_pointer_ref.current = {
      pointerId: Event.pointerId,
      clientX: Event.clientX,
      clientY: Event.clientY,
      x: Event.clientX - Bounds.left,
      y: Event.clientY - Bounds.top,
      width: Bounds.width,
      height: Bounds.height,
    };
    if (Pointer_frame_ref.current !== null) return;
    Pointer_frame_ref.current = window.requestAnimationFrame(() => {
      Pointer_frame_ref.current = null;
      const Pending = Pending_pointer_ref.current;
      if (!Pending) return;
      setCursorPoint({ x: Pending.x, y: Pending.y });
      const Drag = Drag_ref.current;
      if (!Drag || Drag.pointerId !== Pending.pointerId) return;
      const Time_span = Time_span_ref.current;
      const Latest = Latest_time_ref.current;
      const Earliest = Earliest_time_ref.current;
      const Plot_width = Math.max(1, Pending.width - 76);
      const Plot_height = Math.max(1, Pending.height - 42);
      const Time_delta = ((Pending.clientX - Drag.startX) / Plot_width) * Time_span;
      const Minimum_end_time = Math.min(Latest, Earliest + Time_span);
      const Maximum_end_time = Latest + Time_span * (1 - Minimum_follow_position);
      const Follow_time_delta = Drag.startFollow ? Latest - Drag.startLatestTime : 0;
      const Desired_end_time = Drag.startEndTime + Follow_time_delta - Time_delta;
      const New_end_time = Clamp(Desired_end_time, Minimum_end_time, Maximum_end_time);
      const Follow = New_end_time >= Latest - Math.max(Time_span * 0.005, 0.001);
      setView((Current) => ({
        ...Current,
        timeSpan: Time_span,
        endTime: Follow ? null : New_end_time,
        followPosition: Follow
          ? Clamp(1 - (New_end_time - Latest) / Time_span, Minimum_follow_position, 1)
          : Current.followPosition,
        verticalOffset: Clamp(Drag.startVerticalOffset + ((Pending.clientY - Drag.startY) / Plot_height) / Current.verticalZoom, -8, 8),
        follow: Follow,
      }));
    });
  };

  const Handle_pointer_up = (Event: React.PointerEvent<HTMLCanvasElement>) => {
    if (Drag_ref.current?.pointerId !== Event.pointerId) return;
    Drag_ref.current = null;
    if (Event.currentTarget.hasPointerCapture(Event.pointerId)) Event.currentTarget.releasePointerCapture(Event.pointerId);
    setDragging(false);
  };

  const Adjust_follow_position = (Amount: number) => {
    setView((Current) => ({
      ...Current,
      followPosition: Clamp(Number((Current.followPosition + Amount).toFixed(2)), Minimum_follow_position, 1),
      follow: true,
      endTime: null,
    }));
  };

  return (
    <div className={interactive ? "scope-view interactive" : "scope-view compact"}>
      {interactive && (
        <div className="scope-toolbar">
          <div className="scope-adjust-group" title="调节水平时基，鼠标滚轮也可缩放">
            <MoveHorizontal size={14} />
            <button type="button" onClick={() => Adjust_time_span(1.25)} aria-label="缩小时间轴"><Minus size={13} /></button>
            <span><b>{Format_time_base(Effective_time_span / 10)}</b><small>/ 格</small></span>
            <button type="button" onClick={() => Adjust_time_span(0.8)} aria-label="放大时间轴"><Plus size={13} /></button>
          </div>
          <div className="scope-adjust-group" title="调节纵向倍率，按住 Shift 滚轮也可缩放">
            <MoveVertical size={14} />
            <button type="button" onClick={() => Adjust_vertical_zoom(0.8)} aria-label="缩小纵向波形"><Minus size={13} /></button>
            <span><b>{Format_vertical_zoom(View.verticalZoom)} ×</b></span>
            <button type="button" onClick={() => Adjust_vertical_zoom(1.25)} aria-label="放大纵向波形"><Plus size={13} /></button>
          </div>
          <div className="scope-position-control" title="调整实时波形止点在时间轴中的位置">
            <MoveLeft size={14} />
            <button type="button" onClick={() => Adjust_follow_position(-0.05)} aria-label="止点向左移动"><ChevronLeft size={13} /></button>
            <input
              type="range"
              min={Minimum_follow_position}
              max="1"
              step="0.01"
              value={View.followPosition}
              aria-label="波形止点位置"
              onChange={(Event) => {
                const Follow_position = Number(Event.target.value);
                setView((Current) => ({
                  ...Current,
                  followPosition: Follow_position,
                  follow: true,
                  endTime: null,
                }));
              }}
            />
            <button type="button" onClick={() => Adjust_follow_position(0.05)} aria-label="止点向右移动"><ChevronRight size={13} /></button>
            <span>{Math.round(View.followPosition * 100)}%</span>
          </div>
          <button type="button" className="scope-reset-button" onClick={Reset_view} title="复位示波器视图" aria-label="复位示波器视图"><RotateCcw size={14} /></button>
        </div>
      )}
      <div className="scope-canvas-wrap">
        <canvas
          ref={Canvas_ref}
          className={Dragging ? "scope-canvas dragging" : "scope-canvas"}
          aria-label="实时电机波形"
          onDoubleClick={interactive ? Reset_view : undefined}
          onPointerDown={Handle_pointer_down}
          onPointerMove={Handle_pointer_move}
          onPointerUp={Handle_pointer_up}
          onPointerCancel={Handle_pointer_up}
          onPointerLeave={() => {
            if (Drag_ref.current) return;
            Pending_pointer_ref.current = null;
            if (Pointer_frame_ref.current !== null) window.cancelAnimationFrame(Pointer_frame_ref.current);
            Pointer_frame_ref.current = null;
            setCursorPoint(null);
          }}
        />
      </div>
      {interactive && (
        <div className="scope-auto-footer">
          <button type="button" onClick={Fit_vertical_range} title="按当前可见波形重建并锁定纵向量程">量程 AUTO</button>
          <button type="button" onClick={Fit_time_range} title="恢复并锁定5秒时间范围">时间 AUTO</button>
          <button type="button" onClick={Fit_all_ranges} title="按当前波形重建并锁定全部坐标轴">AUTO</button>
        </div>
      )}
    </div>
  );
});
