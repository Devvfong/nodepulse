# NodePulse — Business Rules & Invariants

This document establishes the binding domain invariants and operational business rules (`BR-001` through `BR-012`) governing NodePulse behavior.

---

## Business Rules Matrix

| ID | Title | Scope | Enforcement Point |
|---|---|---|---|
| **BR-001** | Authentication Bypass for Health Checks | Security | `AuthFilter` |
| **BR-002** | Secret Obfuscation & Redaction | Security / Privacy | Logging / Error Serializer |
| **BR-003** | Graceful Degradation on Collector Failure | Resilience | Services & Collectors |
| **BR-004** | Docker Integration Optionality | Availability | `DockerService` / `DockerController` |
| **BR-005** | Docker Socket Privilege Isolation | Security / Audit | Deployment & Configuration |
| **BR-006** | Layer Isolation Rule | Architecture | Code Review & Compiler |
| **BR-007** | Domain Object Decoupling | Architecture | Code Review & Typings |
| **BR-008** | Rate Limiting Enforcement | Traffic Control | `RateLimitFilter` |
| **BR-009** | Non-blocking Event Loop Protection | Performance | Threading & Async Tasks |
| **BR-010** | Idempotent & Read-Only Observation | Safety | Routing & Controller Handlers |
| **BR-011** | Strict Parameter Validation | Input Validation | Controller Request Handlers |
| **BR-012** | Metric Timestamp Synchronization | Accuracy | Domain Serializers / Collectors |

---

## Detailed Business Rules

### BR-001: Authentication Bypass Exemption for Health Checks & Metrics Policy
- **Statement**: The agent liveness and readiness probe endpoint `GET /api/v1/health` MUST NOT require authentication. All other API endpoints under `/api/v1` MUST require a valid `X-API-Key` header. The Prometheus exposition endpoint `GET /metrics` is secure-by-default and requires a valid `X-API-Key` header unless explicitly configured to allow unauthenticated scraping via `prometheus.require_auth: false`. If `prometheus.require_auth` is not set or set to `true`, unauthenticated calls to `/metrics` MUST return HTTP 401 `UNAUTHORIZED`.
- **Rationale**: Infrastructure orchestrators (Kubernetes, AWS ALB, systemd watchdog) require lightweight, credential-free endpoints to assess process vitality without circular credential dependencies. Prometheus endpoints are secured by default to prevent unauthorized telemetry harvesting, with an opt-in exemption for isolated internal scraping networks.

### BR-002: Secret Obfuscation & Redaction
- **Statement**: API keys, database credentials, bearer tokens, and container environment variables containing sensitive keywords (`KEY`, `SECRET`, `PASS`, `TOKEN`, `AUTH`) MUST NEVER be logged, emitted in SSE events, or reflected in error responses.
- **Enforcement**: Logging middleware and error response serializers must run redaction masks (`***REDACTED***`) over any context strings before emitting to `spdlog` or JSON responses.

### BR-003: Graceful Degradation on Collector Failure
- **Statement**: If an individual collector fails to read a virtual kernel file (e.g. `/proc/mounts` unavailable or transient I/O error), the agent MUST NOT crash or terminate. It must return a structured HTTP 500 `COLLECTOR_FAILURE` response or omit the affected optional metrics while keeping the remaining agent endpoints healthy.
- **Enforcement**: Collectors wrap kernel I/O operations in error-checked handlers returning `std::expected` or `std::optional`.

### BR-004: Docker Integration Optionality
- **Statement**: The availability of the Docker Engine daemon is optional. If the Docker daemon is not installed, the socket `/var/run/docker.sock` is absent, or the connection is refused, the agent MUST remain fully functional. Calls to `/api/v1/containers` MUST return HTTP 503 with error code `DOCKER_UNAVAILABLE`.
- **Enforcement**: `DockerService` handles socket connection errors gracefully without re-raising exceptions to the core application runtime.

