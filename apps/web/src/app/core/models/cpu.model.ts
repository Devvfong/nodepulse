export interface LoadAverage {
  one_minute: number;
  five_minute: number;
  fifteen_minute: number;
}

export interface CpuCoreMetrics {
  core_id: number;
  usage_percent: number | null;
}

export interface CpuMetrics {
  usage_percent: number | null;
  measurement_status: 'ready' | 'warming_up' | 'cached';
  model_name: string;
  physical_cores: number;
  logical_cores: number;
  load_average: LoadAverage;
  cores: CpuCoreMetrics[];
}
