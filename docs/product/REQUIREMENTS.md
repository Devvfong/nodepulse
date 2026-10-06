# NodePulse — Functional Requirements Specification

This document details the complete functional requirements catalog for NodePulse. Each requirement is assigned a stable identifier (`FR-xxx`) referenced across architectural designs, tests, and implementation phases.

---

## Requirements Summary Matrix

| ID | Title | Priority | Phase | Target Endpoint / Mechanism |
|---|---|---|---|---|
| **FR-001** | Agent Health & Readiness Check | P0 | Phase 2 | `GET /api/v1/health` |
| **FR-002** | System Host Identification & Telemetry | P0 | Phase 3 | `GET /api/v1/system` |
| **FR-003** | CPU Utilization & Load Average | P0 | Phase 4 | `GET /api/v1/cpu` |
| **FR-004** | Memory & Swap Telemetry | P0 | Phase 5 | `GET /api/v1/memory` |
| **FR-005** | Filesystem & Disk Capacity Inspection | P0 | Phase 6 | `GET /api/v1/disks` |
| **FR-006** | Network Interface Metrics & Bandwidth | P0 | Phase 7 | `GET /api/v1/network` |
| **FR-007** | Process Table Enumeration | P1 | Phase 8 | `GET /api/v1/processes` |
| **FR-008** | Detailed Process Inspection by PID | P1 | Phase 8 | `GET /api/v1/processes/{pid}` |
| **FR-009** | Systemd Service Enumeration | P1 | Phase 8 | `GET /api/v1/services` |
| **FR-010** | Detailed Systemd Service Status | P1 | Phase 8 | `GET /api/v1/services/{name}` |
| **FR-011** | Docker Container Enumeration | P2 | Phase 11 | `GET /api/v1/containers` |
| **FR-012** | Detailed Docker Container Inspection | P2 | Phase 11 | `GET /api/v1/containers/{id}` |
| **FR-013** | Server-Sent Events Live Telemetry Stream | P1 | Phase 12 | `GET /api/v1/events` |
| **FR-014** | Prometheus Metrics Exposition | P0 | Phase 13 | `GET /metrics` |
| **FR-015** | Historical Metric Persistence to PostgreSQL | P2 | Phase 14 | Database background worker |
| **FR-016** | Header-Based API Key Authentication | P0 | Phase 9 | `X-API-Key` middleware filter |
| **FR-017** | Configurable Rate Limiting | P1 | Phase 10 | Token-bucket middleware filter |
| **FR-018** | Standardized JSON Error Responses | P0 | Phase 2 | Uniform error response builder |
| **FR-019** | Configuration Loading and Validation | P0 | Phase 2, 15 | File & ENV config loader |
| **FR-020** | Graceful Lifecycle & Signal Handling | P0 | Phase 2, 15 | `SIGINT`/`SIGTERM` traps |

---

## Detailed Functional Requirements

### FR-001: Agent Health & Readiness Check
- **Description**: The agent must provide an unauthenticated HTTP health check endpoint.
- **Specification**:
  - `GET /api/v1/health`
  - Returns HTTP 200 with status `"healthy"`, agent version, and uptime seconds.
  - Must respond promptly without executing blocking disk or network scans.
  - Exempt from API-key authentication to support load balancers and systemd health checks.

### FR-002: System Host Identification & Telemetry
- **Description**: The agent must identify host operating system, kernel, and machine architecture.
- **Specification**:
  - `GET /api/v1/system`
  - Reads hostname via `gethostname()`, OS distribution from `/etc/os-release`, kernel release/version and architecture via `uname()`, and system uptime from `/proc/uptime`.
  - Returns JSON containing: `hostname`, `os_name`, `os_version`, `kernel_version`, `architecture`, `boot_time_utc`, `uptime_seconds`.

### FR-003: CPU Utilization & Load Average
- **Description**: The agent must collect aggregate and per-core CPU utilization and system load averages.
- **Specification**:
  - `GET /api/v1/cpu`
  - Parses cumulative CPU jiffies from `/proc/stat` and calculates utilization percentage over sampling deltas.
  - Parses `/proc/loadavg` for 1-minute, 5-minute, and 15-minute load averages.
  - Parses `/proc/cpuinfo` for CPU model name, physical core count, and logical thread count.
  - Returns JSON containing: `usage_percent`, `cores` (array of core utilization percentages), `load_average` (`1min`, `5min`, `15min`), `model_name`, `core_count`.

