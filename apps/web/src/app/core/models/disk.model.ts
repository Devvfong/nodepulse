export interface DiskPartitionMetrics {
  filesystem: string;
  mount_point: string;
  fstype: string;
  total_bytes: number;
  used_bytes: number;
  free_bytes: number;
  available_bytes: number;
  usage_percent: number;
  inodes_total: number;
  inodes_free: number;
}
