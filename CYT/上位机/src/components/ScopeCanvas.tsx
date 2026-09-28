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
  moved: boolean;
}

interface Draw_points_t {
  indices: Int32Array;
  envelope: boolean;
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

interface Webgl_line_t {
  color: string;
  vertices: Float32Array;
  mode: "line-strip" | "lines";
  segments?: Uint32Array;
  drawPoints?: boolean;
}

const Angle_channel_keys = new Set<keyof Telemetry_t>([
  "mechanicalAngle",
  "mechanicalAngleTarget",
  "electricalAngle",
  "pllMechanicalAngleEst",
  "pllElectricalAngleEst",
]);
const Angle_range_minimum = 0;
const Angle_range_maximum = 360;

/***********************************************
 * @brief : 编译单个WebGL2着色器
 * @param : Context WebGL2绘图上下文
 * @param : Type 着色器类型
 * @param : Source GLSL源代码
 * @return: 编译成功的着色器，失败时为空
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
function Create_webgl_shader(Context: WebGL2RenderingContext, Type: number, Source: string) {
  const Shader = Context.createShader(Type);
  if (!Shader) return null;
  Context.shaderSource(Shader, Source);
  Context.compileShader(Shader);
  if (Context.getShaderParameter(Shader, Context.COMPILE_STATUS)) return Shader;
  Context.deleteShader(Shader);
  return null;
}

/***********************************************
 * @brief : 创建示波器折线WebGL2着色程序
 * @param : Context WebGL2绘图上下文
 * @return: 链接成功的着色程序，失败时为空
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
function Create_webgl_program(Context: WebGL2RenderingContext) {
  const Vertex_shader = Create_webgl_shader(
    Context,
    Context.VERTEX_SHADER,
    `#version 300 es
    in vec2 a_position;
    void main() {
      gl_Position = vec4(a_position, 0.0, 1.0);
      gl_PointSize = 3.0;
    }`);
  const Fragment_shader = Create_webgl_shader(
    Context,
    Context.FRAGMENT_SHADER,
    `#version 300 es
    precision mediump float;
    uniform vec4 u_color;
    out vec4 Output_color;
    void main() {
      Output_color = u_color;
    }`);
  if (!Vertex_shader || !Fragment_shader) {
    if (Vertex_shader) Context.deleteShader(Vertex_shader);
    if (Fragment_shader) Context.deleteShader(Fragment_shader);
    return null;
  }

  const Program = Context.createProgram();
  if (!Program) {
    Context.deleteShader(Vertex_shader);
    Context.deleteShader(Fragment_shader);
    return null;
  }
  Context.attachShader(Program, Vertex_shader);
  Context.attachShader(Program, Fragment_shader);
  Context.linkProgram(Program);
  Context.deleteShader(Vertex_shader);
  Context.deleteShader(Fragment_shader);
  if (Context.getProgramParameter(Program, Context.LINK_STATUS)) return Program;
  Context.deleteProgram(Program);
  return null;
}

/***********************************************
 * @brief : 将十六进制颜色解析为WebGL归一化颜色
 * @param : Color 十六进制颜色字符串
 * @return: RGB归一化分量
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
function Parse_webgl_color(Color: string): [number, number, number] {
  const Match = Color.trim().match(/^#([0-9a-f]{6})$/i);
  if (!Match) return [1, 1, 1];
  return [
    Number.parseInt(Match[1].slice(0, 2), 16) / 255,
    Number.parseInt(Match[1].slice(2, 4), 16) / 255,
    Number.parseInt(Match[1].slice(4, 6), 16) / 255,
  ];
}

/*===========================================================================*/
/*  WebGL2示波器折线渲染器                                                   */
/*===========================================================================*/
class Scope_webgl_renderer_t {
  private readonly Context: WebGL2RenderingContext | null;
  private readonly Program: WebGLProgram | null;
  private readonly Vertex_buffer: WebGLBuffer | null;
  private readonly Position_location: number;
  private readonly Color_location: WebGLUniformLocation | null;