### FR-004: Memory & Swap Telemetry
- **Description**: The agent must report RAM and swap memory allocation and utilization.
- **Specification**:
  - `GET /api/v1/memory`
  - Parses `/proc/meminfo` fields: `MemTotal`, `MemFree`, `MemAvailable`, `Buffers`, `Cached`, `SwapTotal`, `SwapFree`.
  - Calculates: `used_bytes` ($total - available$), `free_bytes`, `available_bytes`, `usage_percent`, `swap_total_bytes`, `swap_used_bytes`, `swap_free_bytes`, `swap_usage_percent`.
  - Returns values normalized to integer bytes and percentages with 2-decimal precision.

### FR-005: Filesystem & Disk Capacity Inspection
- **Description**: The agent must discover mounted filesystems and report storage utilization.
- **Specification**:
  - `GET /api/v1/disks`
  - Parses `/proc/mounts`, filtering out pseudo-filesystems (`proc`, `sysfs`, `cgroup`, `devpts`, `overlay`, `tmpfs` if configured).
  - Invokes POSIX `statvfs()` on mount points to compute total blocks, free blocks, available blocks, and inodes.
  - Returns JSON array of partitions with `filesystem`, `mount_point`, `fstype`, `total_bytes`, `used_bytes`, `free_bytes`, `usage_percent`.

### FR-006: Network Interface Metrics & Bandwidth
- **Description**: The agent must inspect network interfaces, link states, and cumulative traffic counters.
- **Specification**:
  - `GET /api/v1/network`
  - Parses `/proc/net/dev` for RX/TX bytes, packets, errors, and drops across all interfaces.
  - Queries `/sys/class/net/<iface>/operstate` and `/sys/class/net/<iface>/speed` where available.
  - Returns JSON array with interface name, MAC address, operational state (`up`/`down`), `rx_bytes`, `tx_bytes`, `rx_packets`, `tx_packets`, `rx_errors`, `tx_errors`.

### FR-007: Process Table Enumeration
- **Description**: The agent must enumerate running processes on the host.
- **Specification**:
  - `GET /api/v1/processes`
  - Supports query parameters: `limit` (default 50, max 200), `sort` (`cpu`, `memory`, `pid`).
  - Scans `/proc/[0-9]+` directories reading `/proc/<pid>/stat`, `status`, and `cmdline`.
  - Returns JSON array containing `pid`, `name`, `user`, `state` (R, S, D, Z, T), `cpu_percent`, `memory_rss_bytes`, `cmdline`.

### FR-008: Detailed Process Inspection by PID
- **Description**: The agent must return detailed telemetry for a single process specified by PID.
- **Specification**:
  - `GET /api/v1/processes/{pid}`
  - Validates that `{pid}` is a strictly positive integer (`pid >= 1`). If runtime upper-bound validation is performed, Linux `/proc/sys/kernel/pid_max` is authoritative rather than any hard-coded application constant.
  - Returns 404 `RESOURCE_NOT_FOUND` if the PID is not present or exits during read.
  - Returns JSON containing deep process metrics: parent PID (`ppid`), thread count, open file descriptor count (`/proc/<pid>/fd`), memory virtual size, resident set size, start time.

### FR-009: Systemd Service Enumeration
- **Description**: The agent must enumerate active, loaded, and failed systemd services on the host.
- **Specification**:
  - `GET /api/v1/services`
  - Supports query parameters: `state` (optional filter: `active`, `inactive`, `failed`, `all`; default: `all`), `limit` (optional integer: 1 to 200; default: 50).
  - Connects to systemd via D-Bus (`sd-bus`) to list unit files of type `.service`.
  - Returns JSON array containing: `name`, `description`, `load_state`, `active_state`, `sub_state`, `unit_file_state`.

### FR-010: Detailed Systemd Service Status
- **Description**: The agent must provide deep inspection for a specific named systemd service.
- **Specification**:
  - `GET /api/v1/services/{name}`
  - Path parameter `{name}`: service unit name (e.g. `nodepulse.service`). Automatically normalizes name by appending `.service` suffix if omitted.
  - Validates unit name against regex `^[a-zA-Z0-9_\-\.\@]+$`; malformed names return 400 `INVALID_REQUEST`.
  - Returns 404 `RESOURCE_NOT_FOUND` if the unit does not exist in systemd.
  - Returns JSON containing: `name`, `description`, `load_state`, `active_state`, `sub_state`, `unit_file_state`, `main_pid`, `restart_count`, `active_enter_timestamp_utc`, `memory_current_bytes`.

