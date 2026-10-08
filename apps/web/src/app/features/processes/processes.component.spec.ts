import { ComponentFixture, TestBed } from '@angular/core/testing';
import { ActivatedRoute, convertToParamMap, ParamMap, Router } from '@angular/router';
import { BehaviorSubject, of } from 'rxjs';
import { describe, expect, it, beforeEach, vi } from 'vitest';
import { ProcessesComponent } from './processes.component';
import { ApiClientService } from '../../core/api/api-client.service';
import { ProcessInfo } from '../../core/models/process.model';

describe('ProcessesComponent', () => {
  let component: ProcessesComponent;
  let fixture: ComponentFixture<ProcessesComponent>;
  let queryParamsSubject: BehaviorSubject<ParamMap>;

  const mockProcesses: ProcessInfo[] = [
    {
      pid: 101,
      name: 'systemd',
      user: 'root',
      state: 'S',
      cpu_percent: 0.1,
      memory_rss_bytes: 12000000,
      cmdline: '/sbin/init',
    },
    {
      pid: 102,
      name: 'nodepulse',
      user: 'root',
      state: 'R',
      cpu_percent: 4.5,
      memory_rss_bytes: 65000000,
      cmdline: '/usr/bin/nodepulse',
    },
    {
      pid: 103,
      name: 'defunct_worker',
      user: 'www-data',
      state: 'Z',
      cpu_percent: 0.0,
      memory_rss_bytes: 0,
      cmdline: '[defunct_worker] <defunct>',
    },
  ];

  const mockDetail = {
    pid: 102,
    ppid: 1,
    name: 'nodepulse',
    user: 'root',
    state: 'R',
    cpu_percent: 4.5,
    memory_rss_bytes: 65000000,
    memory_vms_bytes: 120000000,
    thread_count: 4,
    open_fd_count: 12,
    start_time_epoch: 1700000000,
    cmdline: '/usr/bin/nodepulse',
    working_directory: '/var/run/nodepulse',
  };

  const mockApiClient = {
    getProcesses: vi.fn().mockReturnValue(of(mockProcesses)),
    getProcess: vi.fn().mockReturnValue(of(mockDetail)),
    getProcessDetail: vi.fn().mockReturnValue(of(mockDetail)),
  };

  const mockRouter = {
    navigate: vi.fn().mockResolvedValue(true),
  };

  beforeEach(async () => {
    queryParamsSubject = new BehaviorSubject<ParamMap>(convertToParamMap({}));

    mockApiClient.getProcesses.mockReset();
    mockApiClient.getProcess.mockReset();
    mockApiClient.getProcessDetail.mockReset();
    mockApiClient.getProcesses.mockReturnValue(of(mockProcesses));
    mockApiClient.getProcess.mockReturnValue(of(mockDetail));
    mockApiClient.getProcessDetail.mockReturnValue(of(mockDetail));

    mockRouter.navigate.mockReset();
    mockRouter.navigate.mockResolvedValue(true);

    await TestBed.configureTestingModule({
      imports: [ProcessesComponent],
      providers: [
        { provide: ApiClientService, useValue: mockApiClient },
        { provide: Router, useValue: mockRouter },
        {
          provide: ActivatedRoute,
          useValue: {
            queryParamMap: queryParamsSubject.asObservable(),
          },
        },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(ProcessesComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  describe('initial data fetching', () => {
    it('should fetch processes on init with default sort (cpu) and limit (50)', () => {
      fixture.detectChanges();
      expect(mockApiClient.getProcesses).toHaveBeenCalledWith('cpu', 50);
      expect(component.processes().length).toBe(3);
    });

    it('should render processes table with correct headers and column data', () => {
      fixture.detectChanges();
      const el = fixture.nativeElement as HTMLElement;

      expect(el.textContent).toContain('PID');
      expect(el.textContent).toContain('Name');
      expect(el.textContent).toContain('User');
      expect(el.textContent).toContain('State');
      expect(el.textContent).toContain('CPU %');
      expect(el.textContent).toContain('Memory (RSS)');
      expect(el.textContent).toContain('Command Line');

      // Row values
      expect(el.textContent).toContain('101');
      expect(el.textContent).toContain('systemd');
      expect(el.textContent).toContain('nodepulse');
      expect(el.textContent).toContain('4.5%');
    });

    it('should render neutral CPU % text without arbitrary color thresholds', () => {
      fixture.detectChanges();
      const el = fixture.nativeElement as HTMLElement;
      const cpuElements = Array.from(el.querySelectorAll('.text-zinc-100.font-mono'));
      const hasCpu = cpuElements.some((e) => e.textContent?.includes('4.5%'));
      expect(hasCpu).toBe(true);

      const warningElements = el.querySelectorAll('.text-amber-500, .text-red-500, .bg-amber-500, .bg-red-500');
      expect(warningElements.length).toBe(0);
    });
  });

  describe('toolbar controls (sort, limit, refresh)', () => {
    beforeEach(() => {
      fixture.detectChanges();
      mockApiClient.getProcesses.mockClear();
    });

    it('should re-fetch processes when sort is changed', () => {
      component.setSort('memory');
      fixture.detectChanges();

      expect(component.sort()).toBe('memory');
      expect(mockApiClient.getProcesses).toHaveBeenCalledWith('memory', 50);

      component.setSort('pid');
      fixture.detectChanges();

      expect(component.sort()).toBe('pid');
      expect(mockApiClient.getProcesses).toHaveBeenCalledWith('pid', 50);
    });

    it('should re-fetch processes when limit is changed', () => {
      component.setLimit(100);
      fixture.detectChanges();

      expect(component.limit()).toBe(100);
      expect(mockApiClient.getProcesses).toHaveBeenCalledWith('cpu', 100);
    });

    it('should re-fetch processes when refresh button is clicked', () => {
      const refreshBtn = fixture.nativeElement.querySelector('button[aria-label*="Refresh"], button[aria-label*="refresh"]') as HTMLButtonElement;
      expect(refreshBtn).toBeTruthy();

      refreshBtn.click();
      fixture.detectChanges();

      expect(mockApiClient.getProcesses).toHaveBeenCalledWith('cpu', 50);
    });

    it('should disable refresh button while loading is true', () => {
      component.isLoading.set(true);
      fixture.detectChanges();

      const refreshBtn = fixture.nativeElement.querySelector('button[aria-label*="Refresh"], button[aria-label*="refresh"]') as HTMLButtonElement;
      expect(refreshBtn.disabled).toBe(true);
    });
  });

  describe('row interaction and navigation', () => {
    beforeEach(() => {
      fixture.detectChanges();
    });

    it('should navigate and set ?pid query param when a table row is clicked', () => {
      const rows = fixture.nativeElement.querySelectorAll('tbody tr[role="button"]');
      expect(rows.length).toBe(3);

      (rows[1] as HTMLElement).click();

      expect(mockRouter.navigate).toHaveBeenCalledWith([], {
        queryParams: { pid: 102 },
        queryParamsHandling: 'merge',
      });
    });

    it('should navigate on keyboard Enter keydown on row', () => {
      const rows = fixture.nativeElement.querySelectorAll('tbody tr[role="button"]');
      const event = new KeyboardEvent('keydown', { key: 'Enter', bubbles: true });
      rows[1].dispatchEvent(event);

      expect(mockRouter.navigate).toHaveBeenCalledWith([], {
        queryParams: { pid: 102 },
        queryParamsHandling: 'merge',
      });
    });

    it('should navigate on keyboard Space keydown on row', () => {
      const rows = fixture.nativeElement.querySelectorAll('tbody tr[role="button"]');
      const event = new KeyboardEvent('keydown', { key: ' ', bubbles: true, cancelable: true });
      rows[0].dispatchEvent(event);

      expect(mockRouter.navigate).toHaveBeenCalledWith([], {
        queryParams: { pid: 101 },
        queryParamsHandling: 'merge',
      });
    });

    it('table rows should have tabindex="0" and role="button" for a11y', () => {
      const rows = fixture.nativeElement.querySelectorAll('tbody tr[role="button"]');
      for (const row of Array.from(rows)) {
        expect((row as HTMLElement).getAttribute('tabindex')).toBe('0');
        expect((row as HTMLElement).getAttribute('role')).toBe('button');
      }
    });
  });

  describe('drawer integration and route synchronization', () => {
    it('should sync selectedPid from route ?pid query parameter', () => {
      queryParamsSubject.next(convertToParamMap({ pid: '102' }));
      fixture.detectChanges();

      expect(component.selectedPid()).toBe(102);

      queryParamsSubject.next(convertToParamMap({}));
      fixture.detectChanges();

      expect(component.selectedPid()).toBe(null);
    });

    it('should navigate to clear ?pid query param when drawer emits close', () => {
      queryParamsSubject.next(convertToParamMap({ pid: '102' }));
      fixture.detectChanges();

      component.onCloseDrawer();

      expect(mockRouter.navigate).toHaveBeenCalledWith([], {
        queryParams: { pid: null },
        queryParamsHandling: 'merge',
      });
    });
  });
});
