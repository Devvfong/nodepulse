# NodePulse — REST & Streaming API Specification

This document defines the complete API contracts for NodePulse under the initial namespace `/api/v1` and Prometheus exposition route `/metrics`.

---

## Endpoint Summary

| Method | Path | Authentication | Description |
|---|---|---|---|
| `GET` | `/api/v1/health` | None | Agent liveness and readiness probe |
| `GET` | `/api/v1/system` | `X-API-Key` | Host identity, kernel, OS, uptime |
| `GET` | `/api/v1/cpu` | `X-API-Key` | CPU utilization, cores, load averages |
| `GET` | `/api/v1/memory` | `X-API-Key` | RAM and swap utilization metrics |
| `GET` | `/api/v1/disks` | `X-API-Key` | Mounted filesystems and disk capacities |
| `GET` | `/api/v1/network` | `X-API-Key` | Network interfaces and traffic counters |
| `GET` | `/api/v1/processes` | `X-API-Key` | Process table enumeration |
| `GET` | `/api/v1/processes/{pid}` | `X-API-Key` | Detailed inspection of a single process |
| `GET` | `/api/v1/services` | `X-API-Key` | Systemd service unit list |
| `GET` | `/api/v1/services/{name}` | `X-API-Key` | Detailed systemd service unit status |
| `GET` | `/api/v1/containers` | `X-API-Key` | Docker containers list (optional) |
| `GET` | `/api/v1/containers/{id}` | `X-API-Key` | Detailed Docker container inspection |
| `GET` | `/api/v1/events` | `X-API-Key` | Server-Sent Events (SSE) telemetry stream |
| `GET` | `/metrics` | `X-API-Key` (Secure-by-default; unauthenticated only when explicitly configured) | Prometheus metrics exposition |

---

## Detailed Endpoint Specifications

### 1. Health Probe
- **`GET /api/v1/health`**
- **Auth**: None
- **Response 200 OK**:
  ```json
  {
    "status": "healthy",
    "version": "1.0.0",
    "uptime_seconds": 12450.5
  }
  ```

---

### 2. System Host Information
- **`GET /api/v1/system`**
- **Auth**: `X-API-Key` required
- **Response 200 OK**:
  ```json
  {
    "hostname": "prod-app-node-01",
    "os_name": "Ubuntu 22.04.4 LTS",
    "os_version": "22.04",
    "kernel_version": "5.15.0-105-generic",
    "architecture": "x86_64",
    "boot_time_utc": 1728211200,
    "uptime_seconds": 86400.0
  }
  ```
- **Error Responses**:
  - `401 UNAUTHORIZED`: If `X-API-Key` is missing or invalid.
  - `500 COLLECTOR_FAILURE`: If system metrics cannot be collected due to `/proc` reading failure.

---

### 3. CPU Telemetry
- **`GET /api/v1/cpu`**
- **Auth**: `X-API-Key` required
- **Response 200 OK**:
  ```json
  {
    "usage_percent": 18.45,
    "model_name": "AMD EPYC 7763 64-Core Processor",
    "physical_cores": 4,
    "logical_cores": 8,
    "load_average": {
      "one_minute": 0.45,
      "five_minute": 0.62,
      "fifteen_minute": 0.58
    },
    "cores": [
      { "core_id": 0, "usage_percent": 22.1 },
      { "core_id": 1, "usage_percent": 14.8 }
    ]
  }
  ```

---

### 4. Memory Telemetry
- **`GET /api/v1/memory`**
- **Auth**: `X-API-Key` required
- **Response 200 OK**:
  ```json
  {
    "total_bytes": 16777216000,
    "free_bytes": 4194304000,
    "available_bytes": 10485760000,
    "buffers_bytes": 524288000,
    "cached_bytes": 5767168000,
    "usage_percent": 37.5,
    "swap_total_bytes": 8388608000,
    "swap_free_bytes": 8388608000,
    "swap_used_bytes": 0,
    "swap_usage_percent": 0.0
  }
  ```

---

