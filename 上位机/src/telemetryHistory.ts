import type { Telemetry_t } from "./types";

type Telemetry_history_listener_t = () => void;
const Display_notify_interval_ms = 33;
const Timestamp_reset_threshold_s = 5;

/*===========================================================================*/
/*  示波器遥测环形缓冲                                                       */
/*===========================================================================*/
export class Telemetry_history_t {
  private readonly Capacity: number;
  private readonly Storage: Array<Telemetry_t | undefined>;
  private readonly Listeners = new Set<Telemetry_history_listener_t>();
  private Start = 0;
  private Count = 0;
  private Revision = 0;
  private Notify_timer: number | null = null;

  /***********************************************
   * @brief : 创建固定容量的遥测环形缓冲
   * @param : Capacity 最大采样点数
   * @return: 遥测环形缓冲实例
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  constructor(Capacity: number) {
    this.Capacity = Math.max(1, Math.floor(Capacity));
    this.Storage = new Array<Telemetry_t | undefined>(this.Capacity);
  }

  /***********************************************
   * @brief : 订阅适合界面绘制频率的缓冲更新
   * @param : Listener 更新回调
   * @return: 取消订阅函数
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  subscribe = (Listener: Telemetry_history_listener_t) => {
    this.Listeners.add(Listener);
    return () => this.Listeners.delete(Listener);
  };

  /***********************************************
   * @brief : 获取缓冲修订号
   * @return: 当前修订号
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  getRevision = () => this.Revision;

  /***********************************************
   * @brief : 获取当前采样点数量
   * @return: 当前采样点数量
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  getCount() {
    return this.Count;
  }

  /***********************************************
   * @brief : 获取最早的采样点
   * @return: 最早采样点或空值
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  getFirst() {
    return this.Get_sample(0);
  }

  /***********************************************
   * @brief : 获取最新的采样点
   * @return: 最新采样点或空值
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  getLast() {
    return this.Get_sample(this.Count - 1);
  }

  /***********************************************
   * @brief : 追加采样并在满载时覆盖最早数据
   * @param : Sample 新采样点
   * @return: 无
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  append(Sample: Telemetry_t) {
    this.appendBatch([Sample]);
  }

  /***********************************************
   * @brief : 批量追加采样并只触发一次显示通知
   * @param : Samples 新采样点数组
   * @return: 无
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  appendBatch(Samples: Telemetry_t[]) {
    if (Samples.length === 0) return;
    let Previous_timestamp = this.getLast()?.timestamp;
    let Append_count = 0;
    let Reset_count = 0;
    Samples.forEach((Sample) => {
      if (Previous_timestamp !== undefined && Sample.timestamp < Previous_timestamp) {
        /* 少量倒序来自遥测和波形分批传输，只丢弃过期点，不能清空整段波形。 */
        if (Previous_timestamp - Sample.timestamp < Timestamp_reset_threshold_s) return;
        this.Storage.fill(undefined);
        this.Start = 0;
        this.Count = 0;
        Reset_count += 1;
      }
      let Write_index = (this.Start + this.Count) % this.Capacity;
      if (this.Count < this.Capacity) {
        this.Count += 1;
      } else {
        Write_index = this.Start;
        this.Start = (this.Start + 1) % this.Capacity;
      }
      this.Storage[Write_index] = Sample;
      Previous_timestamp = Sample.timestamp;
      Append_count += 1;
    });
    if (Append_count === 0 && Reset_count === 0) return;
    this.Revision += Append_count + Reset_count;
    this.Schedule_notify();
  }

  /***********************************************
   * @brief : 清空全部历史采样
   * @return: 无
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  clear() {
    if (this.Count === 0) return;
    this.Storage.fill(undefined);
    this.Start = 0;
    this.Count = 0;
    this.Revision += 1;
    this.Notify_now();
  }

  /*
   * 原始采集和显示刷新使用不同节奏，避免高速波形逐点触发界面重绘。
   */
  private Schedule_notify() {
    if (this.Notify_timer !== null || this.Listeners.size === 0) return;
    this.Notify_timer = window.setTimeout(() => {
      this.Notify_timer = null;
      this.Listeners.forEach((Listener) => Listener());
    }, Display_notify_interval_ms);
  }

  /***********************************************
   * @brief : 立即通知缓冲发生变化
   * @return: 无
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  private Notify_now() {
    if (this.Notify_timer !== null) window.clearTimeout(this.Notify_timer);
    this.Notify_timer = null;
    this.Listeners.forEach((Listener) => Listener());
  }

  /***********************************************
   * @brief : 提取指定时间范围内的有序采样
   * @param : Start_time 起始时间
   * @param : End_time 结束时间
   * @return: 时间范围内的采样副本
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  getRange(Start_time: number, End_time: number) {
    if (this.Count === 0 || End_time < Start_time) return [];
    const Start_index = this.Find_time_index(Start_time, false);
    const End_index = this.Find_time_index(End_time, true);
    const Samples: Telemetry_t[] = [];
    for (let Index = Start_index; Index < End_index; Index += 1) {
      const Sample = this.Get_sample(Index);
      if (Sample) Samples.push(Sample);
    }
    return Samples;
  }

  /***********************************************
   * @brief : 导出缓冲中的全部有序采样
   * @return: 全部采样副本
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  toArray() {
    const Samples: Telemetry_t[] = [];
    for (let Index = 0; Index < this.Count; Index += 1) {
      const Sample = this.Get_sample(Index);
      if (Sample) Samples.push(Sample);
    }
    return Samples;
  }

  /***********************************************
   * @brief : 按逻辑顺序读取一个采样点
   * @param : Index 逻辑索引
   * @return: 对应采样点或空值
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  private Get_sample(Index: number) {
    if (Index < 0 || Index >= this.Count) return undefined;
    return this.Storage[(this.Start + Index) % this.Capacity];
  }

  /***********************************************
   * @brief : 二分查找时间范围边界
   * @param : Timestamp 目标时间
   * @param : Include_equal 是否将相等时间包含在左侧
   * @return: 时间边界对应的逻辑索引
   * @date  : 2026-08-28
   * @author: L
   ************************************************/
  private Find_time_index(Timestamp: number, Include_equal: boolean) {
    let Left = 0;
    let Right = this.Count;
    while (Left < Right) {
      const Middle = Math.floor((Left + Right) / 2);
      const Sample_time = this.Get_sample(Middle)?.timestamp ?? Number.POSITIVE_INFINITY;
      const Move_right = Include_equal ? Sample_time <= Timestamp : Sample_time < Timestamp;
      if (Move_right) Left = Middle + 1;
      else Right = Middle;
    }
    return Left;
  }

}
