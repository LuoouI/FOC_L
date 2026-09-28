const { app, BrowserWindow, ipcMain, dialog } = require("electron");
const { SerialPort } = require("serialport");
const path = require("path");
const fs = require("fs");
const {
  Frame_type,
  Serial_frame_parser_t,
  encodeFrame,
  encodeControlFrame,
  encodeParameterWriteFrame,
  encodeObserverParameterWriteFrame,
  decodeTelemetryPayload,
  decodeWaveformPayload,
  decodeObserverWaveformPayload,
  decodeParameterWritePayload,
  decodeObserverParameterPayload,
  decodeSongListPayload,
  Observer_stream_field_mask,
  Observer_stream_fields,
  isObserverStreamFieldSelected,
} = require("./serial/protocol.cjs");

app.setName("FOC_L");
app.setAppUserModelId("FOC_L");

let Main_window = null;
let Active_serial_port = null;
let Simulator_connected = false;
let Serial_sequence = 0;
let Serial_parser = new Serial_frame_parser_t();
let Pending_waveform_samples = [];
let Pending_observer_stream_rows = [];
let Pending_observer_stream_mask = 0;
let Pending_observer_stream_stride = 0;
let Pending_observer_stream_sample_count = 0;
let Pending_observer_stream_timestamp_hz = 1000;
let Waveform_flush_timer = null;
let Last_waveform_log_time = 0;
let Latest_control_command = null;
let Latest_loop_parameters = null;
let Pending_loop_parameter_request = null;
let Control_heartbeat_timer = null;
let Control_send_timer = null;
let Pending_control_command = null;
let Pending_control_waiters = [];
let Control_frame_in_flight = 0;
let Serial_write_chain = Promise.resolve();
let Pending_music_tracks = new Map();
let Expected_music_track_count = 0;
let Music_track_list_complete = false;
const Control_heartbeat_interval_ms = 25;
const Control_command_coalesce_interval_ms = 25;
const Waveform_flush_interval_ms = 16;
const Fast_waveform_flush_interval_ms = 1000 / 160;
const Fast_observer_field_limit = 2;
const Observer_stream_tick_hz = 20000;
const Instance_lock = app.requestSingleInstanceLock();

/***********************************************
 * @brief : 注册上位机界面放大和缩小快捷键
 * @param : Window 上位机主窗口
 * @return: 无
 * @date  : 2026-07-24
 * @author: LYF
 ************************************************/
function registerInterfaceZoomShortcuts(Window) {
  Window.webContents.on("before-input-event", (Event, Input) => {
    if (Input.type !== "keyDown" || !Input.control || Input.alt || Input.meta) return;

    const Zoom_in = Input.key === "+" || Input.key === "=" || Input.code === "NumpadAdd";
    const Zoom_out = Input.key === "-" || Input.key === "_" || Input.code === "NumpadSubtract";
    if (!Zoom_in && !Zoom_out) return;

    Event.preventDefault();
    const Current_zoom = Window.webContents.getZoomFactor();
    const Target_zoom = Current_zoom + (Zoom_in ? 0.1 : -0.1);
    const Limited_zoom = Math.min(1.5, Math.max(0.75, Target_zoom));
    Window.webContents.setZoomFactor(Number(Limited_zoom.toFixed(2)));
  });
}

/***********************************************
 * @brief : 创建上位机主窗口
 * @param : 无
 * @return: 无
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function createMainWindow() {
  Main_window = new BrowserWindow({
    title: "FOC_L",
    icon: path.join(__dirname, "../resources/foc_l_icon.png"),
    width: 1500,
    height: 940,
    minWidth: 1180,
    minHeight: 720,
    backgroundColor: "#10151c",
    autoHideMenuBar: true,
    webPreferences: {
      preload: path.join(__dirname, "preload.cjs"),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });
  Main_window.on("page-title-updated", (Event) => {
    Event.preventDefault();
    Main_window.setTitle("FOC_L");
  });
  registerInterfaceZoomShortcuts(Main_window);

  const Dev_url = process.env.VITE_DEV_SERVER_URL || (process.argv.includes("--dev") ? "http://127.0.0.1:5173" : "");
  if (Dev_url) {
    Main_window.loadURL(Dev_url);
  } else {
    Main_window.loadFile(path.join(__dirname, "../dist/index.html"));
  }
  Main_window.on("closed", () => {
    Main_window = null;
  });
}

/***********************************************
 * @brief : 唤起已经运行的上位机窗口
 * @param : 无
 * @return: 无
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function focusMainWindow() {
  if (!Main_window || Main_window.isDestroyed()) return;
  if (Main_window.isMinimized()) Main_window.restore();
  Main_window.show();
  Main_window.focus();
}

/***********************************************
 * @brief : 向渲染进程发送串口状态或数据
 * @param : Channel IPC 通道名称
 * @param : Payload 待发送内容
 * @return: 无
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function sendToRenderer(Channel, Payload) {
  if (Main_window && !Main_window.isDestroyed()) Main_window.webContents.send(Channel, Payload);
}

/***********************************************
 * @brief : 将已成功提交给串口驱动的发送帧记录到通信诊断页面
 * @param : Frame 完整协议帧
 * @return: 无
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
function reportTransmittedFrame(Frame) {
  if (!Buffer.isBuffer(Frame) || Frame.length < 10) return;
  const Payload_length = Frame.readUInt16LE(6);
  if (Frame.length !== Payload_length + 10) return;
  sendToRenderer("serial:frame", {
    type: Frame.readUInt8(3),
    sequence: Frame.readUInt16LE(4),
    timestamp: Date.now(),
    direction: "TX",
    data: [...Frame.subarray(8, 8 + Payload_length)],
  });
}

/***********************************************
 * @brief : 向串口写入一组完整协议帧并在成功后登记发送记录
 * @param : Port 当前已打开的物理串口
 * @param : Frames 待发送的完整协议帧数组
 * @param : Drain 写入后是否等待系统发送队列排空
 * @return: 串口写入完成状态
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
function writeSerialFrames(Port, Frames, Drain = false) {
  const Data = Frames.length === 1 ? Frames[0] : Buffer.concat(Frames);
  const Write_operation = Serial_write_chain.then(() => new Promise((Resolve, Reject) => {
    if (Port !== Active_serial_port || !Port.isOpen) {
      Reject(new Error("串口未打开"));
      return;
    }

    Port.write(Data, (Write_error) => {
      if (Write_error) {
        Reject(Write_error);
        return;
      }

      const Complete = () => {
        Frames.forEach(reportTransmittedFrame);
        Resolve();
      };
      if (!Drain) {
        Complete();
        return;
      }
      Port.drain((Drain_error) => Drain_error ? Reject(Drain_error) : Complete());
    });
  }));
  /* 记录尾部但吞掉单次错误，避免前一帧失败后阻塞后续串口请求。 */
  Serial_write_chain = Write_operation.catch(() => {});
  return Write_operation;
}

