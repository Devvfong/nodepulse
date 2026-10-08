import { TestBed } from '@angular/core/testing';
import { of, throwError } from 'rxjs';
import { ConnectionStateService } from '../services/connection-state.service';
import { HostStateService } from '../services/host-state.service';
import { ApiClientService } from './api-client.service';
import { MetricPulse } from '../models/pulse.model';
import { SseService, EVENT_SOURCE_FACTORY, EventSourceFactory } from './sse.service';

class MockEventSource {
  static CONNECTING = 0;
  static OPEN = 1;
  static CLOSED = 2;

  url: string;
  readyState = MockEventSource.CONNECTING;
  onopen: ((event: Event) => void) | null = null;
  onerror: ((event: Event) => void) | null = null;
  onmessage: ((event: MessageEvent) => void) | null = null;

  close = vi.fn(() => {
    this.readyState = MockEventSource.CLOSED;
  });

  private readonly listeners = new Map<string, Set<(event: any) => void>>();

  constructor(url: string) {
    this.url = url;
  }

  addEventListener(type: string, listener: (event: any) => void): void {
    if (!this.listeners.has(type)) {
      this.listeners.set(type, new Set());
    }
    this.listeners.get(type)!.add(listener);
  }

  removeEventListener(type: string, listener: (event: any) => void): void {
    this.listeners.get(type)?.delete(listener);
  }

  dispatchEvent(event: Event): boolean {
    const set = this.listeners.get(event.type);
    if (set) {
      set.forEach((fn) => fn(event));
    }
    if (event.type === 'open' && this.onopen) {
      this.onopen(event);
    }
    if (event.type === 'error' && this.onerror) {
      this.onerror(event);
    }
    return true;
  }

  simulateOpen(): void {
    this.readyState = MockEventSource.OPEN;
    this.dispatchEvent(new Event('open'));
  }

  simulateError(): void {
    this.dispatchEvent(new Event('error'));
  }

  simulatePulse(data: Partial<MetricPulse> = {}): void {
    const pulse: MetricPulse = {
      timestamp: '2026-10-08T12:00:00Z',
      cpu_usage_percent: 25.5,
      memory_usage_percent: 60.2,
      memory_used_bytes: 4000000000,
      network_rx_bytes_sec: 1024,
      network_tx_bytes_sec: 2048,
      ...data,
    };
    const event = new MessageEvent('metric_pulse', {
      data: JSON.stringify(pulse),
    });
    this.dispatchEvent(event);
  }

  simulateRawMessage(type: string, data: string): void {
    const event = new MessageEvent(type, { data });
    this.dispatchEvent(event);
  }
}

