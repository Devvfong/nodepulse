# NodePulse — Data Flow Architecture

This document diagrams the primary end-to-end data flows through NodePulse.

---

## 1. Flow 1: Synchronous Telemetry Request (`GET /api/v1/cpu`)

```mermaid
sequenceDiagram
    autonumber
    actor Client
    participant Drogon as Drogon Event Loop
    participant Auth as AuthFilter
    participant Rate as RateLimitFilter
    participant Controller as CpuController
    participant Service as CpuService
    participant Collector as CpuCollector
    participant Proc as /proc/stat & /proc/loadavg

    Client->>Drogon: GET /api/v1/cpu (Header: X-API-Key)
    Drogon->>Auth: doFilter(request)
    Auth->>Auth: constant_time_equals(key, configKey)
    alt Invalid Key
        Auth-->>Client: 401 UNAUTHORIZED (JSON error envelope)
    else Valid Key
        Auth->>Rate: doFilter(request)
        alt Rate Limit Exceeded
            Rate-->>Client: 429 RATE_LIMITED (Retry-After: 1)
        else Limit OK
            Rate->>Controller: handleGetCpu(request)
            Controller->>Service: getCpuMetrics()
            Service->>Collector: readJiffies()
            Collector->>Proc: read(/proc/stat)
            Proc-->>Collector: raw string jiffies
            Collector->>Proc: read(/proc/loadavg)
            Proc-->>Collector: raw load averages
            Collector-->>Service: raw sample
            Service->>Service: computeDeltaPercent(current, previous)
            Service-->>Controller: CpuMetrics (C++ domain struct)
            Controller->>Controller: serializeToJson(CpuMetrics)
            Controller-->>Client: HTTP 200 OK (application/json)
        end
    end
```

---

## 2. Flow 2: Process Table Enumeration (`GET /api/v1/processes`)

```mermaid
sequenceDiagram
    autonumber
    actor Client
    participant Controller as ProcessController
    participant Service as ProcessService
    participant Worker as Worker Thread Pool
    participant Collector as ProcessCollector
    participant Proc as /proc/[0-9]+

    Client->>Controller: GET /api/v1/processes?sort=cpu&limit=20
    Controller->>Controller: validateParams(sort, limit)
    Controller->>Service: listProcesses(sort, limit)
    Service->>Worker: dispatchAsyncTask()
    Worker->>Collector: collectAllProcesses()
    loop For each PID in /proc
        Collector->>Proc: open(/proc/<pid>/stat, status, cmdline)
        Proc-->>Collector: parse tokens into ProcessInfo
    end
    Collector-->>Worker: vector<ProcessInfo>
    Worker->>Worker: sortProcesses(by cpu, descending)
    Worker->>Worker: applyLimit(20)
    Worker-->>Service: vector<ProcessInfo> (top 20)
    Service-->>Controller: vector<ProcessInfo>
    Controller->>Controller: serializeToJson()
    Controller-->>Client: HTTP 200 OK (application/json)
```

---

## 3. Flow 3: Server-Sent Events Live Stream (`GET /api/v1/events`)

```mermaid
sequenceDiagram
    autonumber
    actor Client
    participant Drogon as Drogon Event Loop
    participant Controller as EventsController
    participant Broadcaster as StreamService
    participant Timer as Periodic Steady Timer (1000ms)
    participant Collectors as CPU / Mem / Net Collectors

    Client->>Drogon: GET /api/v1/events (Header: X-API-Key)
    Drogon->>Controller: establishSseStream(request)
    Controller->>Controller: setContentType("text/event-stream")
    Controller->>Broadcaster: registerClient(streamChannel)
    Controller-->>Client: HTTP 200 OK (Headers: text/event-stream, Transfer-Encoding: chunked)

    loop Every 1000ms Interval
        Timer->>Broadcaster: onTimerTick()
        Broadcaster->>Collectors: collectSnapshots()
        Collectors-->>Broadcaster: CpuMetrics, MemoryMetrics, NetMetrics
        Broadcaster->>Broadcaster: formatSseFrame("metric_pulse", jsonPayload)
        Broadcaster->>Drogon: asyncSendChunkToAllClients(frame)
        Drogon-->>Client: data: {"timestamp":..., "cpu_usage": 12.4, ...}\n\n
    end

    Client->>Drogon: Client disconnects (TCP FIN/RST)
    Drogon->>Broadcaster: unregisterClient(streamChannel)
    Broadcaster->>Broadcaster: cleanUpResources()
```

---

## 4. Flow 4: Prometheus Scrape (`GET /metrics`)

```mermaid
sequenceDiagram
    autonumber
    actor Scraper as Prometheus Server
    participant Controller as MetricsController
    participant Exporter as MetricsService
    participant Collectors as System Collectors

    Scraper->>Controller: GET /metrics
    Controller->>Exporter: generatePrometheusExposition()
    Exporter->>Collectors: queryLatestSnapshots()
    Collectors-->>Exporter: domain metrics
    Exporter->>Exporter: formatGauge("nodepulse_cpu_usage_ratio", val)
    Exporter->>Exporter: formatGauge("nodepulse_memory_used_bytes", bytes)
    Exporter->>Exporter: formatCounter("nodepulse_network_receive_bytes_total", bytes)
    Exporter-->>Controller: std::string (text/plain; version=0.0.4)
    Controller-->>Scraper: HTTP 200 OK
```

---

## 5. Flow 5: Background PostgreSQL Metric Archival (Phase 14)

```mermaid
sequenceDiagram
    autonumber
    participant Timer as Background Cron/Timer (60s)
    participant Worker as DB Worker Thread
    participant Collectors as System Collectors
    participant Repo as PostgresRepository
    participant DB as PostgreSQL Server

    Timer->>Worker: triggerSnapshot()
    Worker->>Collectors: collectHostSnapshot()
    Collectors-->>Worker: HostSnapshot domain struct
    Worker->>Repo: saveSnapshot(snapshot)
    alt Database Online
        Repo->>DB: INSERT INTO host_metrics (...) VALUES (...)
        DB-->>Repo: SUCCESS
    else Database Offline / Error
        Repo->>Repo: logErrorWithoutCrashing()
        Note over Repo,Worker: REST API remains unaffected (BR-003)
    end
```

