const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("focBridge", {
  saveCsv: (Content) => ipcRenderer.invoke("file:saveCsv", Content),
  listSerialPorts: () => ipcRenderer.invoke("serial:listPorts"),
  connectSerial: (Config) => ipcRenderer.invoke("serial:connect", Config),
  disconnectSerial: () => ipcRenderer.invoke("serial:disconnect"),
  sendSerialFrame: (Type, Data) => ipcRenderer.invoke("serial:sendFrame", Type, Data),
  sendControl: (Command) => ipcRenderer.invoke("serial:sendControl", Command),
  sendParameters: (Parameters) => ipcRenderer.invoke("serial:sendParameters", Parameters),
  onSerialFrame: (Callback) => {
    const Handler = (_Event, Frame) => Callback(Frame);
    ipcRenderer.on("serial:frame", Handler);
    return () => ipcRenderer.removeListener("serial:frame", Handler);
  },
  onSerialTelemetry: (Callback) => {
    const Handler = (_Event, Telemetry) => Callback(Telemetry);
    ipcRenderer.on("serial:telemetry", Handler);
    return () => ipcRenderer.removeListener("serial:telemetry", Handler);
  },
  onSerialWaveformBatch: (Callback) => {
    const Handler = (_Event, Samples) => Callback(Samples);
    ipcRenderer.on("serial:waveformBatch", Handler);
    return () => ipcRenderer.removeListener("serial:waveformBatch", Handler);
  },
  onSerialParameters: (Callback) => {
    const Handler = (_Event, Parameters) => Callback(Parameters);
    ipcRenderer.on("serial:parameters", Handler);
    return () => ipcRenderer.removeListener("serial:parameters", Handler);
  },
  onSerialMusicTracks: (Callback) => {
    const Handler = (_Event, Tracks) => Callback(Tracks);
    ipcRenderer.on("serial:musicTracks", Handler);
    return () => ipcRenderer.removeListener("serial:musicTracks", Handler);
  },
  onSerialStatus: (Callback) => {
    const Handler = (_Event, Status) => Callback(Status);
    ipcRenderer.on("serial:status", Handler);
    return () => ipcRenderer.removeListener("serial:status", Handler);
  },
  onSerialStats: (Callback) => {
    const Handler = (_Event, Stats) => Callback(Stats);
    ipcRenderer.on("serial:stats", Handler);
    return () => ipcRenderer.removeListener("serial:stats", Handler);
  },
});