### 5. Filesystem & Disks
- **`GET /api/v1/disks`**
- **Auth**: `X-API-Key` required
- **Response 200 OK**:
  ```json
  [
    {
      "filesystem": "/dev/nvme0n1p2",
      "mount_point": "/",
      "fstype": "ext4",
      "total_bytes": 536870912000,
      "used_bytes": 107374182400,
      "free_bytes": 429496729600,
      "usage_percent": 20.0,
      "inodes_total": 32768000,
      "inodes_free": 31200000
    }
  ]
  ```

---

### 6. Network Telemetry
- **`GET /api/v1/network`**
- **Auth**: `X-API-Key` required
- **Response 200 OK**:
  ```json
  [
    {
      "name": "eth0",
      "mac_address": "52:54:00:12:34:56",
      "operstate": "up",
      "speed_mbps": 10000,
      "rx_bytes": 1048576000,
      "tx_bytes": 2097152000,
      "rx_packets": 1200000,
      "tx_packets": 1500000,
      "rx_errors": 0,
      "tx_errors": 0,
      "rx_bytes_per_sec": 125000.0,
      "tx_bytes_per_sec": 340000.0
    }
  ]
  ```

---

### 7. Process Table
- **`GET /api/v1/processes?sort=cpu&limit=20`**
- **Auth**: `X-API-Key` required
- **Query Params**:
  - `sort`: `cpu` (default), `memory`, `pid`
  - `limit`: Integer `1` to `200` (default 50)
- **Response 200 OK**:
  ```json
  [
    {
      "pid": 1248,
      "name": "nodepulse_server",
      "user": "nodepulse",
      "state": "S",
      "cpu_percent": 0.8,
      "memory_rss_bytes": 34500000,
      "cmdline": "/usr/local/bin/nodepulse_server --config /etc/nodepulse/config.json"
    }
  ]
  ```

---

### 8. Single Process Inspection
- **`GET /api/v1/processes/{pid}`**
- **Auth**: `X-API-Key` required
- **Path Parameter**:
  - `pid`: Integer process identifier. Must be a strictly positive integer (`pid >= 1`). If runtime upper-bound validation is performed, Linux `/proc/sys/kernel/pid_max` is authoritative rather than any hard-coded application constant.
- **Response 200 OK**:
  ```json
  {
    "pid": 1248,
    "ppid": 1,
    "name": "nodepulse_server",
    "user": "nodepulse",
    "state": "S",
    "cpu_percent": 0.8,
    "memory_rss_bytes": 34500000,
    "memory_vms_bytes": 154000000,
    "thread_count": 8,
    "open_fd_count": 18,
    "start_time_epoch": 1728211250,
    "cmdline": "/usr/local/bin/nodepulse_server --config /etc/nodepulse/config.json",
    "working_directory": "/etc/nodepulse"
  }
  ```
- **Error Responses**:
  - `400 INVALID_REQUEST`: If PID is not a positive integer (e.g. `<= 0` or non-numeric).
  - `401 UNAUTHORIZED`: If `X-API-Key` is missing or invalid.
  - `404 RESOURCE_NOT_FOUND`: If PID does not exist or exited during collection.
  - `500 COLLECTOR_FAILURE`: If `/proc/<pid>` cannot be read due to kernel I/O failure.

---

### 9. Systemd Services
- **`GET /api/v1/services`**
- **Auth**: `X-API-Key` required
- **Query Params**:
  - `state`: Optional string filter. Allowed values: `active`, `inactive`, `failed`, `all` (default: `all`).
  - `limit`: Optional integer limit. Bounded between `1` and `200` (default: `50`).
- **Response 200 OK**:
  ```json
  [
    {
      "name": "nodepulse.service",
      "description": "NodePulse Linux Host Monitoring Agent",
      "load_state": "loaded",
      "active_state": "active",
      "sub_state": "running",
      "unit_file_state": "enabled"
    },
    {
      "name": "ssh.service",
      "description": "OpenBSD Secure Shell server",
      "load_state": "loaded",
      "active_state": "active",
      "sub_state": "running",
      "unit_file_state": "enabled"
    }
  ]
  ```
