import { inject, Injectable, signal } from '@angular/core';
import { forkJoin, Observable, throwError } from 'rxjs';
import { catchError, finalize, tap } from 'rxjs/operators';
import { ApiClientService } from '../api/api-client.service';
import { ConnectionStateService } from './connection-state.service';
import {
  HealthResponse,
  SystemInfo,
  CpuMetrics,
  MemoryMetrics,
  DiskPartitionMetrics,
  NetworkInterfaceMetrics,
  ProcessInfo,
  MetricPulse,
  TelemetrySample,
} from '../models';
import { appendTelemetrySample, createTelemetrySample } from './telemetry-buffer.util';

export interface InitialVitalsSnapshot {
  health: HealthResponse;
  system: SystemInfo;
  cpu: CpuMetrics;
  memory: MemoryMetrics;
  disks: DiskPartitionMetrics[];
  network: NetworkInterfaceMetrics[];
  processes: ProcessInfo[];
}

function extractErrorMessage(error: unknown): string {
  if (error && typeof error === 'object') {
    const apiError = error as { error?: { message?: string }; message?: string };
    if (apiError.error && typeof apiError.error.message === 'string') {
      return apiError.error.message;
    }
    if (typeof apiError.message === 'string') {
      return apiError.message;
    }
  }
  if (typeof error === 'string') {
    return error;
  }
  return 'Failed to fetch initial vitals';
}

@Injectable({
  providedIn: 'root',
})
export class HostStateService {
  private readonly apiClient = inject(ApiClientService);
  private readonly connectionState = inject(ConnectionStateService);

  private readonly _systemInfo = signal<SystemInfo | null>(null);
  private readonly _cpuMetrics = signal<CpuMetrics | null>(null);
  private readonly _memoryMetrics = signal<MemoryMetrics | null>(null);
  private readonly _diskMetrics = signal<DiskPartitionMetrics[] | null>(null);
  private readonly _networkMetrics = signal<NetworkInterfaceMetrics[] | null>(null);
  private readonly _topProcesses = signal<ProcessInfo[] | null>(null);
  private readonly _cpuRollingBuffer = signal<TelemetrySample<number | null>[]>([]);
  private readonly _memoryRollingBuffer = signal<TelemetrySample<number>[]>([]);
  private readonly _networkRxRollingBuffer = signal<TelemetrySample<number>[]>([]);
  private readonly _networkTxRollingBuffer = signal<TelemetrySample<number>[]>([]);
  private readonly _isLoading = signal<boolean>(false);
  private readonly _lastError = signal<string | null>(null);

  readonly systemInfo = this._systemInfo.asReadonly();
  readonly cpuMetrics = this._cpuMetrics.asReadonly();
  readonly memoryMetrics = this._memoryMetrics.asReadonly();
  readonly diskMetrics = this._diskMetrics.asReadonly();
  readonly networkMetrics = this._networkMetrics.asReadonly();
  readonly topProcesses = this._topProcesses.asReadonly();
  readonly cpuRollingBuffer = this._cpuRollingBuffer.asReadonly();
  readonly memoryRollingBuffer = this._memoryRollingBuffer.asReadonly();
  readonly networkRxRollingBuffer = this._networkRxRollingBuffer.asReadonly();
  readonly networkTxRollingBuffer = this._networkTxRollingBuffer.asReadonly();
  readonly isLoading = this._isLoading.asReadonly();
  readonly lastError = this._lastError.asReadonly();

  updateSystem(data: SystemInfo): void {
    this._systemInfo.set(data);
  }

  updateCpu(data: CpuMetrics): void {
    this._cpuMetrics.set(data);
  }

  updateMemory(data: MemoryMetrics): void {
    this._memoryMetrics.set(data);
  }

  updateDisks(data: DiskPartitionMetrics[]): void {
    this._diskMetrics.set(data);
  }

  updateNetwork(data: NetworkInterfaceMetrics[]): void {
    this._networkMetrics.set(data);
  }

  updateTopProcesses(data: ProcessInfo[]): void {
    this._topProcesses.set(data);
  }

  appendPulse(pulse: MetricPulse): void {
    this._cpuRollingBuffer.update((buffer) =>
      appendTelemetrySample(
        buffer,
        createTelemetrySample(pulse.timestamp, pulse.cpu_usage_percent),
      ),
    );
    this._memoryRollingBuffer.update((buffer) =>
      appendTelemetrySample(
        buffer,
        createTelemetrySample(pulse.timestamp, pulse.memory_usage_percent),
      ),
    );
    this._networkRxRollingBuffer.update((buffer) =>
      appendTelemetrySample(
        buffer,
        createTelemetrySample(pulse.timestamp, pulse.network_rx_bytes_sec),
      ),
    );
    this._networkTxRollingBuffer.update((buffer) =>
      appendTelemetrySample(
        buffer,
        createTelemetrySample(pulse.timestamp, pulse.network_tx_bytes_sec),
      ),
    );
  }

  clearBuffers(): void {
    this._cpuRollingBuffer.set([]);
    this._memoryRollingBuffer.set([]);
    this._networkRxRollingBuffer.set([]);
    this._networkTxRollingBuffer.set([]);
  }

  fetchInitialVitals(): Observable<InitialVitalsSnapshot> {
    this._isLoading.set(true);
    this._lastError.set(null);

    return forkJoin({
      health: this.connectionState.checkHealth(),
      system: this.apiClient.getSystem(),
      cpu: this.apiClient.getCpu(),
      memory: this.apiClient.getMemory(),
      disks: this.apiClient.getDisks(),
      network: this.apiClient.getNetwork(),
      processes: this.apiClient.getProcesses('cpu', 5),
    }).pipe(
      tap(({ system, cpu, memory, disks, network, processes }) => {
        this.updateSystem(system);
        this.updateCpu(cpu);
        this.updateMemory(memory);
        this.updateDisks(disks);
        this.updateNetwork(network);
        this.updateTopProcesses(processes);
        this._lastError.set(null);
      }),
      catchError((error: unknown) => {
        const errorMessage = extractErrorMessage(error);
        this._lastError.set(errorMessage);
        return throwError(() => error);
      }),
      finalize(() => {
        this._isLoading.set(false);
      }),
    );
  }
}