/***********************************************
 * @brief : 将普通波形对象和紧凑观测二进制批次交给渲染进程
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function flushWaveformSamples() {
  Waveform_flush_timer = null;
  if (Pending_waveform_samples.length > 0) {
    const Samples = Pending_waveform_samples;
    Pending_waveform_samples = [];
    sendToRenderer("serial:waveformBatch", Samples);
  }
  if (Pending_observer_stream_rows.length > 0) {
    const Rows = Pending_observer_stream_rows;
    const Sample_count = Pending_observer_stream_sample_count;
    const Data_buffer = Buffer.concat(Rows);
    const Data = new Uint8Array(
      Data_buffer.buffer,
      Data_buffer.byteOffset,
      Data_buffer.byteLength);
    Pending_observer_stream_rows = [];
    Pending_observer_stream_sample_count = 0;
    sendToRenderer("serial:observerStreamBatch", {
      fieldMask: Pending_observer_stream_mask,
      sampleStride: Pending_observer_stream_stride,
      sampleCount: Sample_count,
      timestampHz: Pending_observer_stream_timestamp_hz,
      data: Data,
    });
  }
}

/***********************************************
 * @brief : 缓存单个高速波形样本并合并IPC通知
 * @param : Sample 已解码的波形样本
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function queueWaveformSample(Sample) {
  Pending_waveform_samples.push(Sample);
  if (Pending_waveform_samples.length >= 256) {
    if (Waveform_flush_timer !== null) clearTimeout(Waveform_flush_timer);
    flushWaveformSamples();
    return;
  }
  if (Waveform_flush_timer === null) {
    Waveform_flush_timer = setTimeout(flushWaveformSamples, Waveform_flush_interval_ms);
  }
}

/***********************************************
 * @brief : 校验紧凑观测负载并按固定步长缓存为连续二进制采样
 * @param : Payload 单帧紧凑观测负载
 * @return: 无
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
function queueObserverStreamPayload(Payload) {
  if (!Buffer.isBuffer(Payload) || Payload.length < 6) {
    throw new Error("紧凑观测流负载长度不足 6 字节");
  }
  const Field_mask = Payload.readUInt16LE(4);
  if (!Number.isSafeInteger(Field_mask) ||
      Field_mask < 0 ||
      Field_mask > Observer_stream_field_mask) {
    throw new Error("紧凑观测流包含未知字段");
  }
  const Field_count = Observer_stream_fields.reduce(
    (Count, _Field, Bit) => Count + (isObserverStreamFieldSelected(Field_mask, Bit) ? 1 : 0),
    0,
  );
  const Expected_length = 6 + Field_count * 4;
  if (Payload.length !== Expected_length) {
    throw new Error(`紧凑观测流负载长度应为 ${Expected_length} 字节`);
  }

  const Sample_stride = 4 + Field_count * 4;
  if (Pending_observer_stream_rows.length > 0 &&
      (Pending_observer_stream_mask !== Field_mask ||
       Pending_observer_stream_stride !== Sample_stride ||
       Pending_observer_stream_timestamp_hz !== 1000)) {
    if (Waveform_flush_timer !== null) clearTimeout(Waveform_flush_timer);
    flushWaveformSamples();
  }

  const Row = Buffer.allocUnsafe(Sample_stride);
  Payload.copy(Row, 0, 0, 4);
  Payload.copy(Row, 4, 6);
  Pending_observer_stream_mask = Field_mask;
  Pending_observer_stream_stride = Sample_stride;
  Pending_observer_stream_timestamp_hz = 1000;
  Pending_observer_stream_rows.push(Row);
  Pending_observer_stream_sample_count += 1;
  if (Pending_observer_stream_sample_count >= 256) {
    if (Waveform_flush_timer !== null) clearTimeout(Waveform_flush_timer);
    flushWaveformSamples();
    return;
  }
  if (Waveform_flush_timer === null) {
    Waveform_flush_timer = setTimeout(flushWaveformSamples, Waveform_flush_interval_ms);
  }
}

/***********************************************
 * @brief : 将下位机连续浮点批次转换为固定步长IPC二进制行
 * @param : Payload 连续观测批次负载
 * @return: 无
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
function queueObserverStreamBatchPayload(Payload) {
  if (!Buffer.isBuffer(Payload) || Payload.length < 8) {
    throw new Error("连续观测批次负载长度不足 8 字节");
  }
  const Base_timestamp = Payload.readUInt32LE(0);
  const Field_mask = Payload.readUInt16LE(4);
  const Period_ms = Payload.readUInt8(6);
  const Sample_count = Payload.readUInt8(7);
  if (!Number.isSafeInteger(Field_mask) ||
      Field_mask < 0 ||
      Field_mask > Observer_stream_field_mask) {
    throw new Error("连续观测批次包含未知字段");
  }
  if (Period_ms === 0 || Sample_count === 0) {
    throw new Error("连续观测批次的周期或采样数无效");
  }
  const Field_count = Observer_stream_fields.reduce(
    (Count, _Field, Bit) => Count + (isObserverStreamFieldSelected(Field_mask, Bit) ? 1 : 0),
    0,
  );
  const Value_stride = Field_count * 4;
  const Expected_length = 8 + Sample_count * Value_stride;
  if (Field_count === 0 || Payload.length !== Expected_length) {
    throw new Error(`连续观测批次负载长度应为 ${Expected_length} 字节`);
  }

  const Sample_stride = 4 + Value_stride;
  if (Pending_observer_stream_rows.length > 0 &&
      (Pending_observer_stream_mask !== Field_mask ||
       Pending_observer_stream_stride !== Sample_stride ||
       Pending_observer_stream_timestamp_hz !== 1000)) {
    if (Waveform_flush_timer !== null) clearTimeout(Waveform_flush_timer);
    flushWaveformSamples();
  }

  const Rows = Buffer.allocUnsafe(Sample_count * Sample_stride);
  for (let Sample_index = 0; Sample_index < Sample_count; Sample_index += 1) {
    const Row_offset = Sample_index * Sample_stride;
    const Value_offset = 8 + Sample_index * Value_stride;
    Rows.writeUInt32LE((Base_timestamp + Sample_index * Period_ms) >>> 0, Row_offset);
    Payload.copy(Rows, Row_offset + 4, Value_offset, Value_offset + Value_stride);
  }
  Pending_observer_stream_mask = Field_mask;
  Pending_observer_stream_stride = Sample_stride;
  Pending_observer_stream_timestamp_hz = 1000;
  Pending_observer_stream_rows.push(Rows);
  Pending_observer_stream_sample_count += Sample_count;
  if (Pending_observer_stream_sample_count >= 256) {
    if (Waveform_flush_timer !== null) clearTimeout(Waveform_flush_timer);
    flushWaveformSamples();
    return;
  }
  if (Waveform_flush_timer === null) {
    Waveform_flush_timer = setTimeout(flushWaveformSamples, Waveform_flush_interval_ms);
  }
}

/***********************************************
 * @brief : 将64位字段位图观测批次转换为固定步长IPC二进制行
 * @param : Payload 扩展连续观测批次负载
 * @return: 无
 * @date  : 2026-09-23
 * @author: L
 ************************************************/
