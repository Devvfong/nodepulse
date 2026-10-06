# NodePulse — Observability & Self-Monitoring Architecture

This document details the logging standards, internal metrics collection, and diagnostic tracing mechanisms implemented within NodePulse.

---

## 1. Structured Logging Architecture

NodePulse utilizes **`spdlog`** configured as an asynchronous, non-blocking logger to ensure file and console I/O never blocks Drogon event loops.

### 1.1 Logging Modes
- **Development Mode (`"log_format": "text"`)**:
  - Colorized console output for human readability.
  - Pattern: `[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] [%s:%#] %v`
- **Production Mode (`"log_format": "json"`)**:
  - Emits one valid JSON object per line for ingestion by vector, fluentbit, or Grafana Loki.
  - Standard JSON schema:
    ```json
    {
      "timestamp": "2026-10-06T06:53:50.123Z",
      "level": "INFO",
      "logger": "nodepulse",
      "thread_id": 140234,
      "request_id": "c1f728ea-8b43-4a67-93cf-a5e2f7b823b1",
      "message": "Collector finished execution",
      "collector": "cpu_collector",
      "duration_ms": 0.42
    }
    ```

### 1.2 Mandatory Secret Redaction (BR-002)
To prevent accidental exposure of sensitive keys, tokens, or credentials:
- The logging wrapper runs a sanitizer over log context maps and messages.
- The `X-API-Key` header is never logged in plaintext; only its presence or hash prefix is logged at DEBUG level (`key_prefix="np_sec...***"`).
- Any environment variable inspected from Docker containers matching `*KEY*`, `*SECRET*`, `*PASSWORD*`, `*TOKEN*`, or `*AUTH*` is replaced with `***REDACTED***`.

---

## 2. Self-Monitoring Internal Metrics

Alongside host system metrics, NodePulse instruments its own internal operations and exposes them through `GET /metrics`.

### 2.1 Agent Metric Catalog

| Metric Name | Type | Description | Labels |
|---|---|---|---|
| `nodepulse_build_info` | Gauge | Agent version and commit metadata | `version`, `commit`, `compiler` |
| `nodepulse_process_uptime_seconds` | Counter | Total seconds NodePulse agent has been running | None |
| `nodepulse_process_resident_memory_bytes` | Gauge | Resident set size (RSS) of NodePulse process | None |
| `nodepulse_process_cpu_seconds_total` | Counter | Total CPU seconds consumed by NodePulse process | `mode="user"`, `mode="system"` |
| `nodepulse_http_requests_total` | Counter | Cumulative HTTP requests served | `endpoint`, `method`, `status` |
| `nodepulse_http_request_duration_seconds` | Histogram | Latency distribution of HTTP endpoints | `endpoint`, `method` |
| `nodepulse_collector_duration_seconds` | Histogram | Time spent parsing kernel subsystems | `collector` |
| `nodepulse_collector_failures_total` | Counter | Count of collector execution errors | `collector`, `error_code` |
| `nodepulse_sse_active_connections` | Gauge | Current number of open SSE streaming clients | None |
| `nodepulse_rate_limit_rejections_total`| Counter | Count of requests rejected due to rate limit | `client_ip` |

---

## 3. Request Correlation & Tracing

- Every inbound HTTP request is checked for an existing `X-Request-ID` header.
- If missing, Drogon's request pipeline generates a UUIDv4 string.
- The request ID is:
  1. Attached to Drogon's request context.
  2. Included in all log statements emitted during request lifecycle.
  3. Returned in the HTTP response header `X-Request-ID`.
  4. Embedded in JSON error envelopes (`details[].request_id`).

