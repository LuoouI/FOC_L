const Protocol_version = 1;
const Frame_header = Buffer.from([0xaa, 0x55]);
const Maximum_payload_length = 1024;
const Parameter_write_length = 44;
const Speed_ramp_minimum = 1;
const Speed_ramp_maximum = 100000;
const Speed_ramp_default = 0;

const Frame_type = Object.freeze({
  handshake: 0x01,
  heartbeat: 0x02,
  control: 0x10,
  parameterRead: 0x11,
  parameterWrite: 0x12,
  songList: 0x14,
  zeroCalibration: 0x15,
  telemetry: 0x20,
  waveform: 0x21,
  fault: 0x30,
  log: 0x31,
});

const Control_mode = Object.freeze({
  voltage: 0,
  current: 1,
  speed: 2,
  position: 3,
});

const Drive_mode = Object.freeze({
  openLoop: 0,
  encoderFoc: 1,
  foc_voice: 2,
  sensorlessFoc: 3,
});

/***********************************************
 * @brief : 计算串口帧的 CRC16-Modbus 校验值
 * @param : Data 待校验数据
 * @return: 16 位校验值
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function calculateCrc16(Data) {
  let Crc_value = 0xffff;
  for (const Byte of Data) {
    Crc_value ^= Byte;
    for (let Bit_index = 0; Bit_index < 8; Bit_index += 1) {
      Crc_value = (Crc_value & 1) !== 0 ? (Crc_value >>> 1) ^ 0xa001 : Crc_value >>> 1;
    }
  }
  return Crc_value & 0xffff;
}

/***********************************************
 * @brief : 将负载封装成完整串口协议帧
 * @param : Type 帧类型
 * @param : Sequence 帧序号
 * @param : Payload 负载数据
 * @return: 包含帧头和 CRC 的完整报文
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function encodeFrame(Type, Sequence, Payload = Buffer.alloc(0)) {
  const Payload_buffer = Buffer.isBuffer(Payload) ? Payload : Buffer.from(Payload);
  if (Payload_buffer.length > Maximum_payload_length) throw new Error("串口帧负载超过 1024 字节");
  const Frame = Buffer.alloc(10 + Payload_buffer.length);
  Frame_header.copy(Frame, 0);
  Frame.writeUInt8(Protocol_version, 2);
  Frame.writeUInt8(Type & 0xff, 3);
  Frame.writeUInt16LE(Sequence & 0xffff, 4);
  Frame.writeUInt16LE(Payload_buffer.length, 6);
  Payload_buffer.copy(Frame, 8);
  Frame.writeUInt16LE(calculateCrc16(Frame.subarray(2, 8 + Payload_buffer.length)), 8 + Payload_buffer.length);
  return Frame;
}

/***********************************************
 * @brief : 将电机控制命令编码成串口协议帧
 * @param : Command 电机控制命令
 * @param : Sequence 帧序号
 * @return: 完整控制报文
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function encodeControlFrame(Command, Sequence) {
  const Payload = Buffer.alloc(16);
  const Drive_mode_value = Drive_mode[Command.driveMode] ?? Drive_mode.encoderFoc;
  const Position_control =
    (Drive_mode_value === Drive_mode.encoderFoc ||
     Drive_mode_value === Drive_mode.sensorlessFoc) &&
    Command.mode === "position";
  const Signed_target_control =
    (Drive_mode_value === Drive_mode.encoderFoc ||
     Drive_mode_value === Drive_mode.sensorlessFoc) &&
    (Command.mode === "current" || Command.mode === "speed");
  const Direction_target = Command.mode === "current"
    ? Command.iqTarget
    : Command.speedTarget;
  let Flags = 0;
  if (Command.enabled) Flags |= 0x01;
  if ((Position_control && Command.positionReturnMode === "reversePath") ||
      (Signed_target_control && Direction_target < 0) ||
      (!Position_control && !Signed_target_control && Command.direction < 0)) Flags |= 0x02;
  if (Command.emergencyStopped) Flags |= 0x04;

  const Foc_target_map = {
    voltage: Command.voltageTarget,
    current: Math.abs(Command.iqTarget),
    speed: Math.abs(Command.speedTarget),
    position: Command.positionTarget,
  };

  const Primary_target = Drive_mode_value === Drive_mode.openLoop
    ? Command.voltageTarget
    : Foc_target_map[Command.mode] ?? 0;
  const Secondary_target = Drive_mode_value === Drive_mode.openLoop
    ? Command.angleTarget
    : Command.mode === "voltage"
        ? Command.udTarget
        : Command.idTarget;
  const Foc_bandwidth = Number.isFinite(Number(Command.currentBandwidth)) ? Number(Command.currentBandwidth) : 0;
  const Speed_ramp = Number(Command.rampRate);
  const Rate_target = Drive_mode_value === Drive_mode.openLoop
    ? Command.angleStep
    : Number.isFinite(Speed_ramp)
      ? Speed_ramp === 0
        ? 0
        : Math.min(Speed_ramp_maximum, Math.max(Speed_ramp_minimum, Speed_ramp))
      : Speed_ramp_default;

  Payload.writeUInt8(Drive_mode_value, 0);
  Payload.writeUInt8(Flags, 1);
  Payload.writeUInt8(Control_mode[Command.mode] ?? Control_mode.voltage, 2);
  if (Drive_mode_value === Drive_mode.foc_voice) {
    Payload.writeUInt8(Number(Command.focVoiceSongId) & 0xff, 3);
    Payload.writeUInt32LE(Number(Command.focVoiceSession) >>> 0, 4);
    Payload.writeUInt32LE(0, 8);
    Payload.writeUInt32LE(0, 12);
  } else {
    Payload.writeUInt8(0, 3);
    Payload.writeFloatLE(Primary_target ?? 0, 4);
    Payload.writeFloatLE(Secondary_target ?? 0, 8);
    if (Command.mode === "current" && Drive_mode_value !== Drive_mode.openLoop) {
      Payload.writeUInt16LE(Math.min(5000, Math.max(0, Math.round(Foc_bandwidth))), 12);
      Payload.writeUInt16LE(0, 14);
    } else {
      Payload.writeFloatLE(Rate_target ?? 0, 12);
    }
  }
  return encodeFrame(Frame_type.control, Sequence, Payload);
}

/***********************************************
 * @brief : 将FOC各控制环参数编码成参数写入帧
 * @param : Parameters 电流、速度和位置环参数
 * @param : Sequence 帧序号
 * @return: 完整参数写入报文
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
function encodeParameterWriteFrame(Parameters, Sequence) {
  const Payload = Buffer.alloc(Parameter_write_length);
  const Current_bandwidth = Number(Parameters.currentBandwidth);
  const Speed_ramp = Number(Parameters.rampRate);
  Payload.writeUInt16LE(
    Math.min(5000, Math.max(0, Number.isFinite(Current_bandwidth) ? Math.round(Current_bandwidth) : 0)),
    0,
  );
  Payload.writeUInt16LE(0, 2);
  Payload.writeFloatLE(
    Number.isFinite(Speed_ramp)
      ? Speed_ramp === 0
        ? 0
        : Math.min(Speed_ramp_maximum, Math.max(Speed_ramp_minimum, Speed_ramp))
      : Speed_ramp_default,
    4,
  );
  const Ab_filter_bandwidth = Number(Parameters.abFilterBandwidth);
  Payload.writeFloatLE(
    Math.min(500, Math.max(1, Number.isFinite(Ab_filter_bandwidth) ? Ab_filter_bandwidth : 50)),
    20,
  );

  /* 保持44字节参数帧兼容，复用位置环旧积分字段传递到位参数。 */
  const Parameter_offsets = [
    [8, Parameters.speedKp, 100],
    [12, Parameters.speedKi, 100],
    [16, Parameters.speedIntegralLimit, 5],
    [24, Parameters.positionKp, 100],
    [28, Parameters.positionSoftRange, 180],
    [32, Parameters.positionSpeedDeadband, 100],
    [36, Parameters.positionOutputLimit, 30000],
    [40, Parameters.positionDeadband, 180],
  ];
  for (const [Offset, Value, Maximum] of Parameter_offsets) {
    const Number_value = Number(Value);
    const Limited_value = Number.isFinite(Number_value)
      ? Math.min(Maximum, Math.max(0, Number_value))
      : 0;
    Payload.writeFloatLE(Limited_value, Offset);
  }
  return encodeFrame(Frame_type.parameterWrite, Sequence, Payload);
}