function queueExtendedObserverStreamBatchPayload(Payload) {
  if (!Buffer.isBuffer(Payload) || Payload.length < 14) {
    throw new Error("扩展观测批次负载长度不足 14 字节");
  }
  const Base_timestamp = Payload.readUInt32LE(0);
  const Field_mask = Number(Payload.readBigUInt64LE(4));
  const Period_ms = Payload.readUInt8(12);
  const Sample_count = Payload.readUInt8(13);
  if (!Number.isSafeInteger(Field_mask) ||
      Field_mask < 0 ||
      Field_mask > Observer_stream_field_mask) {
    throw new Error("扩展观测批次包含未知字段");
  }
  if (Period_ms === 0 || Sample_count === 0) {
    throw new Error("扩展观测批次的周期或采样数无效");
  }
  const Field_count = Observer_stream_fields.reduce(
    (Count, _Field, Bit) => Count + (isObserverStreamFieldSelected(Field_mask, Bit) ? 1 : 0),
    0,
  );
  const Value_stride = Field_count * 4;
  const Expected_length = 14 + Sample_count * Value_stride;
  if (Field_count === 0 || Payload.length !== Expected_length) {
    throw new Error(`扩展观测批次负载长度应为 ${Expected_length} 字节`);
  }

  const Sample_stride = 4 + Value_stride;
  if (Pending_observer_stream_rows.length > 0 &&
      (Pending_observer_stream_mask !== Field_mask ||
       Pending_observer_stream_stride !== Sample_stride ||
       Pending_observer_stream_timestamp_hz !== 1000)) {
    if (Waveform_flush_timer !== null) clearTimeout(Waveform_flush_timer);
    flushWaveformSamples();
  }

  const Rows = Buffer.allocUnsafe(Sample_count * Sample_stride);
  for (let Sample_index = 0; Sample_index < Sample_count; Sample_index += 1) {
    const Row_offset = Sample_index * Sample_stride;
    const Value_offset = 14 + Sample_index * Value_stride;
    Rows.writeUInt32LE((Base_timestamp + Sample_index * Period_ms) >>> 0, Row_offset);
    Payload.copy(Rows, Row_offset + 4, Value_offset, Value_offset + Value_stride);
  }
  Pending_observer_stream_mask = Field_mask;
  Pending_observer_stream_stride = Sample_stride;
  Pending_observer_stream_timestamp_hz = 1000;
  Pending_observer_stream_rows.push(Rows);
  Pending_observer_stream_sample_count += Sample_count;
  if (Pending_observer_stream_sample_count >= 256) {
    if (Waveform_flush_timer !== null) clearTimeout(Waveform_flush_timer);
    flushWaveformSamples();
    return;
  }
  if (Waveform_flush_timer === null) {
    const Flush_interval = Field_count <= Fast_observer_field_limit
      ? Fast_waveform_flush_interval_ms
      : Waveform_flush_interval_ms;
    Waveform_flush_timer = setTimeout(flushWaveformSamples, Flush_interval);
  }
}

