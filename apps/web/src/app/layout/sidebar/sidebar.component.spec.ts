import { ComponentFixture, TestBed } from '@angular/core/testing';
import { signal } from '@angular/core';
import { provideRouter, Router, RouterLink, RouterLinkActive } from '@angular/router';
import { By } from '@angular/platform-browser';
import { SidebarComponent } from './sidebar.component';
import { HostStateService } from '../../core/services/host-state.service';
import { SystemInfo } from '../../core/models';

describe('SidebarComponent', () => {
  let component: SidebarComponent;
  let fixture: ComponentFixture<SidebarComponent>;
  let mockSystemInfoSignal = signal<SystemInfo | null>(null);

  const createMockHostStateService = () => ({
    systemInfo: mockSystemInfoSignal,
  });

  beforeEach(async () => {
    mockSystemInfoSignal = signal<SystemInfo | null>(null);

    await TestBed.configureTestingModule({
      imports: [SidebarComponent],
      providers: [
        provideRouter([]),
        { provide: HostStateService, useValue: createMockHostStateService() },
      ],
    }).compileComponents();

    const router = TestBed.inject(Router);
    vi.spyOn(router, 'navigateByUrl').mockResolvedValue(true);

    fixture = TestBed.createComponent(SidebarComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  describe('Branding and Hostname', () => {
    it('should display "NodePulse" branding title', () => {
      fixture.detectChanges();
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('NodePulse');
    });

    it('should display "localhost" when systemInfo is null', () => {
      mockSystemInfoSignal.set(null);
      fixture.detectChanges();
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('localhost');
    });

    it('should display the actual hostname when systemInfo is available', () => {
      mockSystemInfoSignal.set({
        hostname: 'prod-srv-01.internal',
        os_name: 'Linux',
        os_version: '6.8.0',
        kernel_version: '6.8.0-generic',
        architecture: 'x86_64',
        boot_time_utc: 1700000000,
        uptime_seconds: 3600,
      });
      fixture.detectChanges();
      const el = fixture.nativeElement as HTMLElement;
      expect(el.textContent).toContain('prod-srv-01.internal');
    });
  });

  describe('Navigation links', () => {
    it('should render exactly 4 primary navigation items', () => {
      fixture.detectChanges();
      const links = fixture.debugElement.queryAll(By.directive(RouterLink));
      expect(links.length).toBe(4);
    });

    it('should render correct navigation targets and labels', () => {
      fixture.detectChanges();
      const links = fixture.debugElement.queryAll(By.directive(RouterLink));
      const linkData = links.map((linkDebug) => {
        const routerLink = linkDebug.injector.get(RouterLink);
        const text = (linkDebug.nativeElement as HTMLElement).textContent?.trim();
        return { href: routerLink.href, text };
      });

      expect(linkData).toEqual([
        expect.objectContaining({ href: '/overview', text: expect.stringContaining('Overview') }),
        expect.objectContaining({ href: '/processes', text: expect.stringContaining('Processes') }),
        expect.objectContaining({ href: '/services', text: expect.stringContaining('Services') }),
        expect.objectContaining({ href: '/containers', text: expect.stringContaining('Containers') }),
      ]);
    });

    it('should configure routerLinkActive with "bg-zinc-800 !text-zinc-100 font-semibold"', () => {
      fixture.detectChanges();
      const rlaDirectives = fixture.debugElement.queryAll(By.directive(RouterLinkActive));
      expect(rlaDirectives.length).toBe(4);

      for (const rlaDebug of rlaDirectives) {
        const rla = rlaDebug.injector.get(RouterLinkActive) as unknown as { classes: string[] };
        expect(rla.classes.join(' ')).toBe('bg-zinc-800 !text-zinc-100 font-semibold');
      }
    });

    it('should configure routerLinkActiveOptions with { exact: false }', () => {
      fixture.detectChanges();
      const rlaDirectives = fixture.debugElement.queryAll(By.directive(RouterLinkActive));
      for (const rlaDebug of rlaDirectives) {
        const rla = rlaDebug.injector.get(RouterLinkActive);
        expect(rla.routerLinkActiveOptions).toEqual({ exact: false });
      }
    });
  });

  describe('linkClick output event', () => {
    it('should emit linkClick when any navigation link is clicked', async () => {
      fixture.detectChanges();
      let clickEmittedCount = 0;
      component.linkClick.subscribe(() => {
        clickEmittedCount++;
      });

      const links = fixture.debugElement.queryAll(By.directive(RouterLink));
      for (const link of links) {
        const event = new MouseEvent('click', { bubbles: true, cancelable: true });
        link.nativeElement.dispatchEvent(event);
      }
      await fixture.whenStable();

      expect(clickEmittedCount).toBe(4);
    });
  });
});