  /***********************************************
   * @brief : 初始化透明背景的WebGL2折线渲染资源
   * @param : Canvas GPU折线画布
   * @return: WebGL2折线渲染器实例
   * @date  : 2026-09-16
   * @author: L
   ************************************************/
  constructor(Canvas: HTMLCanvasElement) {
    this.Context = Canvas.getContext("webgl2", {
      alpha: true,
      antialias: true,
      premultipliedAlpha: false,
      preserveDrawingBuffer: false,
    });
    this.Program = this.Context ? Create_webgl_program(this.Context) : null;
    this.Vertex_buffer = this.Context?.createBuffer() ?? null;
    this.Position_location = this.Context && this.Program
      ? this.Context.getAttribLocation(this.Program, "a_position")
      : -1;
    this.Color_location = this.Context && this.Program
      ? this.Context.getUniformLocation(this.Program, "u_color")
      : null;
  }

  /***********************************************
   * @brief : 查询WebGL2折线资源是否完整可用
   * @return: 可绘制返回true，否则返回false
   * @date  : 2026-09-16
   * @author: L
   ************************************************/
  isReady() {
    return Boolean(
      this.Context &&
      !this.Context.isContextLost() &&
      this.Program &&
      this.Vertex_buffer &&
      this.Position_location >= 0 &&
      this.Color_location);
  }

