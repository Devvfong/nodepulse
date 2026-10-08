export interface ProcessInfo {
  pid: number;
  name: string;
  user: string;
  state: 'R' | 'S' | 'D' | 'Z' | 'T' | string;
  cpu_percent: number;
  memory_rss_bytes: number;
  cmdline: string;
}

export interface ProcessDetail {
  pid: number;
  ppid: number;
  name: string;
  user: string;
  state: string;
  cpu_percent: number;
  memory_rss_bytes: number;
  memory_vms_bytes: number;
  thread_count: number;
  open_fd_count: number;
  start_time_epoch: number;
  cmdline: string;
  working_directory: string;
}
