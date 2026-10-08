export interface MemoryMetrics {
  total_bytes: number;
  used_bytes: number;
  free_bytes: number;
  available_bytes: number;
  buffers_bytes: number;
  cached_bytes: number;
  usage_percent: number;
  swap_total_bytes: number;
  swap_free_bytes: number;
  swap_used_bytes: number;
  swap_usage_percent: number;
}
