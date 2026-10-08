import {
  HealthResponse,
  SystemInfo,
  LoadAverage,
  CpuCoreMetrics,
  CpuMetrics,
  MemoryMetrics,
  DiskPartitionMetrics,
  NetworkInterfaceMetrics,
  ProcessInfo,
  ProcessDetail,
  ServiceInfo,
  ServiceDetail,
  ContainerSummary,
  ContainerDetail,
  MetricPulse,
  TelemetrySample,
  ApiErrorDetail,
  ApiErrorEnvelope,
  ApiErrorCode,
} from './index';

describe('Domain Models Contract Conformance', () => {
  it('should instantiate HealthResponse and SystemInfo models correctly', () => {
    const health: HealthResponse = {
      status: 'healthy',
      version: '1.0.0',
      uptime_seconds: 3600,
    };
    expect(health.status).toBe('healthy');

    const systemInfo: SystemInfo = {
      hostname: 'node-01',
      os_name: 'Linux',
      os_version: '6.8.0',
      kernel_version: '6.8.0-generic',
      architecture: 'x86_64',
      boot_time_utc: 1700000000,
      uptime_seconds: 3600,
    };
    expect(systemInfo.hostname).toBe('node-01');
  });

  it('should model CPU metrics during warm-up with null usage and cores', () => {
    const load: LoadAverage = {
      one_minute: 0.5,
      five_minute: 0.8,
      fifteen_minute: 1.2,
    };
    const core: CpuCoreMetrics = {
      core_id: 0,
      usage_percent: null,
    };
    const cpu: CpuMetrics = {
      usage_percent: null,
      measurement_status: 'warming_up',
      model_name: 'AMD EPYC',
      physical_cores: 4,
      logical_cores: 8,
      load_average: load,
      cores: [core],
    };
    expect(cpu.usage_percent).toBeNull();
    expect(cpu.measurement_status).toBe('warming_up');
  });

  it('should model Memory and Disk metrics with byte counters and percentages', () => {
    const mem: MemoryMetrics = {
      total_bytes: 16000000000,
      used_bytes: 8000000000,
      free_bytes: 4000000000,
      available_bytes: 8000000000,
      buffers_bytes: 500000000,
      cached_bytes: 3500000000,
      usage_percent: 50.0,
      swap_total_bytes: 4000000000,
      swap_free_bytes: 4000000000,
      swap_used_bytes: 0,
      swap_usage_percent: 0.0,
    };
    expect(mem.usage_percent).toBe(50.0);

    const disk: DiskPartitionMetrics = {
      filesystem: '/dev/sda1',
      mount_point: '/',
      fstype: 'ext4',
      total_bytes: 100000000000,
      used_bytes: 40000000000,
      free_bytes: 60000000000,
      available_bytes: 55000000000,
      usage_percent: 40.0,
      inodes_total: 1000000,
      inodes_free: 800000,
    };
    expect(disk.mount_point).toBe('/');
  });

  it('should model NetworkInterfaceMetrics without drop counters', () => {
    const net: NetworkInterfaceMetrics = {
      name: 'eth0',
      mac_address: '00:11:22:33:44:55',
      operstate: 'up',
      speed_mbps: 1000,
      rx_bytes: 100000,
      tx_bytes: 50000,
      rx_packets: 1000,
      tx_packets: 500,
      rx_errors: 0,
      tx_errors: 0,
      rx_bytes_per_sec: 1024.5,
      tx_bytes_per_sec: 512.0,
    };
    expect(net.name).toBe('eth0');
    expect('rx_drops' in net).toBe(false);
  });

  it('should model ProcessInfo and ProcessDetail', () => {
    const procInfo: ProcessInfo = {
      pid: 1234,
      name: 'nodepulse',
      user: 'root',
      state: 'S',
      cpu_percent: 1.5,
      memory_rss_bytes: 45000000,
      cmdline: '/usr/bin/nodepulse',
    };
    expect(procInfo.pid).toBe(1234);

    const procDetail: ProcessDetail = {
      pid: 1234,
      ppid: 1,
      name: 'nodepulse',
      user: 'root',
      state: 'S',
      cpu_percent: 1.5,
      memory_rss_bytes: 45000000,
      memory_vms_bytes: 100000000,
      thread_count: 4,
      open_fd_count: 12,
      start_time_epoch: 1700000000,
      cmdline: '/usr/bin/nodepulse',
      working_directory: '/root',
    };
    expect(procDetail.ppid).toBe(1);
  });

  it('should model ServiceInfo and ServiceDetail', () => {
    const serviceInfo: ServiceInfo = {
      name: 'nodepulse.service',
      description: 'NodePulse Monitoring Agent',
      load_state: 'loaded',
      active_state: 'active',
      sub_state: 'running',
      unit_file_state: 'enabled',
    };
    expect(serviceInfo.active_state).toBe('active');

    const serviceDetail: ServiceDetail = {
      name: 'nodepulse.service',
      description: 'NodePulse Monitoring Agent',
      load_state: 'loaded',
      active_state: 'active',
      sub_state: 'running',
      unit_file_state: 'enabled',
      main_pid: 1234,
      restart_count: 0,
      active_enter_timestamp_utc: 1700000000,
      memory_current_bytes: 45000000,
    };
    expect(serviceDetail.main_pid).toBe(1234);
  });

  it('should model ContainerSummary and ContainerDetail', () => {
    const container: ContainerSummary = {
      id: 'abc1234567890',
      names: ['/web-server'],
      image: 'nginx:alpine',
      status: 'Up 2 hours',
      state: 'running',
      created: 1700000000,
    };
    expect(container.state).toBe('running');

    const containerDetail: ContainerDetail = {
      id: 'abc1234567890',
      name: '/web-server',
      image: 'nginx:alpine',
      status: 'Up 2 hours',
      state: 'running',
      running: true,
      exit_code: 0,
      port_mappings: ['80:80/tcp'],
      mount_sources: ['/var/log'],
      created: 1700000000,
    };
    expect(containerDetail.running).toBe(true);
  });

  it('should model MetricPulse and generic TelemetrySample', () => {
    const pulse: MetricPulse = {
      timestamp: '2026-10-08T10:00:00Z',
      cpu_usage_percent: 25.5,
      memory_usage_percent: 50.0,
      memory_used_bytes: 8000000000,
      network_rx_bytes_sec: 1024,
      network_tx_bytes_sec: 2048,
    };
    expect(pulse.cpu_usage_percent).toBe(25.5);

    const sampleWithNull: TelemetrySample<number> = {
      timestamp: '2026-10-08T10:00:00Z',
      value: null,
    };
    expect(sampleWithNull.value).toBeNull();

    const sampleWithValue: TelemetrySample<number> = {
      timestamp: '2026-10-08T10:00:01Z',
      value: 42.0,
    };
    expect(sampleWithValue.value).toBe(42.0);
  });

  it('should model ApiErrorEnvelope and ApiErrorDetail', () => {
    const detail: ApiErrorDetail = {
      field: 'pid',
      reason: 'must_be_positive_integer',
    };
    const errorCode: ApiErrorCode = 'INVALID_REQUEST';
    const envelope: ApiErrorEnvelope = {
      error: {
        code: errorCode,
        message: 'Process ID must be a positive integer.',
        timestamp: '2026-10-08T10:00:00Z',
        details: [detail],
      },
    };
    expect(envelope.error.code).toBe('INVALID_REQUEST');
    expect(envelope.error.details.length).toBe(1);
  });
});
