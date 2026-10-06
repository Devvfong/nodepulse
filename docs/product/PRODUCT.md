# NodePulse — Product Definition & Vision

## 1. Product Vision & Mission

**NodePulse** is an ultra-lightweight, production-grade Linux server monitoring and management agent written in **C++20** using the **Drogon** asynchronous framework. 

Traditional server monitoring agents often incur heavy runtime overhead, requiring multi-hundred megabyte runtimes (Java, Python, or bloated Go binaries) and complex dependency trees. NodePulse provides a modern, high-throughput, native C++ alternative that interfaces directly with the Linux kernel's virtual filesystems (`/proc` and `/sys`), exposing an idiomatic HTTP/JSON REST API, live Server-Sent Events (SSE) streaming, and Prometheus exposition format with near-zero resource impact.

---

## 2. Target Personas

### 2.1 Site Reliability Engineers (SRE) & Platform Operators
- **Needs**: High reliability, minimal CPU/memory footprint, predictable scraping latency, standard Prometheus integration, and hardened systemd isolation.
- **Pain Point Addressed**: Heavy agents consuming significant CPU and memory on high-density VM fleets.

### 2.2 Systems & C++ Developers
- **Needs**: Clean, modular, idiomatic C++20 reference architecture demonstrating asynchronous networking, RAII, constant-time security routines, and kernel `/proc` parsing.
- **Pain Point Addressed**: Lack of production-quality modern C++ examples for Linux systems management agents.

### 2.3 Edge & Embedded Linux Engineers
- **Needs**: Compact binary size, no heavy runtime interpreter dependencies, read-only host safety, and robust operation in resource-constrained environments.
- **Pain Point Addressed**: Heavy container-based monitoring solutions that cannot run on edge gateways or low-RAM instances.

---

## 3. Core Value Propositions

1. **Native Performance & Efficiency**:
   Written in compiled C++20 with Drogon's non-blocking epoll event loop, achieving sub-millisecond parsing and less than 35MB resident memory (RSS).
2. **Transparent Kernel Telemetry**:
   Directly parses `/proc/stat`, `/proc/meminfo`, `/proc/loadavg`, `/proc/net/dev`, `/proc/mounts`, and `/proc/<pid>` without spawning subprocesses or shell pipelines (`cat | awk | grep`).
3. **Multi-Modal Data Delivery**:
   Provides standard REST JSON responses for interactive queries, continuous real-time Server-Sent Events (SSE) for web dashboards, and standard text exposition for Prometheus scrapers.
4. **Security by Default**:
   Header-based API key authentication (`X-API-Key`), in-memory token-bucket rate limiting, non-root systemd sandboxing, and zero credential logging.
5. **Clear Operational Boundaries**:
   Read-only observability agent. Zero arbitrary remote code execution (RCE) or dangerous host modification capabilities.

---

## 4. Product Scope Boundaries

### 4.1 In-Scope Capabilities
- System host metadata identification (`hostname`, OS release, kernel version, boot uptime).
- Real-time CPU metrics: Aggregate and per-core utilization percentages, load averages.
- Real-time memory & swap metrics: Total, free, available, cached, buffers, and swap usage.
- Disk & filesystem inspection: Mounted block devices, total, free, available capacity via `statvfs()`.
- Network interface telemetry: RX/TX byte and packet counters, error rates, interface status.
- Process inspection: Process table enumeration, per-PID CPU/RAM consumption, cmdline parsing.
- Systemd service enumeration: Active, enabled, failed unit status via systemd D-Bus interface.
- Optional Docker container inspection: Querying `/var/run/docker.sock` via `libcurl`.
- Live event streaming via Server-Sent Events (`GET /api/v1/events`).
- Prometheus scrape endpoint (`GET /metrics`).
- Hardened systemd service management and configuration schema validation.
- Optional metric persistence to PostgreSQL (Phase 14).

### 4.2 Out-of-Scope Capabilities
- **Host Mutation / Remote Code Execution**: NodePulse will not execute arbitrary shell commands or alter system configurations.
- **Process Termination**: No `kill -9` or process management endpoints are provided in v1 to preserve read-only host safety.
- **Container Lifecycle Mutation**: No container start/stop/remove capabilities via Docker socket.
- **Full Application Performance Monitoring (APM)**: No language bytecode instrumentation, distributed tracing (OpenTelemetry span injection), or profilers.
- **Multi-Tenant User Management**: No multi-tenant RBAC or OAuth2 identity provider functionality (single API-key model per host node).

