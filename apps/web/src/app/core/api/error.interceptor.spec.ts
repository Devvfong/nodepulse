import { TestBed } from '@angular/core/testing';
import { HttpClient, provideHttpClient, withInterceptors } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { errorInterceptor } from './error.interceptor';
import { ApiErrorEnvelope } from '../models/api-error.model';

describe('errorInterceptor', () => {
  let httpClient: HttpClient;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [
        provideHttpClient(withInterceptors([errorInterceptor])),
        provideHttpClientTesting(),
      ],
    });

    httpClient = TestBed.inject(HttpClient);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => {
    httpMock.verify();
  });

  describe('Successful HTTP responses', () => {
    it('should pass through successful HTTP responses unchanged', () => {
      const mockPayload = { status: 'healthy', version: '1.0.0', uptime_seconds: 3600 };
      let responseData: unknown;

      httpClient.get('/api/v1/health').subscribe({
        next: (res) => {
          responseData = res;
        },
      });

      const req = httpMock.expectOne('/api/v1/health');
      expect(req.request.method).toBe('GET');
      req.flush(mockPayload);

      expect(responseData).toEqual(mockPayload);
    });
  });

  describe('Standard backend ApiErrorEnvelope handling', () => {
    it('should unmarshal 400 INVALID_REQUEST envelope preserving details and status', () => {
      const errorEnvelope: ApiErrorEnvelope = {
        error: {
          code: 'INVALID_REQUEST',
          message: 'Process ID must be a positive integer.',
          timestamp: '2026-10-08T12:00:00Z',
          details: [{ field: 'pid', reason: 'must_be_positive_integer' }],
        },
      };

      let interceptedError: ApiErrorEnvelope | undefined;
      httpClient.get('/api/v1/processes/-10').subscribe({
        next: () => expect.unreachable('expected request to fail'),
        error: (err: ApiErrorEnvelope) => {
          interceptedError = err;
        },
      });

      const req = httpMock.expectOne('/api/v1/processes/-10');
      req.flush(errorEnvelope, { status: 400, statusText: 'Bad Request' });

      expect(interceptedError).toBeDefined();
      expect(interceptedError?.error.code).toBe('INVALID_REQUEST');
      expect(interceptedError?.error.message).toBe('Process ID must be a positive integer.');
      expect(interceptedError?.error.timestamp).toBe('2026-10-08T12:00:00Z');
      expect(interceptedError?.error.details).toEqual([
        { field: 'pid', reason: 'must_be_positive_integer' },
      ]);
      expect(interceptedError?.status).toBe(400);
    });

    it('should unmarshal 401 UNAUTHORIZED envelope', () => {
      const errorEnvelope: ApiErrorEnvelope = {
        error: {
          code: 'UNAUTHORIZED',
          message: 'Authentication required. Provide a valid X-API-Key header.',
          timestamp: '2026-10-08T12:00:00Z',
          details: [],
        },
      };

      let interceptedError: ApiErrorEnvelope | undefined;
      httpClient.get('/api/v1/system').subscribe({
        next: () => expect.unreachable('expected request to fail'),
        error: (err: ApiErrorEnvelope) => {
          interceptedError = err;
        },
      });

      const req = httpMock.expectOne('/api/v1/system');
      req.flush(errorEnvelope, { status: 401, statusText: 'Unauthorized' });

      expect(interceptedError).toBeDefined();
      expect(interceptedError?.error.code).toBe('UNAUTHORIZED');
      expect(interceptedError?.error.message).toBe(
        'Authentication required. Provide a valid X-API-Key header.'
      );
      expect(interceptedError?.status).toBe(401);
    });

    it('should unmarshal 404 RESOURCE_NOT_FOUND envelope', () => {
      const errorEnvelope: ApiErrorEnvelope = {
        error: {
          code: 'RESOURCE_NOT_FOUND',
          message: 'Process with PID 999999 was not found.',
          timestamp: '2026-10-08T12:00:00Z',
          details: [{ resource_type: 'process', identifier: '999999' }],
        },
      };

      let interceptedError: ApiErrorEnvelope | undefined;
      httpClient.get('/api/v1/processes/999999').subscribe({
        next: () => expect.unreachable('expected request to fail'),
        error: (err: ApiErrorEnvelope) => {
          interceptedError = err;
        },
      });

      const req = httpMock.expectOne('/api/v1/processes/999999');
      req.flush(errorEnvelope, { status: 404, statusText: 'Not Found' });

      expect(interceptedError).toBeDefined();
      expect(interceptedError?.error.code).toBe('RESOURCE_NOT_FOUND');
      expect(interceptedError?.status).toBe(404);
    });

    it('should unmarshal 500 COLLECTOR_FAILURE envelope', () => {
      const errorEnvelope: ApiErrorEnvelope = {
        error: {
          code: 'COLLECTOR_FAILURE',
          message: 'Failed to collect CPU metrics from /proc/stat.',
          timestamp: '2026-10-08T12:00:00Z',
          details: [{ collector: 'cpu_collector', target_file: '/proc/stat' }],
        },
      };

      let interceptedError: ApiErrorEnvelope | undefined;
      httpClient.get('/api/v1/cpu').subscribe({
        next: () => expect.unreachable('expected request to fail'),
        error: (err: ApiErrorEnvelope) => {
          interceptedError = err;
        },
      });

      const req = httpMock.expectOne('/api/v1/cpu');
      req.flush(errorEnvelope, { status: 500, statusText: 'Internal Server Error' });

      expect(interceptedError).toBeDefined();
      expect(interceptedError?.error.code).toBe('COLLECTOR_FAILURE');
      expect(interceptedError?.error.message).toBe(
        'Failed to collect CPU metrics from /proc/stat.'
      );
      expect(interceptedError?.status).toBe(500);
    });

    it('should unmarshal 503 DOCKER_UNAVAILABLE envelope', () => {
      const errorEnvelope: ApiErrorEnvelope = {
        error: {
          code: 'DOCKER_UNAVAILABLE',
          message:
            'Docker daemon is not running or socket /var/run/docker.sock is inaccessible.',
          timestamp: '2026-10-08T12:00:00Z',
          details: [{ socket_path: '/var/run/docker.sock' }],
        },
      };

      let interceptedError: ApiErrorEnvelope | undefined;
      httpClient.get('/api/v1/containers').subscribe({
        next: () => expect.unreachable('expected request to fail'),
        error: (err: ApiErrorEnvelope) => {
          interceptedError = err;
        },
      });

      const req = httpMock.expectOne('/api/v1/containers');
      req.flush(errorEnvelope, { status: 503, statusText: 'Service Unavailable' });

      expect(interceptedError).toBeDefined();
      expect(interceptedError?.error.code).toBe('DOCKER_UNAVAILABLE');
      expect(interceptedError?.status).toBe(503);
    });
  });

  describe('Rate limiting and Retry-After header extraction', () => {
    it('should handle 429 RATE_LIMITED and extract Retry-After header as integer seconds', () => {
      const errorEnvelope: ApiErrorEnvelope = {
        error: {
          code: 'RATE_LIMITED',
          message: 'Too many requests. Please slow down.',
          timestamp: '2026-10-08T12:00:00Z',
          details: [{ retry_after_seconds: 5 }],
        },
      };

      let interceptedError: ApiErrorEnvelope | undefined;
      httpClient.get('/api/v1/events').subscribe({
        next: () => expect.unreachable('expected request to fail'),
        error: (err: ApiErrorEnvelope) => {
          interceptedError = err;
        },
      });

      const req = httpMock.expectOne('/api/v1/events');
      req.flush(errorEnvelope, {
        status: 429,
        statusText: 'Too Many Requests',
        headers: { 'Retry-After': '5' },
      });

      expect(interceptedError).toBeDefined();
      expect(interceptedError?.error.code).toBe('RATE_LIMITED');
      expect(interceptedError?.retryAfter).toBe(5);
      expect(interceptedError?.status).toBe(429);
    });

    it('should extract Retry-After header with zero seconds', () => {
      const errorEnvelope: ApiErrorEnvelope = {
        error: {
          code: 'RATE_LIMITED',
          message: 'Too many requests.',
          timestamp: '2026-10-08T12:00:00Z',
          details: [],
        },
      };

      let interceptedError: ApiErrorEnvelope | undefined;
      httpClient.get('/api/v1/events').subscribe({
        next: () => expect.unreachable('expected request to fail'),
        error: (err: ApiErrorEnvelope) => {
          interceptedError = err;
        },
      });

      const req = httpMock.expectOne('/api/v1/events');
      req.flush(errorEnvelope, {
        status: 429,
        statusText: 'Too Many Requests',
        headers: { 'Retry-After': '0' },
      });

      expect(interceptedError?.retryAfter).toBe(0);
    });

    it('should fallback to details retry_after_seconds when Retry-After header is omitted', () => {
      const errorEnvelope: ApiErrorEnvelope = {
        error: {
          code: 'RATE_LIMITED',
          message: 'Too many requests. Please slow down.',
          timestamp: '2026-10-08T12:00:00Z',
          details: [{ retry_after_seconds: 15 }],
        },
      };

      let interceptedError: ApiErrorEnvelope | undefined;
      httpClient.get('/api/v1/events').subscribe({
        next: () => expect.unreachable('expected request to fail'),
        error: (err: ApiErrorEnvelope) => {
          interceptedError = err;
        },
      });

      const req = httpMock.expectOne('/api/v1/events');
      req.flush(errorEnvelope, { status: 429, statusText: 'Too Many Requests' });

      expect(interceptedError?.retryAfter).toBe(15);
    });
  });

  describe('Fallback ApiErrorEnvelope for network and opaque errors', () => {
    it('should construct fallback ApiErrorEnvelope with code NETWORK_ERROR on status 0', () => {
      let interceptedError: ApiErrorEnvelope | undefined;
      httpClient.get('/api/v1/health').subscribe({
        next: () => expect.unreachable('expected request to fail'),
        error: (err: ApiErrorEnvelope) => {
          interceptedError = err;
        },
      });

      const req = httpMock.expectOne('/api/v1/health');
      req.error(new ProgressEvent('error'), { status: 0, statusText: 'Unknown Error' });

      expect(interceptedError).toBeDefined();
      expect(interceptedError?.error.code).toBe('NETWORK_ERROR');
      expect(interceptedError?.error.message).toBeTruthy();
      expect(interceptedError?.error.timestamp).toBeTruthy();
      expect(Array.isArray(interceptedError?.error.details)).toBe(true);
      expect(interceptedError?.status).toBe(0);
    });

    it('should construct fallback ApiErrorEnvelope with code UNKNOWN_ERROR on non-JSON response body', () => {
      let interceptedError: ApiErrorEnvelope | undefined;
      httpClient.get('/api/v1/memory').subscribe({
        next: () => expect.unreachable('expected request to fail'),
        error: (err: ApiErrorEnvelope) => {
          interceptedError = err;
        },
      });

      const req = httpMock.expectOne('/api/v1/memory');
      req.flush('502 Bad Gateway: Upstream server unreachable', {
        status: 502,
        statusText: 'Bad Gateway',
      });

      expect(interceptedError).toBeDefined();
      expect(interceptedError?.error.code).toBe('UNKNOWN_ERROR');
      expect(interceptedError?.error.message).toContain('502');
      expect(interceptedError?.error.timestamp).toBeTruthy();
      expect(interceptedError?.status).toBe(502);
    });

    it('should construct fallback ApiErrorEnvelope on status 500 with null body', () => {
      let interceptedError: ApiErrorEnvelope | undefined;
      httpClient.get('/api/v1/disks').subscribe({
        next: () => expect.unreachable('expected request to fail'),
        error: (err: ApiErrorEnvelope) => {
          interceptedError = err;
        },
      });

      const req = httpMock.expectOne('/api/v1/disks');
      req.flush(null, { status: 500, statusText: 'Internal Server Error' });

      expect(interceptedError).toBeDefined();
      expect(interceptedError?.error.code).toBe('UNKNOWN_ERROR');
      expect(interceptedError?.status).toBe(500);
    });

    it('should construct fallback ApiErrorEnvelope when JSON body does not match envelope schema', () => {
      let interceptedError: ApiErrorEnvelope | undefined;
      httpClient.get('/api/v1/disks').subscribe({
        next: () => expect.unreachable('expected request to fail'),
        error: (err: ApiErrorEnvelope) => {
          interceptedError = err;
        },
      });

      const req = httpMock.expectOne('/api/v1/disks');
      req.flush(
        { error_description: 'An unexpected format was returned' },
        { status: 400, statusText: 'Bad Request' }
      );

      expect(interceptedError).toBeDefined();
      expect(interceptedError?.error.code).toBe('UNKNOWN_ERROR');
      expect(interceptedError?.status).toBe(400);
    });
  });
});
