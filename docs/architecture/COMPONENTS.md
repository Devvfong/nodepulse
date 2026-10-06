# NodePulse — Subsystem & Component Catalog

This document catalogues all architectural components, header definitions, source file locations, and responsibilities across the NodePulse agent codebase.

---

## Component Inventory

### 1. Application Entry Point
- **`nodepulse_server`** (`apps/server/main.cpp`)
  - Parses command-line arguments (`--config <path>`, `--validate-config`, `--version`).
  - Initializes logging subsystem (`spdlog`).
  - Loads configuration JSON and environment variables.
  - Registers Drogon controllers, filters, and routes.
  - Installs OS signal handlers (`SIGINT`, `SIGTERM`) for clean shutdown.
  - Starts the Drogon event loop.

---

### 2. Middleware Subsystem (`src/middleware/`)

| Component | Header Path | Implementation Path | Purpose |
|---|---|---|---|
| **`AuthFilter`** | `include/nodepulse/middleware/auth_filter.hpp` | `src/middleware/auth_filter.cpp` | Validates `X-API-Key` using constant-time comparison. Bypasses `/api/v1/health`. |
| **`RateLimitFilter`** | `include/nodepulse/middleware/rate_limit_filter.hpp` | `src/middleware/rate_limit_filter.cpp` | Token-bucket rate limiter tracking requests per client IP. Returns 429 when exhausted. |

---

### 3. Controller Subsystem (`src/controllers/`)

| Controller | Header Path | Routes Handled | Target Service |
|---|---|---|---|
| **`HealthController`** | `include/nodepulse/controllers/health_controller.hpp` | `GET /api/v1/health` | Application lifecycle |
| **`SystemController`** | `include/nodepulse/controllers/system_controller.hpp` | `GET /api/v1/system` | `SystemService` |
| **`CpuController`** | `include/nodepulse/controllers/cpu_controller.hpp` | `GET /api/v1/cpu` | `CpuService` |
| **`MemoryController`** | `include/nodepulse/controllers/memory_controller.hpp` | `GET /api/v1/memory` | `MemoryService` |
| **`DiskController`** | `include/nodepulse/controllers/disk_controller.hpp` | `GET /api/v1/disks` | `DiskService` |
| **`NetworkController`** | `include/nodepulse/controllers/network_controller.hpp` | `GET /api/v1/network` | `NetworkService` |
| **`ProcessController`** | `include/nodepulse/controllers/process_controller.hpp` | `GET /api/v1/processes`<br>`GET /api/v1/processes/{pid}` | `ProcessService` |
| **`ServiceController`** | `include/nodepulse/controllers/service_controller.hpp` | `GET /api/v1/services`<br>`GET /api/v1/services/{name}` | `ServiceManagerService` |
| **`DockerController`** | `include/nodepulse/controllers/docker_controller.hpp` | `GET /api/v1/containers`<br>`GET /api/v1/containers/{id}` | `DockerService` |
| **`EventsController`** | `include/nodepulse/controllers/events_controller.hpp` | `GET /api/v1/events` | `StreamService` |
| **`MetricsController`** | `include/nodepulse/controllers/metrics_controller.hpp` | `GET /metrics` | `MetricsService` |

---

### 4. Service Subsystem (`src/services/`)