  /***********************************************
   * @brief : 清空GPU画布并绘制当前可见通道折线
   * @param : Lines 已转换为裁剪空间坐标的折线段
   * @param : Width 画布CSS宽度
   * @param : Height 画布CSS高度
   * @param : Pixel_ratio 设备像素比例
   * @param : Left 绘图区左边界
   * @param : Top 绘图区上边界
   * @param : Plot_width 绘图区宽度
   * @param : Plot_height 绘图区高度
   * @return: 绘制成功返回true，否则返回false
   * @date  : 2026-09-16
   * @author: L
   ************************************************/
  draw(
    Lines: Webgl_line_t[],
    Width: number,
    Height: number,
    Pixel_ratio: number,
    Left: number,
    Top: number,
    Plot_width: number,
    Plot_height: number,
  ) {
    if (!this.isReady() || !this.Context || !this.Program || !this.Vertex_buffer || !this.Color_location) return false;
    const Context = this.Context;
    const Canvas = Context.canvas as HTMLCanvasElement;
    const Pixel_width = Math.max(1, Math.floor(Width * Pixel_ratio));
    const Pixel_height = Math.max(1, Math.floor(Height * Pixel_ratio));
    if (Canvas.width !== Pixel_width || Canvas.height !== Pixel_height) {
      Canvas.width = Pixel_width;
      Canvas.height = Pixel_height;
    }

    Context.viewport(0, 0, Pixel_width, Pixel_height);
    Context.clearColor(0, 0, 0, 0);
    Context.clear(Context.COLOR_BUFFER_BIT);
    Context.enable(Context.SCISSOR_TEST);
    Context.scissor(
      Math.floor(Left * Pixel_ratio),
      Math.floor((Height - Top - Plot_height) * Pixel_ratio),
      Math.max(1, Math.floor(Plot_width * Pixel_ratio)),
      Math.max(1, Math.floor(Plot_height * Pixel_ratio)));
    Context.useProgram(this.Program);
    Context.bindBuffer(Context.ARRAY_BUFFER, this.Vertex_buffer);
    Context.enableVertexAttribArray(this.Position_location);
    Context.vertexAttribPointer(this.Position_location, 2, Context.FLOAT, false, 0, 0);
    Lines.forEach((Line) => {
      if (Line.vertices.length < 2) return;
      const [Red, Green, Blue] = Parse_webgl_color(Line.color);
      Context.uniform4f(this.Color_location, Red, Green, Blue, 1);
      if (Line.segments && Line.segments.length > 2) {
        /* 将断续折线和孤立点分别合批，避免低速通道逐点提交GPU绘制命令。 */
        const Vertex_storage = new Float32Array(Line.vertices.length * 2);
        const Point_storage = Line.drawPoints ? Line.vertices : new Float32Array(Line.vertices.length);
        let Vertex_value_count = 0;
        let Point_value_count = Line.drawPoints ? Line.vertices.length : 0;
        for (let Index = 0; Index < Line.segments.length; Index += 2) {
          const Segment_start = Line.segments[Index];
          const Segment_count = Line.segments[Index + 1];
          if (Segment_count === 1 && !Line.drawPoints) {
            Point_storage.set(Line.vertices.subarray(Segment_start * 2, Segment_start * 2 + 2), Point_value_count);
            Point_value_count += 2;
          }
          for (let Point_index = Segment_start + 1; Point_index < Segment_start + Segment_count; Point_index += 1) {
            const Previous_offset = (Point_index - 1) * 2;
            Vertex_storage.set(Line.vertices.subarray(Previous_offset, Previous_offset + 4), Vertex_value_count);
            Vertex_value_count += 4;
          }
        }
        if (Vertex_value_count > 0) {
          Context.bufferData(Context.ARRAY_BUFFER, Vertex_storage.subarray(0, Vertex_value_count), Context.DYNAMIC_DRAW);
          Context.drawArrays(Context.LINES, 0, Vertex_value_count / 2);
        }
        if (Point_value_count > 0) {
          Context.bufferData(Context.ARRAY_BUFFER, Point_storage.subarray(0, Point_value_count), Context.DYNAMIC_DRAW);
          Context.drawArrays(Context.POINTS, 0, Point_value_count / 2);
        }
        return;
      }
      Context.bufferData(Context.ARRAY_BUFFER, Line.vertices, Context.DYNAMIC_DRAW);
      if (Line.mode === "lines") {
        Context.drawArrays(Context.LINES, 0, Line.vertices.length / 2);
      } else if (Line.segments) {
        for (let Index = 0; Index < Line.segments.length; Index += 2) {
          const Segment_count = Line.segments[Index + 1];
          Context.drawArrays(
            Segment_count === 1 ? Context.POINTS : Context.LINE_STRIP,
            Line.segments[Index],
            Segment_count);
        }
      } else {
        Context.drawArrays(Context.LINE_STRIP, 0, Line.vertices.length / 2);
      }
      if (Line.drawPoints) {
        Context.drawArrays(Context.POINTS, 0, Line.vertices.length / 2);
      }
    });
    Context.disable(Context.SCISSOR_TEST);
    return true;
  }

