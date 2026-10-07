# NodePulse — Architectural Decisions Log

This document consolidates and indexes all foundational architectural, technical, and design decisions made for the NodePulse project. Detailed context, trade-offs, and rationale are documented in the respective [Architectural Decision Records (ADRs)](file:///home/devqii/workspace/nodepulse/docs/adr/).

---

## Decision Index

| Decision ID | Title | Status | Date | Primary Driver | Full ADR Link |
|---|---|---|---|---|---|
| **DEC-001** | Standardize on C++20 | Accepted | 2026-10-06 | Modern language features (`std::span`, concepts, coroutines, `std::format`) | [ADR-001](file:///home/devqii/workspace/nodepulse/docs/adr/001-cpp20.md) |
| **DEC-002** | Drogon Async Web Framework | Accepted | 2026-10-06 | High-throughput non-blocking I/O, native HTTP/1.1 & WebSocket/SSE | [ADR-002](file:///home/devqii/workspace/nodepulse/docs/adr/002-drogon.md) |
| **DEC-003** | CMake Build System (>= 3.22) | Accepted | 2026-10-06 | Cross-compiler standard, target-based modern CMake, tooling integration | [ADR-003](file:///home/devqii/workspace/nodepulse/docs/adr/003-cmake.md) |
| **DEC-004** | Header-Based API Key Authentication | Accepted | 2026-10-06 | Simple, robust server agent security without external IdP dependencies | [ADR-004](file:///home/devqii/workspace/nodepulse/docs/adr/004-api-key-authentication.md) |
| **DEC-005** | Server-Sent Events (SSE) for Real-Time Telemetry | Accepted | 2026-10-06 | Lightweight, unidirectional HTTP streaming without WebSocket overhead | [ADR-005](file:///home/devqii/workspace/nodepulse/docs/adr/005-sse.md) |
| **DEC-006** | Prometheus Metrics Exposition (`/metrics`) | Accepted | 2026-10-06 | Industry standard exposition format for pull-based observability | [ADR-006](file:///home/devqii/workspace/nodepulse/docs/adr/006-prometheus.md) |
| **DEC-007** | Layered Clean Architecture | Accepted | 2026-10-06 | Strict separation of controllers, services, collectors, domain, repositories | [ARCHITECTURE.md](file:///home/devqii/workspace/nodepulse/docs/architecture/ARCHITECTURE.md) |
| **DEC-008** | Virtual Filesystem Direct Parsing (`/proc` & `/sys`) | Accepted | 2026-10-06 | Zero dependency, zero fork-overhead system inspection | [COMPONENTS.md](file:///home/devqii/workspace/nodepulse/docs/architecture/COMPONENTS.md) |
| **DEC-009** | Structured JSON Logging with spdlog | Accepted | 2026-10-06 | Fast asynchronous logging with guaranteed zero secret exposure | [OBSERVABILITY.md](file:///home/devqii/workspace/nodepulse/docs/architecture/OBSERVABILITY.md) |
| **DEC-010** | nlohmann/json for Data Serialization | Accepted | 2026-10-06 | Intuitive, standard modern C++ JSON serialization | [BUILD.md](file:///home/devqii/workspace/nodepulse/docs/engineering/BUILD.md) |
| **DEC-011** | Docker Daemon Inspection via Unix Socket with libcurl | Accepted | 2026-10-06 | Direct communication with `/var/run/docker.sock` without shell execution | [COMPONENTS.md](file:///home/devqii/workspace/nodepulse/docs/architecture/COMPONENTS.md) |
| **DEC-012** | Deferred PostgreSQL Metric Persistence (Phase 14) | Accepted | 2026-10-06 | Keep core agent lightweight and self-contained; optional persistent storage | [IMPLEMENTATION_PLAN.md](file:///home/devqii/workspace/nodepulse/IMPLEMENTATION_PLAN.md#phase-14--postgresql-metric-history) |
| **DEC-013** | Worker Thread Pool Offloading for Heavy Subsystem Ops | Accepted | 2026-10-06 | Preserve Drogon event loop responsiveness during intensive /proc scans | [ARCHITECTURE.md](file:///home/devqii/workspace/nodepulse/docs/architecture/ARCHITECTURE.md#3-threading--concurrency-model) |
| **DEC-014** | CPU Utilization Sampling & First-Sample Contract | Accepted | 2026-10-06 | Deterministic non-blocking delta computation and first-sample contract | [DECISIONS.md](file:///home/devqii/workspace/nodepulse/DECISIONS.md#dec-014-cpu-utilization-sampling--first-sample-contract) |
| **DEC-015** | Server-Sent Events Keepalive Comments | Accepted | 2026-10-06 | Prevent intermediate proxy/NAT connection timeout on persistent SSE streams | [DECISIONS.md](file:///home/devqii/workspace/nodepulse/DECISIONS.md#dec-015-server-sent-events-keepalive-comments) |
| **DEC-016** | Server-Sent Events Maximum Client Capacity and Admission Control | Accepted | 2026-10-06 | Bound concurrent open SSE connections to prevent server resource exhaustion | [DECISIONS.md](file:///home/devqii/workspace/nodepulse/DECISIONS.md#dec-016-server-sent-events-maximum-client-capacity-and-admission-control) |

---

## Detailed Summaries

### DEC-001: Standardize on C++20
- **Context**: The project requires modern systems programming capabilities with memory safety and clean syntax.
- **Decision**: Target ISO C++20 (`-std=c++20`).
- **Consequences**: Enables use of `std::string_view`, `std::span`, concepts for template constraints, structured binding improvements, and coroutines. Supported by GCC 11+ and Clang 14+.

### DEC-002: Drogon Async Web Framework
- **Context**: An asynchronous, non-blocking HTTP engine is needed to handle concurrent API queries, Prometheus scrapes, and persistent SSE streams with minimal thread count and memory footprint.
- **Decision**: Drogon C++ web framework.
- **Consequences**: High performance event-loop architecture based on epoll/kqueue. Native support for filters (middleware), routing, and chunked streaming.

### DEC-003: Modern CMake Build System
- **Context**: Predictable build configuration, test orchestration, and integration with IDEs and CI.
- **Decision**: CMake 3.22+ using target-centric design (`target_include_directories`, `target_link_libraries`).
- **Consequences**: Standardized compilation across developers and GitHub Actions CI.

### DEC-004: Header-Based API Key Authentication (`X-API-Key`)
- **Context**: Securing management and telemetry endpoints on host nodes without complex OAuth2/OIDC brokers.
- **Decision**: Authenticate requests via the `X-API-Key` HTTP header verified against a securely loaded secret in configuration or environment variables. Exemption granted only to `/api/v1/health`.
- **Consequences**: Constant-time key comparison to prevent timing attacks. No plain keys logged.

### DEC-005: Server-Sent Events (SSE) for Real-Time Streaming
- **Context**: Clients require continuous streams of CPU, memory, and disk telemetry without aggressive polling.
- **Decision**: Implement `/api/v1/events` using Server-Sent Events (`text/event-stream`).
- **Consequences**: Simpler than WebSockets for unidirectional agent-to-dashboard communication. Drogon async chunks keep connections alive with zero thread pinning.

### DEC-006: Prometheus Exposition Format
- **Context**: NodePulse must integrate with existing cloud-native monitoring infrastructure (Prometheus, VictoriaMetrics, Grafana Agent).
- **Decision**: Expose `/metrics` using standard Prometheus text exposition format via `prometheus-cpp`.
- **Consequences**: Standard gauge and counter metrics available for host monitoring out of the box.

### DEC-007: Layered Clean Architecture & Separation of Concerns
- **Context**: Avoid tightly coupling HTTP presentation with Linux kernel data parsing.
- **Decision**: Enforce strict layer isolation:
  - **Controllers**: Handle HTTP, JSON deserialization/serialization, and status codes.
  - **Services**: Orchestrate operations, caching, and business logic.
  - **Collectors**: Read `/proc`, `/sys`, and system APIs. Never depend on HTTP or Drogon.
  - **Domain**: Pure C++ structs representing system entities.
  - **Middleware**: Authentication, rate limiting, request tracing.
  - **Repositories**: External storage integration (e.g. PostgreSQL in Phase 14).
- **Consequences**: Isolated unit testing of kernel parsers via file fixtures without running Drogon or HTTP servers.

### DEC-013: Worker Thread Pool Offloading for Heavy Linux Subsystem Operations
- **Context**: Drogon runs an asynchronous reactive event loop model based on epoll. While individual file reads (`/proc/loadavg`, `/proc/uptime`) execute in microseconds, iterating over hundreds of process directories in `/proc/[0-9]+` or issuing blocking D-Bus and Unix socket requests can introduce non-trivial blocking latency.
- **Decision**: Adopt an asynchronous worker thread pool offload strategy for heavy filesystem traversals (such as full process table enumeration in `/api/v1/processes`) and external daemon socket queries.
- **Clarification of Architectural Origin**: This offload strategy is an intentional architectural design decision adopted by the NodePulse engineering team to preserve Drogon reactive event loop responsiveness and fulfill NFR-001 latency constraints (<25ms p95). It was NOT an externally mandated requirement of the original project specification, but an explicit internal engineering choice to prevent event loop thread starvation.
- **Consequences**: Prevents request queueing and latency degradation across concurrent HTTP clients during intensive host process table scans.

### DEC-014: CPU Utilization Sampling, Warming-Up State & Non-Blocking Architecture
- **Context**: CPU utilization cannot be derived instantaneously from a single `/proc/stat` read; it requires measuring differences between two temporal samples. The project requires non-blocking event-loop operation, thread safety, robust counter semantics, and an explicit response contract for the initial request when no prior sample exists.
- **Decision**:
  1. **First-Sample Warming-Up State**: When `CpuService` takes its first sample, it establishes a baseline snapshot. Instead of reporting a misleading `0.0%` (which confuses missing measurement with an idle CPU), `usage_percent` (aggregate and per-core) reports `null` with `measurement_status: "warming_up"`.
  2. **Non-Blocking Background Sampling Mechanism**: `CpuService` supports background periodic sampling (configured via `collectors.cpu.sample_interval_ms`, default 1000ms). When active, a dedicated background thread performs virtual filesystem I/O (`/proc/stat`, `/proc/loadavg`) and updates cached metrics in memory. Drogon event-loop threads service `GET /api/v1/cpu` by copying the cached metrics under a mutex in sub-microseconds without performing blocking file operations or stalling the event loop.
  3. **Calculation Algorithm**:
     - `total = user + nice + system + idle + iowait + irq + softirq + steal`
     - Linux kernel counter semantics: `guest` and `guest_nice` are accounted for in `user` and `nice`, and are NOT added again to prevent double-counting.
     - `idle_all = idle + iowait`
     - `busy = total - idle_all` (includes `user`, `nice`, `system`, `irq`, `softirq`, and `steal`).
     - `usage_percent = (delta_busy / delta_total) * 100.0`, clamped between `0.0` and `100.0`, with `measurement_status: "ready"`.
  4. **Zero Elapsed Total Jiffies (Cached State)**: If rapid consecutive queries arrive with `curr_total == prev_total`, the endpoint returns the previously calculated measurement marked with `measurement_status: "cached"`. This explicitly communicates that it is a preserved previous measurement, not a fresh calculation.
  5. **Counter Resets, Anomalies & Core Changes (Hotplug)**:
     - If `curr_total < prev_total`, `curr_idle < prev_idle`, or `delta_idle > delta_total` (counter wrap or reboot), the baseline snapshot is reset to current, reporting `usage_percent: null` with `measurement_status: "warming_up"`.
     - `logical_cores` dynamically tracks current active cores from `/proc/stat`. If core count changes (hotplug event), aggregate baseline is reset to `warming_up`. Cores without prior baseline report `usage_percent: null`.
  6. **Thread Safety**: All mutable state is synchronized using `std::mutex` and `std::condition_variable` in `CpuService`.
- **Consequences**: Deterministic, truthful metrics reporting with zero Drogon event loop starvation.

### DEC-015: Server-Sent Events Keepalive Comments
- **Context**: Long-lived Server-Sent Events HTTP connections traversing intermediate reverse proxies (e.g. Nginx, Envoy, AWS ALB), corporate gateways, or stateful NAT firewalls are subject to idle connection termination if silent intervals occur or when client consumption experiences backpressure.
- **Clarification of Architectural Origin**: Keepalive comments (`: keepalive\n\n`) were NOT an externally mandated requirement of the pre-Phase-12 specification, but an intentional internal implementation decision.
- **Decision**: Under the W3C Server-Sent Events standard, any line beginning with a colon (`:`) is treated as a comment and ignored by client event parsers. NodePulse emits `: keepalive\n\n` comments between telemetry frames or when explicitly triggered to maintain TCP activity across intermediate network middleboxes.
- **Consequences**: Protocol-compatible with all standard SSE consumers (EventSource, fetch, curl). Does not alter the public `metric_pulse` schema contract.

### DEC-016: Server-Sent Events Maximum Client Capacity and Admission Control
- **Context**: Server-Sent Events connections are persistent and long-lived. While Phase 10 token-bucket rate limiting restricts the rate of incoming HTTP requests per client IP, it does not limit the total number of concurrently held open connections. Without an application-level bound, legitimate or rogue clients could accumulate open streams and exhaust server file descriptors or memory.
- **Decision**:
  1. **Configurable Capacity Limit**: Introduce `sse.max_clients` with a default of 64 concurrent subscribers, validated between 1 and 10000 (`max_clients = 0` is rejected).
  2. **Atomic Admission Control**: Admission decisions are race-safe via `StreamService::try_reserve_slot()` and `try_add_client()`, guaranteeing that active and reserved subscriber slots never exceed `max_clients` even under high concurrency.
  3. **Capacity Rejection Error**: When capacity is reached, new connection attempts are rejected before establishing the streaming response with HTTP 503 `SERVICE_UNAVAILABLE` (generic message: `"SSE connection capacity is currently exhausted."`). HTTP 429 remains strictly reserved for Phase 10 request-rate limit exhaustion.
  4. **Slot Lifecycle & Cleanup**: Slots are released immediately upon client disconnect, send failure, explicit removal, server shutdown, or connection setup rollback. Dead subscribers are automatically pruned upon subsequent admission attempts.
- **Consequences**: Deterministic, bounded resource consumption for live telemetry streaming with strict isolation from request rate limiting.

### DEC-017: PostgreSQL Metric History Persistence (Phase 14)
- **Context**: Operational monitoring requires persistent archival of periodic host telemetry snapshots (CPU, load averages, memory, swap, network) into a PostgreSQL database using `libpqxx`.
- **Pre-Approved Contract vs. Implementation Decisions**:
  - **Pre-Approved Contract**:
    - Internal persistence to PostgreSQL using `libpqxx` (`IPostgresRepository`, `PostgresRepository`).
    - Database schema for `host_metrics` table with composite index `idx_host_metrics_hostname_time` (`docs/domain/DATA_MODEL.md:241-260`).
    - Dedicated background persistence thread periodically inserting snapshots (`docs/architecture/DATA_FLOW.md:145-160`, `IMPLEMENTATION_PLAN.md:437-458`).
    - Error resilience: safe handling of database downtime without crashing or degrading HTTP API (`BR-003`, `docs/architecture/DATA_FLOW.md:158-160`).
    - Public historical query REST endpoints were **explicitly deferred** in `IMPLEMENTATION_PLAN.md:457`. No public `/api/v1/history` route is exposed.
    - Automated retention cleanup was **not present** in pre-Phase-14 contracts. Retention configuration and automated cleanup are omitted.
  - **Phase-14 Implementation Decisions**:
    1. **Parameterized Queries**:
       - Enforce parameterized SQL execution via `pqxx::params` and `tx.exec(sql, p)` across all queries, strictly eliminating SQL injection vulnerabilities.
    2. **Decoupled Architecture & Nullable Metrics Preservation**:
       - Background persistence loop runs in `HistoryService` on a dedicated thread, sampling existing cached metrics from `SystemService`, `CpuService`, `MemoryService`, and `NetworkService` without redundant `/proc` parsing or Drogon event loop interference.
       - Preserves `std::nullopt` (SQL `NULL`) for `cpu_usage_percent`, `net_rx_bytes_per_sec`, and `net_tx_bytes_per_sec` when collectors are in warming up / initial baseline states (DEC-014), avoiding misleading 0.0 metrics.
    3. **Resilience & Graceful Degradation**:
       - PostgreSQL storage is strictly optional (`postgres.enabled`, default `false`). If disabled or if the PostgreSQL server is unreachable/offline, core host monitoring and all real-time endpoints remain 100% operational.
       - Failed background sample writes are safely dropped with error logging; no unbounded memory queues are created.
    4. **Security & Credential Redaction**:
       - Connection strings (`postgres.connection_string`, `NODEPULSE_POSTGRES_URL`) containing secrets are strictly masked in all logs, internal state, and exceptions via `nodepulse::utils::redact_connection_string()`, handling both URI (`postgresql://user:pass@host/db`) and libpq key-value (`host=... password=...`) syntaxes.
- **Consequences**: Scalable, secure historical telemetry persistence with zero impact on real-time event loop latency or core agent availability during database outages.