| Service | Header Path | Implementation Path | Coordinates |
|---|---|---|---|
| **`SystemService`** | `include/nodepulse/services/system_service.hpp` | `src/services/system_service.cpp` | `SystemCollector` |
| **`CpuService`** | `include/nodepulse/services/cpu_service.hpp` | `src/services/cpu_service.cpp` | `CpuCollector` (samples jiffies & deltas) |
| **`MemoryService`** | `include/nodepulse/services/memory_service.hpp` | `src/services/memory_service.cpp` | `MemoryCollector` |
| **`DiskService`** | `include/nodepulse/services/disk_service.hpp` | `src/services/disk_service.cpp` | `DiskCollector` (filters virtual mounts) |
| **`NetworkService`** | `include/nodepulse/services/network_service.hpp` | `src/services/network_service.cpp` | `NetworkCollector` (bandwidth rates) |
| **`ProcessService`** | `include/nodepulse/services/process_service.hpp` | `src/services/process_service.cpp` | `ProcessCollector` (sorting & bounds) |
| **`ServiceManagerService`** | `include/nodepulse/services/service_manager_service.hpp` | `src/services/service_manager_service.cpp` | `ServiceCollector` (systemd D-Bus) |
| **`DockerService`** | `include/nodepulse/services/docker_service.hpp` | `src/services/docker_service.cpp` | `DockerCollector` (daemon availability) |
| **`StreamService`** | `include/nodepulse/services/stream_service.hpp` | `src/services/stream_service.cpp` | Broadcasts `MetricPulse` to SSE clients |
| **`MetricsService`** | `include/nodepulse/services/metrics_service.hpp` | `src/services/metrics_service.cpp` | Serializes metrics into Prometheus format |

---

### 5. Collector Subsystem (`src/collectors/`)

| Collector | Header Path | Implementation Path | Linux Data Source |
|---|---|---|---|
| **`SystemCollector`** | `include/nodepulse/collectors/system_collector.hpp` | `src/collectors/system_collector.cpp` | `/etc/os-release`, `/proc/uptime`, `uname`, `gethostname` |
| **`CpuCollector`** | `include/nodepulse/collectors/cpu_collector.hpp` | `src/collectors/cpu_collector.cpp` | `/proc/stat`, `/proc/loadavg`, `/proc/cpuinfo` |
| **`MemoryCollector`** | `include/nodepulse/collectors/memory_collector.hpp` | `src/collectors/memory_collector.cpp` | `/proc/meminfo` |
| **`DiskCollector`** | `include/nodepulse/collectors/disk_collector.hpp` | `src/collectors/disk_collector.cpp` | `/proc/mounts`, `statvfs()` |
| **`NetworkCollector`** | `include/nodepulse/collectors/network_collector.hpp` | `src/collectors/network_collector.cpp` | `/proc/net/dev`, `/sys/class/net/` |
| **`ProcessCollector`** | `include/nodepulse/collectors/process_collector.hpp` | `src/collectors/process_collector.cpp` | `/proc/[0-9]+/stat`, `status`, `cmdline`, `fd/` |
| **`ServiceCollector`** | `include/nodepulse/collectors/service_collector.hpp` | `src/collectors/service_collector.cpp` | systemd D-Bus (`sd-bus`) |
| **`DockerCollector`** | `include/nodepulse/collectors/docker_collector.hpp` | `src/collectors/docker_collector.cpp` | `/var/run/docker.sock` via `libcurl` |

---

### 6. Repositories Subsystem (`src/repositories/`) (Phase 14)
- **`PostgresRepository`** (`include/nodepulse/repositories/postgres_repository.hpp`, `src/repositories/postgres_repository.cpp`)
  - Encapsulates `libpqxx` connection pool and prepared statements for persisting periodic metric snapshots into `host_metrics` table.

---

### 7. Utilities Subsystem (`src/utils/`)
- **`Logger`** (`include/nodepulse/utils/logger.hpp`, `src/utils/logger.cpp`): Wrapper configuring `spdlog` for asynchronous stdout and file logging with automatic credential redaction.
- **`ConfigManager`** (`include/nodepulse/utils/config_manager.hpp`, `src/utils/config_manager.cpp`): Reads JSON config, applies environment variables, and validates schema constraints.
- **`ErrorResponse`** (`include/nodepulse/utils/error_response.hpp`, `src/utils/error_response.cpp`): Helper generating standardized JSON error envelopes.
- **`SecurityUtils`** (`include/nodepulse/utils/security_utils.hpp`, `src/utils/security_utils.cpp`): Constant-time string equality checker (`constant_time_equals`).
- **`TimeUtils`** (`include/nodepulse/utils/time_utils.hpp`, `src/utils/time_utils.cpp`): Formats UTC ISO 8601 strings and measures monotonic intervals.

