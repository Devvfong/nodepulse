import { inject, Injectable } from '@angular/core';
import { HttpClient, HttpParams } from '@angular/common/http';
import { Observable } from 'rxjs';
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

@Injectable({
  providedIn: 'root',
})
export class ApiClientService {
  private readonly http = inject(HttpClient);
  private readonly baseUrl = '/api/v1';

  getHealth(): Observable<HealthResponse> {
    return this.http.get<HealthResponse>(`${this.baseUrl}/health`);
  }

  getSystem(): Observable<SystemInfo> {
    return this.http.get<SystemInfo>(`${this.baseUrl}/system`);
  }

  getCpu(): Observable<CpuMetrics> {
    return this.http.get<CpuMetrics>(`${this.baseUrl}/cpu`);
  }

  getMemory(): Observable<MemoryMetrics> {
    return this.http.get<MemoryMetrics>(`${this.baseUrl}/memory`);
  }

  getDisks(): Observable<DiskPartitionMetrics[]> {
    return this.http.get<DiskPartitionMetrics[]>(`${this.baseUrl}/disks`);
  }

  getNetwork(): Observable<NetworkInterfaceMetrics[]> {
    return this.http.get<NetworkInterfaceMetrics[]>(`${this.baseUrl}/network`);
  }

  getProcesses(sort?: 'cpu' | 'memory' | 'pid', limit?: number): Observable<ProcessInfo[]> {
    let params = new HttpParams();
    if (sort) {
      params = params.set('sort', sort);
    }
    if (limit !== undefined && limit !== null) {
      params = params.set('limit', limit.toString());
    }
    return this.http.get<ProcessInfo[]>(`${this.baseUrl}/processes`, { params });
  }

  getProcessDetail(pid: number): Observable<ProcessDetail> {
    return this.http.get<ProcessDetail>(`${this.baseUrl}/processes/${pid}`);
  }

  getProcess(pid: number): Observable<ProcessDetail> {
    return this.getProcessDetail(pid);
  }

  getServices(
    state?: 'active' | 'inactive' | 'failed' | 'all',
    limit?: number
  ): Observable<ServiceInfo[]> {
    let params = new HttpParams();
    if (state) {
      params = params.set('state', state);
    }
    if (limit !== undefined && limit !== null) {
      params = params.set('limit', limit.toString());
    }
    return this.http.get<ServiceInfo[]>(`${this.baseUrl}/services`, { params });
  }

  getServiceDetail(name: string): Observable<ServiceDetail> {
    return this.http.get<ServiceDetail>(`${this.baseUrl}/services/${encodeURIComponent(name)}`);
  }

  getContainers(): Observable<ContainerSummary[]> {
    return this.http.get<ContainerSummary[]>(`${this.baseUrl}/containers`);
  }

  getContainerDetail(id: string): Observable<ContainerDetail> {
    return this.http.get<ContainerDetail>(`${this.baseUrl}/containers/${encodeURIComponent(id)}`);
  }
}