/***********************************************
 * @brief : 解码遥测帧中的电机运行数据
 * @param : Payload 遥测负载
 * @return: 解码后的遥测字段
 * @date  : 2026-07-22
 * @author: LYF
 ************************************************/
function decodeTelemetryPayload(Payload) {
  if (!Buffer.isBuffer(Payload) || Payload.length < 56) throw new Error("遥测负载长度不足 56 字节");
  const Flags = Payload.readUInt8(3);
  return {
    state: Payload.readUInt8(0),
    mode: Payload.readUInt8(1),
    fault: Payload.readUInt8(2),
    flags: Flags,
    musicPlaying: (Flags & 0x04) !== 0 ? 1 : 0,
    timestamp: Payload.readUInt32LE(4) / 1000,
    speedTarget: Payload.readFloatLE(8),
    speedActual: Payload.readFloatLE(12),
    idTarget: Payload.readFloatLE(16),
    idActual: Payload.readFloatLE(20),
    iqTarget: Payload.readFloatLE(24),
    iqActual: Payload.readFloatLE(28),
    busVoltage: Payload.readFloatLE(32),
    zeroOffset: Payload.readFloatLE(36),
    mechanicalAngle: Payload.readFloatLE(40),
    electricalAngle: Payload.readFloatLE(44),
    torque: Payload.readFloatLE(48),
    adcRawU: Payload.readUInt16LE(52),
    adcRawW: Payload.readUInt16LE(54),
  };
}

