import { ComponentFixture, TestBed } from '@angular/core/testing';
import { Router } from '@angular/router';
import { of, throwError } from 'rxjs';
import { describe, expect, it, beforeEach, vi } from 'vitest';
import { ServiceDetailDrawerComponent } from './service-detail-drawer.component';
import { ApiClientService } from '../../../../core/api/api-client.service';
import { ServiceDetail } from '../../../../core/models/service.model';

describe('ServiceDetailDrawerComponent', () => {
  let component: ServiceDetailDrawerComponent;
  let fixture: ComponentFixture<ServiceDetailDrawerComponent>;

  const mockDetail: ServiceDetail = {
    name: 'nodepulse.service',
    description: 'NodePulse Host Monitoring Agent',
    load_state: 'loaded',
    active_state: 'active',
    sub_state: 'running',
    unit_file_state: 'enabled',
    main_pid: 4820,
    restart_count: 2,
    active_enter_timestamp_utc: 1700000000,
    memory_current_bytes: 45000000,
  };

  const mockApiClient = {
    getServiceDetail: vi.fn().mockReturnValue(of(mockDetail)),
  };

  const mockRouter = {
    navigate: vi.fn().mockResolvedValue(true),
  };

  beforeEach(async () => {
    mockApiClient.getServiceDetail.mockReset();
    mockApiClient.getServiceDetail.mockReturnValue(of(mockDetail));

    mockRouter.navigate.mockReset();
    mockRouter.navigate.mockResolvedValue(true);

    await TestBed.configureTestingModule({
      imports: [ServiceDetailDrawerComponent],
      providers: [
        { provide: ApiClientService, useValue: mockApiClient },
        { provide: Router, useValue: mockRouter },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(ServiceDetailDrawerComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  describe('drawer visibility and service querying', () => {
    it('should be closed when name is null, undefined, or empty', () => {
      fixture.componentRef.setInput('name', null);
      fixture.detectChanges();
      expect(component.isOpen()).toBe(false);
      expect(mockApiClient.getServiceDetail).not.toHaveBeenCalled();

      fixture.componentRef.setInput('name', undefined);
      fixture.detectChanges();
      expect(component.isOpen()).toBe(false);

      fixture.componentRef.setInput('name', '');
      fixture.detectChanges();
      expect(component.isOpen()).toBe(false);

      fixture.componentRef.setInput('name', '   ');
      fixture.detectChanges();
      expect(component.isOpen()).toBe(false);
    });

    it('should open and fetch service details when a valid name is provided', () => {
      fixture.componentRef.setInput('name', 'nodepulse.service');
      fixture.detectChanges();

      expect(component.isOpen()).toBe(true);
      expect(mockApiClient.getServiceDetail).toHaveBeenCalledWith('nodepulse.service');
    });

    it('should fetch service detail with special characters', () => {
      fixture.componentRef.setInput('name', 'systemd-networkd@eth0.service');
      fixture.detectChanges();

      expect(component.isOpen()).toBe(true);
      expect(mockApiClient.getServiceDetail).toHaveBeenCalledWith('systemd-networkd@eth0.service');
    });
  });

  describe('displaying service details', () => {
    beforeEach(() => {
      fixture.componentRef.setInput('name', 'nodepulse.service');
      fixture.detectChanges();
    });

    it('should display service title and subtitle in drawer', () => {
      expect(component.title()).toBe('nodepulse.service');
      expect(component.subtitle()).toContain('NodePulse Host Monitoring Agent');
    });

    it('should render header badges including active state badge, load state badge, and sub-state', () => {
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('ACTIVE');
      expect(el.textContent).toContain('loaded');
      expect(el.textContent).toContain('running');
    });

    it('should display description', () => {
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('NodePulse Host Monitoring Agent');
    });

    it('should display state and configuration details', () => {
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('running'); // sub_state
      expect(el.textContent).toContain('loaded'); // load_state
      expect(el.textContent).toContain('enabled'); // unit_file_state
    });

    it('should display runtime details (restart count, memory current, active timestamp)', () => {
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('2'); // restart count
      // formatBytes(45000000) ~ 42.9 MB
      expect(el.textContent).toContain('42.9 MB');
      // formatEpochSeconds(1700000000) -> 2023-11-14 22:13:20 UTC
      expect(el.textContent).toContain('2023-11-14 22:13:20 UTC');
    });
  });

  describe('Main PID navigation invariant (CRITICAL)', () => {
    it('when main_pid >= 1, renders interactive link/button and navigates to /processes?pid=<main_pid> on click', () => {
      fixture.componentRef.setInput('name', 'nodepulse.service');
      fixture.detectChanges();

      const el = fixture.nativeElement as HTMLElement;
      const pidBtn = el.querySelector('button.font-mono.text-sky-400, a.font-mono.text-sky-400, [aria-label*="process"]') as HTMLElement;
      expect(pidBtn).toBeTruthy();
      expect(pidBtn.textContent?.trim()).toBe('4820');

      pidBtn.click();

      expect(mockRouter.navigate).toHaveBeenCalledWith(['/processes'], {
        queryParams: { pid: 4820 },
      });
    });

    it('when main_pid <= 0 (pid = 0), renders static " — " text and does NOT navigate or render link', () => {
      mockApiClient.getServiceDetail.mockReturnValue(of({
        ...mockDetail,
        main_pid: 0,
      }));

      fixture.componentRef.setInput('name', 'nodepulse.service');
      fixture.detectChanges();

      const el = fixture.nativeElement as HTMLElement;
      const pidLink = el.querySelector('button.font-mono.text-sky-400, a.font-mono.text-sky-400');
      expect(pidLink).toBeNull();

      expect(el.textContent).toContain('—');
      expect(mockRouter.navigate).not.toHaveBeenCalled();
    });

    it('when main_pid is negative (pid = -1), renders static " — " text and does NOT navigate', () => {
      mockApiClient.getServiceDetail.mockReturnValue(of({
        ...mockDetail,
        main_pid: -1,
      }));

      fixture.componentRef.setInput('name', 'nodepulse.service');
      fixture.detectChanges();

      const el = fixture.nativeElement as HTMLElement;
      const pidLink = el.querySelector('button.font-mono.text-sky-400, a.font-mono.text-sky-400');
      expect(pidLink).toBeNull();

      expect(el.textContent).toContain('—');
      expect(mockRouter.navigate).not.toHaveBeenCalled();
    });
  });

  describe('close output emission', () => {
    it('should emit close output when onClose is triggered', () => {
      fixture.componentRef.setInput('name', 'nodepulse.service');
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
      mockApiClient.getServiceDetail.mockReturnValue(throwError(() => new Error('Service unit not found')));
      fixture.componentRef.setInput('name', 'non-existent.service');
      fixture.detectChanges();

      expect(component.isOpen()).toBe(true);
      expect(component.hasError()).toBe(true);

      const emptyState = fixture.nativeElement.querySelector('np-empty-state');
      expect(emptyState).toBeTruthy();
      expect(fixture.nativeElement.textContent).toContain('Service Not Found');
    });
  });
});
