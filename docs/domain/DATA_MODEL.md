# NodePulse — Data Model Specification

This document details the strongly typed C++20 domain structures and relational database schemas used by NodePulse.

---

## 1. C++20 Domain Model Definitions

All domain structures reside in the `nodepulse::domain` namespace within `include/nodepulse/domain/`.

### 1.1 System Host Domain (`system_info.hpp`)
```cpp
namespace nodepulse::domain {

struct SystemInfo {
    std::string hostname;
    std::string os_name;           // e.g. "Ubuntu 22.04.4 LTS"
    std::string os_version;        // e.g. "22.04"
    std::string kernel_version;    // e.g. "5.15.0-105-generic"
    std::string architecture;      // e.g. "x86_64"
    uint64_t boot_time_utc{0};     // Epoch seconds
    double uptime_seconds{0.0};    // Seconds from /proc/uptime
};

} // namespace nodepulse::domain
```

### 1.2 CPU Telemetry Domain (`cpu_info.hpp`)
```cpp
namespace nodepulse::domain {

struct LoadAverage {
    double one_minute{0.0};
    double five_minute{0.0};
    double fifteen_minute{0.0};
};

struct CpuCoreMetrics {
    uint32_t core_id{0};
    std::optional<double> usage_percent{std::nullopt};
    uint64_t user_jiffies{0};
    uint64_t system_jiffies{0};
    uint64_t idle_jiffies{0};
    uint64_t iowait_jiffies{0};
};

struct CpuMetrics {
    std::optional<double> usage_percent{std::nullopt};
    LoadAverage load_average;
    std::string model_name;
    uint32_t physical_cores{0};
    uint32_t logical_cores{0};
    std::vector<CpuCoreMetrics> cores;
    std::string measurement_status{"warming_up"}; // "warming_up", "ready", "cached"
};

} // namespace nodepulse::domain
```

### 1.3 Memory Telemetry Domain (`memory_info.hpp`)
```cpp
namespace nodepulse::domain {

struct MemoryMetrics {
    uint64_t total_bytes{0};
    uint64_t free_bytes{0};
    uint64_t available_bytes{0};
    uint64_t buffers_bytes{0};
    uint64_t cached_bytes{0};
    double usage_percent{0.0};

    uint64_t swap_total_bytes{0};
    uint64_t swap_free_bytes{0};
    uint64_t swap_used_bytes{0};
    double swap_usage_percent{0.0};
};

} // namespace nodepulse::domain
```

### 1.4 Disk & Filesystem Domain (`disk_info.hpp`)
```cpp
namespace nodepulse::domain {

struct DiskPartitionMetrics {
    std::string filesystem;     // e.g. "/dev/nvme0n1p2"
    std::string mount_point;    // e.g. "/"
    std::string fstype;         // e.g. "ext4"
    uint64_t total_bytes{0};
    uint64_t used_bytes{0};
    uint64_t free_bytes{0};
    double usage_percent{0.0};
    uint64_t inodes_total{0};
    uint64_t inodes_free{0};
};

} // namespace nodepulse::domain
```

### 1.5 Network Telemetry Domain (`network_info.hpp`)
```cpp
namespace nodepulse::domain {

struct NetworkInterfaceMetrics {
    std::string name;           // e.g. "eth0"
    std::string mac_address;
    std::string operstate;      // "up", "down", "unknown"
    uint64_t speed_mbps{0};
    uint64_t rx_bytes{0};
    uint64_t tx_bytes{0};
    uint64_t rx_packets{0};
    uint64_t tx_packets{0};
    uint64_t rx_errors{0};
    uint64_t tx_errors{0};
    uint64_t rx_drops{0};
    uint64_t tx_drops{0};
    double rx_bytes_per_sec{0.0};
    double tx_bytes_per_sec{0.0};
};

} // namespace nodepulse::domain
```

