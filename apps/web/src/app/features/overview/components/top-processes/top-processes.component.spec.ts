import { ComponentFixture, TestBed } from '@angular/core/testing';
import { signal } from '@angular/core';
import { Router } from '@angular/router';
import { describe, expect, it, beforeEach, vi } from 'vitest';
import { TopProcessesComponent } from './top-processes.component';
import { HostStateService } from '../../../../core/services/host-state.service';
import { ProcessInfo } from '../../../../core/models/process.model';

describe('TopProcessesComponent', () => {
  let fixture: ComponentFixture<TopProcessesComponent>;
  let component: TopProcessesComponent;

  const mockTopProcesses = signal<ProcessInfo[] | null>(null);

  const mockHostStateService = {
    topProcesses: mockTopProcesses,
  };

  const mockRouter = {
    navigate: vi.fn(),
  };

  beforeEach(async () => {
    mockTopProcesses.set(null);
    mockRouter.navigate.mockReset();

    await TestBed.configureTestingModule({
      imports: [TopProcessesComponent],
      providers: [
        { provide: HostStateService, useValue: mockHostStateService },
        { provide: Router, useValue: mockRouter },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(TopProcessesComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  it('should render empty state message when topProcesses is null or empty', () => {
    fixture.detectChanges();
    const el = fixture.nativeElement as HTMLElement;
    expect(el.textContent).toContain('Top Processes by CPU');
    expect(el.textContent).toContain('No process telemetry');
  });

  it('should render up to 5 processes with correct columns', () => {
    const processes: ProcessInfo[] = [
      { pid: 101, name: 'nginx', user: 'root', state: 'S', cpu_percent: 15.4, memory_rss_bytes: 50 * 1024 * 1024, cmdline: 'nginx' },
      { pid: 102, name: 'postgres', user: 'postgres', state: 'S', cpu_percent: 12.1, memory_rss_bytes: 200 * 1024 * 1024, cmdline: 'postgres' },
      { pid: 103, name: 'nodepulse', user: 'devqii', state: 'R', cpu_percent: 8.5, memory_rss_bytes: 35 * 1024 * 1024, cmdline: 'nodepulse' },
      { pid: 104, name: 'dockerd', user: 'root', state: 'S', cpu_percent: 4.2, memory_rss_bytes: 120 * 1024 * 1024, cmdline: 'dockerd' },
      { pid: 105, name: 'systemd', user: 'root', state: 'S', cpu_percent: 1.0, memory_rss_bytes: 15 * 1024 * 1024, cmdline: 'systemd' },
      { pid: 106, name: 'sshd', user: 'root', state: 'S', cpu_percent: 0.5, memory_rss_bytes: 10 * 1024 * 1024, cmdline: 'sshd' }, // 6th, should be sliced to 5
    ];
    mockTopProcesses.set(processes);

    fixture.detectChanges();
    const el = fixture.nativeElement as HTMLElement;

    expect(el.textContent).toContain('nginx');
    expect(el.textContent).toContain('101');
    expect(el.textContent).toContain('15.4%');
    expect(el.textContent).toContain('50.0 MB');

    expect(el.textContent).toContain('postgres');
    expect(el.textContent).toContain('nodepulse');
    expect(el.textContent).toContain('dockerd');
    expect(el.textContent).toContain('systemd');
    // 6th item should not be in the top 5
    expect(el.textContent).not.toContain('sshd');
  });

  it('should navigate to /processes?pid=<pid> when row is clicked', () => {
    mockTopProcesses.set([
      { pid: 4242, name: 'custom-app', user: 'app', state: 'R', cpu_percent: 55.0, memory_rss_bytes: 100 * 1024 * 1024, cmdline: 'custom-app' },
    ]);

    fixture.detectChanges();
    const row = fixture.nativeElement.querySelector('tbody tr') as HTMLElement;
    expect(row).toBeTruthy();

    row.click();

    expect(mockRouter.navigate).toHaveBeenCalledWith(['/processes'], {
      queryParams: { pid: 4242 },
    });
  });
});
