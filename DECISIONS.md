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


