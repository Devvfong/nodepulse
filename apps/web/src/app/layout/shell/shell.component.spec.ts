import { ComponentFixture, TestBed } from '@angular/core/testing';
import { signal } from '@angular/core';
import { provideRouter, RouterOutlet } from '@angular/router';
import { By } from '@angular/platform-browser';
import { ShellComponent } from './shell.component';
import { SidebarComponent } from '../sidebar/sidebar.component';
import { HeaderComponent } from '../header/header.component';
import { HostStateService } from '../../core/services/host-state.service';
import { ConnectionStateService } from '../../core/services/connection-state.service';
import { SseService } from '../../core/api/sse.service';

describe('ShellComponent', () => {
  let component: ShellComponent;
  let fixture: ComponentFixture<ShellComponent>;

  const mockSystemInfoSignal = signal(null);
  const mockAgentStatus = signal<'unknown' | 'healthy' | 'unreachable'>('healthy');
  const mockStreamStatus = signal<'connecting' | 'connected' | 'reconnecting' | 'paused'>('connected');
  const mockIsStreamPaused = signal<boolean>(false);

  const mockHostStateService = {
    systemInfo: mockSystemInfoSignal,
  };

  const mockConnectionStateService = {
    agentStatus: mockAgentStatus,
    streamStatus: mockStreamStatus,
    isStreamPaused: mockIsStreamPaused,
  };

  const mockSseService = {
    pause: vi.fn(),
    resume: vi.fn(),
  };

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [ShellComponent],
      providers: [
        provideRouter([]),
        { provide: HostStateService, useValue: mockHostStateService },
        { provide: ConnectionStateService, useValue: mockConnectionStateService },
        { provide: SseService, useValue: mockSseService },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(ShellComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  describe('Mobile Navigation State', () => {
    it('should initialize mobileNavOpen as false', () => {
      expect(component.mobileNavOpen()).toBe(false);
    });

    it('should toggle mobileNavOpen when toggleMobileNav() is called', () => {
      expect(component.mobileNavOpen()).toBe(false);
      component.toggleMobileNav();
      expect(component.mobileNavOpen()).toBe(true);
      component.toggleMobileNav();
      expect(component.mobileNavOpen()).toBe(false);
    });

    it('should close mobile nav when closeMobileNav() is called', () => {
      component.mobileNavOpen.set(true);
      component.closeMobileNav();
      expect(component.mobileNavOpen()).toBe(false);
    });

    it('should open mobile nav when openMobileNav() is called', () => {
      component.openMobileNav();
      expect(component.mobileNavOpen()).toBe(true);
    });
  });

  describe('Desktop Layout Structure', () => {
    it('should render a persistent desktop sidebar with hidden lg:flex', () => {
      fixture.detectChanges();
      const desktopSidebar = fixture.debugElement.query(
        By.css('np-sidebar.hidden.lg\\:flex, np-sidebar.lg\\:flex'),
      );
      expect(desktopSidebar).toBeTruthy();
    });

    it('should render the header component', () => {
      fixture.detectChanges();
      const header = fixture.debugElement.query(By.directive(HeaderComponent));
      expect(header).toBeTruthy();
    });

    it('should render the main content area with router-outlet', () => {
      fixture.detectChanges();
      const main = fixture.nativeElement.querySelector('main');
      expect(main).toBeTruthy();
      expect(main.className).toContain('flex-1');
      expect(main.className).toContain('overflow-y-auto');

      const outlet = fixture.debugElement.query(By.directive(RouterOutlet));
      expect(outlet).toBeTruthy();
    });
  });

  describe('Mobile Overlay Drawer', () => {
    it('should not render mobile drawer overlay when mobileNavOpen is false', () => {
      fixture.detectChanges();
      const mobileDrawer = fixture.nativeElement.querySelector('[role="dialog"]');
      expect(mobileDrawer).toBeNull();
    });

    it('should render mobile drawer overlay when mobileNavOpen is true', () => {
      component.mobileNavOpen.set(true);
      fixture.detectChanges();

      const mobileDrawer = fixture.nativeElement.querySelector('[role="dialog"]');
      expect(mobileDrawer).toBeTruthy();
    });

    it('should close mobile nav when clicking the backdrop', () => {
      component.mobileNavOpen.set(true);
      fixture.detectChanges();

      const backdrop = fixture.nativeElement.querySelector('.mobile-nav-backdrop');
      expect(backdrop).toBeTruthy();

      backdrop.click();
      expect(component.mobileNavOpen()).toBe(false);
    });

    it('should close mobile nav when mobile sidebar emits linkClick', () => {
      component.mobileNavOpen.set(true);
      fixture.detectChanges();

      const mobileDrawer = fixture.debugElement.query(By.css('[role="dialog"]'));
      const mobileSidebar = mobileDrawer.query(By.directive(SidebarComponent));
      expect(mobileSidebar).toBeTruthy();

      (mobileSidebar.componentInstance as SidebarComponent).linkClick.emit();
      expect(component.mobileNavOpen()).toBe(false);
    });

    it('should close mobile nav when Escape key is pressed', () => {
      component.mobileNavOpen.set(true);
      fixture.detectChanges();

      window.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
      fixture.detectChanges();

      expect(component.mobileNavOpen()).toBe(false);
    });

    it('should toggle mobile nav when header emits menuToggle', () => {
      fixture.detectChanges();
      expect(component.mobileNavOpen()).toBe(false);

      const headerDebug = fixture.debugElement.query(By.directive(HeaderComponent));
      (headerDebug.componentInstance as HeaderComponent).menuToggle.emit();

      expect(component.mobileNavOpen()).toBe(true);
    });
  });
});