- **Error Responses**:
  - `400 INVALID_REQUEST`: If `state` filter or `limit` parameter is invalid.
  - `401 UNAUTHORIZED`: If `X-API-Key` is missing or invalid.
  - `500 COLLECTOR_FAILURE`: If systemd D-Bus communication fails.

---

### 10. Single Systemd Service
- **`GET /api/v1/services/{name}`**
- **Auth**: `X-API-Key` required
- **Path Parameter**:
  - `name`: Systemd unit name, e.g. `nodepulse.service`. If `.service` suffix is omitted, it is automatically appended. Must match regex `^[a-zA-Z0-9_\-\.\@]+$`.
- **Response 200 OK**:
  ```json
  {
    "name": "nodepulse.service",
    "description": "NodePulse Linux Host Monitoring Agent",
    "load_state": "loaded",
    "active_state": "active",
    "sub_state": "running",
    "unit_file_state": "enabled",
    "main_pid": 1248,
    "restart_count": 0,
    "active_enter_timestamp_utc": 1728211200,
    "memory_current_bytes": 34500000
  }
  ```
- **Error Responses**:
  - `400 INVALID_REQUEST`: If service name contains invalid characters.
  - `401 UNAUTHORIZED`: If `X-API-Key` is missing or invalid.
  - `404 RESOURCE_NOT_FOUND`: If unit does not exist in systemd.
  - `500 COLLECTOR_FAILURE`: If systemd D-Bus communication fails.

---

### 11. Docker Containers
- **`GET /api/v1/containers`**
- **Auth**: `X-API-Key` required
- **Response 200 OK**:
  ```json
  [
    {
      "id": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
      "names": ["/redis-cache"],
      "image": "redis:7-alpine",
      "status": "Up 3 days",
      "state": "running",
      "created": 1727952000
    }
  ]
  ```
- **Response 503 Service Unavailable**: If Docker daemon is stopped (`DOCKER_UNAVAILABLE`).

---

### 12. Single Docker Container
- **`GET /api/v1/containers/{id}`**
- **Auth**: `X-API-Key` required
- **Response 200 OK**:
  ```json
  {
    "id": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "name": "/redis-cache",
    "image": "redis:7-alpine",
    "status": "Up 3 days",
    "state": "running",
    "running": true,
    "exit_code": 0,
    "port_mappings": ["0.0.0.0:6379->6379/tcp"],
    "mount_sources": ["/var/data/redis"],
    "created": 1727952000
  }
  ```

---

### 13. Server-Sent Events Stream
- **`GET /api/v1/events`**
- **Auth**: `X-API-Key` required
- **Headers Returned**:
  - `Content-Type: text/event-stream; charset=utf-8`
  - `Cache-Control: no-cache`
  - `Connection: keep-alive`
- **Stream Format**:
  ```
  event: metric_pulse
  data: {"timestamp":"2026-10-06T06:53:50Z","cpu_usage_percent":14.2,"memory_usage_percent":38.1,"memory_used_bytes":6395000000,"network_rx_bytes_sec":12040.0,"network_tx_bytes_sec":45800.0}

  ```

---

### 14. Prometheus Metrics Scrape
- **`GET /metrics`**
- **Auth**: `X-API-Key` required by default (secure by default). Unauthenticated access is permitted ONLY when explicitly configured via `prometheus.require_auth: false` in `config.json`.
- **Content-Type**: `text/plain; version=0.0.4; charset=utf-8`
- **Example Response**:
  ```prometheus
  # HELP nodepulse_cpu_usage_ratio Current host CPU usage ratio (0.0 to 1.0)
  # TYPE nodepulse_cpu_usage_ratio gauge
  nodepulse_cpu_usage_ratio 0.1845
  # HELP nodepulse_memory_used_bytes Host RAM memory currently in use
  # TYPE nodepulse_memory_used_bytes gauge
  nodepulse_memory_used_bytes 6291456000
  # HELP nodepulse_http_requests_total Total number of HTTP requests processed
  # TYPE nodepulse_http_requests_total counter
  nodepulse_http_requests_total{endpoint="/api/v1/cpu",method="GET",status="200"} 42
  ```

