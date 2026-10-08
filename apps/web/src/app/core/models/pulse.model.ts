export interface MetricPulse {
  timestamp: string;
  cpu_usage_percent: number | null;
  memory_usage_percent: number;
  memory_used_bytes: number;
  network_rx_bytes_sec: number;
  network_tx_bytes_sec: number;
}

export interface TelemetrySample<T> {
  timestamp: string;
  value: T | null;
}
