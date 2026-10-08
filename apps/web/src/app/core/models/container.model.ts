export interface ContainerSummary {
  id: string;
  names: string[];
  image: string;
  status: string;
  state: 'running' | 'exited' | string;
  created: number;
}

export interface ContainerDetail {
  id: string;
  name: string;
  image: string;
  status: string;
  state: string;
  running: boolean;
  exit_code: number;
  port_mappings: string[];
  mount_sources: string[];
  created: number;
}
