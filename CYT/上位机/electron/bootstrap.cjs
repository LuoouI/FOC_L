const { app, crashReporter, dialog } = require("electron");
const fs = require("fs");
const os = require("os");
const path = require("path");

app.setName("FOC_L");
app.setAppLogsPath();

const Log_directory = app.getPath("logs");
const Startup_log_path = path.join(Log_directory, "startup.log");
const Safe_mode = process.argv.includes("--safe-mode") || app.commandLine.hasSwitch("disable-gpu");
let Fatal_error_reported = false;

/***********************************************
 * @brief : 记录启动阶段及异常退出信息，不记录串口控制或遥测数据
 * @param : Stage 启动阶段或异常来源
 * @param : Details 可序列化的诊断信息
 * @return: 无；日志写入失败时将原因写入标准错误流
 * @date  : 2026-11-16
 * @author: L
 ************************************************/
function writeStartupLog(Stage, Details) {
  try {
    fs.appendFileSync(
      Startup_log_path,
      `${new Date().toISOString()} [${process.pid}] ${Stage} ${JSON.stringify(Details)}\n`,
      "utf8",
    );
  } catch (Error) {
    console.error("启动日志写入失败：", Error);
  }
}

/***********************************************
 * @brief : 保存无法继续运行的异常并提示日志位置，随后终止进程
 * @param : Stage 异常发生阶段
 * @param : Failure 捕获的异常或进程退出信息
 * @return: 无
 * @date  : 2026-11-16
 * @author: L
 ************************************************/
function reportFatalError(Stage, Failure) {
  const Details = Failure instanceof Error ? Failure.stack || Failure.message : String(Failure);
  writeStartupLog(Stage, Details);
  if (Fatal_error_reported) return;
  Fatal_error_reported = true;
  try {
    dialog.showErrorBox(
      "FOC_L 启动或运行失败",
      `程序无法继续运行。\n\n${Details}\n\n请提供以下目录中的日志：\n${Log_directory}`,
    );
  } finally {
    app.exit(1);
  }
}

process.on("uncaughtException", (Error) => reportFatalError("uncaught-exception", Error));
process.on("unhandledRejection", (Reason) => reportFatalError("unhandled-rejection", Reason));

try {
  fs.mkdirSync(Log_directory, { recursive: true });
  app.commandLine.appendSwitch("enable-logging", "file");
  app.commandLine.appendSwitch("log-file", path.join(Log_directory, "chromium.log"));
  if (Safe_mode) app.disableHardwareAcceleration();
  writeStartupLog("starting", {
    version: app.getVersion(),
    electron: process.versions.electron,
    node: process.versions.node,
    platform: process.platform,
    arch: process.arch,
    windowsVersion: os.release(),
    packaged: app.isPackaged,
    safeMode: Safe_mode,
  });
  crashReporter.start({ uploadToServer: false });
  writeStartupLog("crash-reports", app.getPath("crashDumps"));

  app.on("ready", () => writeStartupLog("ready", app.getGPUFeatureStatus()));
  app.on("web-contents-created", (_Event, Contents) => {
    /* 在加载界面前登记异常，覆盖预加载失败和渲染进程直接退出。 */
    Contents.on("did-finish-load", () => writeStartupLog("window-loaded", Contents.id));
    Contents.on("did-fail-load", (_Load_event, Code, Description, _Url, Is_main_frame) => {
      if (Is_main_frame) writeStartupLog("window-load-failed", { code: Code, description: Description });
    });
    Contents.on("preload-error", (_Preload_event, _Preload_path, Error) => {
      reportFatalError("preload-failed", Error);
    });
    Contents.on("render-process-gone", (_Render_event, Details) => {
      writeStartupLog("render-process-gone", Details);
      if (Details.reason !== "clean-exit" && Details.reason !== "killed") {
        reportFatalError("renderer-failed", JSON.stringify(Details));
      }
    });
  });
  app.on("child-process-gone", (_Event, Details) => writeStartupLog("child-process-gone", Details));
  app.on("quit", (_Event, Exit_code) => writeStartupLog("quit", Exit_code));

  /* 先安装日志与异常处理，再加载会初始化串口原生模块的业务入口。 */
  require("./main.cjs");
} catch (Error) {
  reportFatalError("startup-failed", Error);
}