describe('SseService', () => {
  let service: SseService;
  let connectionState: ConnectionStateService;
  let mockHostState: {
    fetchInitialVitals: ReturnType<typeof vi.fn>;
    appendPulse: ReturnType<typeof vi.fn>;
  };
  let mockEventSourceFactory: ReturnType<typeof vi.fn>;
  let mockEventSource: MockEventSource;
  let originalVisibilityState: PropertyDescriptor | undefined;
  let originalHidden: PropertyDescriptor | undefined;

  const mockPulseSample: MetricPulse = {
    timestamp: '2026-10-08T12:00:00Z',
    cpu_usage_percent: 14.2,
    memory_usage_percent: 38.1,
    memory_used_bytes: 6395000000,
    network_rx_bytes_sec: 12040.0,
    network_tx_bytes_sec: 45800.0,
  };

  beforeEach(() => {
    originalVisibilityState = Object.getOwnPropertyDescriptor(document, 'visibilityState');
    originalHidden = Object.getOwnPropertyDescriptor(document, 'hidden');

    Object.defineProperty(document, 'visibilityState', {
      value: 'visible',
      writable: true,
      configurable: true,
    });
    Object.defineProperty(document, 'hidden', {
      value: false,
      writable: true,
      configurable: true,
    });

    const mockApiClient = {
      getHealth: vi.fn().mockReturnValue(of({ status: 'healthy', version: '1.0.0', uptime_seconds: 3600 })),
    };

    mockHostState = {
      fetchInitialVitals: vi.fn().mockReturnValue(of({} as any)),
      appendPulse: vi.fn(),
    };

    mockEventSourceFactory = vi.fn((url: string) => {
      mockEventSource = new MockEventSource(url);
      return mockEventSource as unknown as EventSource;
    });

    TestBed.configureTestingModule({
      providers: [
        ConnectionStateService,
        { provide: ApiClientService, useValue: mockApiClient },
        { provide: HostStateService, useValue: mockHostState },
        { provide: EVENT_SOURCE_FACTORY, useValue: mockEventSourceFactory },
        SseService,
      ],
    });

    connectionState = TestBed.inject(ConnectionStateService);
    service = TestBed.inject(SseService);
  });

  afterEach(() => {
    service.ngOnDestroy();
    if (originalVisibilityState) {
      Object.defineProperty(document, 'visibilityState', originalVisibilityState);
    }
    if (originalHidden) {
      Object.defineProperty(document, 'hidden', originalHidden);
    }
  });

  describe('1. Service Initialization', () => {
    it('should initialize with pulse signal returning null', () => {
      expect(service.pulse()).toBeNull();
    });

    it('should initialize with lastPulseTimestamp as null', () => {
      expect(service.lastPulseTimestamp).toBeNull();
    });

    it('should not open EventSource before connect() is explicitly called', () => {
      expect(mockEventSourceFactory).not.toHaveBeenCalled();
    });
  });

  describe('2. Stream Lifecycle & Events', () => {
    it('should open native EventSource targeting relative endpoint /api/v1/events on connect()', () => {
      service.connect();

      expect(mockEventSourceFactory).toHaveBeenCalledTimes(1);
      expect(mockEventSourceFactory).toHaveBeenCalledWith('/api/v1/events');
    });

    it('should not open a duplicate EventSource if connect() is called while already connected', () => {
      service.connect();
      expect(mockEventSourceFactory).toHaveBeenCalledTimes(1);

      service.connect();
      expect(mockEventSourceFactory).toHaveBeenCalledTimes(1);
    });

    it('should not open EventSource if connect() is called while stream is paused', () => {
      connectionState.setStreamStatus('paused');

      service.connect();
      expect(mockEventSourceFactory).not.toHaveBeenCalled();
    });

    it('open event should transition streamStatus to "connected"', () => {
      service.connect();
      expect(connectionState.streamStatus()).toBe('connecting');

      mockEventSource.simulateOpen();
      expect(connectionState.streamStatus()).toBe('connected');
    });

    it('error event should transition streamStatus to "reconnecting"', () => {
      service.connect();
      mockEventSource.simulateOpen();
      expect(connectionState.streamStatus()).toBe('connected');

      mockEventSource.simulateError();
      expect(connectionState.streamStatus()).toBe('reconnecting');
    });

    it('CRITICAL INVARIANT: SSE error event MUST NOT call connectionState.setAgentStatus or mutate agentStatus', () => {
      connectionState.setAgentStatus('healthy');
      const agentStatusSpy = vi.spyOn(connectionState, 'setAgentStatus');

      service.connect();
      mockEventSource.simulateError();

      expect(agentStatusSpy).not.toHaveBeenCalled();
      expect(connectionState.agentStatus()).toBe('healthy');
      expect(connectionState.streamStatus()).toBe('reconnecting');
    });

    it('metric_pulse event listener should parse MetricPulse JSON and update pulse signal and pulse$ observable', () => {
      service.connect();

      let emittedPulse: MetricPulse | undefined;
      const sub = service.pulse$.subscribe((p: MetricPulse) => {
        emittedPulse = p;
      });

      mockEventSource.simulatePulse(mockPulseSample);

      expect(service.pulse()).toEqual(mockPulseSample);
      expect(emittedPulse).toEqual(mockPulseSample);

      sub.unsubscribe();
    });

    it('metric_pulse event listener should forward received pulse to hostStateService.appendPulse()', () => {
      service.connect();

      mockEventSource.simulatePulse(mockPulseSample);

      expect(mockHostState.appendPulse).toHaveBeenCalledTimes(1);
      expect(mockHostState.appendPulse).toHaveBeenCalledWith(mockPulseSample);
    });

    it('metric_pulse event listener should update lastPulseTimestamp with current epoch time', () => {
      const beforeTime = Date.now();
      service.connect();
      mockEventSource.simulatePulse(mockPulseSample);
      const afterTime = Date.now();

      expect(service.lastPulseTimestamp).not.toBeNull();
      expect(typeof service.lastPulseTimestamp).toBe('number');
      expect(service.lastPulseTimestamp!).toBeGreaterThanOrEqual(beforeTime);
      expect(service.lastPulseTimestamp!).toBeLessThanOrEqual(afterTime);
    });

    it('metric_pulse event should handle malformed JSON gracefully without throwing', () => {
      service.connect();
      mockEventSource.simulatePulse(mockPulseSample);
      expect(service.pulse()).toEqual(mockPulseSample);

      expect(() => {
        mockEventSource.simulateRawMessage('metric_pulse', '{invalid-json');
      }).not.toThrow();

      // State remains intact
      expect(service.pulse()).toEqual(mockPulseSample);
    });

    it('metric_pulse event should ignore null or primitive JSON payloads', () => {
      service.connect();
      mockEventSource.simulatePulse(mockPulseSample);
      expect(service.pulse()).toEqual(mockPulseSample);

      mockEventSource.simulateRawMessage('metric_pulse', 'null');
      expect(service.pulse()).toEqual(mockPulseSample);

      mockEventSource.simulateRawMessage('metric_pulse', '12345');
      expect(service.pulse()).toEqual(mockPulseSample);
    });
  });

  describe('3. Pause & Resume', () => {
    it('pause() should close active EventSource and transition streamStatus to "paused"', () => {
      service.connect();
      mockEventSource.simulateOpen();

      service.pause();

      expect(mockEventSource.close).toHaveBeenCalledTimes(1);
      expect(connectionState.streamStatus()).toBe('paused');
    });

    it('pause() should preserve the last received pulse in pulse signal', () => {
      service.connect();
      mockEventSource.simulatePulse(mockPulseSample);
      expect(service.pulse()).toEqual(mockPulseSample);

      service.pause();

      expect(connectionState.streamStatus()).toBe('paused');
      expect(service.pulse()).toEqual(mockPulseSample);
    });

    it('pause() should handle being called when no active EventSource exists', () => {
      expect(() => service.pause()).not.toThrow();
      expect(connectionState.streamStatus()).toBe('paused');
    });

    it('resume() should transition streamStatus to "connecting", dispatch fetchInitialVitals(), and open a new EventSource', () => {
      service.connect();
      mockEventSource.simulateOpen();
      service.pause();

      expect(connectionState.streamStatus()).toBe('paused');
      expect(mockHostState.fetchInitialVitals).not.toHaveBeenCalled();

      service.resume();

      expect(connectionState.streamStatus()).toBe('connecting');
      expect(mockHostState.fetchInitialVitals).toHaveBeenCalledTimes(1);
      expect(mockEventSourceFactory).toHaveBeenCalledTimes(2); // Initial connect + resume connect
    });

    it('resume() should handle error in fetchInitialVitals() gracefully without uncaught rejection', () => {
      mockHostState.fetchInitialVitals.mockReturnValue(throwError(() => new Error('Snapshot failure')));
      service.pause();

      expect(() => service.resume()).not.toThrow();
      expect(connectionState.streamStatus()).toBe('connecting');
    });
  });

  describe('4. Page Visibility (visibilitychange)', () => {
    it('document.hidden === true MUST NOT close the EventSource connection', () => {
      service.connect();
      mockEventSource.simulateOpen();

      Object.defineProperty(document, 'hidden', { value: true, configurable: true });
      Object.defineProperty(document, 'visibilityState', { value: 'hidden', configurable: true });

      document.dispatchEvent(new Event('visibilitychange'));

      expect(mockEventSource.close).not.toHaveBeenCalled();
      expect(connectionState.streamStatus()).toBe('connected');
    });

    it('returning to visible while streamStatus is "reconnecting" should trigger fetchInitialVitals()', () => {
      service.connect();
      mockEventSource.simulateError();
      expect(connectionState.streamStatus()).toBe('reconnecting');
      expect(mockHostState.fetchInitialVitals).not.toHaveBeenCalled();

      Object.defineProperty(document, 'visibilityState', { value: 'visible', configurable: true });
      document.dispatchEvent(new Event('visibilitychange'));

      expect(mockHostState.fetchInitialVitals).toHaveBeenCalledTimes(1);
    });

    it('returning to visible while last pulse timestamp is stale (>5000ms) should trigger fetchInitialVitals()', () => {
      service.connect();
      mockEventSource.simulateOpen();
      mockEventSource.simulatePulse(mockPulseSample);

      // Simulate stale pulse from 6000ms ago
      service.lastPulseTimestamp = Date.now() - 6000;
      expect(mockHostState.fetchInitialVitals).not.toHaveBeenCalled();

      Object.defineProperty(document, 'visibilityState', { value: 'visible', configurable: true });
      document.dispatchEvent(new Event('visibilitychange'));

      expect(mockHostState.fetchInitialVitals).toHaveBeenCalledTimes(1);
    });

    it('returning to visible when no pulse has been received yet should trigger fetchInitialVitals()', () => {
      service.connect();
      mockEventSource.simulateOpen();
      expect(service.lastPulseTimestamp).toBeNull();

      Object.defineProperty(document, 'visibilityState', { value: 'visible', configurable: true });
      document.dispatchEvent(new Event('visibilitychange'));

      expect(mockHostState.fetchInitialVitals).toHaveBeenCalledTimes(1);
    });

    it('returning to visible while connected and pulse is fresh (<5000ms) should NOT trigger fetchInitialVitals()', () => {
      service.connect();
      mockEventSource.simulateOpen();
      mockEventSource.simulatePulse(mockPulseSample);

      // Fresh pulse
      service.lastPulseTimestamp = Date.now() - 1000;

      Object.defineProperty(document, 'visibilityState', { value: 'visible', configurable: true });
      document.dispatchEvent(new Event('visibilitychange'));

      expect(mockHostState.fetchInitialVitals).not.toHaveBeenCalled();
    });

    it('returning to visible while streamStatus is "paused" should NOT trigger fetchInitialVitals()', () => {
      service.pause();
      expect(connectionState.streamStatus()).toBe('paused');

      Object.defineProperty(document, 'visibilityState', { value: 'visible', configurable: true });
      document.dispatchEvent(new Event('visibilitychange'));

      expect(mockHostState.fetchInitialVitals).not.toHaveBeenCalled();
    });
  });

  describe('5. Cleanup & ngOnDestroy', () => {
    it('ngOnDestroy should close active EventSource', () => {
      service.connect();
      mockEventSource.simulateOpen();

      service.ngOnDestroy();

      expect(mockEventSource.close).toHaveBeenCalledTimes(1);
    });

    it('ngOnDestroy should unbind visibilitychange event listener from document', () => {
      service.connect();
      mockEventSource.simulateError();

      service.ngOnDestroy();

      // Trigger visibility change after destroy
      Object.defineProperty(document, 'visibilityState', { value: 'visible', configurable: true });
      document.dispatchEvent(new Event('visibilitychange'));

      expect(mockHostState.fetchInitialVitals).not.toHaveBeenCalled();
    });

    it('ngOnDestroy should complete pulse$ observable', () => {
      let completed = false;
      service.pulse$.subscribe({
        complete: () => {
          completed = true;
        },
      });

      service.ngOnDestroy();

      expect(completed).toBe(true);
    });
  });
});
