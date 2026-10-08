import { HttpErrorResponse, HttpInterceptorFn } from '@angular/common/http';
import { catchError, throwError } from 'rxjs';
import { ApiErrorDetail, ApiErrorEnvelope } from '../models/api-error.model';

/**
 * Parses a Retry-After header string into integer seconds.
 * Supports both delta-seconds (e.g. '1', '120') and HTTP-date strings.
 */
function parseRetryAfter(headerValue: string | null | undefined): number | undefined {
  if (!headerValue) {
    return undefined;
  }
  const trimmed = headerValue.trim();
  const seconds = Number(trimmed);
  if (!Number.isNaN(seconds) && seconds >= 0) {
    return Math.floor(seconds);
  }
  const dateMs = Date.parse(trimmed);
  if (!Number.isNaN(dateMs)) {
    const diffSeconds = Math.max(0, Math.ceil((dateMs - Date.now()) / 1000));
    return diffSeconds;
  }
  return undefined;
}

/**
 * Fallback extractor for retry_after_seconds from the error details array.
 */
function extractRetryAfterFromBody(details: ApiErrorDetail[] | undefined): number | undefined {
  if (!details || !Array.isArray(details)) {
    return undefined;
  }
  for (const detail of details) {
    if (detail && typeof detail === 'object' && typeof detail['retry_after_seconds'] === 'number') {
      return detail['retry_after_seconds'] as number;
    }
  }
  return undefined;
}

/**
 * Type guard to check if an arbitrary object conforms to the backend ApiErrorEnvelope schema.
 */
function isApiErrorEnvelopeBody(body: unknown): body is ApiErrorEnvelope {
  if (!body || typeof body !== 'object') {
    return false;
  }
  const obj = body as Record<string, unknown>;
  const errorObj = obj['error'];
  if (!errorObj || typeof errorObj !== 'object') {
    return false;
  }
  const err = errorObj as Record<string, unknown>;
  return typeof err['code'] === 'string' && typeof err['message'] === 'string';
}

/**
 * Converts an unknown caught error (typically HttpErrorResponse) into a typed ApiErrorEnvelope.
 */
function normalizeApiError(error: unknown): ApiErrorEnvelope {
  if (error instanceof HttpErrorResponse) {
    let retryAfter: number | undefined;
    if (error.status === 429) {
      retryAfter = parseRetryAfter(error.headers?.get('Retry-After'));
    }

    if (isApiErrorEnvelopeBody(error.error)) {
      const envelopeBody = error.error;
      const details: ApiErrorDetail[] = Array.isArray(envelopeBody.error.details)
        ? envelopeBody.error.details
        : [];
      const timestamp =
        typeof envelopeBody.error.timestamp === 'string'
          ? envelopeBody.error.timestamp
          : new Date().toISOString();

      if (error.status === 429 && retryAfter === undefined) {
        retryAfter = extractRetryAfterFromBody(details);
      }

      return {
        error: {
          code: envelopeBody.error.code,
          message: envelopeBody.error.message,
          timestamp,
          details,
        },
        status: error.status,
        ...(retryAfter !== undefined ? { retryAfter } : {}),
      };
    }

    // Status 0 represents network failure, client offline, or blocked request
    if (error.status === 0) {
      return {
        error: {
          code: 'NETWORK_ERROR',
          message:
            error.statusText && error.statusText !== 'Unknown Error'
              ? `Network connection error: ${error.statusText}`
              : 'Network connection error: unable to reach NodePulse agent',
          timestamp: new Date().toISOString(),
          details: [
            {
              reason: error.message || 'Network connection failure (status 0)',
            },
          ],
        },
        status: 0,
      };
    }

    // Non-JSON or unmarshaled response body
    let message: string;
    if (typeof error.error === 'string' && error.error.trim().length > 0) {
      message = error.error.trim();
    } else if (error.statusText) {
      message = `HTTP ${error.status}: ${error.statusText}`;
    } else {
      message = `HTTP ${error.status} error occurred`;
    }

    return {
      error: {
        code: 'UNKNOWN_ERROR',
        message,
        timestamp: new Date().toISOString(),
        details: [
          {
            reason: error.message || `HTTP ${error.status}`,
          },
        ],
      },
      status: error.status,
      ...(retryAfter !== undefined ? { retryAfter } : {}),
    };
  }

  // Generic or unexpected error
  const message =
    error instanceof Error ? error.message : 'An unexpected application error occurred';
  return {
    error: {
      code: 'UNKNOWN_ERROR',
      message,
      timestamp: new Date().toISOString(),
      details: [{ reason: String(error) }],
    },
    status: 0,
  };
}

/**
 * Global HTTP Interceptor that intercepts failing HTTP requests, unmarshals
 * the backend ApiErrorEnvelope (or synthesizes a fallback envelope),
 * extracts Retry-After headers, and propagates a typed ApiErrorEnvelope.
 */
export const errorInterceptor: HttpInterceptorFn = (req, next) => {
  return next(req).pipe(
    catchError((error: unknown) => {
      const envelope = normalizeApiError(error);
      return throwError(() => envelope);
    })
  );
};