### FR-011: Docker Container Enumeration
- **Description**: The agent must list Docker containers running on the host when Docker integration is enabled.
- **Specification**:
  - `GET /api/v1/containers`
  - Communicates with `/var/run/docker.sock` over Unix domain socket via `libcurl`.
  - Returns 503 `DOCKER_UNAVAILABLE` if the Docker daemon is offline or socket inaccessible.
  - Returns JSON array containing container ID, names, image, created timestamp, status, and ports.

### FR-012: Detailed Docker Container Inspection
- **Description**: The agent must return detailed configuration and resource metrics for a single container.
- **Specification**:
  - `GET /api/v1/containers/{id}`
  - Queries `/containers/{id}/json` from the Docker API.
  - Returns 404 `RESOURCE_NOT_FOUND` if container ID is unknown.
  - Returns JSON with container network settings, mounts, environment variables (with secret redaction), restart policy, state.

### FR-013: Server-Sent Events Live Telemetry Stream
- **Description**: The agent must provide a real-time, unidirectional stream of telemetry pulses to HTTP clients.
- **Specification**:
  - `GET /api/v1/events`
  - Responds with `Content-Type: text/event-stream; charset=utf-8` and `Cache-Control: no-cache`.
  - Emits JSON payloads periodically (configurable interval, default 1 second) under the `metric_pulse` event name.
  - Payload contains aggregated CPU %, memory used %, and network delta bytes.
  - Cleanly handles client disconnection without leaking sockets or thread starvation.

### FR-014: Prometheus Metrics Exposition
- **Description**: The agent must expose system and application metrics in standard Prometheus text exposition format.
- **Specification**:
  - `GET /metrics`
  - Authentication: Secure by default; requires `X-API-Key` unless explicitly configured via `prometheus.require_auth: false`.
  - Content-Type: `text/plain; version=0.0.4; charset=utf-8`.
  - Exposes standard gauges and counters:
    - Host CPU, memory, disk, network counters.
    - NodePulse internal metrics: HTTP request counts by status, endpoint latency histograms, collector duration gauges.

### FR-015: Historical Metric Persistence to PostgreSQL
- **Description**: The agent must optionally persist periodic metric snapshots to a PostgreSQL database (Phase 14).
- **Specification**:
  - Asynchronous background worker using `libpqxx`.
  - Batch inserts periodic host metric snapshots.
  - Database downtime must never fail or block the HTTP REST endpoints.

### FR-016: Header-Based API Key Authentication
- **Description**: The agent must authenticate incoming HTTP requests via the `X-API-Key` header.
- **Specification**:
  - Drogon filter intercepts all requests except `/api/v1/health`.
  - Enforces authentication across all `/api/v1` routes and `/metrics` (by default).
  - Header `X-API-Key` compared in constant time against configured secret.
  - Missing or incorrect key yields HTTP 401 `UNAUTHORIZED`.
  - Keys must be loaded from config file or `NODEPULSE_API_KEY` environment variable.

### FR-017: Configurable Rate Limiting
- **Description**: The agent must enforce request rate limits per client IP address.
- **Specification**:
  - Token-bucket algorithm with configurable requests per second and burst size.
  - Exceeding limit yields HTTP 429 `RATE_LIMITED` with `Retry-After` header.

### FR-018: Standardized JSON Error Responses
- **Description**: All API error responses must adhere to a standardized JSON schema.
- **Specification**:
  - Standard JSON envelope: `{ "error": { "code": string, "message": string, "timestamp": string, "details": [] } }`.
  - Uniform across all controllers and middlewares.

### FR-019: Configuration Loading and Validation
- **Description**: The agent must support configuration files and environment variable overrides.
- **Specification**:
  - Loads JSON config from `/etc/nodepulse/config.json` or path provided via `--config`.
  - Supports `--validate-config` flag to test syntax without starting server.
  - Environment variables override JSON keys (`NODEPULSE_PORT`, `NODEPULSE_API_KEY`).

### FR-020: Graceful Lifecycle & Signal Handling
- **Description**: The agent must handle termination signals cleanly.
- **Specification**:
  - Listens for `SIGINT` and `SIGTERM`.
  - Closes listening sockets, terminates active SSE streams gracefully, flushes logs, and exits with code 0 within 5 seconds.

