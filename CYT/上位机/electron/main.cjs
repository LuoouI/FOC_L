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
  decodeTelemetryPayload,
  decodeWaveformPayload,
  decodeParameterWritePayload,
  decodeSongListPayload,
} = require("./serial/protocol.cjs");

app.setName("FOC_L");
app.setAppUserModelId("FOC_L");

let Main_window = null;
let Active_serial_port = null;
let Simulator_connected = false;
let Serial_sequence = 0;
let Serial_parser = new Serial_frame_parser_t();
let Pending_waveform_samples = [];
let Waveform_flush_timer = null;
let Last_waveform_log_time = 0;
let Latest_control_command = null;
let Latest_loop_parameters = null;
let Pending_loop_parameter_request = null;
let Control_heartbeat_timer = null;
let Pending_music_tracks = new Map();
let Expected_music_track_count = 0;
let Music_track_list_complete = false;
const Control_heartbeat_interval_ms = 50;
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
 * @brief : 将待发送的高速波形样本批量交给渲染进程
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function flushWaveformSamples() {
  Waveform_flush_timer = null;
  if (Pending_waveform_samples.length === 0) return;
  const Samples = Pending_waveform_samples;
  Pending_waveform_samples = [];
  sendToRenderer("serial:waveformBatch", Samples);
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
  if (Waveform_flush_timer === null) Waveform_flush_timer = setTimeout(flushWaveformSamples, 16);
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
  else Pending_waveform_samples = [];
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
  return new Promise((Resolve, Reject) => {
    Active_serial_port.write(Frame, (Error) => Error ? Reject(Error) : Resolve());
  });
}

/***********************************************
 * @brief : 向当前物理下位机读取FOC环路参数
 * @param : 无
 * @return: 读取帧发送完成状态
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
function requestLoopParameters() {
  if (Simulator_connected || !Active_serial_port?.isOpen) return Promise.resolve();
  const Frame = encodeFrame(Frame_type.parameterRead, Serial_sequence++);
  return new Promise((Resolve, Reject) => {
    Active_serial_port.write(Frame, (Error) => Error ? Reject(Error) : Resolve());
  });
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
 * @brief : 读取FOC环路参数并等待下位机有效回包
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
 * @brief : 发送最近一次控制命令
 * @param : 无
 * @return: 控制命令发送结果
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function sendLatestControlCommand() {
  if (!Latest_control_command || Simulator_connected || !Active_serial_port?.isOpen) {
    return Promise.resolve();
  }
  const Frame = encodeControlFrame(Latest_control_command, Serial_sequence++);
  return new Promise((Resolve, Reject) => {
    Active_serial_port.write(Frame, (Error) => Error ? Reject(Error) : Resolve());
  });
}

/***********************************************
 * @brief : 发送最近一次FOC环路参数
 * @param : 无
 * @return: 参数帧发送结果
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
function sendLatestLoopParameters() {
  if (!Latest_loop_parameters || Simulator_connected || !Active_serial_port?.isOpen) {
    return Promise.resolve();
  }
  const Frame = encodeParameterWriteFrame(Latest_loop_parameters, Serial_sequence++);
  return new Promise((Resolve, Reject) => {
    Active_serial_port.write(Frame, (Error) => Error ? Reject(Error) : Resolve());
  });
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
 * @brief : 停止控制心跳并清除缓存命令
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function stopControlHeartbeat() {
  if (Control_heartbeat_timer !== null) clearInterval(Control_heartbeat_timer);
  Control_heartbeat_timer = null;
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
      const Is_waveform = Frame.type === Frame_type.waveform;
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
    await new Promise((Resolve, Reject) => Active_serial_port.write(Frame, (Error) => Error ? Reject(Error) : Resolve()));
    return { ok: true, message: "串口帧已发送" };
  } catch (Error) {
    return { ok: false, message: `串口发送失败：${Error.message}` };
  }
});

ipcMain.handle("serial:sendControl", async (_Event, Command) => {
  try {
    if (Simulator_connected) return { ok: true, message: "仿真控制命令已发送" };
    if (!Active_serial_port?.isOpen) return { ok: false, message: "串口未打开" };
    Latest_control_command = { ...Command };
    startControlHeartbeat();
    await sendLatestControlCommand();
    return { ok: true, message: "控制命令已发送" };
  } catch (Error) {
    stopControlHeartbeat();
    return { ok: false, message: `控制命令发送失败：${Error.message}` };
  }
});

ipcMain.handle("serial:sendParameters", async (_Event, Parameters) => {
  try {
    Latest_loop_parameters = { ...Parameters };
    if (Simulator_connected) return { ok: true, message: "仿真参数已同步" };
    if (!Active_serial_port?.isOpen) return { ok: false, message: "串口未打开" };
    await sendLatestLoopParameters();
    return { ok: true, message: "FOC环路参数已同步" };
  } catch (Error) {
    return { ok: false, message: `参数发送失败：${Error.message}` };
  }
});
