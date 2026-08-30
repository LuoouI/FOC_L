const Test = require("node:test");
const Assert = require("node:assert/strict");
const {
  Frame_type,
  Serial_frame_parser_t,
  calculateCrc16,
  encodeFrame,
  encodeControlFrame,
  encodeParameterWriteFrame,
  decodeParameterWritePayload,
  decodeTelemetryPayload,
  decodeWaveformPayload,
  decodeSongListPayload,
} = require("../electron/serial/protocol.cjs");

Test("FOC环路参数包含速度斜率、滤波及位置到位参数", () => {
  const Parameters = {
    currentBandwidth: 1000,
    rampRate: 2000,
    speedKp: 0,
    speedKi: 0,
    speedIntegralLimit: 0,
    abFilterBandwidth: 50,
    positionKp: 0,
    positionSoftRange: 0,
    positionSpeedDeadband: 0,
    positionOutputLimit: 0,
    positionDeadband: 0,
  };
  const Data = encodeParameterWriteFrame(Parameters, 1);
  const Decoded = decodeParameterWritePayload(Data.subarray(8, 52));

  Assert.equal(Data.readUInt16LE(6), 44);
  Assert.equal(Data.readFloatLE(12), 2000);
  Assert.equal(Data.readFloatLE(28), 50);
  Assert.equal(Data.readFloatLE(48), 0);
  Assert.equal(Decoded.currentBandwidth, 1000);
  Assert.equal(Decoded.rampRate, 2000);
  Assert.equal(Decoded.abFilterBandwidth, 50);
  Assert.equal(Decoded.positionSoftRange, 0);
  Assert.equal(Decoded.positionSpeedDeadband, 0);
  Assert.equal(Decoded.positionDeadband, 0);
  Assert.equal("speedOutputLimit" in Decoded, false);
});

Test("FOC环路参数回读不会展开单精度浮点尾数", () => {
  const Parameters = {
    currentBandwidth: 1000,
    rampRate: 1500,
    speedKp: 0.03,
    speedKi: 0.05,
    speedIntegralLimit: 0.8,
    abFilterBandwidth: 500,
    positionKp: 0.03,
    positionSoftRange: 12.5,
    positionSpeedDeadband: 1.5,
    positionOutputLimit: 3000,
    positionDeadband: 1.5,
  };
  const Data = encodeParameterWriteFrame(Parameters, 2);
  const Decoded = decodeParameterWritePayload(Data.subarray(8, 52));

  Assert.equal(Decoded.speedKp, 0.03);
  Assert.equal(Decoded.speedKi, 0.05);
  Assert.equal(Decoded.speedIntegralLimit, 0.8);
  Assert.equal(Decoded.abFilterBandwidth, 500);
  Assert.equal(Decoded.positionKp, 0.03);
  Assert.equal(Decoded.positionSoftRange, 12.5);
  Assert.equal(Decoded.positionSpeedDeadband, 1.5);
  Assert.equal(Decoded.positionOutputLimit, 3000);
  Assert.equal(Decoded.positionDeadband, 1.5);
  Assert.equal(Data.readFloatLE(36), 12.5);
  Assert.equal(Data.readFloatLE(40), 1.5);
  Assert.equal(Data.readFloatLE(44), 3000);
  Assert.equal(Data.readFloatLE(48), 1.5);
});

Test("控制命令编码包含模式、标志和目标值", () => {
  const Data = encodeControlFrame({
    enabled: true,
    emergencyStopped: false,
    direction: 1,
    driveMode: "encoderFoc",
    mode: "current",
    iqTarget: -2.5,
    idTarget: -0.25,
    currentBandwidth: 1800,
  }, 513);

  Assert.equal(Data.readUInt8(3), Frame_type.control);
  Assert.equal(Data.readUInt16LE(4), 513);
  Assert.equal(Data.readUInt16LE(6), 16);
  Assert.equal(Data.readUInt8(8), 1);
  Assert.equal(Data.readUInt8(9), 0x03);
  Assert.equal(Data.readUInt8(10), 1);
  Assert.equal(Data.readFloatLE(12), 2.5);
  Assert.equal(Data.readFloatLE(16), -0.25);
  Assert.equal(Data.readUInt16LE(20), 1800);
  Assert.equal(Data.readUInt16LE(24), calculateCrc16(Data.subarray(2, 24)));
});