### 1.6 Process Subsystem Domain (`process_info.hpp`)
```cpp
namespace nodepulse::domain {

struct ProcessInfo {
    int32_t pid{0};
    std::string name;
    std::string user;
    char state{'R'};            // 'R', 'S', 'D', 'Z', 'T'
    double cpu_percent{0.0};
    uint64_t memory_rss_bytes{0};
    std::string cmdline;
};

struct ProcessDetail {
    int32_t pid{0};
    int32_t ppid{0};
    std::string name;
    std::string user;
    char state{'R'};
    double cpu_percent{0.0};
    uint64_t memory_rss_bytes{0};
    uint64_t memory_vms_bytes{0};
    uint32_t thread_count{0};
    uint32_t open_fd_count{0};
    uint64_t start_time_epoch{0};
    std::string cmdline;
    std::string working_directory;
};

} // namespace nodepulse::domain
```

### 1.7 Systemd Service Domain (`service_info.hpp`)
```cpp
namespace nodepulse::domain {

struct ServiceInfo {
    std::string name;           // e.g. "nodepulse.service"
    std::string description;
    std::string load_state;     // "loaded", "not-found"
    std::string active_state;   // "active", "inactive", "failed"
    std::string sub_state;      // "running", "dead", "exited"
    std::string unit_file_state;// "enabled", "disabled", "static"
};

struct ServiceDetail {
    std::string name;
    std::string description;
    std::string load_state;
    std::string active_state;
    std::string sub_state;
    std::string unit_file_state;
    int32_t main_pid{0};
    uint32_t restart_count{0};
    uint64_t active_enter_timestamp_utc{0};
    uint64_t memory_current_bytes{0};
};

} // namespace nodepulse::domain
```

### 1.8 Docker Container Domain (`container_info.hpp`)
```cpp
namespace nodepulse::domain {

struct ContainerSummary {
    std::string id;             // 64-char hex
    std::vector<std::string> names;
    std::string image;
    std::string status;         // "Up 4 hours"
    std::string state;          // "running", "exited"
    uint64_t created{0};
};

struct ContainerDetail {
    std::string id;
    std::string name;
    std::string image;
    std::string status;
    std::string state;
    bool running{false};
    int32_t exit_code{0};
    std::vector<std::string> port_mappings;
    std::vector<std::string> mount_sources;
    uint64_t created{0};
};

} // namespace nodepulse::domain
```

### 1.9 SSE Pulse Aggregate Domain (`metric_pulse.hpp`)
```cpp
namespace nodepulse::domain {

struct MetricPulse {
    std::string timestamp;      // ISO 8601 UTC
    double cpu_usage_percent{0.0};
    double memory_usage_percent{0.0};
    uint64_t memory_used_bytes{0};
    double network_rx_bytes_sec{0.0};
    double network_tx_bytes_sec{0.0};
};

} // namespace nodepulse::domain
```

---

## 2. Relational Schema for PostgreSQL (Phase 14)

When PostgreSQL storage is activated, snapshots are stored in a time-partitioned table.

```sql
-- Phase 14 Database Schema
CREATE TABLE IF NOT EXISTS host_metrics (
    id BIGSERIAL PRIMARY KEY,
    captured_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    hostname VARCHAR(255) NOT NULL,
    cpu_usage_percent DOUBLE PRECISION NOT NULL,
    load_1m DOUBLE PRECISION NOT NULL,
    load_5m DOUBLE PRECISION NOT NULL,
    load_15m DOUBLE PRECISION NOT NULL,
    mem_total_bytes BIGINT NOT NULL,
    mem_used_bytes BIGINT NOT NULL,
    mem_available_bytes BIGINT NOT NULL,
    swap_total_bytes BIGINT NOT NULL,
    swap_used_bytes BIGINT NOT NULL,
    net_rx_bytes_per_sec DOUBLE PRECISION NOT NULL,
    net_tx_bytes_per_sec DOUBLE PRECISION NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_host_metrics_hostname_time 
    ON host_metrics (hostname, captured_at DESC);
```

