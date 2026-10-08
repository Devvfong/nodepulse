import { ComponentFixture, TestBed } from '@angular/core/testing';
import { ActivatedRoute, convertToParamMap, ParamMap, Router } from '@angular/router';
import { BehaviorSubject, of, throwError } from 'rxjs';
import { describe, expect, it, beforeEach, vi } from 'vitest';
import { ContainersComponent } from './containers.component';
import { ApiClientService } from '../../core/api/api-client.service';
import { ContainerDetail, ContainerSummary } from '../../core/models/container.model';

describe('ContainersComponent', () => {
  let component: ContainersComponent;
  let fixture: ComponentFixture<ContainersComponent>;
  let queryParamsSubject: BehaviorSubject<ParamMap>;

  const mockContainers: ContainerSummary[] = [
    {
      id: 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
      names: ['/redis-cache'],
      image: 'redis:7.2-alpine',
      status: 'Up 3 days',
      state: 'running',
      created: 1700000000,
    },
    {
      id: 'c8f7a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0',
      names: ['/web-nginx'],
      image: 'nginx:1.25',
      status: 'Exited (0) 2 hours ago',
      state: 'exited',
      created: 1699900000,
    },
  ];

  const mockDetail: ContainerDetail = {
    id: 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
    name: '/redis-cache',
    image: 'redis:7.2-alpine',
    status: 'Up 3 days',
    state: 'running',
    running: true,
    exit_code: 0,
    port_mappings: ['0.0.0.0:6379->6379/tcp'],
    mount_sources: ['/var/lib/docker/volumes/redis_data/_data'],
    created: 1700000000,
  };

  const mockApiClient = {
    getContainers: vi.fn().mockReturnValue(of(mockContainers)),
    getContainerDetail: vi.fn().mockReturnValue(of(mockDetail)),
  };

  const mockRouter = {
    navigate: vi.fn().mockResolvedValue(true),
  };

  beforeEach(async () => {
    queryParamsSubject = new BehaviorSubject<ParamMap>(convertToParamMap({}));

    mockApiClient.getContainers.mockReset();
    mockApiClient.getContainerDetail.mockReset();
    mockApiClient.getContainers.mockReturnValue(of(mockContainers));
    mockApiClient.getContainerDetail.mockReturnValue(of(mockDetail));

    mockRouter.navigate.mockReset();
    mockRouter.navigate.mockResolvedValue(true);

    await TestBed.configureTestingModule({
      imports: [ContainersComponent],
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

    fixture = TestBed.createComponent(ContainersComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  describe('Scenario A (HTTP 200: Docker Available)', () => {
    it('should fetch containers on init and render table with columns and data', () => {
      fixture.detectChanges();

      expect(mockApiClient.getContainers).toHaveBeenCalledTimes(1);
      expect(component.containers().length).toBe(2);
      expect(component.isDockerUnavailable()).toBe(false);

      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('Container Name');
      expect(el.textContent).toContain('Image');
      expect(el.textContent).toContain('State');
      expect(el.textContent).toContain('Status');
      expect(el.textContent).toContain('Created Time');

      // Names stripped of leading slash
      expect(el.textContent).toContain('redis-cache');
      expect(el.textContent).toContain('web-nginx');
      expect(el.textContent).toContain('redis:7.2-alpine');
      expect(el.textContent).toContain('nginx:1.25');
      expect(el.textContent).toContain('Up 3 days');
      expect(el.textContent).toContain('Exited (0) 2 hours ago');
      // Formatted timestamp
      expect(el.textContent).toContain('2023-11-14 22:13:20 UTC');
    });

    it('should render empty state when container list is empty', () => {
      mockApiClient.getContainers.mockReturnValue(of([]));
      fixture = TestBed.createComponent(ContainersComponent);
      component = fixture.componentInstance;
      fixture.detectChanges();

      expect(component.containers().length).toBe(0);

      const emptyState = fixture.nativeElement.querySelector('np-empty-state');
      expect(emptyState).toBeTruthy();
      expect(fixture.nativeElement.textContent).toContain('No Containers Found');
      expect(fixture.nativeElement.textContent).toContain(
        'No Docker containers are currently running or stopped on this host.'
      );
    });
  });

  describe('Scenario B (HTTP 503 / DOCKER_UNAVAILABLE: Docker Daemon Unavailable)', () => {
    it('should render "Docker Unavailable on Host" empty state without broken layout on HTTP 503', () => {
      mockApiClient.getContainers.mockReturnValue(
        throwError(() => ({
          status: 503,
          error: {
            code: 'DOCKER_UNAVAILABLE',
            message: 'Docker daemon is not responding',
          },
        }))
      );
      fixture = TestBed.createComponent(ContainersComponent);
      component = fixture.componentInstance;
      fixture.detectChanges();

      expect(component.isDockerUnavailable()).toBe(true);

      const emptyState = fixture.nativeElement.querySelector('np-empty-state');
      expect(emptyState).toBeTruthy();
      expect(fixture.nativeElement.textContent).toContain('Docker Unavailable on Host');
      expect(fixture.nativeElement.textContent).toContain(
        'NodePulse could not communicate with /var/run/docker.sock. Docker may be stopped or not installed on this host.'
      );

      // Verify no table is displayed
      const table = fixture.nativeElement.querySelector('table');
      expect(table).toBeNull();
    });

    it('should render "Docker Unavailable on Host" when error code is DOCKER_UNAVAILABLE', () => {
      mockApiClient.getContainers.mockReturnValue(
        throwError(() => ({
          status: 500,
          error: {
            error: {
              code: 'DOCKER_UNAVAILABLE',
            },
          },
        }))
      );
      fixture = TestBed.createComponent(ContainersComponent);
      component = fixture.componentInstance;
      fixture.detectChanges();

      expect(component.isDockerUnavailable()).toBe(true);
      expect(fixture.nativeElement.textContent).toContain('Docker Unavailable on Host');
    });

    it('should trigger re-fetch when retry button in Docker unavailable state is clicked', () => {
      mockApiClient.getContainers.mockReturnValue(
        throwError(() => ({ status: 503 }))
      );
      fixture = TestBed.createComponent(ContainersComponent);
      component = fixture.componentInstance;
      fixture.detectChanges();

      expect(component.isDockerUnavailable()).toBe(true);

      // Change API response to succeed on retry
      mockApiClient.getContainers.mockReturnValue(of(mockContainers));

      const retryBtn = fixture.nativeElement.querySelector(
        'button[aria-label*="Retry"], button[aria-label*="retry"]'
      ) as HTMLButtonElement;
      expect(retryBtn).toBeTruthy();

      retryBtn.click();
      fixture.detectChanges();

      expect(component.isDockerUnavailable()).toBe(false);
      expect(component.containers().length).toBe(2);
      expect(fixture.nativeElement.textContent).toContain('redis-cache');
    });
  });

  describe('Scenario C (General Error)', () => {
    it('should render "Failed to Load Containers" empty state with error description and retry button', () => {
      mockApiClient.getContainers.mockReturnValue(
        throwError(() => new Error('Network connection failed'))
      );
      fixture = TestBed.createComponent(ContainersComponent);
      component = fixture.componentInstance;
      fixture.detectChanges();

      expect(component.isDockerUnavailable()).toBe(false);
      expect(component.errorMessage()).toContain('Network connection failed');

      const emptyState = fixture.nativeElement.querySelector('np-empty-state');
      expect(emptyState).toBeTruthy();
      expect(fixture.nativeElement.textContent).toContain('Failed to Load Containers');
      expect(fixture.nativeElement.textContent).toContain('Network connection failed');

      const retryBtn = fixture.nativeElement.querySelector(
        'button[aria-label*="Retry"], button[aria-label*="retry"]'
      ) as HTMLButtonElement;
      expect(retryBtn).toBeTruthy();
    });
  });

  describe('table row interactions, navigation, and accessibility', () => {
    beforeEach(() => {
      fixture.detectChanges();
    });

    it('should navigate and set ?id query param when a table row is clicked', () => {
      const rows = fixture.nativeElement.querySelectorAll('tbody tr[role="button"]');
      expect(rows.length).toBe(2);

      (rows[0] as HTMLElement).click();

      expect(mockRouter.navigate).toHaveBeenCalledWith([], {
        queryParams: { id: mockContainers[0].id },
        queryParamsHandling: 'merge',
      });
    });

    it('should navigate on keyboard Enter keydown on row', () => {
      const rows = fixture.nativeElement.querySelectorAll('tbody tr[role="button"]');
      const event = new KeyboardEvent('keydown', { key: 'Enter', bubbles: true });
      rows[1].dispatchEvent(event);

      expect(mockRouter.navigate).toHaveBeenCalledWith([], {
        queryParams: { id: mockContainers[1].id },
        queryParamsHandling: 'merge',
      });
    });

    it('should navigate on keyboard Space keydown on row', () => {
      const rows = fixture.nativeElement.querySelectorAll('tbody tr[role="button"]');
      const event = new KeyboardEvent('keydown', { key: ' ', bubbles: true, cancelable: true });
      rows[0].dispatchEvent(event);

      expect(mockRouter.navigate).toHaveBeenCalledWith([], {
        queryParams: { id: mockContainers[0].id },
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

  describe('toolbar controls (refresh)', () => {
    beforeEach(() => {
      fixture.detectChanges();
      mockApiClient.getContainers.mockClear();
    });

    it('should re-fetch containers when refresh button is clicked', () => {
      const refreshBtn = fixture.nativeElement.querySelector(
        'header button[aria-label*="Refresh"], header button[aria-label*="refresh"]'
      ) as HTMLButtonElement;
      expect(refreshBtn).toBeTruthy();

      refreshBtn.click();
      fixture.detectChanges();

      expect(mockApiClient.getContainers).toHaveBeenCalledTimes(1);
    });

    it('should disable refresh button while loading is true', () => {
      component.isLoading.set(true);
      fixture.detectChanges();

      const refreshBtn = fixture.nativeElement.querySelector(
        'header button[aria-label*="Refresh"], header button[aria-label*="refresh"]'
      ) as HTMLButtonElement;
      expect(refreshBtn.disabled).toBe(true);
    });
  });

  describe('drawer integration and route synchronization', () => {
    it('should sync selectedId from route ?id query parameter', () => {
      queryParamsSubject.next(convertToParamMap({ id: 'test-container-id-123' }));
      fixture.detectChanges();

      expect(component.selectedId()).toBe('test-container-id-123');

      queryParamsSubject.next(convertToParamMap({}));
      fixture.detectChanges();

      expect(component.selectedId()).toBe(null);
    });

    it('should navigate to clear ?id query param when drawer emits close', () => {
      queryParamsSubject.next(convertToParamMap({ id: 'test-container-id-123' }));
      fixture.detectChanges();

      component.onCloseDrawer();

      expect(mockRouter.navigate).toHaveBeenCalledWith([], {
        queryParams: { id: null },
        queryParamsHandling: 'merge',
      });
    });
  });
});
