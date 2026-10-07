# NodePulse — Linux Server Monitoring & Management Agent

NodePulse is a high-performance, lightweight Linux server monitoring and management agent written in modern **C++20** using the **Drogon** asynchronous web framework. It provides secure, low-overhead HTTP REST endpoints, real-time Server-Sent Events (SSE) telemetry, and Prometheus metrics exposition by reading kernel state directly from `/proc`, `/sys`, and POSIX APIs.

---

## Key Features & Goals

- **Production-Grade Modern C++20**: Demonstrates clean systems programming, RAII, strong typing, concepts, and zero-overhead abstractions.
- **Direct Linux Subsystem Inspection**: Parses `/proc/stat`, `/proc/meminfo`, `/proc/cpuinfo`, `/proc/loadavg`, `/proc/uptime`, `/proc/net/dev`, `/proc/<pid>`, `/proc/mounts`, `/sys/class/net`, and utilizes `statvfs()`, `uname()`, and `gethostname()`.
- **Asynchronous, Non-Blocking Networking**: Powered by Drogon's epoll-based event loop for high concurrency with minimal CPU and memory overhead.
- **Real-Time Telemetry Streaming**: Stream live system telemetry pulses over Server-Sent Events (`GET /api/v1/events`).
- **Prometheus Scrape Endpoint**: Built-in `/metrics` endpoint compatible with standard Prometheus, VictoriaMetrics, and Grafana Agent collectors.
- **Optional Docker Inspection**: Inspect container state via `/var/run/docker.sock` using `libcurl` without shell execution.
- **Defense in Depth Security**:
  - API-Key authentication via the `X-API-Key` HTTP header.
  - Per-client token-bucket rate limiting.
  - Constant-time secret verification; zero secrets in log outputs.
  - Explicit warning and handling of Docker socket root-equivalence.
  - Strictly non-privileged execution and hardened systemd service isolation.
- **Enterprise Operations**: Fully managed via systemd with sandboxing directives (`ProtectSystem=strict`, `NoNewPrivileges=true`).

---

## Technology Stack

| Component | Technology | Rationale |
|---|---|---|
| **Language** | ISO C++20 | Modern type safety, `std::span`, concepts, coroutines |
| **HTTP Framework** | Drogon | High-throughput non-blocking asynchronous event loop |
| **Build System** | CMake (>= 3.22) + Ninja | Target-centric modular builds across GCC & Clang |
| **Testing** | GoogleTest (GTest) | Fixture-based unit testing and integration suites |
| **Logging** | spdlog | Blazing fast asynchronous structured logging |
| **Serialization** | nlohmann/json | Modern C++ JSON serialization for REST payloads |
| **Socket / HTTP Client** | libcurl | Robust communication with Docker daemon Unix socket |
| **Metrics** | prometheus-cpp | Standard Prometheus exposition format |
| **Database (Phase 14)** | PostgreSQL + libpqxx | Optional persistence of historical metric snapshots |
| **Service Supervisor** | systemd | Production Linux service lifecycle and sandboxing |

---

## Architectural Layers

NodePulse strictly separates responsibilities into decoupled layers:

1. **Controllers** (`src/controllers/`): Handle HTTP request routing, parameter validation, and JSON serialization. Never read kernel files directly.
2. **Services** (`src/services/`): Coordinate collectors, implement delta calculations, and manage business workflows.
3. **Collectors** (`src/collectors/`): Pure system observers reading `/proc`, `/sys`, and system APIs. Never depend on HTTP concepts.
4. **Domain** (`include/nodepulse/domain/`): Strongly typed C++ structures representing system entities (`CpuMetrics`, `ProcessInfo`, etc.).
5. **Middleware** (`src/middleware/`): Request filtering, API key verification, and rate limiting.
6. **Repositories** (`src/repositories/`): External data persistence (PostgreSQL).
7. **Utilities** (`src/utils/`): String manipulation, constant-time cryptography helpers, and logging.

