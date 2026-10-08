import { TestBed } from '@angular/core/testing';
import { of, throwError } from 'rxjs';
import { ConnectionStateService, AgentStatus, StreamStatus } from './connection-state.service';
import { ApiClientService } from '../api/api-client.service';
import { HealthResponse } from '../models';

describe('ConnectionStateService', () => {
  let service: ConnectionStateService;
  let mockApiClient: {
    getHealth: ReturnType<typeof vi.fn>;
  };

  const mockHealthResponse: HealthResponse = {
    status: 'healthy',
    version: '1.0.0',
    uptime_seconds: 3600,
  };

  beforeEach(() => {
    mockApiClient = {
      getHealth: vi.fn(),
    };

    TestBed.configureTestingModule({
      providers: [ConnectionStateService, { provide: ApiClientService, useValue: mockApiClient }],
    });

    service = TestBed.inject(ConnectionStateService);
  });

  describe('Initial State', () => {
    it('should initialize agentStatus as "unknown"', () => {
      expect(service.agentStatus()).toBe('unknown');
    });

    it('should initialize streamStatus as "connecting"', () => {
      expect(service.streamStatus()).toBe('connecting');
    });
  });

  describe('setAgentStatus()', () => {
    it('should update agentStatus to "healthy"', () => {
      service.setAgentStatus('healthy');
      expect(service.agentStatus()).toBe('healthy');
    });

    it('should update agentStatus to "unreachable"', () => {
      service.setAgentStatus('unreachable');
      expect(service.agentStatus()).toBe('unreachable');
    });

    it('should update agentStatus to "unknown"', () => {
      service.setAgentStatus('healthy');
      service.setAgentStatus('unknown');
      expect(service.agentStatus()).toBe('unknown');
    });

    it('should not mutate streamStatus when agentStatus changes', () => {
      service.setStreamStatus('connected');
      service.setAgentStatus('unreachable');
      expect(service.streamStatus()).toBe('connected');
      expect(service.agentStatus()).toBe('unreachable');
    });
  });

  describe('setStreamStatus()', () => {
    it('should update streamStatus to "connected"', () => {
      service.setStreamStatus('connected');
      expect(service.streamStatus()).toBe('connected');
    });

    it('should update streamStatus to "reconnecting"', () => {
      service.setStreamStatus('reconnecting');
      expect(service.streamStatus()).toBe('reconnecting');
    });

    it('should update streamStatus to "paused"', () => {
      service.setStreamStatus('paused');
      expect(service.streamStatus()).toBe('paused');
    });

    it('should update streamStatus to "connecting"', () => {
      service.setStreamStatus('paused');
      service.setStreamStatus('connecting');
      expect(service.streamStatus()).toBe('connecting');
    });
  });

  describe('Decoupling Invariant: streamStatus changes MUST NOT mutate agentStatus', () => {
    it('should keep agentStatus intact when streamStatus transitions to "reconnecting"', () => {
      service.setAgentStatus('healthy');
      service.setStreamStatus('reconnecting');

      expect(service.agentStatus()).toBe('healthy');
      expect(service.streamStatus()).toBe('reconnecting');
    });

    it('should keep agentStatus intact when streamStatus transitions to "connected"', () => {
      service.setAgentStatus('unknown');
      service.setStreamStatus('connected');

      expect(service.agentStatus()).toBe('unknown');
      expect(service.streamStatus()).toBe('connected');
    });

    it('should keep agentStatus intact when streamStatus transitions to "paused"', () => {
      service.setAgentStatus('healthy');
      service.setStreamStatus('paused');

      expect(service.agentStatus()).toBe('healthy');
      expect(service.streamStatus()).toBe('paused');
    });

    it('should support simultaneous "healthy" agent and "reconnecting" stream status', () => {
      service.setAgentStatus('healthy');
      service.setStreamStatus('reconnecting');

      expect(service.agentStatus()).toBe('healthy');
      expect(service.streamStatus()).toBe('reconnecting');
    });
  });

  describe('checkHealth()', () => {
    it('should call apiClient.getHealth() and update agentStatus to "healthy" on success', () => {
      mockApiClient.getHealth.mockReturnValue(of(mockHealthResponse));

      let receivedResponse: HealthResponse | undefined;
      service.checkHealth().subscribe((res: HealthResponse) => {
        receivedResponse = res;
      });

      expect(mockApiClient.getHealth).toHaveBeenCalledTimes(1);
      expect(receivedResponse).toEqual(mockHealthResponse);
      expect(service.agentStatus()).toBe('healthy');
    });

    it('should update agentStatus to "unreachable" and re-throw error when health probe fails', () => {
      const probeError = new Error('Connection refused: agent offline');
      mockApiClient.getHealth.mockReturnValue(throwError(() => probeError));

      let receivedError: unknown;
      service.checkHealth().subscribe({
        next: () => {
          throw new Error('Should not have succeeded');
        },
        error: (err: unknown) => {
          receivedError = err;
        },
      });

      expect(mockApiClient.getHealth).toHaveBeenCalledTimes(1);
      expect(receivedError).toBe(probeError);
      expect(service.agentStatus()).toBe('unreachable');
    });

    it('should not mutate streamStatus on health probe success', () => {
      service.setStreamStatus('reconnecting');
      mockApiClient.getHealth.mockReturnValue(of(mockHealthResponse));

      service.checkHealth().subscribe();

      expect(service.agentStatus()).toBe('healthy');
      expect(service.streamStatus()).toBe('reconnecting');
    });

    it('should not mutate streamStatus on health probe failure', () => {
      service.setStreamStatus('connected');
      mockApiClient.getHealth.mockReturnValue(throwError(() => new Error('Probe failed')));

      service.checkHealth().subscribe({
        error: () => {
          // Expected error
        },
      });

      expect(service.agentStatus()).toBe('unreachable');
      expect(service.streamStatus()).toBe('connected');
    });

    it('should transition agentStatus from "unreachable" back to "healthy" upon probe recovery', () => {
      service.setAgentStatus('unreachable');
      mockApiClient.getHealth.mockReturnValue(of(mockHealthResponse));

      service.checkHealth().subscribe();

      expect(service.agentStatus()).toBe('healthy');
    });

    it('should set agentStatus to "unreachable" when response status is not "healthy"', () => {
      const nonHealthyResponse: HealthResponse = {
        status: 'degraded',
        version: '1.0.0',
        uptime_seconds: 3600,
      };
      mockApiClient.getHealth.mockReturnValue(of(nonHealthyResponse));

      service.checkHealth().subscribe();

      expect(service.agentStatus()).toBe('unreachable');
    });
  });

  describe('isStreamPaused computed signal', () => {
    it('should be false when streamStatus is "connecting"', () => {
      expect(service.streamStatus()).toBe('connecting');
      expect(service.isStreamPaused()).toBe(false);
    });

    it('should be false when streamStatus is "connected"', () => {
      service.setStreamStatus('connected');
      expect(service.isStreamPaused()).toBe(false);
    });

    it('should be false when streamStatus is "reconnecting"', () => {
      service.setStreamStatus('reconnecting');
      expect(service.isStreamPaused()).toBe(false);
    });

    it('should be true when streamStatus is "paused"', () => {
      service.setStreamStatus('paused');
      expect(service.isStreamPaused()).toBe(true);
    });

    it('should revert to false when streamStatus leaves "paused"', () => {
      service.setStreamStatus('paused');
      expect(service.isStreamPaused()).toBe(true);
      service.setStreamStatus('connecting');
      expect(service.isStreamPaused()).toBe(false);
    });
  });

  describe('Readonly Signal Encapsulation', () => {
    it('should expose agentStatus and streamStatus as readonly signals without public set method', () => {
      expect((service.agentStatus as unknown as Record<string, unknown>)['set']).toBeUndefined();
      expect((service.streamStatus as unknown as Record<string, unknown>)['set']).toBeUndefined();
    });
  });
});
