# NodePulse — Error Handling & Error Taxonomy

This document specifies the standard error response envelope and complete error code taxonomy for NodePulse.

---

## 1. Standard Error Response Envelope

All error responses emitted by controllers, middlewares, and the Drogon exception handler adhere to a consistent JSON schema:

```json
{
  "error": {
    "code": "STRING_ERROR_CODE",
    "message": "Human-readable description of what caused the failure.",
    "timestamp": "2026-10-06T06:53:50Z",
    "details": [
      {
        "field": "optional_parameter_name",
        "reason": "specific_validation_failure"
      }
    ]
  }
}
```

---

## 2. Standard Error Taxonomy

| Error Code | HTTP Status | Description & Typical Causes |
|---|---|---|
| **`INVALID_REQUEST`** | 400 Bad Request | Malformed JSON payload, invalid query parameter, negative or non-numeric `{pid}`, invalid service name syntax. |
| **`UNAUTHORIZED`** | 401 Unauthorized | Missing `X-API-Key` header or supplied API key does not match configured key. |
| **`FORBIDDEN`** | 403 Forbidden | Request is authenticated, but client is barred from accessing the resource (e.g. access restriction by client subnet). |
| **`RESOURCE_NOT_FOUND`** | 404 Not Found | Requested PID does not exist in `/proc`, systemd unit not found, or Docker container ID does not exist. |
| **`RATE_LIMITED`** | 429 Too Many Requests | Client has exceeded token-bucket rate limit quota. Includes `Retry-After` header. |
| **`COLLECTOR_FAILURE`** | 500 Internal Error | Subsystem failure during `/proc` or `/sys` reading (e.g. kernel file unreadable, disk I/O stall). |
| **`DOCKER_UNAVAILABLE`** | 503 Service Unavailable | Docker integration requested but `/var/run/docker.sock` is missing, permissions denied, or daemon offline. |
| **`SERVICE_UNAVAILABLE`** | 503 Service Unavailable | NodePulse agent is in graceful shutdown or internal thread pool is saturated. |
| **`INTERNAL_ERROR`** | 500 Internal Error | Unhandled server error or fatal invariant breach. |

---

## 3. Error Code Specifications & Examples

### 3.1 `INVALID_REQUEST` (HTTP 400)
- **Cause**: Client supplied an invalid integer for `{pid}` (e.g. `/api/v1/processes/-12` or `/api/v1/processes/abc`).
- **Response**:
  ```json
  {
    "error": {
      "code": "INVALID_REQUEST",
      "message": "Process ID must be a positive integer.",
      "timestamp": "2026-10-06T06:53:50Z",
      "details": [
        {
          "field": "pid",
          "reason": "must_be_positive_integer"
        }
      ]
    }
  }
  ```

---

### 3.2 `UNAUTHORIZED` (HTTP 401)
- **Cause**: Request missing `X-API-Key` header.
- **Response**:
  ```json
  {
    "error": {
      "code": "UNAUTHORIZED",
      "message": "Authentication required. Provide a valid X-API-Key header.",
      "timestamp": "2026-10-06T06:53:50Z",
      "details": []
    }
  }
  ```

---

### 3.3 `RESOURCE_NOT_FOUND` (HTTP 404)
- **Cause**: Queried process PID 999999 terminated or does not exist.
- **Response**:
  ```json
  {
    "error": {
      "code": "RESOURCE_NOT_FOUND",
      "message": "Process with PID 999999 was not found.",
      "timestamp": "2026-10-06T06:53:50Z",
      "details": [
        {
          "resource_type": "process",
          "identifier": "999999"
        }
      ]
    }
  }
  ```

---

### 3.4 `RATE_LIMITED` (HTTP 429)
- **Cause**: Inbound request bursts exceeded token bucket rate limits.
- **Headers**: `Retry-After: 1`
- **Response**:
  ```json
  {
    "error": {
      "code": "RATE_LIMITED",
      "message": "Too many requests. Please slow down.",
      "timestamp": "2026-10-06T06:53:50Z",
      "details": [
        {
          "retry_after_seconds": 1
        }
      ]
    }
  }
  ```

---

### 3.5 `COLLECTOR_FAILURE` (HTTP 500)
- **Cause**: Linux kernel virtual file could not be read or parsed.
- **Response**:
  ```json
  {
    "error": {
      "code": "COLLECTOR_FAILURE",
      "message": "Failed to collect CPU metrics from /proc/stat.",
      "timestamp": "2026-10-06T06:53:50Z",
      "details": [
        {
          "collector": "cpu_collector",
          "target_file": "/proc/stat"
        }
      ]
    }
  }
  ```

---

### 3.6 `DOCKER_UNAVAILABLE` (HTTP 503)
- **Cause**: Client queried `/api/v1/containers` but `/var/run/docker.sock` does not exist.
- **Response**:
  ```json
  {
    "error": {
      "code": "DOCKER_UNAVAILABLE",
      "message": "Docker daemon is not running or socket /var/run/docker.sock is inaccessible.",
      "timestamp": "2026-10-06T06:53:50Z",
      "details": [
        {
          "socket_path": "/var/run/docker.sock"
        }
      ]
    }
  }
  ```

---

### 3.7 `INTERNAL_ERROR` (HTTP 500)
- **Cause**: Unexpected server condition.
- **Response**:
  ```json
  {
    "error": {
      "code": "INTERNAL_ERROR",
      "message": "An unexpected error occurred processing your request.",
      "timestamp": "2026-10-06T06:53:50Z",
      "details": []
    }
  }
  ```

