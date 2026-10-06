# NodePulse — Use Cases Specification

This document details all primary system use cases (`UC-001` to `UC-015`) describing interactions between human operators, monitoring systems, and the NodePulse agent.

---

## Use Case Index

| ID | Title | Primary Actor | Target Endpoint |
|---|---|---|---|
| **UC-001** | Liveness & Readiness Verification | Orchestrator / Load Balancer | `GET /api/v1/health` |
| **UC-002** | Host Identity & System Metadata Query | Operator / Inventory System | `GET /api/v1/system` |
| **UC-003** | CPU Utilization & Load Average Telemetry | Dashboard / Metrics Scraper | `GET /api/v1/cpu` |
| **UC-004** | Memory & Swap Consumption Auditing | SRE / Automation Tool | `GET /api/v1/memory` |
| **UC-005** | Filesystem Usage & Mount Inspection | Storage Auditor / Alert Engine | `GET /api/v1/disks` |
| **UC-006** | Network Interface Telemetry & Bandwidth | Network Monitor | `GET /api/v1/network` |
| **UC-007** | Process List Exploration & Ranking | SRE / Incident Responder | `GET /api/v1/processes` |
| **UC-008** | Deep Inspection of a Specific Process | SRE / Security Auditor | `GET /api/v1/processes/{pid}` |
| **UC-009** | Systemd Service State Inspection | SRE / Configuration Agent | `GET /api/v1/services` |
| **UC-010** | Docker Container Inspection | SRE / Container Auditor | `GET /api/v1/containers` |
| **UC-011** | Real-Time Telemetry Streaming via SSE | Web Dashboard / Terminal UI | `GET /api/v1/events` |
| **UC-012** | Prometheus Metrics Scraping | Prometheus / VictoriaMetrics | `GET /metrics` |
| **UC-013** | Unauthorized Request Rejection | Malicious Client / Scanner | Protected endpoints |
| **UC-014** | Rate Limit Mitigation under Traffic Bursts | Aggressive Client | Protected endpoints |
| **UC-015** | Historical Metric Archival to PostgreSQL | PostgreSQL Worker | Background DB Task |

---

## Detailed Use Cases

### UC-001: Liveness & Readiness Verification
- **Actor**: Container orchestrator, systemd health checker, or HAProxy load balancer.
- **Trigger**: Periodic health probe (e.g. every 5–15 seconds).
- **Preconditions**: NodePulse process is running and bound to listening port.
- **Main Success Scenario**:
  1. Client sends `GET /api/v1/health` without an `X-API-Key` header.
  2. Agent processes request immediately on event loop without disk scanning.
  3. Agent returns HTTP 200 with JSON payload `{"status": "healthy", "version": "1.0.0", "uptime_seconds": 3600}`.
- **Alternative Flow**: If internal core thread pool is deadlocked or server shutting down, connection is refused or returns 503 `SERVICE_UNAVAILABLE`.

### UC-002: Host Identity & System Metadata Query
- **Actor**: Fleet inventory manager or automation script.
- **Trigger**: Host provisioning verification or periodic inventory poll.
- **Preconditions**: Valid API key available in client request.
- **Main Success Scenario**:
  1. Client sends `GET /api/v1/system` with valid `X-API-Key` header.
  2. Agent validates API key via `AuthFilter`.
  3. `SystemService` retrieves host info from `SystemCollector`.
  4. Agent returns HTTP 200 with JSON containing hostname, OS distribution, kernel version, and boot time.
- **Alternative Flow**: If `X-API-Key` header is missing, agent returns HTTP 401 `UNAUTHORIZED`.

### UC-003: CPU Utilization & Load Average Telemetry
- **Actor**: Monitoring dashboard or auto-scaling daemon.
- **Trigger**: Telemetry refresh interval.
- **Preconditions**: Valid API key.
- **Main Success Scenario**:
  1. Client issues `GET /api/v1/cpu` with `X-API-Key`.
  2. `CpuService` samples jiffies from `CpuCollector` and calculates delta against prior sample.
  3. Agent returns HTTP 200 with aggregate CPU utilization, per-core percentages, and 1m/5m/15m load averages.
- **Alternative Flow**: If `/proc/stat` cannot be read due to file descriptor exhaustion, agent returns HTTP 500 `COLLECTOR_FAILURE`.

### UC-004: Memory & Swap Consumption Auditing
- **Actor**: SRE or memory leak alerting script.
- **Trigger**: Alert threshold trigger or routine monitoring.
- **Main Success Scenario**:
  1. Client issues `GET /api/v1/memory` with `X-API-Key`.
  2. `MemoryService` invokes `MemoryCollector` parsing `/proc/meminfo`.
  3. Agent calculates total, available, free, and swap metrics.
  4. Returns HTTP 200 with memory breakdown in bytes and usage percentages.

### UC-005: Filesystem Usage & Mount Inspection
- **Actor**: Disk capacity alerting daemon.
- **Trigger**: Hourly or daily storage audit.
- **Main Success Scenario**:
  1. Client issues `GET /api/v1/disks` with `X-API-Key`.
  2. `DiskCollector` reads `/proc/mounts`, ignores virtual filesystems, and calls `statvfs()` on active mountpoints.
  3. Agent returns HTTP 200 with partition list, mount paths, free/used bytes, and usage percentages.

