import { ComponentFixture, TestBed } from '@angular/core/testing';
import { StatusBadgeComponent } from './status-badge.component';

describe('StatusBadgeComponent', () => {
  let component: StatusBadgeComponent;
  let fixture: ComponentFixture<StatusBadgeComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [StatusBadgeComponent],
    }).compileComponents();

    fixture = TestBed.createComponent(StatusBadgeComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  describe('label generation and display', () => {
    it('should format status to uppercase human-readable label when label is omitted', () => {
      fixture.componentRef.setInput('status', 'healthy');
      fixture.detectChanges();
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent?.trim()).toContain('HEALTHY');

      fixture.componentRef.setInput('status', 'warming_up');
      fixture.detectChanges();
      expect(el.textContent?.trim()).toContain('WARMING UP');

      fixture.componentRef.setInput('status', 'reconnecting');
      fixture.detectChanges();
      expect(el.textContent?.trim()).toContain('RECONNECTING');
    });

    it('should use explicit label when provided', () => {
      fixture.componentRef.setInput('status', 'active');
      fixture.componentRef.setInput('label', 'OPERATIONAL');
      fixture.detectChanges();
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent?.trim()).toContain('OPERATIONAL');
    });

    it('should always render a visible textual label for color independence', () => {
      fixture.componentRef.setInput('status', 'failed');
      fixture.detectChanges();
      const textSpan = fixture.nativeElement.querySelector('.status-label');
      expect(textSpan).toBeTruthy();
      expect(textSpan?.textContent?.trim()).toBe('FAILED');
    });
  });

  describe('semantic color mappings', () => {
    it('should apply emerald classes for healthy / active / running / ready / connected', () => {
      const emeraldStatuses = ['healthy', 'active', 'running', 'ready', 'connected'];
      for (const status of emeraldStatuses) {
        fixture.componentRef.setInput('status', status);
        fixture.detectChanges();
        const badge = fixture.nativeElement.firstElementChild as HTMLElement;
        expect(badge.className).toContain('text-emerald-400');
        expect(badge.className).toContain('bg-emerald-950/40');
        expect(badge.className).toContain('border-emerald-800/50');
      }
    });

    it('should apply amber classes for warning / warming_up / reconnecting / cached', () => {
      const amberStatuses = ['warning', 'warming_up', 'reconnecting', 'cached'];
      for (const status of amberStatuses) {
        fixture.componentRef.setInput('status', status);
        fixture.detectChanges();
        const badge = fixture.nativeElement.firstElementChild as HTMLElement;
        expect(badge.className).toContain('text-amber-400');
        expect(badge.className).toContain('bg-amber-950/40');
        expect(badge.className).toContain('border-amber-800/50');
      }
    });

    it('should apply rose classes for critical / failed / unreachable', () => {
      const roseStatuses = ['critical', 'failed', 'unreachable'];
      for (const status of roseStatuses) {
        fixture.componentRef.setInput('status', status);
        fixture.detectChanges();
        const badge = fixture.nativeElement.firstElementChild as HTMLElement;
        expect(badge.className).toContain('text-rose-400');
        expect(badge.className).toContain('bg-rose-950/40');
        expect(badge.className).toContain('border-rose-800/50');
      }
    });

    it('should apply zinc classes for inactive / paused / unknown / stopped / exited / fallback', () => {
      const zincStatuses = ['inactive', 'paused', 'unknown', 'stopped', 'exited', 'something_else'];
      for (const status of zincStatuses) {
        fixture.componentRef.setInput('status', status);
        fixture.detectChanges();
        const badge = fixture.nativeElement.firstElementChild as HTMLElement;
        expect(badge.className).toContain('text-zinc-400');
        expect(badge.className).toContain('bg-zinc-800/60');
        expect(badge.className).toContain('border-zinc-700/50');
      }
    });

    it('should handle case insensitivity and dash-separated statuses', () => {
      fixture.componentRef.setInput('status', 'WARMING-UP');
      fixture.detectChanges();
      const badge = fixture.nativeElement.firstElementChild as HTMLElement;
      expect(badge.className).toContain('text-amber-400');
      expect(badge.textContent?.trim()).toContain('WARMING UP');
    });
  });

  describe('typography & styling invariants', () => {
    it('should contain expected typography and geometry classes', () => {
      fixture.componentRef.setInput('status', 'active');
      fixture.detectChanges();
      const badge = fixture.nativeElement.firstElementChild as HTMLElement;
      expect(badge.className).toContain('font-mono');
      expect(badge.className).toContain('text-xs');
      expect(badge.className).toContain('px-2');
      expect(badge.className).toContain('py-0.5');
      expect(badge.className).toContain('rounded-sm');
      expect(badge.className).toContain('inline-flex');
      expect(badge.className).toContain('items-center');
      expect(badge.className).toContain('gap-1.5');
      expect(badge.className).toContain('tracking-wider');
      expect(badge.className).toContain('font-semibold');
    });

    it('should render an indicator dot with matching status color', () => {
      fixture.componentRef.setInput('status', 'healthy');
      fixture.detectChanges();
      const dot = fixture.nativeElement.querySelector('.status-dot');
      expect(dot).toBeTruthy();
      expect(dot?.className).toContain('bg-emerald-400');
    });
  });
});