  /***********************************************
   * @brief : 释放WebGL2折线缓冲和着色程序
   * @return: 无
   * @date  : 2026-09-16
   * @author: L
   ************************************************/
  destroy() {
    if (!this.Context) return;
    if (this.Vertex_buffer) this.Context.deleteBuffer(this.Vertex_buffer);
    if (this.Program) this.Context.deleteProgram(this.Program);
  }
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
    if (Angle_channel_keys.has(Channel.key)) {
      const Existing = Unit_ranges.get(Channel.unit);
      Unit_ranges.set(Channel.unit, {
        minimum: Math.min(Angle_range_minimum, Existing?.minimum ?? Number.POSITIVE_INFINITY),
        maximum: Math.max(Angle_range_maximum, Existing?.maximum ?? Number.NEGATIVE_INFINITY),
      });
    }
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
 * @brief : 根据单个通道的有效采样间隔计算折线允许跨越的最大时间空洞
 * @param : Samples 当前视窗内的采样数据
 * @param : Channel 当前绘制通道
 * @return: 最大连续间隔，单位为秒
 * @date  : 2026-09-28
 * @author: L
 ************************************************/
function Calculate_connection_gap(Samples: Telemetry_t[], Channel: Channel_definition_t) {
  let Minimum_gap = Number.POSITIVE_INFINITY;
  let Previous_timestamp: number | null = null;
  for (const Sample of Samples) {
    if (!Number.isFinite(Number(Sample[Channel.key]))) continue;
    if (Previous_timestamp !== null) {
      const Gap = Sample.timestamp - Previous_timestamp;
      if (Gap > 0 && Gap < Minimum_gap) Minimum_gap = Gap;
    }
    Previous_timestamp = Sample.timestamp;
  }
  return Number.isFinite(Minimum_gap) ? Math.max(0.02, Minimum_gap * 4) : Number.POSITIVE_INFINITY;
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
 * @brief : 按画布像素合并密集采样并保留波形特征点
 * @param : Samples 当前视窗内的采样数据
 * @param : Channel 当前绘制通道
 * @param : View_start_time 视窗起始时间
 * @param : Time_span 视窗时间跨度
 * @param : Plot_width 绘图区宽度
 * @return: 用于绘制的有序采样索引及绘制模式
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function Build_draw_points(
  Samples: Telemetry_t[],
  Channel: Channel_definition_t,
  View_start_time: number,
  Time_span: number,
  Plot_width: number,
): Draw_points_t {
  const Maximum_points = Math.max(2, Math.floor(Plot_width) * 2);
  const Is_angle_channel = Angle_channel_keys.has(Channel.key);
  if (Samples.length <= Maximum_points) {
    const Draw_indices = new Int32Array(Samples.length);
    let Draw_count = 0;
    Samples.forEach((Sample, Index) => {
      const Value = Number(Sample[Channel.key]);
      if (Number.isFinite(Value)) Draw_indices[Draw_count++] = Index;
    });
    return { indices: Draw_indices.subarray(0, Draw_count), envelope: false };
  }

  const Bucket_count = Math.max(1, Math.floor(Plot_width));
  if (Is_angle_channel) {
    const First_indices = new Int32Array(Bucket_count);
    const Last_indices = new Int32Array(Bucket_count);
    const Selected_indices = new Uint8Array(Samples.length);
    First_indices.fill(-1);
    Last_indices.fill(-1);
    let Previous_index = -1;
    let Previous_value = 0;
    Samples.forEach((Sample, Index) => {
      const Value = Number(Sample[Channel.key]);
      if (!Number.isFinite(Value)) return;
      const Ratio = (Sample.timestamp - View_start_time) / Math.max(Time_span, 0.0001);
      const Bucket_index = Clamp(Math.floor(Ratio * Bucket_count), 0, Bucket_count - 1);
      if (First_indices[Bucket_index] < 0) First_indices[Bucket_index] = Index;
      Last_indices[Bucket_index] = Index;
      /* 回绕两侧的采样必须同时保留，避免降采样抹掉角度跳变。 */
      if (Previous_index >= 0 && Math.abs(Value - Previous_value) > (Angle_range_maximum - Angle_range_minimum) / 2) {
        Selected_indices[Previous_index] = 1;
        Selected_indices[Index] = 1;
      }
      Previous_index = Index;
      Previous_value = Value;
    });
    for (let Bucket_index = 0; Bucket_index < Bucket_count; Bucket_index += 1) {
      const First_index = First_indices[Bucket_index];
      const Last_index = Last_indices[Bucket_index];
      if (First_index >= 0) Selected_indices[First_index] = 1;
      if (Last_index >= 0) Selected_indices[Last_index] = 1;
    }
    const Draw_indices = new Int32Array(Samples.length);
    let Draw_count = 0;
    Selected_indices.forEach((Selected, Index) => {
      if (Selected !== 0) Draw_indices[Draw_count++] = Index;
    });
    return { indices: Draw_indices.subarray(0, Draw_count), envelope: false };
  }

  const Minimum_indices = new Int32Array(Bucket_count);
  const Maximum_indices = new Int32Array(Bucket_count);
  Minimum_indices.fill(-1);
  Maximum_indices.fill(-1);
  Samples.forEach((Sample, Index) => {
    const Value = Number(Sample[Channel.key]);
    if (!Number.isFinite(Value)) return;
    const Ratio = (Sample.timestamp - View_start_time) / Math.max(Time_span, 0.0001);
    const Bucket_index = Clamp(Math.floor(Ratio * Bucket_count), 0, Bucket_count - 1);
    const Minimum_index = Minimum_indices[Bucket_index];
    if (Minimum_index < 0) {
      Minimum_indices[Bucket_index] = Index;
      Maximum_indices[Bucket_index] = Index;
      return;
    }
    if (Value < Number(Samples[Minimum_index][Channel.key])) {
      Minimum_indices[Bucket_index] = Index;
    }
    const Maximum_index = Maximum_indices[Bucket_index];
    if (Value > Number(Samples[Maximum_index][Channel.key])) {
      Maximum_indices[Bucket_index] = Index;
    }
  });

  const Draw_indices = new Int32Array(Bucket_count * 2 + 2);
  let Draw_count = 0;
  const Append_point = (Index: number) => {
    if (Index < 0 || Draw_indices[Draw_count - 1] === Index) return;
    const Sample = Samples[Index];
    const Value = Number(Sample[Channel.key]);
    if (Number.isFinite(Value)) Draw_indices[Draw_count++] = Index;
  };
  Append_point(0);
  for (let Bucket_index = 0; Bucket_index < Bucket_count; Bucket_index += 1) {
    const Minimum_index = Minimum_indices[Bucket_index];
    const Maximum_index = Maximum_indices[Bucket_index];
    if (Minimum_index < 0 || Maximum_index < 0) continue;
    Append_point(Math.min(Minimum_index, Maximum_index));
    Append_point(Math.max(Minimum_index, Maximum_index));
  }
  Append_point(Samples.length - 1);
  return { indices: Draw_indices.subarray(0, Draw_count), envelope: false };
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
 * @brief : 格式化测量点的设备时间戳
 * @param : Seconds 设备时间戳，单位为秒
 * @return: 以秒为单位的时间戳字符串
 * @date  : 2026-09-23
 * @author: L
 ************************************************/
function Format_measure_timestamp(Seconds: number) {
  if (!Number.isFinite(Seconds)) return "--";
  return `${Seconds.toFixed(5)} s`;
}

/***********************************************
 * @brief : 格式化两个测量点之间的时间差
 * @param : Seconds 时间差，单位为秒
 * @return: 带时间单位的时间差字符串
 * @date  : 2026-09-23
 * @author: L
 ************************************************/
function Format_measure_delta(Seconds: number) {
  if (!Number.isFinite(Seconds)) return "--";
  if (Math.abs(Seconds) < 1) return `${(Seconds * 1000).toFixed(3)} ms`;
  return `${Seconds.toFixed(5)} s`;
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
  const Gpu_canvas_ref = useRef<HTMLCanvasElement>(null);
  const Gpu_renderer_ref = useRef<Scope_webgl_renderer_t | null>(null);
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
  const [Measure_times, setMeasureTimes] = useState<number[]>([]);
  const [Dragging, setDragging] = useState(false);
  const [Canvas_visible, setCanvasVisible] = useState(true);
  const [Visible_unit_order, setVisibleUnitOrder] = useState<string[]>([]);
  const Subscribe_history = useCallback((Listener: () => void) => paused ? () => undefined : history.subscribe(Listener), [history, paused]);
  const Get_history_revision = useCallback(() => paused ? 0 : history.getRevision(), [history, paused]);
  const History_revision = useSyncExternalStore(Subscribe_history, Get_history_revision, Get_history_revision);
  const Frozen_source = paused ? (frozenSamples ?? Empty_frozen_samples) : null;

  useEffect(() => () => {
    Gpu_renderer_ref.current?.destroy();
    Gpu_renderer_ref.current = null;
  }, []);

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
  const Previous_visible_count_ref = useRef(0);
  useEffect(() => {
    const Previous_count = Previous_visible_count_ref.current;
    Previous_visible_count_ref.current = Visible_channels.length;
    if (Previous_count !== 0 || Visible_channels.length === 0) return;
    Manual_ranges_ref.current.clear();
    setView((Current) => ({ ...Current, verticalZoom: 1, verticalOffset: 0 }));
  }, [Visible_channels.length]);
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
    if (Gpu_canvas_ref.current && Gpu_renderer_ref.current === null) {
      Gpu_renderer_ref.current = new Scope_webgl_renderer_t(Gpu_canvas_ref.current);
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
      const Scope_background_edge = Canvas_style.getPropertyValue("--scope-background-edge").trim() || "#090d12";
      const Scope_grid = Canvas_style.getPropertyValue("--scope-grid").trim() || "rgba(133, 150, 170, 0.13)";
      const Scope_grid_major = Canvas_style.getPropertyValue("--scope-grid-major").trim() || "rgba(155, 177, 196, 0.24)";
      const Scope_label = Canvas_style.getPropertyValue("--scope-label").trim() || "#768394";
      const Scope_axis = Canvas_style.getPropertyValue("--scope-axis").trim() || "#778596";
      const Scope_cursor = Canvas_style.getPropertyValue("--scope-cursor").trim() || "rgba(222, 232, 240, 0.62)";
      const Scope_start_marker = Canvas_style.getPropertyValue("--scope-start-marker").trim() || "#4ecaa0";
      const Scope_stop_marker = Canvas_style.getPropertyValue("--scope-stop-marker").trim() || "#f0b35c";

      const Left = interactive ? 62 : 48;
      const Right = 14;
      const Top = 13;
      const Bottom = 29;
      const Plot_width = Math.max(1, Width - Left - Right);
      const Plot_height = Math.max(1, Height - Top - Bottom);
      const Gpu_renderer = Gpu_renderer_ref.current;
      const Gpu_ready = Gpu_renderer?.isReady() ?? false;
      const Webgl_lines: Webgl_line_t[] = [];

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

      if (Visible_samples.length >= 1 && Visible_channels.length > 0) {
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
          if (Draw_points.indices.length === 0) return;
          const Is_angle_channel = Angle_channel_keys.has(Channel.key);
          /* 抽稀后的相邻点可能跨越两个像素列，不能将这段正常间隔误判为断流。 */
          const Maximum_connection_gap = Is_angle_channel
            ? Math.max(Calculate_connection_gap(Visible_samples, Channel), Effective_time_span / Plot_width * 2)
            : Number.POSITIVE_INFINITY;
          const Show_angle_points = Is_angle_channel && Draw_points.indices.length <= Plot_width;
          if (Gpu_ready) {
            const Vertices = new Float32Array(Draw_points.indices.length * 2);
            let Previous_timestamp: number | null = null;
            const Segment_storage = !Draw_points.envelope && Is_angle_channel
              ? new Uint32Array(Draw_points.indices.length * 2)
              : null;
            let Segment_start = 0;
            let Segment_value_count = 0;
            Draw_points.indices.forEach((Sample_index, Point_index) => {
              const Sample = Visible_samples[Sample_index];
              const Value = Number(Sample[Channel.key]);
              const Discontinuous = Segment_storage !== null &&
                Previous_timestamp !== null &&
                Sample.timestamp - Previous_timestamp > Maximum_connection_gap;
              if (Discontinuous) {
                const Segment_count = Point_index - Segment_start;
                if (Segment_count >= 1) {
                  Segment_storage[Segment_value_count++] = Segment_start;
                  Segment_storage[Segment_value_count++] = Segment_count;
                }
                Segment_start = Point_index;
              }
              const X = Left + ((Sample.timestamp - View_start_time) / Effective_time_span) * Plot_width;
              const Value_ratio = (Value - Range.minimum) / Span;
              const Y = Top + Plot_height * (Plot_area.topRatio + (1 - Value_ratio) * Plot_area.heightRatio);
              Vertices[Point_index * 2] = (X / Width) * 2 - 1;
              Vertices[Point_index * 2 + 1] = 1 - (Y / Height) * 2;
              Previous_timestamp = Sample.timestamp;
            });
            if (Segment_storage !== null) {
              const Segment_count = Draw_points.indices.length - Segment_start;
              if (Segment_count >= 1) {
                Segment_storage[Segment_value_count++] = Segment_start;
                Segment_storage[Segment_value_count++] = Segment_count;
              }
            }
            Webgl_lines.push({
              color: Channel.color,
              vertices: Vertices,
              mode: Draw_points.envelope ? "lines" : "line-strip",
              segments: Segment_storage?.subarray(0, Segment_value_count),
              drawPoints: Show_angle_points,
            });
          } else {
            let Previous_timestamp: number | null = null;
            let Previous_x: number | null = null;
            let Previous_y: number | null = null;
            let Segment_point_count = 0;
            const Single_points: Array<{ X: number; Y: number }> = [];
            const Angle_points: Array<{ X: number; Y: number }> = [];
            Context.beginPath();
            Draw_points.indices.forEach((Sample_index, Index) => {
              const Sample = Visible_samples[Sample_index];
              const Value = Number(Sample[Channel.key]);
              const X = Left + ((Sample.timestamp - View_start_time) / Effective_time_span) * Plot_width;
              const Value_ratio = (Value - Range.minimum) / Span;
              const Y = Top + Plot_height * (Plot_area.topRatio + (1 - Value_ratio) * Plot_area.heightRatio);
              if (Draw_points.envelope) {
                if (Index % 2 === 0) Context.moveTo(X, Y);
                else Context.lineTo(X, Y);
              } else {
                const Discontinuous = Is_angle_channel &&
                  Previous_timestamp !== null &&
                  Sample.timestamp - Previous_timestamp > Maximum_connection_gap;
                if (Index === 0 || Discontinuous) {
                  if (Segment_point_count === 1 && Previous_x !== null && Previous_y !== null) {
                    Single_points.push({ X: Previous_x, Y: Previous_y });
                  }
                  Context.moveTo(X, Y);
                  Segment_point_count = 1;
                } else {
                  Context.lineTo(X, Y);
                  Segment_point_count += 1;
                }
                if (Show_angle_points) Angle_points.push({ X, Y });
              }
              Previous_timestamp = Sample.timestamp;
              Previous_x = X;
              Previous_y = Y;
            });
            if (!Draw_points.envelope &&
                Segment_point_count === 1 &&
                Previous_x !== null &&
                Previous_y !== null) {
              Single_points.push({ X: Previous_x, Y: Previous_y });
            }
            Context.strokeStyle = Channel.color;
            Context.lineWidth = 1.15;
            Context.lineJoin = "bevel";
            Context.lineCap = "butt";
            Context.stroke();
            if (Single_points.length > 0) {
              Context.fillStyle = Channel.color;
              Single_points.forEach((Point) => {
                Context.fillRect(Point.X - 0.75, Point.Y - 0.75, 1.5, 1.5);
              });
            }
            if (Angle_points.length > 0) {
              Context.fillStyle = Channel.color;
              Angle_points.forEach((Point) => {
                Context.fillRect(Point.X - 1, Point.Y - 1, 2, 2);
              });
            }
          }
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

      if (Gpu_ready && Gpu_renderer) {
        Gpu_renderer.draw(
          Webgl_lines,
          Width,
          Height,
          Pixel_ratio,
          Left,
          Top,
          Plot_width,
          Plot_height);
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

      const Valid_measure_times = Measure_times
        .map((Timestamp, Index) => ({ Timestamp, Index }))
        .filter(({ Timestamp }) => Timestamp >= View_start_time && Timestamp <= View_end_time);
      Valid_measure_times.forEach(({ Timestamp, Index }) => {
        const Measure_x = Left + ((Timestamp - View_start_time) / Effective_time_span) * Plot_width;
        Context.save();
        Context.strokeStyle = Index === 0 ? Scope_start_marker : Scope_stop_marker;
        Context.lineWidth = 1.4;
        Context.setLineDash([6, 3]);
        Context.beginPath();
        Context.moveTo(Measure_x, Top);
        Context.lineTo(Measure_x, Top + Plot_height);
        Context.stroke();
        Context.fillStyle = Index === 0 ? Scope_start_marker : Scope_stop_marker;
        Context.font = "600 9px 'Microsoft YaHei', sans-serif";
        Context.textAlign = Measure_x < Left + Plot_width / 2 ? "left" : "right";
        Context.textBaseline = "top";
        Context.fillText(`T${Index + 1} ${Format_measure_timestamp(Timestamp)}`, Measure_x, Top + 4);
        Context.restore();
      });
      if (Measure_times.length === 2) {
        const Delta_time = Math.abs(Measure_times[1] - Measure_times[0]);
        const Label = `T1 ${Format_measure_timestamp(Measure_times[0])}   T2 ${Format_measure_timestamp(Measure_times[1])}   Δt ${Format_measure_delta(Delta_time)}`;
        Context.save();
        Context.font = "600 10px 'Microsoft YaHei', sans-serif";
        const Label_width = Context.measureText(Label).width + 16;
        const Label_x = Left + 6;
        const Label_y = Top + 6;
        Context.fillStyle = Canvas_style.getPropertyValue("--scope-tooltip").trim() || "rgba(20, 26, 32, 0.94)";
        Context.fillRect(Label_x, Label_y, Label_width, 24);
        Context.strokeStyle = Canvas_style.getPropertyValue("--scope-tooltip-border").trim() || "#3b4651";
        Context.strokeRect(Label_x + 0.5, Label_y + 0.5, Label_width - 1, 23);
        Context.fillStyle = Scope_cursor;
        Context.textAlign = "left";
        Context.textBaseline = "middle";
        Context.fillText(Label, Label_x + 8, Label_y + 12);
        Context.restore();
      }

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
        Context.fillText(`t = ${Cursor_sample.timestamp.toFixed(5)} s`, Tooltip_x + 10, Tooltip_y + 15);
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
  }, [Canvas_visible, Cursor_point, Effective_time_span, Measure_times, Ordered_visible_units, Primary_unit, View, View_end_time, Visible_channels, Visible_samples, View_start_time, interactive, paused, Request_draw]);

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
    setMeasureTimes([]);
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
      moved: false,
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
      if (Math.hypot(Pending.clientX - Drag.startX, Pending.clientY - Drag.startY) >= 4) {
        Drag.moved = true;
      }
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
    const Drag = Drag_ref.current;
    if (Drag?.pointerId !== Event.pointerId) return;
    Drag_ref.current = null;
    if (Event.currentTarget.hasPointerCapture(Event.pointerId)) Event.currentTarget.releasePointerCapture(Event.pointerId);
    setDragging(false);
    if (Drag.moved || Visible_samples.length === 0) return;
    const Bounds = Event.currentTarget.getBoundingClientRect();
    const Plot_left = 62;
    const Plot_width = Math.max(1, Bounds.width - 76);
    const Pointer_x = Event.clientX - Bounds.left;
    if (Pointer_x < Plot_left || Pointer_x > Plot_left + Plot_width) return;
    const Pointer_time = View_start_time + ((Pointer_x - Plot_left) / Plot_width) * Effective_time_span;
    const Measure_sample = Find_nearest_sample(Visible_samples, Pointer_time);
    if (!Measure_sample) return;
    setMeasureTimes((Current) => Current.length >= 2
      ? [Measure_sample.timestamp]
      : [...Current, Measure_sample.timestamp]);
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
          ref={Gpu_canvas_ref}
          className="scope-waveform-canvas"
          aria-hidden="true"
        />
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
