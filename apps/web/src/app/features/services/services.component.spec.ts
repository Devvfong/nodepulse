import { ComponentFixture, TestBed } from '@angular/core/testing';
import { ActivatedRoute, convertToParamMap, ParamMap, Router } from '@angular/router';
import { BehaviorSubject, of } from 'rxjs';
import { describe, expect, it, beforeEach, vi } from 'vitest';
import { ServicesComponent } from './services.component';
import { ApiClientService } from '../../core/api/api-client.service';
import { ServiceDetail, ServiceInfo } from '../../core/models/service.model';

describe('ServicesComponent', () => {
  let component: ServicesComponent;
  let fixture: ComponentFixture<ServicesComponent>;
  let queryParamsSubject: BehaviorSubject<ParamMap>;

  const mockServices: ServiceInfo[] = [
    {
      name: 'nodepulse.service',
      description: 'NodePulse Monitoring Agent',
      load_state: 'loaded',
      active_state: 'active',
      sub_state: 'running',
      unit_file_state: 'enabled',
    },
    {
      name: 'nginx.service',
      description: 'A high performance web server',
      load_state: 'loaded',
      active_state: 'active',
      sub_state: 'running',
      unit_file_state: 'enabled',
    },
    {
      name: 'docker.service',
      description: 'Docker Application Container Engine',
      load_state: 'loaded',
      active_state: 'failed',
      sub_state: 'failed',
      unit_file_state: 'enabled',
    },
    {
      name: 'bluetooth.service',
      description: 'Bluetooth service',
      load_state: 'loaded',
      active_state: 'inactive',
      sub_state: 'dead',
      unit_file_state: 'disabled',
    },
  ];

  const mockDetail: ServiceDetail = {
    name: 'nodepulse.service',
    description: 'NodePulse Monitoring Agent',
    load_state: 'loaded',
    active_state: 'active',
    sub_state: 'running',
    unit_file_state: 'enabled',
    main_pid: 4820,
    restart_count: 0,
    active_enter_timestamp_utc: 1700000000,
    memory_current_bytes: 45000000,
  };

  const mockApiClient = {
    getServices: vi.fn().mockReturnValue(of(mockServices)),
    getServiceDetail: vi.fn().mockReturnValue(of(mockDetail)),
  };

  const mockRouter = {
    navigate: vi.fn().mockResolvedValue(true),
  };

  beforeEach(async () => {
    queryParamsSubject = new BehaviorSubject<ParamMap>(convertToParamMap({}));

    mockApiClient.getServices.mockReset();
    mockApiClient.getServiceDetail.mockReset();
    mockApiClient.getServices.mockReturnValue(of(mockServices));
    mockApiClient.getServiceDetail.mockReturnValue(of(mockDetail));

    mockRouter.navigate.mockReset();
    mockRouter.navigate.mockResolvedValue(true);

    await TestBed.configureTestingModule({
      imports: [ServicesComponent],
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

    fixture = TestBed.createComponent(ServicesComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  describe('initial data fetching', () => {
    it('should fetch services on init with default filter (all) and limit (50)', () => {
      fixture.detectChanges();
      expect(mockApiClient.getServices).toHaveBeenCalledWith('all', 50);
      expect(component.services().length).toBe(4);
    });

    it('should render services table with correct headers and column data', () => {
      fixture.detectChanges();
      const el = fixture.nativeElement as HTMLElement;

      expect(el.textContent).toContain('Unit Name');
      expect(el.textContent).toContain('Description');
      expect(el.textContent).toContain('Active State');
      expect(el.textContent).toContain('Sub-State');
      expect(el.textContent).toContain('Unit File State');

      // Row values
      expect(el.textContent).toContain('nodepulse.service');
      expect(el.textContent).toContain('NodePulse Monitoring Agent');
      expect(el.textContent).toContain('ACTIVE');
      expect(el.textContent).toContain('running');
      expect(el.textContent).toContain('docker.service');
      expect(el.textContent).toContain('FAILED');
    });

    it('should have responsive Unit File State column hidden on mobile (hidden sm:table-cell)', () => {
      fixture.detectChanges();
      const th = fixture.nativeElement.querySelector('th.hidden.sm\\:table-cell');
      expect(th).toBeTruthy();
      expect(th.textContent).toContain('Unit File State');
    });
  });

  describe('toolbar controls (state filter, limit, refresh)', () => {
    beforeEach(() => {
      fixture.detectChanges();
      mockApiClient.getServices.mockClear();
    });

    it('should re-fetch services when state filter is changed (active, failed, inactive, all)', () => {
      component.setStateFilter('active');
      fixture.detectChanges();
      expect(component.stateFilter()).toBe('active');
      expect(mockApiClient.getServices).toHaveBeenCalledWith('active', 50);

      component.setStateFilter('failed');
      fixture.detectChanges();
      expect(component.stateFilter()).toBe('failed');
      expect(mockApiClient.getServices).toHaveBeenCalledWith('failed', 50);

      component.setStateFilter('inactive');
      fixture.detectChanges();
      expect(component.stateFilter()).toBe('inactive');
      expect(mockApiClient.getServices).toHaveBeenCalledWith('inactive', 50);

      component.setStateFilter('all');
      fixture.detectChanges();
      expect(component.stateFilter()).toBe('all');
      expect(mockApiClient.getServices).toHaveBeenCalledWith('all', 50);
    });

    it('should re-fetch services when limit is changed', () => {
      component.setLimit(100);
      fixture.detectChanges();
      expect(component.limit()).toBe(100);
      expect(mockApiClient.getServices).toHaveBeenCalledWith('all', 100);

      component.setLimit(200);
      fixture.detectChanges();
      expect(component.limit()).toBe(200);
      expect(mockApiClient.getServices).toHaveBeenCalledWith('all', 200);
    });

    it('should re-fetch services when refresh button is clicked', () => {
      const refreshBtn = fixture.nativeElement.querySelector('button[aria-label*="Refresh"], button[aria-label*="refresh"]') as HTMLButtonElement;
      expect(refreshBtn).toBeTruthy();

      refreshBtn.click();
      fixture.detectChanges();

      expect(mockApiClient.getServices).toHaveBeenCalledWith('all', 50);
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

    it('should navigate and set ?name query param when a table row is clicked', () => {
      const rows = fixture.nativeElement.querySelectorAll('tbody tr[role="button"]');
      expect(rows.length).toBe(4);

      (rows[0] as HTMLElement).click();

      expect(mockRouter.navigate).toHaveBeenCalledWith([], {
        queryParams: { name: 'nodepulse.service' },
        queryParamsHandling: 'merge',
      });
    });

    it('should navigate on keyboard Enter keydown on row', () => {
      const rows = fixture.nativeElement.querySelectorAll('tbody tr[role="button"]');
      const event = new KeyboardEvent('keydown', { key: 'Enter', bubbles: true });
      rows[1].dispatchEvent(event);

      expect(mockRouter.navigate).toHaveBeenCalledWith([], {
        queryParams: { name: 'nginx.service' },
        queryParamsHandling: 'merge',
      });
    });

    it('should navigate on keyboard Space keydown on row', () => {
      const rows = fixture.nativeElement.querySelectorAll('tbody tr[role="button"]');
      const event = new KeyboardEvent('keydown', { key: ' ', bubbles: true, cancelable: true });
      rows[2].dispatchEvent(event);

      expect(mockRouter.navigate).toHaveBeenCalledWith([], {
        queryParams: { name: 'docker.service' },
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
    it('should sync selectedName from route ?name query parameter', () => {
      queryParamsSubject.next(convertToParamMap({ name: 'nodepulse.service' }));
      fixture.detectChanges();

      expect(component.selectedName()).toBe('nodepulse.service');

      queryParamsSubject.next(convertToParamMap({}));
      fixture.detectChanges();

      expect(component.selectedName()).toBe(null);
    });

    it('should navigate to clear ?name query param when drawer emits close', () => {
      queryParamsSubject.next(convertToParamMap({ name: 'nodepulse.service' }));
      fixture.detectChanges();

      component.onCloseDrawer();

      expect(mockRouter.navigate).toHaveBeenCalledWith([], {
        queryParams: { name: null },
        queryParamsHandling: 'merge',
      });
    });
  });
});
