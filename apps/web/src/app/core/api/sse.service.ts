import { DOCUMENT } from '@angular/common';
import { inject, Injectable, InjectionToken, OnDestroy, signal } from '@angular/core';
import { Subject } from 'rxjs';
import { ConnectionStateService } from '../services/connection-state.service';
import { HostStateService } from '../services/host-state.service';
import { MetricPulse } from '../models/pulse.model';

export type EventSourceFactory = (url: string) => EventSource;

export const EVENT_SOURCE_FACTORY = new InjectionToken<EventSourceFactory>(
  'EVENT_SOURCE_FACTORY',
  {
    providedIn: 'root',
    factory: () => (url: string) => new EventSource(url),
  },
);

export const SSE_ENDPOINT = new InjectionToken<string>('SSE_ENDPOINT', {
  providedIn: 'root',
  factory: () => '/api/v1/events',
});

export const STALE_PULSE_THRESHOLD_MS = 5000;

@Injectable({
  providedIn: 'root',
})
export class SseService implements OnDestroy {
  private readonly connectionState = inject(ConnectionStateService);
  private readonly hostStateService = inject(HostStateService);
  private readonly eventSourceFactory = inject(EVENT_SOURCE_FACTORY);
  private readonly endpoint = inject(SSE_ENDPOINT);
  private readonly document = inject(DOCUMENT);

  private eventSource: EventSource | null = null;
  private readonly _pulse = signal<MetricPulse | null>(null);
  private readonly _pulse$ = new Subject<MetricPulse>();

  readonly pulse = this._pulse.asReadonly();
  readonly pulse$ = this._pulse$.asObservable();

  lastPulseTimestamp: number | null = null;

  private readonly handleVisibilityChange = (): void => {
    if (this.document.visibilityState === 'visible') {
      if (this.connectionState.isStreamPaused()) {
        return;
      }
      const isReconnecting = this.connectionState.streamStatus() === 'reconnecting';
      const isStale =
        this.lastPulseTimestamp === null ||
        Date.now() - this.lastPulseTimestamp > STALE_PULSE_THRESHOLD_MS;

      if (isReconnecting || isStale) {
        this.hostStateService.fetchInitialVitals().subscribe({
          error: () => {
            // Error handled by HostStateService
          },
        });
      }
    }
  };

  constructor() {
    this.document.addEventListener('visibilitychange', this.handleVisibilityChange);
  }

  connect(): void {
    if (this.eventSource || this.connectionState.isStreamPaused()) {
      return;
    }

    const es = this.eventSourceFactory(this.endpoint);
    this.eventSource = es;

    es.addEventListener('open', () => {
      this.connectionState.setStreamStatus('connected');
    });

    es.addEventListener('error', () => {
      this.connectionState.setStreamStatus('reconnecting');
    });

    es.addEventListener('metric_pulse', (event: Event) => {
      const messageEvent = event as MessageEvent;
      try {
        if (typeof messageEvent.data === 'string') {
          const pulse = JSON.parse(messageEvent.data);
          if (pulse && typeof pulse === 'object') {
            const metricPulse = pulse as MetricPulse;
            this._pulse.set(metricPulse);
            this._pulse$.next(metricPulse);
            this.hostStateService.appendPulse(metricPulse);
            this.lastPulseTimestamp = Date.now();
          }
        }
      } catch {
        // Silently ignore malformed frames
      }
    });
  }

  pause(): void {
    this.closeEventSource();
    this.connectionState.setStreamStatus('paused');
  }

  resume(): void {
    this.closeEventSource();
    this.connectionState.setStreamStatus('connecting');
    this.hostStateService.fetchInitialVitals().subscribe({
      error: () => {
        // Error handled by HostStateService
      },
    });
    this.connect();
  }

  ngOnDestroy(): void {
    this.closeEventSource();
    this.document.removeEventListener('visibilitychange', this.handleVisibilityChange);
    this._pulse$.complete();
  }

  private closeEventSource(): void {
    if (this.eventSource) {
      this.eventSource.close();
      this.eventSource = null;
    }
  }
}