/***********************************************
 * @brief : 将20 kHz时间基准的自适应观测批次转换为固定步长IPC二进制行
 * @param : Payload 高分辨率连续观测批次负载
 * @return: 无
 * @date  : 2026-09-16
 * @author: L
 ************************************************/
function queueAdaptiveObserverStreamBatchPayload(Payload) {
  if (!Buffer.isBuffer(Payload) || Payload.length < 9) {
    throw new Error("自适应观测批次负载长度不足 9 字节");
  }
  const Base_timestamp_tick = Payload.readUInt32LE(0);
  const Field_mask = Payload.readUInt16LE(4);
  const Period_tick = Payload.readUInt16LE(6);
  const Sample_count = Payload.readUInt8(8);
  if (!Number.isSafeInteger(Field_mask) ||
      Field_mask < 0 ||
      Field_mask > Observer_stream_field_mask) {
    throw new Error("自适应观测批次包含未知字段");
  }
  if (Period_tick === 0 || Sample_count === 0) {
    throw new Error("自适应观测批次的周期或采样数无效");
  }
  const Field_count = Observer_stream_fields.reduce(
    (Count, _Field, Bit) => Count + (isObserverStreamFieldSelected(Field_mask, Bit) ? 1 : 0),
    0,
  );
  const Value_stride = Field_count * 4;
  const Expected_length = 9 + Sample_count * Value_stride;
  if (Field_count === 0 || Payload.length !== Expected_length) {
    throw new Error(`自适应观测批次负载长度应为 ${Expected_length} 字节`);
  }

  const Sample_stride = 4 + Value_stride;
  if (Pending_observer_stream_rows.length > 0 &&
      (Pending_observer_stream_mask !== Field_mask ||
       Pending_observer_stream_stride !== Sample_stride ||
       Pending_observer_stream_timestamp_hz !== Observer_stream_tick_hz)) {
    if (Waveform_flush_timer !== null) clearTimeout(Waveform_flush_timer);
    flushWaveformSamples();
  }

  const Rows = Buffer.allocUnsafe(Sample_count * Sample_stride);
  for (let Sample_index = 0; Sample_index < Sample_count; Sample_index += 1) {
    const Row_offset = Sample_index * Sample_stride;
    const Value_offset = 9 + Sample_index * Value_stride;
    Rows.writeUInt32LE((Base_timestamp_tick + Sample_index * Period_tick) >>> 0, Row_offset);
    Payload.copy(Rows, Row_offset + 4, Value_offset, Value_offset + Value_stride);
  }
  Pending_observer_stream_mask = Field_mask;
  Pending_observer_stream_stride = Sample_stride;
  Pending_observer_stream_timestamp_hz = Observer_stream_tick_hz;
  Pending_observer_stream_rows.push(Rows);
  Pending_observer_stream_sample_count += Sample_count;
  if (Pending_observer_stream_sample_count >= 256) {
    if (Waveform_flush_timer !== null) clearTimeout(Waveform_flush_timer);
    flushWaveformSamples();
    return;
  }
  if (Waveform_flush_timer === null) {
    const Flush_interval = Field_count <= Fast_observer_field_limit
      ? Fast_waveform_flush_interval_ms
      : Waveform_flush_interval_ms;
    Waveform_flush_timer = setTimeout(flushWaveformSamples, Flush_interval);
  }
}

