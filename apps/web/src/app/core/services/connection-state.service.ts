import { computed, inject, Injectable, signal } from '@angular/core';
import { Observable, throwError } from 'rxjs';
import { catchError, tap } from 'rxjs/operators';
import { ApiClientService } from '../api/api-client.service';
import { HealthResponse } from '../models';

export type AgentStatus = 'unknown' | 'healthy' | 'unreachable';
export type StreamStatus = 'connecting' | 'connected' | 'reconnecting' | 'paused';

@Injectable({
  providedIn: 'root',
})
export class ConnectionStateService {
  private readonly apiClient = inject(ApiClientService);

  private readonly _agentStatus = signal<AgentStatus>('unknown');
  private readonly _streamStatus = signal<StreamStatus>('connecting');

  readonly agentStatus = this._agentStatus.asReadonly();
  readonly streamStatus = this._streamStatus.asReadonly();
  readonly isStreamPaused = computed(() => this.streamStatus() === 'paused');

  setAgentStatus(status: AgentStatus): void {
    this._agentStatus.set(status);
  }

  setStreamStatus(status: StreamStatus): void {
    this._streamStatus.set(status);
  }

  checkHealth(): Observable<HealthResponse> {
    return this.apiClient.getHealth().pipe(
      tap((res: HealthResponse) => {
        this.setAgentStatus(res.status === 'healthy' ? 'healthy' : 'unreachable');
      }),
      catchError((error: unknown) => {
        this.setAgentStatus('unreachable');
        return throwError(() => error);
      }),
    );
  }
}