Test("速度控制命令由目标值符号生成方向位并包含可调速度斜坡", () => {
  const Data = encodeControlFrame({
    enabled: true,
    emergencyStopped: false,
    direction: 1,
    driveMode: "encoderFoc",
    mode: "speed",
    speedTarget: -3000,
    idTarget: 0,
    rampRate: 1500,
  }, 514);

  Assert.equal(Data.readUInt8(9), 0x03);
  Assert.equal(Data.readFloatLE(12), 3000);
  Assert.equal(Data.readFloatLE(20), 1500);
});

Test("速度控制命令缺少斜率时保持未配置状态", () => {
  const Data = encodeControlFrame({
    enabled: true,
    emergencyStopped: false,
    direction: 1,
    driveMode: "encoderFoc",
    mode: "speed",
    speedTarget: 3000,
    idTarget: 0,
  }, 515);

  Assert.equal(Data.readFloatLE(20), 0);
});

Test("位置控制命令使用方向位选择回正方式", () => {
  const Reverse_path_data = encodeControlFrame({
    enabled: true,
    emergencyStopped: false,
    direction: 1,
    positionReturnMode: "reversePath",
    driveMode: "encoderFoc",
    mode: "position",
    positionTarget: 0,
    idTarget: 0,
  }, 516);
  const Shortest_data = encodeControlFrame({
    enabled: true,
    emergencyStopped: false,
    direction: -1,
    positionReturnMode: "shortest",
    driveMode: "encoderFoc",
    mode: "position",
    positionTarget: 0,
    idTarget: 0,
  }, 517);

  Assert.equal(Reverse_path_data.readUInt8(9), 0x03);
  Assert.equal(Shortest_data.readUInt8(9), 0x01);
});

Test("开环命令编码包含 Uq、Angle 和角度步进", () => {
  const Data = encodeControlFrame({
    enabled: true,
    emergencyStopped: false,
    direction: 1,
    driveMode: "openLoop",
    mode: "voltage",
    voltageTarget: 3.5,
    angleTarget: 8192,
    angleStep: 12,
  }, 8);

  Assert.equal(Data.readUInt8(8), 0);
  Assert.equal(Data.readUInt8(10), 0);
  Assert.equal(Data.readFloatLE(12), 3.5);
  Assert.equal(Data.readFloatLE(16), 8192);
  Assert.equal(Data.readFloatLE(20), 12);
});

Test("音乐命令编码包含曲目编号和防重播放会话号", () => {
  const Data = encodeControlFrame({
    enabled: true,
    emergencyStopped: false,
    direction: 1,
    driveMode: "foc_voice",
    mode: "voltage",
    focVoiceSongId: 3,
    focVoiceSession: 0x12345678,
  }, 9);

  Assert.equal(Data.readUInt8(8), 2);
  Assert.equal(Data.readUInt8(9), 0x01);
  Assert.equal(Data.readUInt8(11), 3);
  Assert.equal(Data.readUInt32LE(12), 0x12345678);
  Assert.equal(Data.readUInt32LE(16), 0);
  Assert.equal(Data.readUInt32LE(20), 0);
});

