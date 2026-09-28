import type { Observer_stream_batch_t } from "./types";

declare global {
  interface Window {
    focBridge?: {
      saveCsv: (Content: string) => Promise<boolean>;
      listSerialPorts: () => Promise<Array<{ path: string; manufacturer?: string; friendlyName?: string; serialNumber?: string; vendorId?: string; productId?: string; simulated?: boolean }>>;
      connectSerial: (Config: Record<string, unknown>) => Promise<{ ok: boolean; message: string; parameters?: Record<string, number> }>;
      disconnectSerial: () => Promise<{ ok: boolean; message: string }>;
      sendSerialFrame: (Type: number, Data: number[]) => Promise<{ ok: boolean; message: string }>;
      sendControl: (Command: Record<string, unknown>, Priority_disable?: boolean) => Promise<{ ok: boolean; message: string }>;
      sendParameters: (Parameters: Record<string, unknown>, Parameter_group: "foc" | "observer") => Promise<{ ok: boolean; message: string }>;
      onSerialFrame: (Callback: (Frame: { type: number; sequence: number; timestamp: number; direction?: "RX" | "TX"; data: number[] }) => void) => () => void;
      onSerialTelemetry: (Callback: (Telemetry: Record<string, number>) => void) => () => void;
      onSerialWaveformBatch: (Callback: (Samples: Array<Record<string, number>>) => void) => () => void;
      onSerialObserverStreamBatch: (Callback: (Batch: Observer_stream_batch_t) => void) => () => void;
      onSerialParameters: (Callback: (Parameters: Record<string, number>) => void) => () => void;
      onSerialMusicTracks: (Callback: (Tracks: Array<{ id: number; name: string }>) => void) => () => void;
      onSerialStatus: (Callback: (Status: { state: string; message: string }) => void) => () => void;
      onSerialStats: (Callback: (Stats: { crcErrors: number; formatErrors: number }) => void) => () => void;
    };
  }
}