/***********************************************
 * @brief : 清理高速波形批处理状态
 * @param : Flush 是否先发送剩余样本
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function resetWaveformBatch(Flush) {
  if (Waveform_flush_timer !== null) clearTimeout(Waveform_flush_timer);
  Waveform_flush_timer = null;
  if (Flush) flushWaveformSamples();
  else {
    Pending_waveform_samples = [];
    Pending_observer_stream_rows = [];
  }
  Pending_observer_stream_sample_count = 0;
  Pending_observer_stream_mask = 0;
  Pending_observer_stream_stride = 0;
  Pending_observer_stream_timestamp_hz = 1000;
  Last_waveform_log_time = 0;
}

/***********************************************
 * @brief : 清空本次连接中的乐曲列表收集状态
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function resetMusicTrackCollection() {
  Pending_music_tracks = new Map();
  Expected_music_track_count = 0;
  Music_track_list_complete = false;
}

/***********************************************
 * @brief : 收集单条乐曲信息并在列表完整后通知界面
 * @param : Payload 下位机乐曲信息负载
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function collectMusicTrack(Payload) {
  const Track = decodeSongListPayload(Payload);
  if (Expected_music_track_count !== 0 && Expected_music_track_count !== Track.total) {
    resetMusicTrackCollection();
  }
  Expected_music_track_count = Track.total;
  Pending_music_tracks.set(Track.id, { id: Track.id, name: Track.name });
  if (Music_track_list_complete || Pending_music_tracks.size !== Expected_music_track_count) return;

  Music_track_list_complete = true;
  const Tracks = [...Pending_music_tracks.values()].sort((Left, Right) => Left.id - Right.id);
  sendToRenderer("serial:musicTracks", Tracks);
}

/***********************************************
 * @brief : 向当前物理下位机查询内置乐曲列表
 * @param : 无
 * @return: 查询帧发送完成状态
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function requestMusicTracks() {
  resetMusicTrackCollection();
  if (Simulator_connected || !Active_serial_port?.isOpen) return Promise.resolve();
  const Frame = encodeFrame(Frame_type.songList, Serial_sequence++);
  return writeSerialFrames(Active_serial_port, [Frame]);
}

/***********************************************
 * @brief : 向当前物理下位机读取FOC及无感观测器参数
 * @param : 无
 * @return: 读取帧发送完成状态
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
function requestLoopParameters() {
  if (Simulator_connected || !Active_serial_port?.isOpen) return Promise.resolve();
  const Frames = [
    encodeFrame(Frame_type.parameterRead, Serial_sequence++),
    encodeFrame(Frame_type.observerParameterRead, Serial_sequence++),
  ];
  return writeSerialFrames(Active_serial_port, Frames);
}

/***********************************************
 * @brief : 结束当前FOC环路参数读取等待
 * @param : Error 读取失败原因，成功时为空
 * @param : Parameters 下位机回传的环路参数
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
function finishLoopParameterRequest(Error, Parameters) {
  const Pending_request = Pending_loop_parameter_request;
  if (!Pending_request) return;
  Pending_loop_parameter_request = null;
  clearTimeout(Pending_request.retryTimer);
  clearTimeout(Pending_request.timeoutTimer);
  if (Error) Pending_request.reject(Error);
  else Pending_request.resolve(Parameters);
}

/***********************************************
 * @brief : 读取FOC及无感观测器参数并等待下位机有效回包
 * @param : 无
 * @return: 下位机回传的环路参数
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
function requestLoopParametersWithResponse() {
  if (Pending_loop_parameter_request) {
    finishLoopParameterRequest(new Error("FOC环路参数读取请求已被替换"));
  }
  return new Promise((Resolve, Reject) => {
    const Pending_request = {
      resolve: Resolve,
      reject: Reject,
      retryTimer: null,
      timeoutTimer: null,
    };
    Pending_loop_parameter_request = Pending_request;
    Pending_request.retryTimer = setTimeout(() => {
      if (Pending_loop_parameter_request !== Pending_request) return;
      void requestLoopParameters().catch((Error) => finishLoopParameterRequest(Error));
    }, 200);
    Pending_request.timeoutTimer = setTimeout(() => {
      finishLoopParameterRequest(new Error("下位机未返回FOC环路参数"));
    }, 800);
    void requestLoopParameters().catch((Error) => finishLoopParameterRequest(Error));
  });
}

/***********************************************
 * @brief : 结束等待控制帧发送结果的请求
 * @param : Waiters 等待发送结果的请求列表
 * @param : Error 发送失败原因，成功时为空
 * @return: 无
 * @date  : 2026-09-23
 * @author: L
 ************************************************/
function settleControlWaiters(Waiters, Error) {
  Waiters.forEach((Waiter) => {
    if (Error) Waiter.reject(Error);
    else Waiter.resolve();
  });
}

/***********************************************
 * @brief : 写入一帧控制命令并记录发送中的控制状态
 * @param : Command 待发送的完整控制命令
 * @param : Drain 写入后是否等待系统发送队列排空
 * @return: 控制帧写入完成状态
 * @date  : 2026-09-23
 * @author: L
 ************************************************/
function writeControlCommand(Command, Drain = false) {
  if (Simulator_connected || !Active_serial_port?.isOpen) return Promise.resolve();
  const Port = Active_serial_port;
  const Frame = encodeControlFrame(Command, Serial_sequence++);
  Control_frame_in_flight += 1;
  return writeSerialFrames(Port, [Frame], Drain).finally(() => {
    Control_frame_in_flight = Math.max(0, Control_frame_in_flight - 1);
  });
}

/***********************************************
 * @brief : 刷新合并后的最新控制命令
 * @param : 无
 * @return: 控制帧写入完成状态
 * @date  : 2026-09-23
 * @author: L
 ************************************************/
function flushPendingControlCommand() {
  Control_send_timer = null;
  const Command = Pending_control_command;
  const Waiters = Pending_control_waiters;
  Pending_control_command = null;
  Pending_control_waiters = [];
  if (!Command) {
    settleControlWaiters(Waiters, null);
    return Promise.resolve();
  }

  return writeControlCommand(Command).then(() => {
    settleControlWaiters(Waiters, null);
  }).catch((Error) => {
    settleControlWaiters(Waiters, Error);
    throw Error;
  });
}

