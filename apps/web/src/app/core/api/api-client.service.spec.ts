import { TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { ApiClientService } from './api-client.service';
import {
  HealthResponse,
  SystemInfo,
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
} from '../models';

describe('ApiClientService', () => {
  let service: ApiClientService;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [
        provideHttpClient(),
        provideHttpClientTesting(),
        ApiClientService,
      ],
    });

    service = TestBed.inject(ApiClientService);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => {
    httpMock.verify();
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });

  describe('getHealth()', () => {
    it('should send GET request to /api/v1/health and return health response', () => {
      const mockHealth: HealthResponse = {
        status: 'healthy',
        version: '1.0.0',
        uptime_seconds: 3600,
      };

      let result: HealthResponse | undefined;
      service.getHealth().subscribe((res) => {
        result = res;
      });

      const req = httpMock.expectOne('/api/v1/health');
      expect(req.request.method).toBe('GET');
      req.flush(mockHealth);

      expect(result).toEqual(mockHealth);
    });
  });

  describe('getSystem()', () => {
    it('should send GET request to /api/v1/system and return system info', () => {
      const mockSystem: SystemInfo = {
        hostname: 'node-01',
        os_name: 'Linux',
        os_version: '6.8.0',
        kernel_version: '6.8.0-generic',
        architecture: 'x86_64',
        boot_time_utc: 1700000000,
        uptime_seconds: 3600,
      };

      let result: SystemInfo | undefined;
      service.getSystem().subscribe((res) => {
        result = res;
      });

      const req = httpMock.expectOne('/api/v1/system');
      expect(req.request.method).toBe('GET');
      req.flush(mockSystem);

      expect(result).toEqual(mockSystem);
    });
  });

  describe('getCpu()', () => {
    it('should send GET request to /api/v1/cpu and return cpu metrics', () => {
      const mockCpu: CpuMetrics = {
        usage_percent: 25.5,
        measurement_status: 'ready',
        model_name: 'AMD EPYC',
        physical_cores: 4,
        logical_cores: 8,
        load_average: {
          one_minute: 0.5,
          five_minute: 0.8,
          fifteen_minute: 1.2,
        },
        cores: [
          { core_id: 0, usage_percent: 20.0 },
          { core_id: 1, usage_percent: 31.0 },
        ],
      };

      let result: CpuMetrics | undefined;
      service.getCpu().subscribe((res) => {
        result = res;
      });

      const req = httpMock.expectOne('/api/v1/cpu');
      expect(req.request.method).toBe('GET');
      req.flush(mockCpu);

      expect(result).toEqual(mockCpu);
    });
  });

  describe('getMemory()', () => {
    it('should send GET request to /api/v1/memory and return memory metrics', () => {
      const mockMemory: MemoryMetrics = {
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

      let result: MemoryMetrics | undefined;
      service.getMemory().subscribe((res) => {
        result = res;
      });

      const req = httpMock.expectOne('/api/v1/memory');
      expect(req.request.method).toBe('GET');
      req.flush(mockMemory);

      expect(result).toEqual(mockMemory);
    });
  });

  describe('getDisks()', () => {
    it('should send GET request to /api/v1/disks and return disk partition metrics array', () => {
      const mockDisks: DiskPartitionMetrics[] = [
        {
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
        },
      ];

      let result: DiskPartitionMetrics[] | undefined;
      service.getDisks().subscribe((res) => {
        result = res;
      });

      const req = httpMock.expectOne('/api/v1/disks');
      expect(req.request.method).toBe('GET');
      req.flush(mockDisks);

      expect(result).toEqual(mockDisks);
    });
  });

  describe('getNetwork()', () => {
    it('should send GET request to /api/v1/network and return network interface metrics array', () => {
      const mockNetwork: NetworkInterfaceMetrics[] = [
        {
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
        },
      ];

      let result: NetworkInterfaceMetrics[] | undefined;
      service.getNetwork().subscribe((res) => {
        result = res;
      });

      const req = httpMock.expectOne('/api/v1/network');
      expect(req.request.method).toBe('GET');
      req.flush(mockNetwork);

      expect(result).toEqual(mockNetwork);
    });
  });

  describe('getProcesses()', () => {
    const mockProcesses: ProcessInfo[] = [
      {
        pid: 1234,
        name: 'nodepulse',
        user: 'root',
        state: 'S',
        cpu_percent: 1.5,
        memory_rss_bytes: 45000000,
        cmdline: '/usr/bin/nodepulse',
      },
    ];

    it('should send GET request to /api/v1/processes with no query params when omitted', () => {
      let result: ProcessInfo[] | undefined;
      service.getProcesses().subscribe((res) => {
        result = res;
      });

      const req = httpMock.expectOne('/api/v1/processes');
      expect(req.request.method).toBe('GET');
      expect(req.request.params.keys().length).toBe(0);
      req.flush(mockProcesses);

      expect(result).toEqual(mockProcesses);
    });

    it('should send GET request with sort query param when provided', () => {
      service.getProcesses('cpu').subscribe();

      const req = httpMock.expectOne((r) => r.url === '/api/v1/processes');
      expect(req.request.method).toBe('GET');
      expect(req.request.params.get('sort')).toBe('cpu');
      expect(req.request.params.has('limit')).toBe(false);
      req.flush(mockProcesses);
    });

    it('should send GET request with limit query param when provided', () => {
      service.getProcesses(undefined, 50).subscribe();

      const req = httpMock.expectOne((r) => r.url === '/api/v1/processes');
      expect(req.request.method).toBe('GET');
      expect(req.request.params.has('sort')).toBe(false);
      expect(req.request.params.get('limit')).toBe('50');
      req.flush(mockProcesses);
    });

    it('should send GET request with both sort and limit query params when provided', () => {
      service.getProcesses('memory', 25).subscribe();

      const req = httpMock.expectOne((r) => r.url === '/api/v1/processes');
      expect(req.request.method).toBe('GET');
      expect(req.request.params.get('sort')).toBe('memory');
      expect(req.request.params.get('limit')).toBe('25');
      req.flush(mockProcesses);
    });
  });

  describe('getProcessDetail()', () => {
    it('should send GET request to /api/v1/processes/{pid} and return process detail', () => {
      const mockDetail: ProcessDetail = {
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

      let result: ProcessDetail | undefined;
      service.getProcessDetail(1234).subscribe((res) => {
        result = res;
      });

      const req = httpMock.expectOne('/api/v1/processes/1234');
      expect(req.request.method).toBe('GET');
      req.flush(mockDetail);

      expect(result).toEqual(mockDetail);
    });

    it('getProcess should delegate to getProcessDetail', () => {
      const mockDetail: ProcessDetail = {
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

      let result: ProcessDetail | undefined;
      service.getProcess(1234).subscribe((res) => {
        result = res;
      });

      const req = httpMock.expectOne('/api/v1/processes/1234');
      expect(req.request.method).toBe('GET');
      req.flush(mockDetail);

      expect(result).toEqual(mockDetail);
    });
  });

  describe('getServices()', () => {
    const mockServices: ServiceInfo[] = [
      {
        name: 'nodepulse.service',
        description: 'NodePulse Monitoring Agent',
        load_state: 'loaded',
        active_state: 'active',
        sub_state: 'running',
        unit_file_state: 'enabled',
      },
    ];

    it('should send GET request to /api/v1/services with no query params when omitted', () => {
      let result: ServiceInfo[] | undefined;
      service.getServices().subscribe((res) => {
        result = res;
      });

      const req = httpMock.expectOne('/api/v1/services');
      expect(req.request.method).toBe('GET');
      expect(req.request.params.keys().length).toBe(0);
      req.flush(mockServices);

      expect(result).toEqual(mockServices);
    });

    it('should send GET request with state query param when provided', () => {
      service.getServices('failed').subscribe();

      const req = httpMock.expectOne((r) => r.url === '/api/v1/services');
      expect(req.request.method).toBe('GET');
      expect(req.request.params.get('state')).toBe('failed');
      expect(req.request.params.has('limit')).toBe(false);
      req.flush(mockServices);
    });

    it('should send GET request with limit query param when provided', () => {
      service.getServices(undefined, 100).subscribe();

      const req = httpMock.expectOne((r) => r.url === '/api/v1/services');
      expect(req.request.method).toBe('GET');
      expect(req.request.params.has('state')).toBe(false);
      expect(req.request.params.get('limit')).toBe('100');
      req.flush(mockServices);
    });

    it('should send GET request with both state and limit query params when provided', () => {
      service.getServices('active', 50).subscribe();

      const req = httpMock.expectOne((r) => r.url === '/api/v1/services');
      expect(req.request.method).toBe('GET');
      expect(req.request.params.get('state')).toBe('active');
      expect(req.request.params.get('limit')).toBe('50');
      req.flush(mockServices);
    });
  });

  describe('getServiceDetail()', () => {
    const mockDetail: ServiceDetail = {
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

    it('should send GET request to /api/v1/services/{name} and return service detail', () => {
      let result: ServiceDetail | undefined;
      service.getServiceDetail('nodepulse.service').subscribe((res) => {
        result = res;
      });

      const req = httpMock.expectOne('/api/v1/services/nodepulse.service');
      expect(req.request.method).toBe('GET');
      req.flush(mockDetail);

      expect(result).toEqual(mockDetail);
    });

    it('should encode service name with special characters in URL', () => {
      service.getServiceDetail('user@1000.service').subscribe();

      const req = httpMock.expectOne('/api/v1/services/user%401000.service');
      expect(req.request.method).toBe('GET');
      req.flush(mockDetail);
    });
  });

  describe('getContainers()', () => {
    it('should send GET request to /api/v1/containers and return container summaries', () => {
      const mockContainers: ContainerSummary[] = [
        {
          id: 'abc1234567890',
          names: ['/web-server'],
          image: 'nginx:alpine',
          status: 'Up 2 hours',
          state: 'running',
          created: 1700000000,
        },
      ];

      let result: ContainerSummary[] | undefined;
      service.getContainers().subscribe((res) => {
        result = res;
      });

      const req = httpMock.expectOne('/api/v1/containers');
      expect(req.request.method).toBe('GET');
      req.flush(mockContainers);

      expect(result).toEqual(mockContainers);
    });
  });

  describe('getContainerDetail()', () => {
    const mockDetail: ContainerDetail = {
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

    it('should send GET request to /api/v1/containers/{id} and return container detail', () => {
      let result: ContainerDetail | undefined;
      service.getContainerDetail('abc1234567890').subscribe((res) => {
        result = res;
      });

      const req = httpMock.expectOne('/api/v1/containers/abc1234567890');
      expect(req.request.method).toBe('GET');
      req.flush(mockDetail);

      expect(result).toEqual(mockDetail);
    });

    it('should encode container id with special characters in URL', () => {
      service.getContainerDetail('sha256:abcd1234efgh').subscribe();

      const req = httpMock.expectOne('/api/v1/containers/sha256%3Aabcd1234efgh');
      expect(req.request.method).toBe('GET');
      req.flush(mockDetail);
    });
  });
});