Test("基础遥测报文能够完整解码", () => {
  const Data = Buffer.alloc(56);
  Data.writeUInt8(1, 0);
  Data.writeUInt8(2, 1);
  Data.writeUInt8(0, 2);
  Data.writeUInt8(0x04, 3);
  Data.writeUInt32LE(123456, 4);
  Data.writeFloatLE(3000, 8);
  Data.writeFloatLE(2980.5, 12);
  Data.writeFloatLE(0, 16);
  Data.writeFloatLE(-0.1, 20);
  Data.writeFloatLE(2.5, 24);
  Data.writeFloatLE(2.4, 28);
  Data.writeFloatLE(47.9, 32);
  Data.writeFloatLE(2048, 36);
  Data.writeFloatLE(123.4, 40);
  Data.writeFloatLE(143.8, 44);
  Data.writeFloatLE(0.21, 48);
  Data.writeUInt16LE(2051, 52);
  Data.writeUInt16LE(2049, 54);

  const Result = decodeTelemetryPayload(Data);
  Assert.equal(Result.timestamp, 123.456);
  Assert.equal(Result.musicPlaying, 1);
  Assert.ok(Math.abs(Result.speedActual - 2980.5) < 0.001);
  Assert.ok(Math.abs(Result.busVoltage - 47.9) < 0.001);
  Assert.equal(Result.zeroOffset, 2048);
  Assert.ok(Math.abs(Result.electricalAngle - 143.8) < 0.001);
  Assert.equal(Result.adcRawU, 2051);
  Assert.equal(Result.adcRawW, 2049);
});

Test("高速波形报文能够解码三相电流和PWM", () => {
  const Data = Buffer.alloc(36);
  Data.writeUInt32LE(2500, 0);
  Data.writeFloatLE(1.1, 4);
  Data.writeFloatLE(-0.4, 8);
  Data.writeFloatLE(-0.7, 12);
  Data.writeFloatLE(0.2, 16);
  Data.writeFloatLE(4.8, 20);
  Data.writeFloatLE(48.5, 24);
  Data.writeFloatLE(51.2, 28);
  Data.writeFloatLE(50.3, 32);

  const Result = decodeWaveformPayload(Data);
  Assert.equal(Result.timestamp, 2.5);
  Assert.ok(Math.abs(Result.ia - 1.1) < 0.001);
  Assert.ok(Math.abs(Result.uq - 4.8) < 0.001);
  Assert.ok(Math.abs(Result.dutyB - 51.2) < 0.001);
});

Test("乐曲列表报文能够解码UTF-8曲名", () => {
  const Name = Buffer.from("天使的翅膀（片段）", "utf8");
  const Data = Buffer.concat([Buffer.from([4, 4, Name.length]), Name]);
  const Result = decodeSongListPayload(Data);

  Assert.equal(Frame_type.songList, 0x14);
  Assert.deepEqual(Result, { id: 4, total: 4, name: "天使的翅膀（片段）" });
});

Test("零点校准使用独立的一次性命令帧", () => {
  const Data = encodeFrame(Frame_type.zeroCalibration, 10);

  Assert.equal(Frame_type.zeroCalibration, 0x15);
  Assert.equal(Data.readUInt8(3), 0x15);
  Assert.equal(Data.readUInt16LE(6), 0);
});

Test("乐曲列表报文拒绝不完整的UTF-8曲名", () => {
  const Data = Buffer.from([1, 1, 2, 0xe5, 0xa4]);
  Assert.throws(() => decodeSongListPayload(Data), /UTF-8/);
});

Test("流式解析器能够处理分包和前导干扰字节", () => {
  const Parser = new Serial_frame_parser_t();
  const Frame = encodeFrame(Frame_type.heartbeat, 9, Buffer.from([1, 2, 3, 4]));
  Assert.equal(Parser.push(Buffer.concat([Buffer.from([0x00, 0xff]), Frame.subarray(0, 5)])).length, 0);
  const Result = Parser.push(Frame.subarray(5));
  Assert.equal(Result.length, 1);
  Assert.equal(Result[0].type, Frame_type.heartbeat);
  Assert.equal(Result[0].sequence, 9);
  Assert.deepEqual([...Result[0].payload], [1, 2, 3, 4]);
});

Test("流式解析器拒绝 CRC 错误帧并恢复同步", () => {
  const Parser = new Serial_frame_parser_t();
  const Broken_frame = encodeFrame(Frame_type.log, 1, Buffer.from([0x55]));
  Broken_frame[8] ^= 0xff;
  const Good_frame = encodeFrame(Frame_type.heartbeat, 2, Buffer.from([0xaa]));
  const Result = Parser.push(Buffer.concat([Broken_frame, Good_frame]));
  Assert.equal(Parser.Crc_error_count, 1);
  Assert.equal(Result.length, 1);
  Assert.equal(Result[0].sequence, 2);
});