/***********************************************
 * @brief : 合并控制命令并按需立即发送
 * @param : Command 待发送的完整控制命令
 * @param : Immediate 是否跳过合并窗口立即发送
 * @param : Drain 立即发送后是否等待系统发送队列排空
 * @return: 控制帧写入完成状态
 * @date  : 2026-09-23
 * @author: L
 ************************************************/
function queueControlCommand(Command, Immediate = false, Drain = false) {
  if (Immediate) {
    if (Control_send_timer !== null) clearTimeout(Control_send_timer);
    Control_send_timer = null;
    const Waiters = Pending_control_waiters;
    Pending_control_command = null;
    Pending_control_waiters = [];
    const Immediate_result = writeControlCommand(Command, Drain);
    return Immediate_result.then(() => {
      settleControlWaiters(Waiters, null);
    }).catch((Error) => {
      settleControlWaiters(Waiters, Error);
      throw Error;
    });
  }

  return new Promise((Resolve, Reject) => {
    /* 新目标会覆盖尚未发出的旧目标，旧请求无需继续占用IPC响应队列。 */
    const Superseded_waiters = Pending_control_waiters;
    Pending_control_waiters = [];
    settleControlWaiters(Superseded_waiters, null);
    Pending_control_command = { ...Command };
    Pending_control_waiters.push({ resolve: Resolve, reject: Reject });
    if (Control_send_timer !== null) return;
    Control_send_timer = setTimeout(() => {
      void flushPendingControlCommand().catch(() => {});
    }, Control_command_coalesce_interval_ms);
  });
}

/***********************************************
 * @brief : 取消尚未写入串口的合并控制命令
 * @param : Error 取消原因
 * @return: 无
 * @date  : 2026-09-23
 * @author: L
 ************************************************/
function cancelPendingControlCommand(Error) {
  if (Control_send_timer !== null) clearTimeout(Control_send_timer);
  Control_send_timer = null;
  const Waiters = Pending_control_waiters;
  Pending_control_command = null;
  Pending_control_waiters = [];
  settleControlWaiters(Waiters, Error);
}

/***********************************************
 * @brief : 发送最近一次控制命令
 * @param : 无
 * @return: 控制命令发送结果
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function sendLatestControlCommand() {
  if (!Latest_control_command || Simulator_connected || !Active_serial_port?.isOpen ||
      Pending_control_command || Control_frame_in_flight)
  {
    return Promise.resolve();
  }
  return writeControlCommand(Latest_control_command);
}

/***********************************************
 * @brief : 立即发送一帧失能命令并等待系统串口发送队列排空
 * @param : Command 已清零目标的失能控制命令
 * @return: 失能帧发出后的完成状态
 * @date  : 2026-09-23
 * @author: L
 ************************************************/
function sendPriorityDisableCommand(Command) {
  if (Simulator_connected || !Active_serial_port?.isOpen) return Promise.resolve();
  return queueControlCommand(Command, true, true);
}

/***********************************************
 * @brief : 按分组发送最近一次FOC或无感观测器参数
 * @param : Parameter_group 参数分组
 * @return: 参数帧发送结果
 * @date  : 2026-09-15
 * @author: L
 ************************************************/
function sendLatestLoopParameters(Parameter_group) {
  if (!Latest_loop_parameters || Simulator_connected || !Active_serial_port?.isOpen) {
    return Promise.resolve();
  }
  const Port = Active_serial_port;
  let Frame;
  if (Parameter_group === "foc") {
    Frame = encodeParameterWriteFrame(Latest_loop_parameters, Serial_sequence++);
  } else if (Parameter_group === "observer") {
    Frame = encodeObserverParameterWriteFrame(Latest_loop_parameters, Serial_sequence++);
  } else {
    return Promise.reject(new Error("未知参数分组"));
  }

  return writeSerialFrames(Port, [Frame]);
}

