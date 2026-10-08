import { TestBed } from '@angular/core/testing';
import { of, Subject, throwError } from 'rxjs';
import { HostStateService } from './host-state.service';
import { ConnectionStateService } from './connection-state.service';
import { ApiClientService } from '../api/api-client.service';
import {
  HealthResponse,
  SystemInfo,
  CpuMetrics,
  MemoryMetrics,
  DiskPartitionMetrics,
  NetworkInterfaceMetrics,
  ProcessInfo,
  MetricPulse,
} from '../models';

describe('HostStateService', () => {
  let service: HostStateService;
  let connectionStateService: ConnectionStateService;
  let mockApiClient: {
    getHealth: ReturnType<typeof vi.fn>;
    getSystem: ReturnType<typeof vi.fn>;
    getCpu: ReturnType<typeof vi.fn>;
    getMemory: ReturnType<typeof vi.fn>;
    getDisks: ReturnType<typeof vi.fn>;
    getNetwork: ReturnType<typeof vi.fn>;
    getProcesses: ReturnType<typeof vi.fn>;
  };

  const mockHealth: HealthResponse = {
    status: 'healthy',
    version: '1.0.0',
    uptime_seconds: 7200,
  };

  const mockSystem: SystemInfo = {
    hostname: 'node-edge-01',
    os_name: 'Ubuntu',
    os_version: '22.04 LTS',
    kernel_version: '6.5.0-44-generic',
    architecture: 'x86_64',
    boot_time_utc: 1700000000,
    uptime_seconds: 7200,
  };

  const mockCpu: CpuMetrics = {
    usage_percent: 18.4,
    measurement_status: 'ready',
    model_name: 'AMD EPYC 7763',
    physical_cores: 8,
    logical_cores: 16,
    load_average: {
      one_minute: 0.85,
      five_minute: 1.12,
      fifteen_minute: 0.98,
    },
    cores: [
      { core_id: 0, usage_percent: 20.1 },
      { core_id: 1, usage_percent: 16.7 },
    ],
  };

  const mockMemory: MemoryMetrics = {
    total_bytes: 33554432000,
    used_bytes: 12884901888,
    free_bytes: 10737418240,
    available_bytes: 20669530112,
    buffers_bytes: 2147483648,
    cached_bytes: 7784013824,
    usage_percent: 38.4,
    swap_total_bytes: 8589934592,
    swap_free_bytes: 8589934592,
    swap_used_bytes: 0,
    swap_usage_percent: 0.0,
  };

  const mockDisks: DiskPartitionMetrics[] = [
    {
      filesystem: '/dev/nvme0n1p2',
      mount_point: '/',
      fstype: 'ext4',
      total_bytes: 512000000000,
      used_bytes: 128000000000,
      free_bytes: 384000000000,
      available_bytes: 358400000000,
      usage_percent: 25.0,
      inodes_total: 32768000,
      inodes_free: 31500000,
    },
  ];

  const mockNetwork: NetworkInterfaceMetrics[] = [
    {
      name: 'eth0',
      mac_address: '02:42:ac:11:00:02',
      operstate: 'up',
      speed_mbps: 10000,
      rx_bytes: 1048576000,
      tx_bytes: 524288000,
      rx_packets: 1500000,
      tx_packets: 900000,
      rx_errors: 0,
      tx_errors: 0,
      rx_bytes_per_sec: 102400.0,
      tx_bytes_per_sec: 51200.0,
    },
  ];

  const mockProcesses: ProcessInfo[] = [
    {
      pid: 1001,
      name: 'nodepulse',
      user: 'nodepulse',
      state: 'R',
      cpu_percent: 4.2,
      memory_rss_bytes: 104857600,
      cmdline: '/usr/local/bin/nodepulse --config /etc/nodepulse.json',
    },
    {
      pid: 1002,
      name: 'nginx',
      user: 'www-data',
      state: 'S',
      cpu_percent: 1.8,
      memory_rss_bytes: 67108864,
      cmdline: 'nginx: worker process',
    },
  ];

  beforeEach(() => {
    mockApiClient = {
      getHealth: vi.fn(),
      getSystem: vi.fn(),
      getCpu: vi.fn(),
      getMemory: vi.fn(),
      getDisks: vi.fn(),
      getNetwork: vi.fn(),
      getProcesses: vi.fn(),
    };

    TestBed.configureTestingModule({
      providers: [
        HostStateService,
        ConnectionStateService,
        { provide: ApiClientService, useValue: mockApiClient },
      ],
    });

    service = TestBed.inject(HostStateService);
    connectionStateService = TestBed.inject(ConnectionStateService);
  });

  describe('Initial State', () => {
    it('should initialize all vitals signals to null', () => {
      expect(service.systemInfo()).toBeNull();
      expect(service.cpuMetrics()).toBeNull();
      expect(service.memoryMetrics()).toBeNull();
      expect(service.diskMetrics()).toBeNull();
      expect(service.networkMetrics()).toBeNull();
      expect(service.topProcesses()).toBeNull();
    });

    it('should initialize status signals correctly', () => {
      expect(service.isLoading()).toBe(false);
      expect(service.lastError()).toBeNull();
    });

    it('should initialize rolling buffers to empty arrays', () => {
      expect(service.cpuRollingBuffer()).toEqual([]);
      expect(service.memoryRollingBuffer()).toEqual([]);
      expect(service.networkRxRollingBuffer()).toEqual([]);
      expect(service.networkTxRollingBuffer()).toEqual([]);
    });
  });

  describe('Individual Signal Updaters', () => {
    it('should update systemInfo signal via updateSystem()', () => {
      service.updateSystem(mockSystem);
      expect(service.systemInfo()).toEqual(mockSystem);
    });

    it('should update cpuMetrics signal via updateCpu()', () => {
      service.updateCpu(mockCpu);
      expect(service.cpuMetrics()).toEqual(mockCpu);
    });

    it('should update memoryMetrics signal via updateMemory()', () => {
      service.updateMemory(mockMemory);
      expect(service.memoryMetrics()).toEqual(mockMemory);
    });

    it('should update diskMetrics signal via updateDisks()', () => {
      service.updateDisks(mockDisks);
      expect(service.diskMetrics()).toEqual(mockDisks);
    });

    it('should update networkMetrics signal via updateNetwork()', () => {
      service.updateNetwork(mockNetwork);
      expect(service.networkMetrics()).toEqual(mockNetwork);
    });

    it('should update topProcesses signal via updateTopProcesses()', () => {
      service.updateTopProcesses(mockProcesses);
      expect(service.topProcesses()).toEqual(mockProcesses);
    });

    it('should maintain isolation between individual updates', () => {
      service.updateCpu(mockCpu);
      expect(service.cpuMetrics()).toEqual(mockCpu);
      expect(service.memoryMetrics()).toBeNull();
      expect(service.systemInfo()).toBeNull();
    });
  });

  describe('fetchInitialVitals()', () => {
    beforeEach(() => {
      mockApiClient.getHealth.mockReturnValue(of(mockHealth));
      mockApiClient.getSystem.mockReturnValue(of(mockSystem));
      mockApiClient.getCpu.mockReturnValue(of(mockCpu));
      mockApiClient.getMemory.mockReturnValue(of(mockMemory));
      mockApiClient.getDisks.mockReturnValue(of(mockDisks));
      mockApiClient.getNetwork.mockReturnValue(of(mockNetwork));
      mockApiClient.getProcesses.mockReturnValue(of(mockProcesses));
    });

    it('should execute forkJoin for all 7 endpoints and update all state signals', () => {
      let emittedData: unknown;
      service.fetchInitialVitals().subscribe((res: unknown) => {
        emittedData = res;
      });

      expect(mockApiClient.getHealth).toHaveBeenCalledTimes(1);
      expect(mockApiClient.getSystem).toHaveBeenCalledTimes(1);
      expect(mockApiClient.getCpu).toHaveBeenCalledTimes(1);
      expect(mockApiClient.getMemory).toHaveBeenCalledTimes(1);
      expect(mockApiClient.getDisks).toHaveBeenCalledTimes(1);
      expect(mockApiClient.getNetwork).toHaveBeenCalledTimes(1);
      expect(mockApiClient.getProcesses).toHaveBeenCalledWith('cpu', 5);

      // Verify all domain signals populated
      expect(service.systemInfo()).toEqual(mockSystem);
      expect(service.cpuMetrics()).toEqual(mockCpu);
      expect(service.memoryMetrics()).toEqual(mockMemory);
      expect(service.diskMetrics()).toEqual(mockDisks);
      expect(service.networkMetrics()).toEqual(mockNetwork);
      expect(service.topProcesses()).toEqual(mockProcesses);

      // Verify connection state updated to healthy
      expect(connectionStateService.agentStatus()).toBe('healthy');

      // Verify status signals
      expect(service.isLoading()).toBe(false);
      expect(service.lastError()).toBeNull();

      // Verify emitted combined data
      expect(emittedData).toEqual({
        health: mockHealth,
        system: mockSystem,
        cpu: mockCpu,
        memory: mockMemory,
        disks: mockDisks,
        network: mockNetwork,
        processes: mockProcesses,
      });
    });

    it('should set isLoading to false and lastError on health check error', () => {
      const probeError = new Error('HTTP 503: Agent daemon unavailable');
      mockApiClient.getHealth.mockReturnValue(throwError(() => probeError));

      let caughtError: unknown;
      service.fetchInitialVitals().subscribe({
        next: () => {
          throw new Error('Should not succeed');
        },
        error: (err: unknown) => {
          caughtError = err;
        },
      });

      expect(caughtError).toBe(probeError);
      expect(connectionStateService.agentStatus()).toBe('unreachable');
      expect(service.isLoading()).toBe(false);
      expect(service.lastError()).toBe('HTTP 503: Agent daemon unavailable');
      expect(service.systemInfo()).toBeNull();
    });

    it('should set isLoading to false and lastError on subsystem failure', () => {
      const systemError = new Error('Failed to read /proc/version');
      mockApiClient.getSystem.mockReturnValue(throwError(() => systemError));

      let caughtError: unknown;
      service.fetchInitialVitals().subscribe({
        next: () => {
          throw new Error('Should not succeed');
        },
        error: (err: unknown) => {
          caughtError = err;
        },
      });

      expect(caughtError).toBe(systemError);
      expect(service.isLoading()).toBe(false);
      expect(service.lastError()).toBe('Failed to read /proc/version');
    });

    it('should extract error message from ApiErrorEnvelope if structured error is returned', () => {
      const structuredError = {
        error: {
          code: 'COLLECTOR_FAILURE',
          message: 'CPU proc parser failed',
          timestamp: '2026-10-08T12:00:00Z',
          details: [],
        },
        status: 500,
      };
      mockApiClient.getCpu.mockReturnValue(throwError(() => structuredError));

      service.fetchInitialVitals().subscribe({
        error: () => {
          // Expected error
        },
      });

      expect(service.isLoading()).toBe(false);
      expect(service.lastError()).toBe('CPU proc parser failed');
    });

    it('should reset isLoading to false via finalize() upon mid-flight unsubscription / cancellation', () => {
      const pendingSubject = new Subject<HealthResponse>();
      mockApiClient.getHealth.mockReturnValue(pendingSubject.asObservable());

      const sub = service.fetchInitialVitals().subscribe();

      expect(service.isLoading()).toBe(true);

      // Unsubscribe before completion
      sub.unsubscribe();

      expect(service.isLoading()).toBe(false);
    });
  });

  describe('Rolling Telemetry Buffers', () => {
    const mockPulse: MetricPulse = {
      timestamp: '2026-10-08T12:00:00Z',
      cpu_usage_percent: 18.5,
      memory_usage_percent: 42.1,
      memory_used_bytes: 8589934592,
      network_rx_bytes_sec: 1048576,
      network_tx_bytes_sec: 524288,
    };

    it('should append pulse samples to all 4 rolling buffers', () => {
      service.appendPulse(mockPulse);

      expect(service.cpuRollingBuffer()).toHaveLength(1);
      expect(service.cpuRollingBuffer()[0]).toEqual({
        timestamp: '2026-10-08T12:00:00Z',
        value: 18.5,
      });

      expect(service.memoryRollingBuffer()).toHaveLength(1);
      expect(service.memoryRollingBuffer()[0]).toEqual({
        timestamp: '2026-10-08T12:00:00Z',
        value: 42.1,
      });

      expect(service.networkRxRollingBuffer()).toHaveLength(1);
      expect(service.networkRxRollingBuffer()[0]).toEqual({
        timestamp: '2026-10-08T12:00:00Z',
        value: 1048576,
      });

      expect(service.networkTxRollingBuffer()).toHaveLength(1);
      expect(service.networkTxRollingBuffer()[0]).toEqual({
        timestamp: '2026-10-08T12:00:00Z',
        value: 524288,
      });
    });

    it('should honestly preserve null cpu_usage_percent during warming up', () => {
      const warmupPulse: MetricPulse = {
        ...mockPulse,
        cpu_usage_percent: null,
      };

      service.appendPulse(warmupPulse);

      expect(service.cpuRollingBuffer()).toHaveLength(1);
      expect(service.cpuRollingBuffer()[0].value).toBeNull();
      expect(service.cpuRollingBuffer()[0].value).not.toBe(0);
      expect(Number.isNaN(service.cpuRollingBuffer()[0].value)).toBe(false);
    });

    it('should cap all 4 rolling buffers at 60 items when receiving > 60 pulses', () => {
      for (let i = 1; i <= 65; i++) {
        service.appendPulse({
          timestamp: `2026-10-08T12:00:${i.toString().padStart(2, '0')}Z`,
          cpu_usage_percent: i,
          memory_usage_percent: i * 0.5,
          memory_used_bytes: i * 1000000,
          network_rx_bytes_sec: i * 100,
          network_tx_bytes_sec: i * 200,
        });
      }

      expect(service.cpuRollingBuffer()).toHaveLength(60);
      expect(service.memoryRollingBuffer()).toHaveLength(60);
      expect(service.networkRxRollingBuffer()).toHaveLength(60);
      expect(service.networkTxRollingBuffer()).toHaveLength(60);

      // Oldest 5 samples (1 to 5) should have shifted out; sample 6 is now first
      expect(service.cpuRollingBuffer()[0].value).toBe(6);
      expect(service.cpuRollingBuffer()[0].timestamp).toBe('2026-10-08T12:00:06Z');
      expect(service.cpuRollingBuffer()[59].value).toBe(65);
      expect(service.cpuRollingBuffer()[59].timestamp).toBe('2026-10-08T12:00:65Z');
    });

    it('should reset all 4 rolling buffers to empty arrays via clearBuffers()', () => {
      service.appendPulse(mockPulse);
      expect(service.cpuRollingBuffer()).toHaveLength(1);
      expect(service.memoryRollingBuffer()).toHaveLength(1);
      expect(service.networkRxRollingBuffer()).toHaveLength(1);
      expect(service.networkTxRollingBuffer()).toHaveLength(1);

      service.clearBuffers();

      expect(service.cpuRollingBuffer()).toEqual([]);
      expect(service.memoryRollingBuffer()).toEqual([]);
      expect(service.networkRxRollingBuffer()).toEqual([]);
      expect(service.networkTxRollingBuffer()).toEqual([]);
    });
  });

  describe('Readonly Signal Encapsulation', () => {
    it('should expose all vitals, status, and buffer signals as readonly without public set method', () => {
      expect((service.systemInfo as unknown as Record<string, unknown>)['set']).toBeUndefined();
      expect((service.cpuMetrics as unknown as Record<string, unknown>)['set']).toBeUndefined();
      expect((service.memoryMetrics as unknown as Record<string, unknown>)['set']).toBeUndefined();
      expect((service.diskMetrics as unknown as Record<string, unknown>)['set']).toBeUndefined();
      expect((service.networkMetrics as unknown as Record<string, unknown>)['set']).toBeUndefined();
      expect((service.topProcesses as unknown as Record<string, unknown>)['set']).toBeUndefined();
      expect((service.cpuRollingBuffer as unknown as Record<string, unknown>)['set']).toBeUndefined();
      expect((service.memoryRollingBuffer as unknown as Record<string, unknown>)['set']).toBeUndefined();
      expect((service.networkRxRollingBuffer as unknown as Record<string, unknown>)['set']).toBeUndefined();
      expect((service.networkTxRollingBuffer as unknown as Record<string, unknown>)['set']).toBeUndefined();
      expect((service.isLoading as unknown as Record<string, unknown>)['set']).toBeUndefined();
      expect((service.lastError as unknown as Record<string, unknown>)['set']).toBeUndefined();
    });
  });
});
