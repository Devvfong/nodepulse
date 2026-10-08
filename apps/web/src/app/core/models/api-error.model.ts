export interface ApiErrorDetail {
  field?: string;
  reason?: string;
  [key: string]: unknown;
}

export type ApiErrorCode =
  | 'INVALID_REQUEST'
  | 'UNAUTHORIZED'
  | 'FORBIDDEN'
  | 'RESOURCE_NOT_FOUND'
  | 'RATE_LIMITED'
  | 'COLLECTOR_FAILURE'
  | 'DOCKER_UNAVAILABLE'
  | 'SERVICE_UNAVAILABLE'
  | 'INTERNAL_ERROR'
  | 'NETWORK_ERROR'
  | 'UNKNOWN_ERROR'
  | string;

export interface ApiErrorEnvelope {
  error: {
    code: ApiErrorCode;
    message: string;
    timestamp: string;
    details: ApiErrorDetail[];
  };
  retryAfter?: number;
  status?: number;
}
