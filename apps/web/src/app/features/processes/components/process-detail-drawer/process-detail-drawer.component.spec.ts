import { ComponentFixture, TestBed } from '@angular/core/testing';
import { of, throwError } from 'rxjs';
import { describe, expect, it, beforeEach, vi } from 'vitest';
import { ProcessDetailDrawerComponent } from './process-detail-drawer.component';
import { ApiClientService } from '../../../../core/api/api-client.service';
import { ProcessDetail } from '../../../../core/models/process.model';

describe('ProcessDetailDrawerComponent', () => {
  let component: ProcessDetailDrawerComponent;
  let fixture: ComponentFixture<ProcessDetailDrawerComponent>;

  const mockDetail: ProcessDetail = {
    pid: 1234,
    ppid: 1,
    name: 'nodepulse',
    user: 'root',
    state: 'S',
    cpu_percent: 1.5,
    memory_rss_bytes: 45000000,
    memory_vms_bytes: 120000000,
    thread_count: 4,
    open_fd_count: 12,
    start_time_epoch: 1700000000,
    cmdline: '/usr/bin/nodepulse --config /etc/nodepulse.json',
    working_directory: '/var/run/nodepulse',
  };

  const mockApiClient = {
    getProcess: vi.fn().mockReturnValue(of(mockDetail)),
    getProcessDetail: vi.fn().mockReturnValue(of(mockDetail)),
  };

  beforeEach(async () => {
    mockApiClient.getProcess.mockReset();
    mockApiClient.getProcessDetail.mockReset();
    mockApiClient.getProcess.mockReturnValue(of(mockDetail));
    mockApiClient.getProcessDetail.mockReturnValue(of(mockDetail));

    await TestBed.configureTestingModule({
      imports: [ProcessDetailDrawerComponent],
      providers: [
        { provide: ApiClientService, useValue: mockApiClient },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(ProcessDetailDrawerComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  describe('drawer visibility and PID querying', () => {
    it('should be closed when pid is null, undefined, or <= 0', () => {
      fixture.componentRef.setInput('pid', null);
      fixture.detectChanges();
      expect(component.isOpen()).toBe(false);
      expect(mockApiClient.getProcess).not.toHaveBeenCalled();

      fixture.componentRef.setInput('pid', 0);
      fixture.detectChanges();
      expect(component.isOpen()).toBe(false);

      fixture.componentRef.setInput('pid', -5);
      fixture.detectChanges();
      expect(component.isOpen()).toBe(false);
    });

    it('should open and fetch process details when a valid positive PID is provided', () => {
      fixture.componentRef.setInput('pid', 1234);
      fixture.detectChanges();

      expect(component.isOpen()).toBe(true);
      expect(mockApiClient.getProcess).toHaveBeenCalledWith(1234);
    });
  });

  describe('displaying process details', () => {
    beforeEach(() => {
      fixture.componentRef.setInput('pid', 1234);
      fixture.detectChanges();
    });

    it('should display process title and subtitle in drawer', () => {
      expect(component.title()).toBe('nodepulse');
      expect(component.subtitle()).toContain('1234');
    });

    it('should render header badges including PID badge and state badge', () => {
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('PID 1234');
      expect(el.textContent).toContain('S');
    });

    it('should display core telemetry (CPU %, RSS, VMS, Threads, FDs)', () => {
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('1.5%');
      // formatBytes(45000000) ~ 42.9 MB
      expect(el.textContent).toContain('42.9 MB');
      // formatBytes(120000000) ~ 114.4 MB
      expect(el.textContent).toContain('114.4 MB');
      expect(el.textContent).toContain('4'); // thread count
      expect(el.textContent).toContain('12'); // open fds
    });

    it('should render neutral CPU % text with high-contrast font-mono without warning colors', () => {
      const el = fixture.nativeElement as HTMLElement;
      const cpuElements = Array.from(el.querySelectorAll('.text-zinc-100.font-mono'));
      const hasCpu = cpuElements.some((e) => e.textContent?.includes('1.5%'));
      expect(hasCpu).toBe(true);

      // Verify zero arbitrary warning colors (amber, red, yellow)
      const warningElements = el.querySelectorAll('.text-amber-500, .text-red-500, .text-yellow-500, .bg-amber-500, .bg-red-500');
      expect(warningElements.length).toBe(0);
    });

    it('should display metadata (PPID, User, Start Time)', () => {
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('1'); // PPID
      expect(el.textContent).toContain('root'); // User
      // formatEpochSeconds(1700000000) -> 2023-11-14 22:13:20 UTC
      expect(el.textContent).toContain('2023-11-14 22:13:20 UTC');
    });

    it('should display selectable monospace working directory', () => {
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('/var/run/nodepulse');
    });

    it('should display command line and copy to clipboard on copy button click', async () => {
      vi.useFakeTimers();
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('/usr/bin/nodepulse --config /etc/nodepulse.json');

      const writeTextMock = vi.fn().mockResolvedValue(undefined);
      Object.assign(navigator, {
        clipboard: { writeText: writeTextMock },
      });

      const copyBtn = el.querySelector('button[aria-label*="Copy"], button[aria-label*="copy"]') as HTMLButtonElement;
      expect(copyBtn).toBeTruthy();
      copyBtn.click();
      await vi.advanceTimersByTimeAsync(10);
      fixture.detectChanges();

      expect(writeTextMock).toHaveBeenCalledWith(mockDetail.cmdline);
      expect(component.isCopied()).toBe(true);
      expect(el.textContent).toContain('Copied!');

      // After 2000ms timeout it reverts
      await vi.advanceTimersByTimeAsync(2000);
      fixture.detectChanges();
      expect(component.isCopied()).toBe(false);
      vi.useRealTimers();
    });
  });

  describe('close output emission', () => {
    it('should emit close output when onClose is triggered', () => {
      fixture.componentRef.setInput('pid', 1234);
      fixture.detectChanges();

      let emitted = false;
      component.close.subscribe(() => {
        emitted = true;
      });

      component.onClose();
      expect(emitted).toBe(true);
    });
  });

  describe('error / not found handling', () => {
    it('should display empty state inside drawer when API returns an error', () => {
      mockApiClient.getProcess.mockReturnValue(throwError(() => new Error('Process exited')));
      fixture.componentRef.setInput('pid', 9999);
      fixture.detectChanges();

      expect(component.isOpen()).toBe(true);
      expect(component.hasError()).toBe(true);

      const emptyState = fixture.nativeElement.querySelector('np-empty-state');
      expect(emptyState).toBeTruthy();
      expect(fixture.nativeElement.textContent).toContain('Process Not Found');
    });
  });
});
