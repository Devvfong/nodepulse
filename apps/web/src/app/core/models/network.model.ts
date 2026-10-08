export interface NetworkInterfaceMetrics {
  name: string;
  mac_address: string;
  operstate: 'up' | 'down' | 'unknown' | string;
  speed_mbps: number;
  rx_bytes: number;
  tx_bytes: number;
  rx_packets: number;
  tx_packets: number;
  rx_errors: number;
  tx_errors: number;
  rx_bytes_per_sec: number;
  tx_bytes_per_sec: number;
}