---

## Planned API Endpoints (`/api/v1`)

| Method | Endpoint | Description | Auth Required |
|---|---|---|---|
| `GET` | `/api/v1/health` | Agent liveness & readiness check | No |
| `GET` | `/api/v1/system` | Hostname, OS distribution, kernel version, uptime | Yes (`X-API-Key`) |
| `GET` | `/api/v1/cpu` | Aggregate and per-core CPU utilization & load average | Yes (`X-API-Key`) |
| `GET` | `/api/v1/memory` | Physical RAM, virtual memory, and swap utilization | Yes (`X-API-Key`) |
| `GET` | `/api/v1/disks` | Filesystem mount points, total/free/used disk space | Yes (`X-API-Key`) |
| `GET` | `/api/v1/network` | Network interface statistics, bandwidth, errors | Yes (`X-API-Key`) |
| `GET` | `/api/v1/processes` | Process table enumeration with CPU and memory usage | Yes (`X-API-Key`) |
| `GET` | `/api/v1/processes/{pid}` | Detailed metrics for a specific process by PID | Yes (`X-API-Key`) |
| `GET` | `/api/v1/services` | Active systemd service units status | Yes (`X-API-Key`) |
| `GET` | `/api/v1/services/{name}` | Detailed status for a specific systemd service | Yes (`X-API-Key`) |
| `GET` | `/api/v1/containers` | Docker containers list (if Docker integration enabled) | Yes (`X-API-Key`) |
| `GET` | `/api/v1/containers/{id}` | Detailed Docker container inspection | Yes (`X-API-Key`) |
| `GET` | `/api/v1/events` | Real-time Server-Sent Events (SSE) telemetry stream | Yes (`X-API-Key`) |
| `GET` | `/metrics` | Prometheus exposition format metrics | Yes (Secure by default; unauthenticated only when explicitly configured) |

---

## Project Status

- **Status**: Phase 2 — HTTP Foundation Complete.
- **Next Approved Phase**: [Phase 3: System Collector](file:///home/devqii/workspace/nodepulse/IMPLEMENTATION_PLAN.md#phase-3--system-collector).
- **Production Ready**: No (Sequential phase progression).
- Consult [CURRENT_STATE.md](file:///home/devqii/workspace/nodepulse/CURRENT_STATE.md) for live tracking.

---

## Documentation Quick Links

- **Execution Policy**: [AGENTS.md](file:///home/devqii/workspace/nodepulse/AGENTS.md)
- **Implementation Plan**: [IMPLEMENTATION_PLAN.md](file:///home/devqii/workspace/nodepulse/IMPLEMENTATION_PLAN.md)
- **Decisions & ADRs**: [DECISIONS.md](file:///home/devqii/workspace/nodepulse/DECISIONS.md) & [docs/adr/](file:///home/devqii/workspace/nodepulse/docs/adr/)
- **Documentation Master Index**: [docs/INDEX.md](file:///home/devqii/workspace/nodepulse/docs/INDEX.md)
- **Product Requirements**: [docs/product/REQUIREMENTS.md](file:///home/devqii/workspace/nodepulse/docs/product/REQUIREMENTS.md)
- **Architecture Overview**: [docs/architecture/ARCHITECTURE.md](file:///home/devqii/workspace/nodepulse/docs/architecture/ARCHITECTURE.md)
- **API Specification**: [docs/api/API.md](file:///home/devqii/workspace/nodepulse/docs/api/API.md)
- **Engineering & Build**: [docs/engineering/BUILD.md](file:///home/devqii/workspace/nodepulse/docs/engineering/BUILD.md)
- **Operations & Systemd**: [docs/operations/DEPLOYMENT.md](file:///home/devqii/workspace/nodepulse/docs/operations/DEPLOYMENT.md) & [docs/operations/SYSTEMD.md](file:///home/devqii/workspace/nodepulse/docs/operations/SYSTEMD.md)

# nodepulse
