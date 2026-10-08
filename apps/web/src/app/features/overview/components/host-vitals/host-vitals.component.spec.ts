import { ComponentFixture, TestBed } from '@angular/core/testing';
import { signal } from '@angular/core';
import { By } from '@angular/platform-browser';
import { describe, expect, it, beforeEach } from 'vitest';
import { HostVitalsComponent } from './host-vitals.component';
import { HostStateService } from '../../../../core/services/host-state.service';
import { ConnectionStateService } from '../../../../core/services/connection-state.service';
import { StatusBadgeComponent } from '../../../../shared/ui/status-badge/status-badge.component';
import { SystemInfo } from '../../../../core/models/system.model';

describe('HostVitalsComponent', () => {
  let fixture: ComponentFixture<HostVitalsComponent>;
  let component: HostVitalsComponent;

  const mockSystemInfo = signal<SystemInfo | null>(null);
  const mockAgentStatus = signal<'unknown' | 'healthy' | 'unreachable'>('healthy');
  const mockStreamStatus = signal<'connecting' | 'connected' | 'reconnecting' | 'paused'>('connected');

  const mockHostStateService = {
    systemInfo: mockSystemInfo,
  };

  const mockConnectionStateService = {
    agentStatus: mockAgentStatus,
    streamStatus: mockStreamStatus,
  };

  beforeEach(async () => {
    mockSystemInfo.set(null);
    mockAgentStatus.set('healthy');
    mockStreamStatus.set('connected');

    await TestBed.configureTestingModule({
      imports: [HostVitalsComponent],
      providers: [
        { provide: HostStateService, useValue: mockHostStateService },
        { provide: ConnectionStateService, useValue: mockConnectionStateService },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(HostVitalsComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  it('should display fallback "localhost" and dashes when systemInfo is null', () => {
    fixture.detectChanges();
    const el = fixture.nativeElement as HTMLElement;
    expect(el.textContent).toContain('localhost');
  });

  it('should render host metadata correctly when systemInfo is provided', () => {
    mockSystemInfo.set({
      hostname: 'prod-node-01',
      os_name: 'Ubuntu',
      os_version: '22.04 LTS',
      kernel_version: '5.15.0-generic',
      architecture: 'x86_64',
      boot_time_utc: 1700000000,
      uptime_seconds: 1233120, // 14d 06h 32m
    });

    fixture.detectChanges();
    const el = fixture.nativeElement as HTMLElement;

    expect(el.textContent).toContain('prod-node-01');
    expect(el.textContent).toContain('Ubuntu 22.04 LTS');
    expect(el.textContent).toContain('5.15.0-generic');
    expect(el.textContent).toContain('x86_64');
    expect(el.textContent).toContain('14d 06h 32m');
  });

  it('should render decoupled agent and stream status badges', () => {
    mockAgentStatus.set('healthy');
    mockStreamStatus.set('connected');
    fixture.detectChanges();

    const badges = fixture.debugElement.queryAll(By.directive(StatusBadgeComponent));
    expect(badges.length).toBe(2);

    const agentBadge = badges[0].componentInstance as StatusBadgeComponent;
    const streamBadge = badges[1].componentInstance as StatusBadgeComponent;

    expect(agentBadge.status()).toBe('healthy');
    expect(agentBadge.label()).toBe('AGENT: HEALTHY');
    expect(streamBadge.status()).toBe('connected');
    expect(streamBadge.label()).toBe('STREAM: CONNECTED');
  });
});
