export interface ServiceInfo {
  name: string;
  description: string;
  load_state: string;
  active_state: 'active' | 'inactive' | 'failed' | string;
  sub_state: string;
  unit_file_state: string;
}

export interface ServiceDetail {
  name: string;
  description: string;
  load_state: string;
  active_state: string;
  sub_state: string;
  unit_file_state: string;
  main_pid: number;
  restart_count: number;
  active_enter_timestamp_utc: number;
  memory_current_bytes: number;
}