/***********************************************
 * @brief : 启动独立于界面渲染的控制心跳
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function startControlHeartbeat() {
  if (Control_heartbeat_timer !== null) return;
  Control_heartbeat_timer = setInterval(() => {
    void sendLatestControlCommand().catch((Error) => {
      stopControlHeartbeat();
      sendToRenderer("serial:status", { state: "error", message: `控制心跳发送失败：${Error.message}` });
    });
  }, Control_heartbeat_interval_ms);
}

/***********************************************
 * @brief : 停止控制心跳并清除缓存命令和参数
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function stopControlHeartbeat() {
  if (Control_heartbeat_timer !== null) clearInterval(Control_heartbeat_timer);
  Control_heartbeat_timer = null;
  cancelPendingControlCommand(new Error("控制心跳已停止"));
  Latest_control_command = null;
  Latest_loop_parameters = null;
}

/***********************************************
 * @brief : 安全关闭当前物理串口
 * @param : 无
 * @return: 串口关闭完成状态
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function closeActiveSerialPort() {
  return new Promise((Resolve) => {
    finishLoopParameterRequest(new Error("串口已关闭"));
    stopControlHeartbeat();
    resetWaveformBatch(true);
    resetMusicTrackCollection();
    if (!Active_serial_port || !Active_serial_port.isOpen) {
      Active_serial_port = null;
      Resolve();
      return;
    }
    const Port_to_close = Active_serial_port;
    Active_serial_port = null;
    Port_to_close.close(() => Resolve());
  });
}

/***********************************************
 * @brief : 注册串口数据、错误和断开事件
 * @param : Port 已打开的串口实例
 * @return: 无
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function registerSerialEvents(Port) {
  Port.on("data", (Data) => {
    const Previous_crc_errors = Serial_parser.Crc_error_count;
    const Previous_format_errors = Serial_parser.Format_error_count;
    const Frames = Serial_parser.push(Data);
    if (Serial_parser.Crc_error_count !== Previous_crc_errors || Serial_parser.Format_error_count !== Previous_format_errors) {
      sendToRenderer("serial:stats", {
        crcErrors: Serial_parser.Crc_error_count,
        formatErrors: Serial_parser.Format_error_count,
      });
    }
    for (const Frame of Frames) {
      const Is_waveform = Frame.type === Frame_type.waveform ||
        Frame.type === Frame_type.observerWaveform ||
        Frame.type === Frame_type.observerStream ||
        Frame.type === Frame_type.observerStreamBatch ||
        Frame.type === Frame_type.observerStreamAdaptiveBatch ||
        Frame.type === Frame_type.observerStreamExtendedBatch;
      const Current_time = Date.now();
      if (!Is_waveform || Current_time - Last_waveform_log_time >= 250) {
        sendToRenderer("serial:frame", {
          type: Frame.type,
          sequence: Frame.sequence,
          timestamp: Current_time,
          data: [...Frame.payload],
        });
        if (Is_waveform) Last_waveform_log_time = Current_time;
      }
      if (Frame.type === Frame_type.telemetry) {
        try {
          sendToRenderer("serial:telemetry", decodeTelemetryPayload(Frame.payload));
        } catch (Error) {
          sendToRenderer("serial:status", { state: "error", message: Error.message });
        }
      } else if (Frame.type === Frame_type.waveform) {
        try {
          queueWaveformSample(decodeWaveformPayload(Frame.payload));
        } catch (Error) {
          sendToRenderer("serial:status", { state: "error", message: Error.message });
        }
      } else if (Frame.type === Frame_type.observerWaveform) {
        try {
          queueWaveformSample(decodeObserverWaveformPayload(Frame.payload));
        } catch (Error) {
          sendToRenderer("serial:status", { state: "error", message: Error.message });
        }
      } else if (Frame.type === Frame_type.observerStream) {
        try {
          queueObserverStreamPayload(Frame.payload);
        } catch (Error) {
          sendToRenderer("serial:status", { state: "error", message: Error.message });
        }
      } else if (Frame.type === Frame_type.observerStreamBatch) {
        try {
          queueObserverStreamBatchPayload(Frame.payload);
        } catch (Error) {
          sendToRenderer("serial:status", { state: "error", message: Error.message });
        }
      } else if (Frame.type === Frame_type.observerStreamAdaptiveBatch) {
        try {
          queueAdaptiveObserverStreamBatchPayload(Frame.payload);
        } catch (Error) {
          sendToRenderer("serial:status", { state: "error", message: Error.message });
        }
      } else if (Frame.type === Frame_type.observerStreamExtendedBatch) {
        try {
          queueExtendedObserverStreamBatchPayload(Frame.payload);
        } catch (Error) {
          sendToRenderer("serial:status", { state: "error", message: Error.message });
        }
      } else if (Frame.type === Frame_type.songList) {
        try {
          collectMusicTrack(Frame.payload);
        } catch (Error) {
          sendToRenderer("serial:status", { state: "warning", message: `乐曲列表解析失败：${Error.message}` });
        }
      } else if (Frame.type === Frame_type.parameterWrite) {
        try {
          const Parameters = decodeParameterWritePayload(Frame.payload);
          Latest_loop_parameters = Parameters;
          finishLoopParameterRequest(null, Parameters);
          sendToRenderer("serial:parameters", Parameters);
        } catch (Error) {
          sendToRenderer("serial:status", { state: "warning", message: `FOC参数解析失败：${Error.message}` });
        }
      } else if (Frame.type === Frame_type.observerParameterWrite || Frame.type === Frame_type.observerParameterRead) {
        try {
          const Parameters = decodeObserverParameterPayload(Frame.payload);
          Latest_loop_parameters = { ...(Latest_loop_parameters || {}), ...Parameters };
          sendToRenderer("serial:parameters", Parameters);
        } catch (Error) {
          sendToRenderer("serial:status", { state: "warning", message: `SMO/PLL参数解析失败：${Error.message}` });
        }
      }
    }
  });
  Port.on("error", (Error) => {
    if (Active_serial_port === Port) stopControlHeartbeat();
    sendToRenderer("serial:status", { state: "error", message: `串口错误：${Error.message}` });
  });
  Port.on("close", () => {
    if (Active_serial_port === Port) {
      Active_serial_port = null;
      stopControlHeartbeat();
    }
    sendToRenderer("serial:status", { state: "closed", message: "串口已关闭" });
  });
}

if (!Instance_lock) {
  app.quit();
} else {
  app.on("second-instance", () => focusMainWindow());
  app.whenReady().then(() => {
    createMainWindow();
    app.on("activate", () => {
      if (BrowserWindow.getAllWindows().length === 0) createMainWindow();
    });
  });
}

app.on("window-all-closed", () => {
  void closeActiveSerialPort();
  if (process.platform !== "darwin") app.quit();
});

ipcMain.handle("file:saveCsv", async (_Event, Payload) => {
  const Result = await dialog.showSaveDialog({
    title: "导出波形数据",
    defaultPath: `foc-wave-${new Date().toISOString().slice(0, 19).replaceAll(":", "-")}.csv`,
    filters: [{ name: "CSV 数据", extensions: ["csv"] }],
  });
  if (Result.canceled || !Result.filePath) return false;
  fs.writeFileSync(Result.filePath, Payload, "utf8");
  return true;
});

ipcMain.handle("serial:listPorts", async () => {
  const Ports = await SerialPort.list();
  return [
    { path: "simulator", manufacturer: "内置仿真", simulated: true },
    ...Ports.map((Port) => ({
      path: Port.path,
      manufacturer: Port.manufacturer,
      friendlyName: Port.friendlyName,
      serialNumber: Port.serialNumber,
      vendorId: Port.vendorId,
      productId: Port.productId,
    })),
  ];
});

ipcMain.handle("serial:connect", async (_Event, Config) => {
  await closeActiveSerialPort();
  Simulator_connected = Config.port === "simulator";
  Serial_sequence = 0;
  Serial_parser = new Serial_frame_parser_t();
  resetWaveformBatch(false);
  if (Simulator_connected) return { ok: true, message: "串口仿真设备已连接" };

  try {
    const New_port = new SerialPort({
      path: String(Config.port),
      baudRate: Number(Config.baudRate),
      dataBits: Number(Config.dataBits),
      stopBits: Number(Config.stopBits),
      parity: String(Config.parity),
      autoOpen: false,
    });
    await new Promise((Resolve, Reject) => New_port.open((Error) => Error ? Reject(Error) : Resolve()));
    Active_serial_port = New_port;
    registerSerialEvents(New_port);
    const Parameters = await requestLoopParametersWithResponse();
    await requestMusicTracks();
    return { ok: true, message: `${Config.port} 已打开`, parameters: Parameters };
  } catch (Error) {
    Simulator_connected = false;
    await closeActiveSerialPort();
    return { ok: false, message: `串口打开失败：${Error.message}` };
  }
});

ipcMain.handle("serial:disconnect", async () => {
  Simulator_connected = false;
  await closeActiveSerialPort();
  return { ok: true, message: "串口已断开" };
});

ipcMain.handle("serial:sendFrame", async (_Event, Type, Data) => {
  try {
    const Frame = encodeFrame(Number(Type), Serial_sequence++, Buffer.from(Data));
    if (Simulator_connected) return { ok: true, message: "仿真帧已发送" };
    if (!Active_serial_port?.isOpen) return { ok: false, message: "串口未打开" };
    await writeSerialFrames(Active_serial_port, [Frame]);
    return { ok: true, message: "串口帧已发送" };
  } catch (Error) {
    return { ok: false, message: `串口发送失败：${Error.message}` };
  }
});

ipcMain.handle("serial:sendControl", async (_Event, Command, Priority_disable) => {
  try {
    if (Simulator_connected) return { ok: true, message: "仿真控制命令已发送" };
    if (!Active_serial_port?.isOpen) return { ok: false, message: "串口未打开" };
    const Previous_control_command = Latest_control_command;
    Latest_control_command = { ...Command };
    /* 失能状态也保留控制保活，确保下位机继续发送遥测和参数回传。 */
    startControlHeartbeat();
    const Disable_transition = Command.enabled === false &&
      Previous_control_command?.enabled === true;
    /* 板端超时失能后需要立即完成失能握手，解除重新使能锁。 */
    if ((Priority_disable === true || Disable_transition) && Command.enabled === false) {
      await sendPriorityDisableCommand(Latest_control_command);
      return { ok: true, message: "停止帧已从串口发出" };
    }
    /* 只有从失能切换到使能时立即发帧，其余连续目标更新合并到固定窗口。 */
    const Enable_transition = Command.enabled === true && Previous_control_command?.enabled !== true;
    await queueControlCommand(Latest_control_command, Enable_transition);
    return { ok: true, message: "控制命令已写入串口" };
  } catch (Error) {
    stopControlHeartbeat();
    return { ok: false, message: `控制命令发送失败：${Error.message}` };
  }
});

ipcMain.handle("serial:sendParameters", async (_Event, Parameters, Parameter_group) => {
  try {
    if (Parameter_group !== "foc" && Parameter_group !== "observer") {
      return { ok: false, message: "参数分组无效" };
    }
    Latest_loop_parameters = { ...(Latest_loop_parameters || {}), ...Parameters };
    if (Simulator_connected) return { ok: true, message: "仿真参数已同步" };
    if (!Active_serial_port?.isOpen) return { ok: false, message: "串口未打开" };
    await sendLatestLoopParameters(Parameter_group);
    return {
      ok: true,
      message: Parameter_group === "foc" ? "FOC参数帧已发送" : "SMO/PLL参数帧已发送",
    };
  } catch (Error) {
    return { ok: false, message: `参数发送失败：${Error.message}` };
  }
});