/***********************************************
 * @brief : 解码高速波形帧中的单个采样点
 * @param : Payload 高速波形负载
 * @return: 解码后的高速波形字段
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function decodeWaveformPayload(Payload) {
  if (!Buffer.isBuffer(Payload) || Payload.length < 36) throw new Error("波形负载长度不足 36 字节");
  return {
    timestamp: Payload.readUInt32LE(0) / 1000,
    ia: Payload.readFloatLE(4),
    ib: Payload.readFloatLE(8),
    ic: Payload.readFloatLE(12),
    ud: Payload.readFloatLE(16),
    uq: Payload.readFloatLE(20),
    dutyA: Payload.readFloatLE(24),
    dutyB: Payload.readFloatLE(28),
    dutyC: Payload.readFloatLE(32),
  };
}

/***********************************************
 * @brief : 解码下位机返回的FOC环路参数
 * @param : Payload 44字节参数负载
 * @return: 解码后的电流、速度和位置环参数
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
function decodeParameterWritePayload(Payload) {
  if (!Buffer.isBuffer(Payload) || Payload.length !== Parameter_write_length) throw new Error("参数负载长度必须为 44 字节");

  const Read_parameter = (Offset) => Number(Payload.readFloatLE(Offset).toFixed(6));
  return {
    currentBandwidth: Payload.readUInt16LE(0),
    rampRate: Read_parameter(4),
    speedKp: Read_parameter(8),
    speedKi: Read_parameter(12),
    speedIntegralLimit: Read_parameter(16),
    abFilterBandwidth: Read_parameter(20),
    positionKp: Read_parameter(24),
    positionSoftRange: Read_parameter(28),
    positionSpeedDeadband: Read_parameter(32),
    positionOutputLimit: Read_parameter(36),
    positionDeadband: Read_parameter(40),
  };
}

/***********************************************
 * @brief : 解码下位机返回的单条内置乐曲信息
 * @param : Payload 曲目编号、总数、名称长度和UTF-8名称
 * @return: 解码后的曲目信息
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
function decodeSongListPayload(Payload) {
  if (!Buffer.isBuffer(Payload) || Payload.length < 3) throw new Error("乐曲列表负载长度不足 3 字节");
  const Id = Payload.readUInt8(0);
  const Total = Payload.readUInt8(1);
  const Name_length = Payload.readUInt8(2);
  if (Total === 0 || Id === 0 || Id > Total) throw new Error("乐曲列表编号或总数无效");
  if (Name_length === 0 || Payload.length !== 3 + Name_length) throw new Error("乐曲名称长度无效");
  const Name = Payload.subarray(3).toString("utf8");
  if (Name.length === 0 || Name.includes("\ufffd")) throw new Error("乐曲名称不是有效的UTF-8文本");
  return { id: Id, total: Total, name: Name };
}

class Serial_frame_parser_t {
  constructor() {
    this.Receive_buffer = Buffer.alloc(0);
    this.Crc_error_count = 0;
    this.Format_error_count = 0;
  }

  /***********************************************
   * @brief : 输入任意长度串口数据并提取完整协议帧
   * @param : Data 新收到的串口字节
   * @return: 本次解析出的完整帧数组
   * @date  : 2026-07-22
   * @author: LYF
   ************************************************/
  push(Data) {
    this.Receive_buffer = Buffer.concat([this.Receive_buffer, Buffer.from(Data)]);
    const Frames = [];

    while (this.Receive_buffer.length >= 2) {
      const Header_index = this.Receive_buffer.indexOf(Frame_header);
      if (Header_index < 0) {
        this.Receive_buffer = this.Receive_buffer.at(-1) === Frame_header[0] ? this.Receive_buffer.subarray(-1) : Buffer.alloc(0);
        break;
      }
      if (Header_index > 0) this.Receive_buffer = this.Receive_buffer.subarray(Header_index);
      if (this.Receive_buffer.length < 8) break;

      const Payload_length = this.Receive_buffer.readUInt16LE(6);
      if (Payload_length > Maximum_payload_length) {
        this.Format_error_count += 1;
        this.Receive_buffer = this.Receive_buffer.subarray(1);
        continue;
      }

      const Frame_length = 10 + Payload_length;
      if (this.Receive_buffer.length < Frame_length) break;
      const Expected_crc = this.Receive_buffer.readUInt16LE(8 + Payload_length);
      const Actual_crc = calculateCrc16(this.Receive_buffer.subarray(2, 8 + Payload_length));
      if (Expected_crc !== Actual_crc || this.Receive_buffer.readUInt8(2) !== Protocol_version) {
        this.Crc_error_count += 1;
        this.Receive_buffer = this.Receive_buffer.subarray(1);
        continue;
      }

      Frames.push({
        version: this.Receive_buffer.readUInt8(2),
        type: this.Receive_buffer.readUInt8(3),
        sequence: this.Receive_buffer.readUInt16LE(4),
        payload: Buffer.from(this.Receive_buffer.subarray(8, 8 + Payload_length)),
        raw: Buffer.from(this.Receive_buffer.subarray(0, Frame_length)),
      });
      this.Receive_buffer = this.Receive_buffer.subarray(Frame_length);
    }

    return Frames;
  }
}

module.exports = {
  Protocol_version,
  Frame_header,
  Frame_type,
  Control_mode,
  Drive_mode,
  Serial_frame_parser_t,
  calculateCrc16,
  encodeFrame,
  encodeControlFrame,
  encodeParameterWriteFrame,
  Parameter_write_length,
  decodeTelemetryPayload,
  decodeWaveformPayload,
  decodeParameterWritePayload,
  decodeSongListPayload,
};
