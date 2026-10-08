export interface HealthResponse {
  status: 'healthy' | string;
  version: string;
  uptime_seconds: number;
}

export interface SystemInfo {
  hostname: string;
  os_name: string;
  os_version: string;
  kernel_version: string;
  architecture: string;
  boot_time_utc: number;
  uptime_seconds: number;
}
