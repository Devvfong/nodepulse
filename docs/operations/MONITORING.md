# NodePulse — Production Monitoring & Alerting Guide

This document provides instructions for integrating NodePulse with Prometheus, VictoriaMetrics, and Grafana, including recommended alert rules.

---

## 1. Prometheus Scrape Configuration

Add the following scrape job to your central `prometheus.yml`:

```yaml
scrape_configs:
  - job_name: 'nodepulse'
    scrape_interval: 15s
    scrape_timeout: 5s
    metrics_path: '/metrics'
    static_configs:
      - targets:
          - '10.0.1.10:8080'
          - '10.0.1.11:8080'
          - '10.0.1.12:8080'
    # Optional authorization header if require_auth is enabled in config.json
    http_headers:
      X-API-Key: 'np_live_scrape_secret_key'
```

---

## 2. Core Prometheus Metrics

### 2.1 Host Resource Metrics
- **`nodepulse_cpu_usage_ratio`** (Gauge): Ratio of CPU consumed (0.0 to 1.0).
- **`nodepulse_memory_used_bytes`** (Gauge): RAM used by host in bytes.
- **`nodepulse_memory_total_bytes`** (Gauge): Total physical host RAM in bytes.
- **`nodepulse_disk_used_bytes`** (Gauge, labeled by `mount_point`, `filesystem`).
- **`nodepulse_network_receive_bytes_total`** (Counter, labeled by `interface`).
- **`nodepulse_network_transmit_bytes_total`** (Counter, labeled by `interface`).

### 2.2 Agent Internal Metrics
- **`nodepulse_process_resident_memory_bytes`** (Gauge): NodePulse agent memory footprint.
- **`nodepulse_http_requests_total`** (Counter, labeled by `endpoint`, `status`).
- **`nodepulse_collector_failures_total`** (Counter, labeled by `collector`).

---

## 3. Recommended Prometheus Alerting Rules

Create `/etc/prometheus/rules/nodepulse_alerts.yml`:

```yaml
groups:
  - name: nodepulse_host_alerts
    rules:
      - alert: NodePulseAgentDown
        expr: up{job="nodepulse"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "NodePulse agent on {{ $labels.instance }} is unreachable"
          description: "Health/metrics scrape failed for over 1 minute."

      - alert: HostHighCpuUsage
        expr: nodepulse_cpu_usage_ratio > 0.90
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Host CPU utilization > 90% on {{ $labels.instance }}"
          description: "Host has maintained > 90% CPU usage for 5 consecutive minutes."

      - alert: HostLowMemory
        expr: (nodepulse_memory_used_bytes / nodepulse_memory_total_bytes) > 0.95
        for: 3m
        labels:
          severity: critical
        annotations:
          summary: "Host RAM memory exhausted on {{ $labels.instance }}"
          description: "Over 95% of physical memory is currently utilized."

      - alert: HostDiskAlmostFull
        expr: (nodepulse_disk_used_bytes / nodepulse_disk_total_bytes) > 0.90
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "Mount point {{ $labels.mount_point }} on {{ $labels.instance }} is > 90% full"

      - alert: NodePulseMemoryLeak
        expr: nodepulse_process_resident_memory_bytes > 104857600  # 100MB
        for: 15m
        labels:
          severity: warning
        annotations:
          summary: "NodePulse agent RSS exceeds 100MB on {{ $labels.instance }}"
          description: "Agent memory footprint exceeds expected budget (<65MB)."
```

