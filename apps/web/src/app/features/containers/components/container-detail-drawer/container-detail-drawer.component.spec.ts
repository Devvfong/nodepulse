import { ComponentFixture, TestBed } from '@angular/core/testing';
import { of, throwError } from 'rxjs';
import { describe, expect, it, beforeEach, vi } from 'vitest';
import { ContainerDetailDrawerComponent } from './container-detail-drawer.component';
import { ApiClientService } from '../../../../core/api/api-client.service';
import { ContainerDetail } from '../../../../core/models/container.model';

describe('ContainerDetailDrawerComponent', () => {
  let component: ContainerDetailDrawerComponent;
  let fixture: ComponentFixture<ContainerDetailDrawerComponent>;

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
    getContainerDetail: vi.fn().mockReturnValue(of(mockDetail)),
  };

  beforeEach(async () => {
    mockApiClient.getContainerDetail.mockReset();
    mockApiClient.getContainerDetail.mockReturnValue(of(mockDetail));

    await TestBed.configureTestingModule({
      imports: [ContainerDetailDrawerComponent],
      providers: [
        { provide: ApiClientService, useValue: mockApiClient },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(ContainerDetailDrawerComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  describe('drawer visibility and container querying', () => {
    it('should be closed when id is null, undefined, or empty', () => {
      fixture.componentRef.setInput('id', null);
      fixture.detectChanges();
      expect(component.isOpen()).toBe(false);
      expect(mockApiClient.getContainerDetail).not.toHaveBeenCalled();

      fixture.componentRef.setInput('id', undefined);
      fixture.detectChanges();
      expect(component.isOpen()).toBe(false);

      fixture.componentRef.setInput('id', '');
      fixture.detectChanges();
      expect(component.isOpen()).toBe(false);

      fixture.componentRef.setInput('id', '   ');
      fixture.detectChanges();
      expect(component.isOpen()).toBe(false);
    });

    it('should open and fetch container details when a valid id is provided', () => {
      fixture.componentRef.setInput('id', 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855');
      fixture.detectChanges();

      expect(component.isOpen()).toBe(true);
      expect(mockApiClient.getContainerDetail).toHaveBeenCalledWith(
        'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'
      );
    });
  });

  describe('displaying container details', () => {
    beforeEach(() => {
      fixture.componentRef.setInput(
        'id',
        'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'
      );
      fixture.detectChanges();
    });

    it('should display container title without leading slash and subtitle as image name', () => {
      expect(component.title()).toBe('redis-cache');
      expect(component.subtitle()).toBe('redis:7.2-alpine');
    });

    it('should render header badges including state badge and running boolean indicator', () => {
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('RUNNING');
      expect(el.textContent?.toLowerCase()).toContain('running');
    });

    it('should display full 64-character container ID in monospace select-all block', () => {
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain(
        'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'
      );
    });

    it('should copy container ID to clipboard on copy button click', async () => {
      vi.useFakeTimers();
      const el = fixture.nativeElement as HTMLElement;

      const writeTextMock = vi.fn().mockResolvedValue(undefined);
      Object.assign(navigator, {
        clipboard: { writeText: writeTextMock },
      });

      const copyBtn = el.querySelector(
        'button[aria-label*="Copy"], button[aria-label*="copy"]'
      ) as HTMLButtonElement;
      expect(copyBtn).toBeTruthy();

      copyBtn.click();
      await vi.advanceTimersByTimeAsync(10);
      fixture.detectChanges();

      expect(writeTextMock).toHaveBeenCalledWith(mockDetail.id);
      expect(component.isCopied()).toBe(true);
      expect(el.textContent).toContain('Copied!');

      await vi.advanceTimersByTimeAsync(2000);
      fixture.detectChanges();
      expect(component.isCopied()).toBe(false);
      vi.useRealTimers();
    });

    it('should display image, status, and formatted creation timestamp', () => {
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('redis:7.2-alpine');
      expect(el.textContent).toContain('Up 3 days');
      // formatEpochSeconds(1700000000) -> 2023-11-14 22:13:20 UTC
      expect(el.textContent).toContain('2023-11-14 22:13:20 UTC');
    });

    it('should display neutral text styling for exit_code === 0', () => {
      const el = fixture.nativeElement as HTMLElement;
      const exitEl = el.querySelector('.text-zinc-100.font-mono');
      expect(exitEl).toBeTruthy();
      expect(exitEl?.textContent?.trim()).toBe('0');
    });

    it('should display highlighted rose text styling for exit_code !== 0', () => {
      const exitedDetail: ContainerDetail = {
        ...mockDetail,
        state: 'exited',
        running: false,
        exit_code: 137,
      };
      mockApiClient.getContainerDetail.mockReturnValue(of(exitedDetail));

      fixture.componentRef.setInput('id', 'different-id');
      fixture.detectChanges();

      const el = fixture.nativeElement as HTMLElement;
      const exitEl = el.querySelector('.text-rose-400.font-mono');
      expect(exitEl).toBeTruthy();
      expect(exitEl?.textContent?.trim()).toBe('137');
    });

    it('should display formatted port mappings and volume mount sources', () => {
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('0.0.0.0:6379->6379/tcp');
      expect(el.textContent).toContain('/var/lib/docker/volumes/redis_data/_data');
    });

    it('should display placeholders when port mappings and mount sources are empty', () => {
      const emptyListsDetail: ContainerDetail = {
        ...mockDetail,
        port_mappings: [],
        mount_sources: [],
      };
      mockApiClient.getContainerDetail.mockReturnValue(of(emptyListsDetail));

      fixture.componentRef.setInput('id', 'empty-lists-id');
      fixture.detectChanges();

      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('No port mappings');
      expect(el.textContent).toContain('No volume mounts');
    });
  });

  describe('close output emission', () => {
    it('should emit close output when onClose is triggered', () => {
      fixture.componentRef.setInput('id', 'test-id');
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
      mockApiClient.getContainerDetail.mockReturnValue(
        throwError(() => new Error('Container not found'))
      );
      fixture.componentRef.setInput('id', 'non-existent-id');
      fixture.detectChanges();

      expect(component.isOpen()).toBe(true);
      expect(component.hasError()).toBe(true);

      const emptyState = fixture.nativeElement.querySelector('np-empty-state');
      expect(emptyState).toBeTruthy();
      expect(fixture.nativeElement.textContent).toContain('Container Not Found');
    });
  });
});