### UC-006: Network Interface Telemetry & Bandwidth
- **Actor**: Network operations dashboard.
- **Trigger**: Periodic bandwidth query.
- **Main Success Scenario**:
  1. Client issues `GET /api/v1/network` with `X-API-Key`.
  2. `NetworkCollector` reads `/proc/net/dev` and `/sys/class/net`.
  3. Agent computes throughput rates across interfaces and returns HTTP 200 with byte/packet counters and interface operational states.

### UC-007: Process List Exploration & Ranking
- **Actor**: SRE diagnosing a high-CPU incident.
- **Trigger**: Investigating sluggish system response.
- **Main Success Scenario**:
  1. Operator issues `GET /api/v1/processes?sort=cpu&limit=20` with `X-API-Key`.
  2. `ProcessCollector` iterates `/proc/[0-9]+`, collects CPU/memory utilization, and ranks processes descending.
  3. Agent returns HTTP 200 with top 20 processes by CPU consumption.
- **Alternative Flow**: Invalid query parameter returns HTTP 400 `INVALID_REQUEST`.

### UC-008: Deep Inspection of a Specific Process
- **Actor**: Security auditor or software developer.
- **Trigger**: Investigating suspected rogue or crashing process PID.
- **Main Success Scenario**:
  1. Client issues `GET /api/v1/processes/1024` with `X-API-Key`.
  2. Agent validates PID is a valid integer.
  3. `ProcessCollector` reads `/proc/1024/stat`, `status`, `cmdline`, and count of open FDs.
  4. Returns HTTP 200 with full process metadata.
- **Alternative Flow**: If process 1024 does not exist, agent returns HTTP 404 `RESOURCE_NOT_FOUND`.

### UC-009: Systemd Service State Inspection
- **Actor**: Configuration management agent or SRE.
- **Trigger**: Verifying that essential daemon services are active.
- **Main Success Scenario**:
  1. Client issues `GET /api/v1/services` with `X-API-Key`.
  2. `ServiceCollector` queries systemd D-Bus interface.
  3. Agent returns HTTP 200 with list of active, loaded, and failed services.
- **Alternative Flow**: If querying a specific service via `GET /api/v1/services/nginx.service` that does not exist, returns HTTP 404 `RESOURCE_NOT_FOUND`.

### UC-010: Docker Container Inspection
- **Actor**: Container platform administrator.
- **Trigger**: Container inventory synchronization.
- **Main Success Scenario**:
  1. Client issues `GET /api/v1/containers` with `X-API-Key`.
  2. `DockerCollector` communicates with `/var/run/docker.sock` via `libcurl`.
  3. Agent returns HTTP 200 with list of running and stopped containers.
- **Alternative Flow**: If Docker daemon is stopped, returns HTTP 503 `DOCKER_UNAVAILABLE`.

### UC-011: Real-Time Telemetry Streaming via SSE
- **Actor**: Web frontend or terminal dashboard (`curl -N`).
- **Trigger**: User opens real-time live monitoring view.
- **Main Success Scenario**:
  1. Client establishes connection to `GET /api/v1/events` with `X-API-Key`.
  2. Agent accepts connection, issues `Content-Type: text/event-stream`, and begins periodic event loop pulse.
  3. Every 1000ms, agent pushes `event: metric_pulse\ndata: {...}\n\n`.
  4. Client terminates connection; agent cleans up streaming channel cleanly without leaking resources.

### UC-012: Prometheus Metrics Scraping
- **Actor**: Prometheus scraper.
- **Trigger**: Periodic scrape interval (e.g. every 15s).
- **Main Success Scenario**:
  1. Scraper sends `GET /metrics`.
  2. Agent generates Prometheus text exposition formatted string of gauges and counters.
  3. Returns HTTP 200 with `Content-Type: text/plain; version=0.0.4`.

### UC-013: Unauthorized Request Rejection
- **Actor**: Unauthorized client or automated scanner.
- **Trigger**: Request without valid authorization header.
- **Main Success Scenario**:
  1. Client sends `GET /api/v1/system` with missing or incorrect `X-API-Key`.
  2. `AuthFilter` intercepts request, performs constant-time comparison, and fails authentication.
  3. Agent writes security warning to log (omitting supplied key string).
  4. Agent returns HTTP 401 `UNAUTHORIZED` with standard JSON error envelope.

### UC-014: Rate Limit Mitigation under Traffic Bursts
- **Actor**: Buggy client loop or denial-of-service attempt.
- **Trigger**: Sending requests exceeding configured requests-per-second.
- **Main Success Scenario**:
  1. Client sends rapid succession of requests exceeding token bucket depth.
  2. `RateLimitFilter` checks tokens for client IP and detects exhaustion.
  3. Agent returns HTTP 429 `RATE_LIMITED` with `Retry-After: 1` header and standard JSON error envelope.

### UC-015: Historical Metric Archival to PostgreSQL
- **Actor**: NodePulse internal background worker.
- **Trigger**: Configured snapshot timer (e.g. every 60s).
- **Preconditions**: `ENABLE_POSTGRES` enabled in build and valid database connection string configured.
- **Main Success Scenario**:
  1. Snapshot timer fires on background thread.
  2. Collector retrieves system, CPU, memory, and disk telemetry.
  3. `PostgresRepository` opens transaction and inserts snapshot record into `host_metrics` table.
- **Alternative Flow**: If database connection is dropped, error is logged and snapshot dropped; HTTP API operations continue unaffected.