### BR-005: Docker Socket Privilege Isolation
- **Statement**: Access to `/var/run/docker.sock` confers root-equivalent capabilities over the host operating system. NodePulse documentation and configuration MUST NOT describe Docker socket access as "safe" merely because the API endpoints are read-only. NodePulse MUST NOT be executed as a `--privileged` container.
- **Enforcement**: System documentation and deployment runbooks must require non-root execution and explicitly flag Docker socket mounting as a critical security perimeter.

### BR-006: Layer Isolation Rule
- **Statement**: Controllers MUST NOT open, parse, or directly access Linux virtual filesystems (`/proc`, `/sys`) or invoke system commands. Collectors MUST NOT import Drogon HTTP headers, inspect HTTP request objects, or construct JSON responses.
- **Enforcement**: Strictly enforced by module separation and `#include` hygiene. Violations must be rejected during code review and automated checks.

### BR-007: Domain Object Decoupling
- **Statement**: Data passing between Collectors, Services, and Controllers MUST be encapsulated in strongly typed C++ domain structures (e.g. `CpuMetrics`, `SystemInfo`). Passing raw JSON strings, `nlohmann::json` objects, or Drogon JSON variants through lower layers is prohibited.
- **Rationale**: Prevents coupling the low-level kernel parsing logic to HTTP presentation formats and enables fast, typed unit testing.

### BR-008: Rate Limiting Enforcement
- **Statement**: Inbound requests exceeding the configured token bucket threshold (default 100 requests/minute per client IP) MUST be rejected with HTTP 429 `RATE_LIMITED`. The response must include a `Retry-After` header indicating the backoff duration in seconds.
- **Enforcement**: Handled at the Drogon filter layer prior to invoking controller routing.

### BR-009: Non-blocking Event Loop Protection
- **Statement**: Long-running or synchronous blocking operations (such as traversing all `/proc/[0-9]+` entries on systems with hundreds of processes, or executing remote SQL queries) MUST NOT run on Drogon's primary I/O event loop threads.
- **Architectural Design Decision**: Offloading heavy filesystem scans and external socket queries to a worker thread pool is an intentional architectural design choice made to preserve Drogon reactive event loop responsiveness and fulfill NFR-001 latency constraints; it is not a requirement mandated by the original product specification.
- **Enforcement**: Heavy operations are dispatched to Drogon's background task worker pool or dedicated asynchronous worker threads.

### BR-010: Idempotent & Read-Only Observation
- **Statement**: All monitoring and telemetry endpoints under `/api/v1` MUST be strictly read-only and idempotent HTTP `GET` operations. They must produce zero side effects on monitored host processes, kernel tunables, or service configurations.
- **Rationale**: Ensures the agent cannot destabilize production servers or be exploited as a vector for host modification.

### BR-011: Strict PID & Identifier Validation
- **Statement**: Process IDs in path parameters (`/api/v1/processes/{pid}`) MUST be strictly positive decimal integers (`pid >= 1`). If an upper-bound check is verified at runtime, Linux `/proc/sys/kernel/pid_max` is authoritative rather than any hard-coded application constant. Service names (`/api/v1/services/{name}`) must match standard systemd unit naming conventions (`[a-zA-Z0-9_\-\.\@]+`). Malformed identifiers must immediately return HTTP 400 `INVALID_REQUEST`.
- **Enforcement**: Integer bounds verification (`pid >= 1`, runtime check against `/proc/sys/kernel/pid_max` if configured) and regular expression validation in controller entry points before invoking services.

### BR-012: Metric Timestamp Synchronization
- **Statement**: All telemetry timestamps generated by NodePulse must be recorded in UTC ISO 8601 string format (`YYYY-MM-DDTHH:MM:SSZ`) or Unix epoch milliseconds. Differential rate calculations (e.g. CPU usage percentages and network throughput) MUST use `std::chrono::steady_clock` to prevent clock skew errors during NTP adjustments.
- **Enforcement**: Handled within domain time utilities (`src/utils/time.cpp`).

