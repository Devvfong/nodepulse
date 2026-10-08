import { ComponentFixture, TestBed } from '@angular/core/testing';
import { signal } from '@angular/core';
import { By } from '@angular/platform-browser';
import { of } from 'rxjs';
import { provideRouter } from '@angular/router';
import { describe, expect, it, beforeEach, vi } from 'vitest';
import { OverviewComponent } from './overview.component';
import { HostStateService } from '../../core/services/host-state.service';
import { ConnectionStateService } from '../../core/services/connection-state.service';
import { SseService } from '../../core/api/sse.service';
import { HostVitalsComponent } from './components/host-vitals/host-vitals.component';
import { CpuCardComponent } from './components/cpu-card/cpu-card.component';
import { MemoryCardComponent } from './components/memory-card/memory-card.component';
import { NetworkCardComponent } from './components/network-card/network-card.component';
import { DisksCardComponent } from './components/disks-card/disks-card.component';
import { TopProcessesComponent } from './components/top-processes/top-processes.component';

describe('OverviewComponent', () => {
  let fixture: ComponentFixture<OverviewComponent>;
  let component: OverviewComponent;

  const mockSystemInfo = signal(null);
  const mockCpuMetrics = signal(null);
  const mockMemoryMetrics = signal(null);
  const mockDiskMetrics = signal(null);
  const mockNetworkMetrics = signal(null);
  const mockTopProcesses = signal(null);
  const mockCpuRollingBuffer = signal([]);
  const mockMemoryRollingBuffer = signal([]);
  const mockNetworkRxRollingBuffer = signal([]);
  const mockNetworkTxRollingBuffer = signal([]);

  const mockFetchInitialVitals = vi.fn().mockReturnValue(of({}));

  const mockHostStateService = {
    systemInfo: mockSystemInfo,
    cpuMetrics: mockCpuMetrics,
    memoryMetrics: mockMemoryMetrics,
    diskMetrics: mockDiskMetrics,
    networkMetrics: mockNetworkMetrics,
    topProcesses: mockTopProcesses,
    cpuRollingBuffer: mockCpuRollingBuffer,
    memoryRollingBuffer: mockMemoryRollingBuffer,
    networkRxRollingBuffer: mockNetworkRxRollingBuffer,
    networkTxRollingBuffer: mockNetworkTxRollingBuffer,
    fetchInitialVitals: mockFetchInitialVitals,
  };

  const mockConnectionStateService = {
    agentStatus: signal('healthy'),
    streamStatus: signal('connected'),
  };

  const mockSseService = {
    pulse: signal(null),
  };

  beforeEach(async () => {
    mockFetchInitialVitals.mockClear();

    await TestBed.configureTestingModule({
      imports: [OverviewComponent],
      providers: [
        provideRouter([]),
        { provide: HostStateService, useValue: mockHostStateService },
        { provide: ConnectionStateService, useValue: mockConnectionStateService },
        { provide: SseService, useValue: mockSseService },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(OverviewComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  it('should dispatch fetchInitialVitals() on route entry (ngOnInit)', () => {
    fixture.detectChanges();
    expect(mockFetchInitialVitals).toHaveBeenCalledTimes(1);
  });

  it('should render all constituent overview subsystem components', () => {
    fixture.detectChanges();

    expect(fixture.debugElement.query(By.directive(HostVitalsComponent))).toBeTruthy();
    expect(fixture.debugElement.query(By.directive(CpuCardComponent))).toBeTruthy();
    expect(fixture.debugElement.query(By.directive(MemoryCardComponent))).toBeTruthy();
    expect(fixture.debugElement.query(By.directive(NetworkCardComponent))).toBeTruthy();
    expect(fixture.debugElement.query(By.directive(DisksCardComponent))).toBeTruthy();
    expect(fixture.debugElement.query(By.directive(TopProcessesComponent))).toBeTruthy();
  });
});
