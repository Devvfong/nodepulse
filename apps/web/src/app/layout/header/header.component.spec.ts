import { ComponentFixture, TestBed } from '@angular/core/testing';
import { signal } from '@angular/core';
import { By } from '@angular/platform-browser';
import { HeaderComponent } from './header.component';
import { ConnectionStateService } from '../../core/services/connection-state.service';
import { SseService } from '../../core/api/sse.service';
import { StatusBadgeComponent } from '../../shared/ui/status-badge/status-badge.component';

describe('HeaderComponent', () => {
  let component: HeaderComponent;
  let fixture: ComponentFixture<HeaderComponent>;

  let mockAgentStatus = signal<'unknown' | 'healthy' | 'unreachable'>('unknown');
  let mockStreamStatus = signal<'connecting' | 'connected' | 'reconnecting' | 'paused'>('connecting');
  let mockIsStreamPaused = signal<boolean>(false);

  let mockSseService: {
    pause: ReturnType<typeof vi.fn>;
    resume: ReturnType<typeof vi.fn>;
  };

  beforeEach(async () => {
    mockAgentStatus = signal<'unknown' | 'healthy' | 'unreachable'>('healthy');
    mockStreamStatus = signal<'connecting' | 'connected' | 'reconnecting' | 'paused'>('connected');
    mockIsStreamPaused = signal<boolean>(false);

    mockSseService = {
      pause: vi.fn(),
      resume: vi.fn(),
    };

    const mockConnectionStateService = {
      agentStatus: mockAgentStatus,
      streamStatus: mockStreamStatus,
      isStreamPaused: mockIsStreamPaused,
    };

    await TestBed.configureTestingModule({
      imports: [HeaderComponent],
      providers: [
        { provide: ConnectionStateService, useValue: mockConnectionStateService },
        { provide: SseService, useValue: mockSseService },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(HeaderComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  describe('Mobile Hamburger Menu Toggle', () => {
    it('should render mobile hamburger button with responsive visibility (lg:hidden)', () => {
      fixture.detectChanges();
      const btn = fixture.nativeElement.querySelector('button[aria-label="Toggle navigation menu"]');
      expect(btn).toBeTruthy();
      expect(btn.className).toContain('lg:hidden');
    });

    it('should emit menuToggle output when mobile hamburger button is clicked', () => {
      fixture.detectChanges();
      let toggled = false;
      component.menuToggle.subscribe(() => {
        toggled = true;
      });

      const btn = fixture.nativeElement.querySelector('button[aria-label="Toggle navigation menu"]');
      btn.click();

      expect(toggled).toBe(true);
    });
  });

  describe('Independent Status Badges (Decoupled Agent Health & Stream State)', () => {
    it('should render exactly two distinct StatusBadgeComponents', () => {
      fixture.detectChanges();
      const badges = fixture.debugElement.queryAll(By.directive(StatusBadgeComponent));
      expect(badges.length).toBe(2);
    });

    it('should bind agentStatus to the first badge with AGENT label prefix', () => {
      mockAgentStatus.set('healthy');
      mockStreamStatus.set('connected');
      fixture.detectChanges();

      const badges = fixture.debugElement.queryAll(By.directive(StatusBadgeComponent));
      const agentBadge = badges[0].componentInstance as StatusBadgeComponent;

      expect(agentBadge.status()).toBe('healthy');
      expect(agentBadge.label()).toBe('AGENT: HEALTHY');
    });

    it('should bind streamStatus to the second badge with STREAM label prefix', () => {
      mockAgentStatus.set('healthy');
      mockStreamStatus.set('connected');
      fixture.detectChanges();

      const badges = fixture.debugElement.queryAll(By.directive(StatusBadgeComponent));
      const streamBadge = badges[1].componentInstance as StatusBadgeComponent;

      expect(streamBadge.status()).toBe('connected');
      expect(streamBadge.label()).toBe('STREAM: CONNECTED');
    });

    it('should independently update agentStatus badge without mutating stream badge', () => {
      mockAgentStatus.set('healthy');
      mockStreamStatus.set('connected');
      fixture.detectChanges();

      mockAgentStatus.set('unreachable');
      fixture.detectChanges();

      const badges = fixture.debugElement.queryAll(By.directive(StatusBadgeComponent));
      const agentBadge = badges[0].componentInstance as StatusBadgeComponent;
      const streamBadge = badges[1].componentInstance as StatusBadgeComponent;

      expect(agentBadge.status()).toBe('unreachable');
      expect(agentBadge.label()).toBe('AGENT: UNREACHABLE');
      expect(streamBadge.status()).toBe('connected');
      expect(streamBadge.label()).toBe('STREAM: CONNECTED');
    });

    it('should independently update streamStatus badge without mutating agent badge', () => {
      mockAgentStatus.set('healthy');
      mockStreamStatus.set('connected');
      fixture.detectChanges();

      mockStreamStatus.set('reconnecting');
      fixture.detectChanges();

      const badges = fixture.debugElement.queryAll(By.directive(StatusBadgeComponent));
      const agentBadge = badges[0].componentInstance as StatusBadgeComponent;
      const streamBadge = badges[1].componentInstance as StatusBadgeComponent;

      expect(agentBadge.status()).toBe('healthy');
      expect(agentBadge.label()).toBe('AGENT: HEALTHY');
      expect(streamBadge.status()).toBe('reconnecting');
      expect(streamBadge.label()).toBe('STREAM: RECONNECTING');
    });

    it('should never conflate agentStatus and streamStatus into a single element', () => {
      mockAgentStatus.set('healthy');
      mockStreamStatus.set('reconnecting');
      fixture.detectChanges();

      const text = (fixture.nativeElement as HTMLElement).textContent;
      expect(text).toContain('AGENT: HEALTHY');
      expect(text).toContain('STREAM: RECONNECTING');
    });
  });

  describe('Stream Pause and Resume Controls', () => {
    it('should display "Pause" button when stream is not paused', () => {
      mockIsStreamPaused.set(false);
      fixture.detectChanges();

      const pauseBtn = fixture.nativeElement.querySelector('.stream-toggle-btn');
      expect(pauseBtn).toBeTruthy();
      expect(pauseBtn.textContent.trim()).toBe('Pause');
    });

    it('should call sseService.pause() when clicking "Pause" button', () => {
      mockIsStreamPaused.set(false);
      fixture.detectChanges();

      const pauseBtn = fixture.nativeElement.querySelector('.stream-toggle-btn');
      pauseBtn.click();

      expect(mockSseService.pause).toHaveBeenCalledTimes(1);
      expect(mockSseService.resume).not.toHaveBeenCalled();
    });

    it('should display "Resume" button when stream is paused', () => {
      mockIsStreamPaused.set(true);
      fixture.detectChanges();

      const resumeBtn = fixture.nativeElement.querySelector('.stream-toggle-btn');
      expect(resumeBtn).toBeTruthy();
      expect(resumeBtn.textContent.trim()).toBe('Resume');
    });

    it('should call sseService.resume() when clicking "Resume" button', () => {
      mockIsStreamPaused.set(true);
      fixture.detectChanges();

      const resumeBtn = fixture.nativeElement.querySelector('.stream-toggle-btn');
      resumeBtn.click();

      expect(mockSseService.resume).toHaveBeenCalledTimes(1);
      expect(mockSseService.pause).not.toHaveBeenCalled();
    });
  });
});
