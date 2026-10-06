# NodePulse — Non-Functional Requirements Specification

This document defines the quality attributes, performance envelopes, security controls, and operational constraints for NodePulse (`NFR-001` through `NFR-010`).

---

## Non-Functional Requirements Matrix

| ID | Category | Target Metric / Constraint | Verification Method |
|---|---|---|---|
| **NFR-001** | Performance & Latency | p95 latency < 25ms; Health check < 2ms | Load test (`wrk` / `k6`) |
| **NFR-002** | Resource Efficiency | Idle RSS < 35MB; Load RSS < 65MB; CPU < 2% | Process monitor (`ps`, `valgrind`) |
| **NFR-003** | Reliability & Resilience | Zero unhandled exceptions; Crash recovery | Stress & fault injection |
| **NFR-004** | Security & Least Privilege | Non-root daemon; Zero secrets logged; Socket warning | Security scan & log audit |
| **NFR-005** | Concurrency & Async I/O | Non-blocking event loop; Thread pool isolation | Concurrency stress test |
| **NFR-006** | Portability & Architecture | Linux kernel >= 4.18; x86_64 and arm64 | Multi-arch CI builds |
| **NFR-007** | Code Quality & Standards | C++20 standard; `-Wall -Wextra -Werror` clean | Compiler & Clang-Tidy |
| **NFR-008** | Observability & Telemetry | Structured JSON logging; Self-metrics exposed | Log schema validation |
| **NFR-009** | Testability & Mockability | > 80% code coverage; Fixture-driven `/proc` parsers | `lcov` coverage reports |
| **NFR-010** | Build & CI Automation | CMake >= 3.22; Reproducible GitHub Actions CI | Automated CI execution |

---

## Detailed Specifications

### NFR-001: Performance & Latency
- **Specification**:
  - `GET /api/v1/health` must respond in less than **2ms** at p99 under normal load.
  - Core telemetry endpoints (`/api/v1/cpu`, `/api/v1/memory`, `/api/v1/system`, `/api/v1/disks`, `/api/v1/network`) must complete with **p95 latency < 25ms** under a load of 100 requests per second.
  - Process table endpoint (`/api/v1/processes`) with 500 running host processes must complete with **p95 latency < 50ms**.
- **Rationale**: Server telemetry agents must not add perceptible load or latency to monitored infrastructure.

### NFR-002: Resource Efficiency & Footprint
- **Specification**:
  - **Resident Set Size (RSS)**:
    - Idle state: **< 35 MB**.
    - Under sustained 100 req/sec load: **< 65 MB**.
  - **CPU Utilization**:
    - Idle background polling: **< 0.5%** of a single CPU core.
    - Active polling load (10 req/sec): **< 2.0%** of a single CPU core.
  - **Binary Footprint**: Stripped release binary size must not exceed **25 MB** (excluding external shared libraries).
- **Rationale**: NodePulse must run comfortably on resource-constrained virtual machines and edge appliances.

### NFR-003: Reliability & Fault Tolerance
- **Specification**:
  - The agent must exhibit **zero uncaught C++ exceptions** across all runtime paths.
  - Failure of any single collector (e.g. inability to parse `/proc/mounts` or Docker daemon offline) must degrade gracefully by returning an error response without crashing or terminating the agent process.
  - Integrated systemd service configuration must specify `Restart=on-failure` and `RestartSec=5s` to guarantee automatic recovery.
- **Rationale**: An observability agent must be more reliable than the applications it observes.

### NFR-004: Security & Least Privilege
- **Specification**:
  - NodePulse must execute as an unprivileged dedicated service user (`nodepulse:nodepulse`). It must never require `root` privileges for core metrics collection.
  - Access to `/var/run/docker.sock` must be documented explicitly as conferring root-equivalent access to the host. The agent must never run as a `--privileged` container.
  - **Zero Credential Exposure**: API keys, database credentials, and authorization headers must be stripped, redacted, or masked (`***`) before being recorded in any log output, error envelope, or metric tag.
  - Constant-time string comparison algorithms must be employed for authentication checks to eliminate timing side-channel attacks.
- **Rationale**: Monitoring daemons are frequent targets for privilege escalation.

### NFR-005: Concurrency & Asynchronous Non-Blocking I/O
- **Specification**:
  - All HTTP connection handling and event routing must run on Drogon's non-blocking epoll thread pool.
  - Potentially blocking operations (such as traversing `/proc` for hundreds of processes or querying remote PostgreSQL databases) must be scheduled on dedicated worker threads to prevent starving Drogon's primary I/O event loops.
  - SSE streams must utilize asynchronous chunked writes without pinning worker threads.
- **Rationale**: Ensures responsive networking even when host disk I/O experiences transient stalls.

### NFR-006: Portability & Linux Compatibility
- **Specification**:
  - Target Operating Systems: Any standard Linux distribution running Linux kernel **>= 4.18** with `glibc >= 2.31` or `musl >= 1.2` (including Ubuntu 20.04+, Debian 11+, RHEL/Rocky Linux 8+, Alpine 3.16+).
  - Target Architectures: `x86_64` (AMD64) and `aarch64` (ARM64).
  - The agent must not depend on non-standard kernel extensions or out-of-tree kernel modules.
- **Rationale**: Ensures broad deployability across cloud instances, on-premises servers, and edge ARM devices.

### NFR-007: Code Quality & Modern Standards
- **Specification**:
  - Source code must strictly comply with the **ISO C++20** standard (`-std=c++20`).
  - Compilation must succeed with zero warnings under `-Wall -Wextra -Wpedantic -Werror`.
  - All code must adhere to `.clang-format` (Google style derivative) and pass `.clang-tidy` checks without suppressions.
  - Strict RAII memory management: no raw owning pointers, zero memory leaks verified via AddressSanitizer and Valgrind.
- **Rationale**: Ensures maintainability, auditability, and safety across long-term evolution.

### NFR-008: Observability & Self-Monitoring
- **Specification**:
  - Logging must be performed using `spdlog` emitting structured JSON in production mode (`{"timestamp": ..., "level": "INFO", "msg": ..., "context": {...}}`).
  - The agent must expose its own health and operational telemetry via `/metrics` (e.g. `nodepulse_http_requests_total`, `nodepulse_collector_duration_seconds`).
- **Rationale**: Operators must be able to monitor the health and performance of the monitoring agent itself.

### NFR-009: Testability & Fixture Decoupling
- **Specification**:
  - Core collectors must decouple parsing logic from the physical host filesystem by accepting abstract stream readers or mock root directory prefixes.
  - Every parser must have comprehensive fixture-driven unit tests in `tests/fixtures/proc/` covering nominal, truncated, malformed, and edge-case inputs.
  - Overall unit and integration test coverage must exceed **80%** lines of code.
- **Rationale**: Guarantees deterministic testing on macOS or Windows developer machines without access to a native Linux kernel.

### NFR-010: Build & CI Automation
- **Specification**:
  - The project must use modern target-based CMake (>= 3.22) with Ninja generator support.
  - CI pipeline must run automated build, format verification, static analysis, and test suites on GCC 11+ and Clang 14+ on every pull request.
  - Build times for clean debug builds must not exceed 2 minutes on standard 4-core CI runners.
- **Rationale**: Rapid feedback loops and reproducible builds across developer and production environments.

