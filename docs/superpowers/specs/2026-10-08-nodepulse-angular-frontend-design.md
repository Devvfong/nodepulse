# NodePulse — Angular Frontend Architecture Specification

- **Document Identifier**: `SPEC-FRONTEND-001`
- **Specification Date**: 2026-10-08
- **Status**: Validated / Ready for Planning
- **Target Subsystem**: `apps/web/`
- **Deployment Topology**: Single-host operator console behind same-origin reverse proxy

---

## 1. Product Scope & Operational Identity

### 1.1 Classification & Purpose
The NodePulse Web Frontend is a dedicated, real-time Linux host observability and triage operator console. It communicates exclusively with a single local or remote NodePulse C++20/Drogon agent instance via HTTP REST endpoints and a Server-Sent Events (SSE) telemetry stream.

The console is an **operator-centric observability tool**, not a general administrative portal or SaaS application. Its purpose is to provide immediate, low-latency visibility into host health, resource utilization (CPU, memory, filesystems, network), running processes, systemd service units, and Docker containers without requiring SSH access.

### 1.2 Inspection-Only Operational Boundary
In strict alignment with NodePulse's security model and backend design:
- The frontend is **strictly read-only and inspection-oriented**.
- The interface **must never invent mutating lifecycle actions**. Specifically:
  - No process termination (`kill`), priority changes (`renice`), or signal dispatch.
  - No systemd unit operations (`start`, `stop`, `restart`, `enable`, `disable`).
  - No Docker container lifecycle management (`start`, `stop`, `restart`, `rm`, `exec`).
  - No host power operations (`reboot`, `shutdown`).
- All interactive controls are limited to sorting, filtering, manual data refresh triggers, viewport resizing, drawer inspection, and explicit stream pause/resume.
- No configurable automatic polling or auto-refresh rate adjustment subsystems are included in v1. Telemetry streaming is driven by the backend SSE pulse, and route-level operational tables load upon view entry or explicit manual refresh.

---

## 2. Information Architecture & Navigation

### 2.1 Navigation Structure
The v1 navigation hierarchy is intentionally compact to prioritize fast triage over fragmented subsystem pages:

```
NodePulse Console
│
├── [/overview]   ── System Identity, Vitals, Rolling Charts, Filesystems, Top Processes by CPU
├── [/processes]  ── Sortable Process Table ──> [Inspection Drawer: ?pid=<pid>]
├── [/services]   ── Systemd Units Filter Table ──> [Inspection Drawer: ?name=<name>]
└── [/containers] ── Docker Containers Table (or Unavailable State) ──> [Inspection Drawer: ?id=<id>]
```

### 2.2 Route Inventory & Scope Boundaries

| Route | Primary View | Deep-Link Inspection Drawer | Backend Endpoints Consumed |
|---|---|---|---|
| `/overview` | Host identity, vitals matrix, rolling trend charts, mounts, top processes by CPU | None | `GET /api/v1/health`<br>`GET /api/v1/system`<br>`GET /api/v1/cpu`<br>`GET /api/v1/memory`<br>`GET /api/v1/disks`<br>`GET /api/v1/network`<br>`GET /api/v1/processes?sort=cpu&limit=5`<br>`GET /api/v1/events` (SSE) |
| `/processes` | Process table with sort & limit controls | `?pid=<pid>` | `GET /api/v1/processes?sort={sort}&limit={limit}`<br>`GET /api/v1/processes/{pid}` |
| `/services` | Systemd unit table with status filter | `?name=<unit_name>` | `GET /api/v1/services?state={state}&limit={limit}`<br>`GET /api/v1/services/{name}` |
| `/containers` | Docker container table or intentional unavailable state | `?id=<container_id>` | `GET /api/v1/containers`<br>`GET /api/v1/containers/{id}` |

### 2.3 Explicitly Deferred Surfaces (Post-v1)
- Dedicated `/cpu`, `/memory`, `/disks`, and `/network` pages (telemetry is fully consolidated into `/overview` for v1).
- Raw `/metrics` Prometheus browser (Prometheus exposition exists for external scrapers, not human console inspection).
- Dedicated `/settings` configuration subsystem (proxy manages authentication; client preferences remain inline).
- Multi-host management and cluster topology views.

---

## 3. Reverse-Proxy & Security Architecture

### 3.1 Deployment Topology

```
+-------------------------------------------------------------+
| Browser Runtime                                             |
|   Angular SPA (`apps/web`)                                  |
|   Relative origin: https://operator.infra.internal          |
+-------------------------------------------------------------+
                      |
                      | HTTPS (Relative calls)
                      v
+-------------------------------------------------------------+
| Reverse Proxy (e.g., Nginx, Caddy, Traefik)                |
|   - Serves compiled static SPA assets for /                 |
|   - Restricts operator access (auth or private boundary)   |
|   - Injects upstream header: `X-API-Key: <secret>`         |
+-------------------------------------------------------------+
         |                                  |
         | Reverse Proxy (HTTP)             | Reverse Proxy (SSE Stream)
         v                                  v
+-------------------------------------------------------------+
| NodePulse C++20 Agent (127.0.0.1:8080)                     |
|   - Drogon Web Engine                                       |
|   - AuthFilter verifies constant-time `X-API-Key`          |
|   - RateLimitFilter enforces token-bucket quotas            |
+-------------------------------------------------------------+
```

### 3.2 Security Invariants
1. **Zero Secret Footprint in Browser Code**:
   - The NodePulse API key must **never** be committed to source code.
   - The API key must **never** appear in Angular `environment.ts` or any build configuration.
   - The API key must **never** be bundled into compiled client JavaScript.
   - The frontend UI must **never** render an input prompt requesting the NodePulse agent API key.
2. **Access Control Boundary**:
   - The dashboard and reverse proxy **must not be publicly exposed without operator access control**.
   - Operator access must be enforced by reverse-proxy authentication (such as mTLS, HTTP Basic Authentication, or upstream forward SSO), or restricted by deployment within a trusted private management network or VPN boundary. (VPN or network perimeter isolation is a transport-level boundary and is not an authentication mechanism performed by Nginx.)
3. **Same-Origin Addressing**:
   - The Angular application executes HTTP requests against relative endpoints: `/api/v1/health`, `/api/v1/system`, `/api/v1/cpu`, `/api/v1/events`, etc.
   - The browser runtime remains unaware of the internal host, port, or loopback binding of the Drogon server.
4. **Upstream Header Injection**:
   - The reverse proxy is the authoritative owner of the agent API key. It injects `X-API-Key: <agent_api_key>` into all forwarded `/api/*` and `/api/v1/events` requests.
5. **Native EventSource Compatibility**:
   - Because browser `EventSource` does not support custom request headers, routing SSE requests through a same-origin reverse proxy that injects `X-API-Key` enables native streaming without third-party fetch polyfills.
6. **Reference Nginx Configuration**:
   ```nginx
   server {
       listen 443 ssl http2;
       server_name node-01.infra.internal;

       # Static Angular SPA Assets
       location / {
           root /usr/share/nodepulse/web;
           try_files $uri $uri/ /index.html;
       }

       # NodePulse REST API Proxy
       location /api/ {
           proxy_pass http://127.0.0.1:8080/api/;
           proxy_http_version 1.1;
           proxy_set_header Connection "";
           proxy_set_header Host $host;
           proxy_set_header X-Real-IP $remote_addr;
           proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
           proxy_set_header X-API-Key "np_live_REDACTED_PROXY_KEY";
       }

       # NodePulse Realtime SSE Telemetry Stream
       location /api/v1/events {
           proxy_pass http://127.0.0.1:8080/api/v1/events;
           proxy_http_version 1.1;
           proxy_set_header Connection "";
           proxy_set_header Host $host;
           proxy_set_header X-Real-IP $remote_addr;
           proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
           proxy_set_header X-API-Key "np_live_REDACTED_PROXY_KEY";
           proxy_buffering off;
           proxy_cache off;
           chunked_transfer_encoding off;
       }
   }
   ```

---

## 4. Angular Application Architecture

### 4.1 Framework & Core Technologies
- **Framework**: Current supported Angular release at implementation time.
- **Component Model**: 100% standalone components (`imports: [...]`, zero `NgModule` declarations).
- **Reactivity Paradigm**: Angular Signals (`signal()`, `computed()`, `effect()`) for all view and component state.
- **Asynchronous Boundaries**: RxJS strictly at I/O and event boundaries (`HttpClient`, SSE streams), bridged to Signals via `toSignal()` and `takeUntilDestroyed()`.
- **Styling**: Tailwind CSS integration supported at implementation time, utilizing the approved dark-first design tokens.
- **Routing**: Angular Router with standalone function-based route tables and lazy-loaded route components.
- **Dependency Philosophy**: Zero unjustified third-party runtime dependencies. Native SVG for charts; no heavy charting or admin-dashboard libraries.

### 4.2 Workspace Layout (`apps/web/`)

```
apps/web/
├── src/
│   ├── app/
│   │   ├── core/
│   │   │   ├── api/
│   │   │   │   ├── api-client.service.ts       # Typed wrapper around HttpClient
│   │   │   │   ├── sse.service.ts              # Native EventSource wrapper & stream state tracker
│   │   │   │   └── error.interceptor.ts        # Global ApiErrorEnvelope unmarshaling
│   │   │   ├── models/
│   │   │   │   ├── system.model.ts             # SystemInfo, HealthInfo
│   │   │   │   ├── cpu.model.ts                # CpuMetrics, CpuCoreMetrics, LoadAverage
│   │   │   │   ├── memory.model.ts             # MemoryMetrics
│   │   │   │   ├── disk.model.ts               # DiskPartitionMetrics
│   │   │   │   ├── network.model.ts            # NetworkInterfaceMetrics
│   │   │   │   ├── process.model.ts            # ProcessInfo, ProcessDetail
│   │   │   │   ├── service.model.ts            # ServiceInfo, ServiceDetail
│   │   │   │   ├── container.model.ts          # ContainerSummary, ContainerDetail
│   │   │   │   ├── pulse.model.ts              # MetricPulse, TelemetrySample<T>
│   │   │   │   └── api-error.model.ts          # ApiErrorEnvelope, ApiErrorDetail
│   │   │   └── services/
│   │   │       ├── host-state.service.ts       # Central Signal store for vitals & telemetry
│   │   │       └── connection-state.service.ts # Independent agentStatus & streamStatus signals
│   │   ├── layout/
│   │   │   ├── shell/
│   │   │   │   └── shell.component.ts          # Responsive frame (sidebar + header + outlet)
│   │   │   ├── sidebar/
│   │   │   │   └── sidebar.component.ts        # Navigation links, host identity, status badges
│   │   │   └── header/
│   │   │       └── header.component.ts         # Breadcrumbs, mobile toggle, stream pause/resume
│   │   ├── shared/
│   │   │   ├── ui/
│   │   │   │   ├── status-badge/               # Accessible semantic status badge
│   │   │   │   ├── metric-card/                # Monospace value, label, subtext card
│   │   │   │   ├── data-table/                 # Table shell with responsive overflow
│   │   │   │   ├── drawer/                     # Desktop slide-over / mobile full overlay
│   │   │   │   └── empty-state/                # Informative missing-state card
│   │   │   └── charts/
│   │   │       ├── sparkline/                  # Lightweight inline SVG sparkline
│   │   │       └── rolling-area-chart/         # Native SVG trend graph over rolling telemetry buffer
│   │   ├── features/
│   │   │   ├── overview/
│   │   │   │   ├── overview.component.ts       # Orchestrator for Overview surface
│   │   │   │   └── components/
│   │   │   │       ├── host-vitals/            # System metadata & load average card
│   │   │   │       ├── cpu-card/               # CPU usage %, core metrics, rolling chart
│   │   │   │       ├── memory-card/            # RAM / Swap gauges & rolling chart
│   │   │   │       ├── network-card/           # Headline RX / TX throughput & rolling chart
│   │   │   │       ├── disks-card/             # Filesystem partition breakdown
│   │   │   │       └── top-processes/          # Top 5 processes by CPU table
│   │   │   ├── processes/
│   │   │   │   ├── processes.component.ts      # Process table, sort & limit bar, manual refresh
│   │   │   │   └── components/
│   │   │   │       └── process-detail-drawer/  # Inspection drawer for single PID
│   │   │   ├── services/
│   │   │   │   ├── services.component.ts       # Systemd unit table & state filter
│   │   │   │   └── components/
│   │   │   │       └── service-detail-drawer/  # Inspection drawer for single unit
│   │   │   └── containers/
│   │   │       ├── containers.component.ts     # Docker table / 503 unavailable display
│   │   │       └── components/
│   │   │           └── container-detail-drawer/# Inspection drawer for container ID
│   │   ├── app.routes.ts
│   │   └── app.config.ts
│   ├── environments/
│   │   └── environment.ts                      # Production environment flag only
│   ├── index.html
│   ├── main.ts
│   └── styles.css
├── tsconfig.json
└── package.json
```

---

## 5. Reactive State & Communication Patterns

### 5.1 Division of Responsibilities: Signals vs. RxJS

```
+-------------------------------------------------------------------------+
| ASYNCHRONOUS I/O & EVENT BOUNDARIES (RxJS)                              |
| - HttpClient GET streams                                                |
| - Native EventSource `metric_pulse` stream                              |
| - Window `visibilitychange` events (fresh snapshot trigger on resume)   |
+-------------------------------------------------------------------------+
                                    |
                                    | `.pipe(...)` -> `toSignal()`
                                    v
+-------------------------------------------------------------------------+
| SYNCHRONOUS REACTIVE VIEW STATE (Angular Signals)                       |
| - `agentStatus`: Signal<'unknown' | 'healthy' | 'unreachable'>          |
| - `streamStatus`: Signal<'connecting' | 'connected' | 'reconnecting'    |
|                          | 'paused'>                                    |
| - `pulse`: Signal<MetricPulse | null>                                   |
| - `cpuRollingBuffer`: Signal<TelemetrySample<number | null>[]>          |
| - `memoryRollingBuffer`: Signal<TelemetrySample<number>[]>              |
| - `networkRxRollingBuffer`: Signal<TelemetrySample<number>[]>           |
| - `networkTxRollingBuffer`: Signal<TelemetrySample<number>[]>           |
| - `selectedPid`: Signal<number | null>                                  |
| - `isStreamPaused`: Signal<boolean>                                     |
+-------------------------------------------------------------------------+
                                    |
                                    | `computed()` derivations
                                    v
+-------------------------------------------------------------------------+
| TEMPLATE RENDERING (OnPush / Fine-Grained Change Detection)             |
| - `<metric-card [value]="currentCpu()" [status]="measurementStatus()">` |
| - `<rolling-area-chart [samples]="cpuRollingBuffer()">`                 |
+-------------------------------------------------------------------------+
```

### 5.2 Decoupled Agent Health and Stream State
Agent reachability and SSE stream state represent fundamentally distinct operational dimensions and **must never be conflated into a single status indicator**:

1. **`agentStatus`** (`'unknown'` | `'healthy'` | `'unreachable'`):
   - Authoritatively derived from `GET /api/v1/health`.
   - Fetched on initial app load, on route entry, and upon returning from a paused/suspended state.
2. **`streamStatus`** (`'connecting'` | `'connected'` | `'reconnecting'` | `'paused'`):
   - Derived from `EventSource` event callbacks (`open`, `error`) and user pause/resume toggling.
3. **Operational Independence**:
   - An SSE stream drop or reconnection attempt **must not** automatically mark the agent as unreachable.
   - The UI explicitly supports presenting:
     ```
     Agent: Healthy  |  Live stream: Reconnecting
     ```
     allowing operators to distinguish a severed push channel from an agent daemon crash.

### 5.3 Bounded Rolling Telemetry Buffers
Telemetry samples are modeled with explicit timestamp awareness:

```typescript
export interface TelemetrySample<T> {
  timestamp: string; // ISO 8601 UTC from agent pulse
  value: T | null;   // null represents warming_up or gap
}
```

- **Buffer Semantics**:
  - Telemetry buffers for CPU, memory, and network maintain a strictly capped array of the **last 60 received samples**.
  - **No Fixed-Duration Assumptions**: The NodePulse SSE broadcast interval is configurable on the backend (`sse.interval_ms`). The client **must not assume or claim that 60 samples equals 60 seconds**.
  - All charts describe their domain as the *recent telemetry sample window (last 60 samples)*, not a fixed "60-second history".
  - Adding a new sample shifts or slices the array to `buffer.slice(-60)`, guaranteeing an $O(1)$ memory bound.
- **Honest Gap Preservation**:
  - Missing telemetry samples across disconnections or user pauses **must never be artificially smoothed or interpolated**.
  - If a time delta between consecutive samples exceeds expected intervals, or if `value === null` (e.g. CPU `warming_up`), the chart renderer visibly preserves the gap (rendering a discontinuous line break), ensuring operators are never presented with fabricated metrics.

---

## 6. Realtime Telemetry Architecture (Server-Sent Events)

### 6.1 Native EventSource Integration
- The realtime telemetry service (`SseService`) wraps the standard browser `EventSource` targeting relative endpoint `/api/v1/events`.
- **Event Listeners**:
  1. `open`: Triggered when the HTTP streaming connection is established. Sets `streamStatus` to `'connected'`.
  2. `error`: Triggered when the streaming connection is severed or encountering transport errors. Sets `streamStatus` to `'reconnecting'`. Does not mutate `agentStatus`.
  3. `metric_pulse`: Listens explicitly for events with name `event: metric_pulse`. Parses payload into `MetricPulse`:
     ```json
     {
       "timestamp": "2026-10-08T12:00:00Z",
       "cpu_usage_percent": 14.2,
       "memory_usage_percent": 38.1,
       "memory_used_bytes": 6395000000,
       "network_rx_bytes_sec": 12040.0,
       "network_tx_bytes_sec": 45800.0
     }
     ```
- **SSE Protocol Invariant**: Native browser `EventSource` handles SSE comments (such as `: keepalive\n\n`) internally and does not surface comment lines to JavaScript. The frontend design does not attempt to attach listeners or reset timers based on SSE comments.
- **Reconnection Policy**: The client relies on native `EventSource` reconnection behavior for automatic retries during transient disconnections. Custom reconnect scheduling is avoided in v1.
- **Connection State Tracking**:
  - `streamStatus` is updated directly by native `open` and `error` events, combined with user pause state.
  - The timestamp of the last received `metric_pulse` is tracked in memory to determine sample freshness.

### 6.2 Stream Pause / Resume & Page Visibility
- **Explicit User Control**:
  - Stream pausing and resuming is **strictly user-controlled** via an interactive pause/resume toggle in the application header.
  - When the operator clicks **Pause**:
    1. The `EventSource` instance is explicitly closed.
    2. `streamStatus` transitions to `'paused'`.
    3. Live chart updates halt; last received values remain visible.
  - When the operator clicks **Resume**:
    1. `streamStatus` transitions to `'connecting'`.
    2. The application immediately dispatches fresh REST snapshots (`GET /api/v1/health`, `GET /api/v1/cpu`, `GET /api/v1/memory`, `GET /api/v1/network`) to synchronize view state.
    3. A new `EventSource` is opened targeting `/api/v1/events`.
- **Page Visibility (`visibilitychange`)**:
  - The application **must not automatically close or sever the SSE stream** simply because `document.hidden` becomes true.
  - When the operator returns to the tab (`document.visibilityState === 'visible'`):
    - If `streamStatus === 'reconnecting'`, or if the last received pulse timestamp indicates stale data, the client dispatches a fresh REST snapshot (`GET /api/v1/health`, `GET /api/v1/cpu`, `GET /api/v1/memory`, `GET /api/v1/network`) to ensure immediate display accuracy while the stream settles.
    - Missing samples during background throttling are left as visible gaps in rolling charts.
  - No hidden preferences or background settings subsystems are introduced.

---

## 7. REST API Integration & Contracts

All REST calls communicate with `/api/v1/*` using relative paths.

### 7.1 Data Contracts Consumed

#### 1. Host Health & Identity
- `GET /api/v1/health`
  ```typescript
  export interface HealthResponse {
    status: 'healthy' | string;
    version: string;
    uptime_seconds: number;
  }
  ```
  *Operational Role*: Drives the independent `agentStatus` signal (`'healthy'` vs `'unreachable'`).
- `GET /api/v1/system`
  ```typescript
  export interface SystemInfo {
    hostname: string;
    os_name: string;
    os_version: string;
    kernel_version: string;
    architecture: string;
    boot_time_utc: number;
    uptime_seconds: number;
  }
  ```

#### 2. CPU Subsystem
- `GET /api/v1/cpu`
  ```typescript
  export interface LoadAverage {
    one_minute: number;
    five_minute: number;
    fifteen_minute: number;
  }

  export interface CpuCoreMetrics {
    core_id: number;
    usage_percent: number | null; // null during warming_up
  }

  export interface CpuMetrics {
    usage_percent: number | null; // null during warming_up
    measurement_status: 'ready' | 'warming_up' | 'cached';
    model_name: string;
    physical_cores: number;
    logical_cores: number;
    load_average: LoadAverage;
    cores: CpuCoreMetrics[];
  }
  ```
  *UI Requirement*: When `usage_percent` is `null` and `measurement_status === 'warming_up'`, display an amber `"Warming Up"` badge rather than `0%` or `NaN`.

#### 3. Memory Subsystem
- `GET /api/v1/memory`
  ```typescript
  export interface MemoryMetrics {
    total_bytes: number;
    used_bytes: number;
    free_bytes: number;
    available_bytes: number;
    buffers_bytes: number;
    cached_bytes: number;
    usage_percent: number;
    swap_total_bytes: number;
    swap_free_bytes: number;
    swap_used_bytes: number;
    swap_usage_percent: number;
  }
  ```

#### 4. Disk & Filesystem Subsystem
- `GET /api/v1/disks`
  ```typescript
  export interface DiskPartitionMetrics {
    filesystem: string;
    mount_point: string;
    fstype: string;
    total_bytes: number;
    used_bytes: number;
    free_bytes: number;
    available_bytes: number;
    usage_percent: number;
    inodes_total: number;
    inodes_free: number;
  }
  ```

#### 5. Network Subsystem
- **Distinction of Data Sources**:
  1. **Aggregate Host Realtime Telemetry (from SSE `MetricPulse`)**:
     - `network_rx_bytes_sec`: Host total ingress rate.
     - `network_tx_bytes_sec`: Host total egress rate.
     - *Usage*: Powers the headline throughput numbers on the Overview card and feeds the rolling trend chart.
  2. **Per-Interface Inventory & Errors (from `GET /api/v1/network`)**:
     ```typescript
     export interface NetworkInterfaceMetrics {
       name: string;
       mac_address: string;
       operstate: 'up' | 'down' | 'unknown' | string;
       speed_mbps: number;
       rx_bytes: number;
       tx_bytes: number;
       rx_packets: number;
       tx_packets: number;
       rx_errors: number;
       tx_errors: number;
       rx_bytes_per_sec: number;
       tx_bytes_per_sec: number;
     }
     ```
     *Usage*: Powers interface-level inventory, link status badges (`operstate`), link speed, cumulative counters, and hardware error tallies (`rx_errors`, `tx_errors`).
  3. *Contract Boundary*: The frontend **must never reference or display packet drop counters**, as they are not supported by the backend contract.

#### 6. Processes Subsystem
- `GET /api/v1/processes?sort={cpu|memory|pid}&limit={limit}`
  ```typescript
  export interface ProcessInfo {
    pid: number;
    name: string;
    user: string;
    state: 'R' | 'S' | 'D' | 'Z' | 'T' | string;
    cpu_percent: number;
    memory_rss_bytes: number;
    cmdline: string;
  }
  ```
- `GET /api/v1/processes/{pid}`
  ```typescript
  export interface ProcessDetail {
    pid: number;
    ppid: number;
    name: string;
    user: string;
    state: string;
    cpu_percent: number;
    memory_rss_bytes: number;
    memory_vms_bytes: number;
    thread_count: number;
    open_fd_count: number;
    start_time_epoch: number;
    cmdline: string;
    working_directory: string;
  }
  ```
  *Contract Boundary*: The frontend must only display fields provided by this backend endpoint. It must not imply that Angular directly parses `/proc/<pid>` or expect a non-existent "executable path" field.

#### 7. Systemd Services Subsystem
- `GET /api/v1/services?state={active|inactive|failed|all}&limit={limit}`
  ```typescript
  export interface ServiceInfo {
    name: string;
    description: string;
    load_state: string;
    active_state: 'active' | 'inactive' | 'failed' | string;
    sub_state: string;
    unit_file_state: string;
  }
  ```
- `GET /api/v1/services/{name}`
  ```typescript
  export interface ServiceDetail {
    name: string;
    description: string;
    load_state: string;
    active_state: string;
    sub_state: string;
    unit_file_state: string;
    main_pid: number;
    restart_count: number;
    active_enter_timestamp_utc: number;
    memory_current_bytes: number;
  }
  ```
  *Navigation Invariant*: `main_pid` must only be rendered as an interactive link to `/processes?pid=<main_pid>` when `main_pid >= 1`. If `main_pid` is zero or not a valid positive PID, render a neutral non-clickable placeholder such as `" — "` or `"Not running"`. Never navigate to `/processes?pid=0`.

#### 8. Docker Containers Subsystem
- `GET /api/v1/containers`
  ```typescript
  export interface ContainerSummary {
    id: string;
    names: string[];
    image: string;
    status: string;
    state: 'running' | 'exited' | string;
    created: number;
  }
  ```
- `GET /api/v1/containers/{id}`
  ```typescript
  export interface ContainerDetail {
    id: string;
    name: string;
    image: string;
    status: string;
    state: string;
    running: boolean;
    exit_code: number;
    port_mappings: string[];
    mount_sources: string[];
    created: number;
  }
  ```

### 7.2 Standard Error Response Envelope
All error responses from NodePulse conform to the schema defined in `docs/api/ERRORS.md`:

```typescript
export interface ApiErrorDetail {
  field?: string;
  reason?: string;
  [key: string]: unknown;
}

export interface ApiErrorEnvelope {
  error: {
    code:
      | 'INVALID_REQUEST'
      | 'UNAUTHORIZED'
      | 'FORBIDDEN'
      | 'RESOURCE_NOT_FOUND'
      | 'RATE_LIMITED'
      | 'COLLECTOR_FAILURE'
      | 'DOCKER_UNAVAILABLE'
      | 'SERVICE_UNAVAILABLE'
      | 'INTERNAL_ERROR'
      | string;
    message: string;
    timestamp: string;
    details: ApiErrorDetail[];
  };
}
```

The Angular HTTP Interceptor (`error.interceptor.ts`) intercepts failing HTTP requests, unmarshals the `ApiErrorEnvelope`, and passes structured errors to calling services.

---

## 8. View Layouts & User Experience

### 8.1 Overview Surface (`/overview`)
The primary monitoring view provides a consolidated dashboard for immediate host triage:

1. **System Identity Header**:
   - Hostname (`font-mono`, bold).
   - OS Distribution (`Ubuntu 22.04 LTS`), Kernel version, Architecture (`x86_64`).
   - Agent Status badge (`agentStatus`: `HEALTHY` green, `UNREACHABLE` red).
   - Live Stream Status badge (`streamStatus`: `CONNECTED` green, `RECONNECTING` amber, `PAUSED` neutral).
   - Host Uptime (formatted as `14d 06h 32m`) and Boot Time UTC.
2. **Vitals Telemetry Grid**:
   - **Card 1: CPU Usage**:
     - Large monospace percentage display.
     - Status badge (`Ready`, `Warming Up`, or `Cached`).
     - Load averages: `1m: 0.45` | `5m: 0.62` | `15m: 0.58`.
     - Model name and Core count (`4 physical / 8 logical`).
     - Native SVG trend chart rendering recent samples (last 60 samples).
   - **Card 2: Memory Usage**:
     - Large monospace RAM percentage display.
     - Used / Total memory (humanized GiB).
     - Buffers and Cached breakdown.
     - Swap usage percentage and Used / Total Swap.
     - Native SVG trend chart rendering recent samples (last 60 samples).
   - **Card 3: Network Throughput**:
     - Headline aggregate throughput from SSE `MetricPulse` (`network_rx_bytes_sec`, `network_tx_bytes_sec` formatted as KB/s or MB/s).
     - Interface summary and hardware error counts (`rx_errors`, `tx_errors`) from `GET /api/v1/network`.
     - Native SVG dual-trace chart rendering recent samples (last 60 samples, RX vs TX).
   - **Card 4: Filesystem Capacity**:
     - Root filesystem `/` utilization percentage bar.
     - Partition summary list showing mount points, fstype, used/total, and percentage.
3. **No Arbitrary Warning Thresholds**:
   - Telemetry percentage values (CPU %, RAM %, Disk %) are rendered in neutral high-contrast monospace text (`text-zinc-100`).
   - **Invariant**: The UI must not paint percentages amber or red based on unapproved, arbitrary thresholds (e.g. coloring 80% amber or 90% red). Semantic status colors are reserved strictly for explicit system states (`healthy`, `warming_up`, `active`, `inactive`, `failed`, `reconnecting`, `unavailable`).
4. **Top Processes by CPU**:
   - Section header: `"Top Processes by CPU"`.
   - Table displaying top 5 processes queried via `GET /api/v1/processes?sort=cpu&limit=5`.
   - Columns: `PID` (mono), `Name`, `User`, `CPU %` (mono), `Memory (RSS)`.
   - Row click navigates to `/processes?pid=<pid>`, opening the process inspection drawer.

### 8.2 Processes Surface (`/processes`)
1. **Control Toolbar**:
   - Sort selector: `CPU %` (default), `Memory (RSS)`, `PID`.
   - Row limit selector: `25`, `50` (default), `100`, `200`.
   - Manual refresh trigger button with spinning indicator during fetch.
2. **Process Table**:
   - Headers: `PID`, `Name`, `User`, `State`, `CPU %`, `RSS Memory`, `Command Line`.
   - Visual state indicators: `R` (running, emerald), `S` (sleeping, neutral), `D` (disk sleep, neutral), `Z` (zombie, rose).
   - Clicking a row sets query param `?pid=<pid>` and opens the inspection drawer.
3. **Process Inspection Drawer (`/processes?pid=<pid>`)**:
   - Slides over from right (desktop) or appears as a full-screen sheet (mobile).
   - Header: Process Name, PID badge, State badge.
   - Core Telemetry: CPU %, RSS Memory, Virtual Memory Size (VMS), Thread Count, Open File Descriptors (FDs).
   - Metadata: Parent PID (`PPID`), User, Start Time Epoch (and human-readable duration).
   - Working Directory: Rendered in a selectable monospace block.
   - Full Command Line: Selectable monospace code block with copy-to-clipboard action.

### 8.3 Services Surface (`/services`)
1. **Control Toolbar**:
   - State Filter tabs: `All` (default), `Active`, `Failed`, `Inactive`.
   - Row limit selector: `50` (default), `100`, `200`.
2. **Services Table**:
   - Headers: `Unit Name`, `Description`, `Active State`, `Sub-State`, `Unit File State`.
   - State badges: `active` (emerald), `failed` (rose), `inactive` (zinc).
   - Clicking a row sets query param `?name=<unit_name>` and opens the inspection drawer.
3. **Service Inspection Drawer (`/services?name=<unit_name>`)**:
   - Unit Name, Description, Load State (`loaded`/`not-found`).
   - State: Active State, Sub-State, Unit File State (`enabled`/`disabled`).
   - Runtime Details:
     - Main PID: When `main_pid >= 1`, rendered as a clickable link navigating to `/processes?pid=<main_pid>`. When `main_pid <= 0`, rendered as `" — "` (non-clickable).
     - Restart Count, Active Enter Timestamp UTC, Current Memory Usage.

### 8.4 Containers Surface (`/containers`)
1. **Operational State Handling**:
   - **Scenario A: Docker Daemon Available (HTTP 200)**:
     - Table rendering: `Container Name`, `Image`, `State` (`running`/`exited`), `Status` (`Up 3 days`), `Created Time`.
     - Clicking a row sets query param `?id=<container_id>` and opens the inspection drawer.
   - **Scenario B: Docker Unavailable (HTTP 503 `DOCKER_UNAVAILABLE`)**:
     - The route remains active in the navigation.
     - Renders an intentional, informative empty/unavailable state:
       - Header: `"Docker Unavailable on Host"`.
       - Description: `"NodePulse could not communicate with /var/run/docker.sock. Docker may be stopped or not installed on this host."`.
       - No broken layout or unhandled error alerts.
2. **Container Inspection Drawer (`/containers?id=<container_id>`)**:
   - Name, Full 64-character Container ID, Image name.
   - State, Running status, Exit Code (`0` neutral, non-zero rose).
   - Port Mappings: Formatted list (e.g. `0.0.0.0:6379 -> 6379/tcp`).
   - Mount Points / Volumes: Host mount sources.
   - Creation Timestamp.

---

## 9. Drawer Navigation & Deep-Linking Architecture

### 9.1 Query-Parameter Drawer Model
All inspection drawers are managed via Angular Router query parameters rather than sub-route segments:

- **Process Detail**: `/processes?pid=1248`
- **Service Detail**: `/services?name=nodepulse.service`
- **Container Detail**: `/containers?id=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`

### 9.2 Cross-Subsystem Navigation
When inspecting a systemd service in `/services?name=nodepulse.service`, the `main_pid` field is conditionally interactive:
- If `serviceDetail.main_pid >= 1`:
  ```typescript
  router.navigate(['/processes'], { queryParams: { pid: serviceDetail.main_pid } });
  ```
- If `serviceDetail.main_pid <= 0`: Rendered as static neutral text (`" — "`).

### 9.3 Drawer UX Mechanics
- **Deep-Link Persistence**: Reloading the browser preserves the open inspection drawer.
- **Browser History**: Pressing the browser "Back" button closes the drawer without navigating away from the page.
- **Dismissal Actions**:
  - Clicking the background backdrop.
  - Clicking the explicit close button (`✕`).
  - Pressing the `Escape` key.
  All dismissal actions clear the corresponding query parameter via `router.navigate([], { queryParams: { pid: null } })`.

---

## 10. Design System & Visual Specification

### 10.1 Philosophy
- **Dark-First Infrastructure Console**: Tailored for high-density monitoring in operations centers and terminal-adjacent environments.
- **Zero Decorative Bloat**: No gradients, no glassmorphism/backdrop blur, no floating SaaS drop-shadows, and no excessive animation.
- **Monospace Emphasis**: All telemetry numbers, addresses, identifiers, timestamps, and paths are styled in monospace typography to ensure alignment and rapid scanning.

### 10.2 Color Tokens & Semantic Mappings

| Token Category | Tailwind Utility | Hex / CSS Value | Operational Usage |
|---|---|---|---|
| **Canvas Background** | `bg-zinc-950` | `#09090b` | App shell, root viewport background |
| **Surface (Card / Table)** | `bg-zinc-900` | `#18181b` | Cards, table headers, table rows, drawer background |
| **Surface Accent / Hover** | `bg-zinc-800/50` | `#27272a80` | Table row hover, button hover, input background |
| **Border (Primary)** | `border-zinc-800` | `#27272a` | Card borders, table dividers, drawer perimeter |
| **Border (Subtle)** | `border-zinc-800/60` | `#27272a99` | Secondary inner dividers |
| **Text (Primary)** | `text-zinc-100` | `#f4f4f5` | Headings, primary values, active labels |
| **Text (Muted)** | `text-zinc-400` | `#a1a1aa` | Field labels, secondary descriptions, table headers |
| **Status: Healthy / Active** | `text-emerald-400`<br>`bg-emerald-950/40`<br>`border-emerald-800/50` | Muted Emerald | Explicit healthy agent state, active service, running container |
| **Status: Warning / Warm** | `text-amber-400`<br>`bg-amber-950/40`<br>`border-amber-800/50` | Muted Amber | CPU `warming_up`, SSE `reconnecting` |
| **Status: Critical / Failed** | `text-rose-400`<br>`bg-rose-950/40`<br>`border-rose-800/50` | Muted Rose | Service `failed`, agent `unreachable`, non-zero exit code |
| **Status: Inactive / Neutral**| `text-zinc-400`<br>`bg-zinc-800/60`<br>`border-zinc-700/50` | Muted Zinc | Service `inactive`, container `stopped`, stream `paused` |

### 10.3 Typography & Geometry
- **Primary Interface Font**: Sans-serif (`Inter`, system UI font fallback).
- **Technical Figures Font**: Monospace (`JetBrains Mono`, `ui-monospace`, `monospace`). Applied to:
  - Percentages, byte quantities, bandwidth rates, PIDs, UUIDs, exit codes, IP addresses, MAC addresses, and UTC timestamps.
- **Border Radius**: Restrained `rounded-sm` (2px) for badges and buttons; `rounded` (4px) for cards and modals.
- **Visual Density**: Table row heights constrained to 32px–36px; padding kept tight to maximize displayed telemetry per screen height.

---

## 11. Responsive Breakpoint Matrix

| Viewport Category | Breakpoint Range | Navigation Shell | Overview Surface | Operational Tables | Inspection Drawers |
|---|---|---|---|---|---|
| **Desktop (Primary)** | $\ge 1024\text{px}$ (`lg:`) | Static 240px left sidebar | 4-column metric grid; side-by-side charts | Full column display; inline utilization bars | 480px slide-over drawer on right; underlying table remains visible |
| **Tablet (Triage)** | $768\text{px} - 1023\text{px}$ (`md:`) | Collapsible sidebar or slide-out menu | 2-column metric grid; full-width charts | Priority columns; horizontal scroll for secondary columns | 600px wide slide-over or 85vw overlay sheet |
| **Mobile (On-Call)** | $< 768\text{px}` (< `md:`) | 52px top bar with hamburger drawer | 1-column vertically stacked cards | Compact priority columns only (e.g. PID, Name, CPU, RAM); secondary hidden | **100vw full-screen overlay sheet** with sticky close/back bar |

---

## 12. Accessibility & Usability (a11y)

1. **Accessible Contrast Ratios**:
   - All foreground text against `zinc-950` and `zinc-900` surfaces exceeds WCAG AA contrast (4.5:1 for normal text, 3:1 for large text and UI components).
2. **Color Independence**:
   - Status indicators never rely on color alone. Every badge includes an explicit textual label (`ACTIVE`, `FAILED`, `WARMING UP`, `HEALTHY`, `RECONNECTING`, `PAUSED`).
3. **Keyboard Navigation & Focus Management**:
   - Drawers trap focus when open; pressing `Escape` closes the drawer and restores focus to the triggering element.
   - All interactive controls have distinct `focus-visible:ring-1 focus-visible:ring-zinc-400` focus indicators.
4. **Semantic Structure**:
   - Proper landmark elements: `<header>`, `<nav>`, `<main>`, `<aside>`, `<section>`.
   - Data tables use standard `<table>`, `<thead>`, `<tbody>`, `<th scope="col">`, and `<th scope="row">` tags.

---

## 13. Testing Strategy

### 13.1 Test Framework & Runner
In accordance with modern Angular standards and avoiding unnecessary test-runner abstractions:
- **Test Runner**: Angular CLI default unit-test tooling.
- **Engine**: Vitest with `jsdom` for fast, lightweight in-memory unit and component testing.
- **Browser-Mode Testing**: Reserved strictly for tests where real browser APIs (such as `EventSource` connection lifecycles, focus trapping, or CSS media query drawer transitions) are explicitly under test.

### 13.2 Unit & Component Test Suite Scope
1. **API Client & Error Interceptor**:
   - Verify unmarshaling of standard `ApiErrorEnvelope` for 400, 401, 404, 429, 500, and 503 HTTP statuses.
   - Verify extraction of `Retry-After` header when receiving HTTP 429 `RATE_LIMITED`.
2. **Realtime SSE Service**:
   - Test `EventSource` event routing (`open`, `error`, `metric_pulse`).
   - Verify `MetricPulse` deserialization and Signal synchronization.
   - Verify proper resource cleanup and stream closure upon user pause or component destruction.
   - Verify that SSE error transitions `streamStatus` to `'reconnecting'` without mutating `agentStatus`.
3. **Ring Buffer & Telemetry State**:
   - Test array capping at exactly 60 samples.
   - Verify `TelemetrySample<T>` timestamp preservation.
   - Verify handling of `warming_up` state (`null` CPU percentage) without NaN or crash, preserving gaps in charts.
4. **Feature Component Testing**:
   - `OverviewComponent`: Correct rendering of live vitals, disk meters, network throughput, top processes by CPU, and distinct `agentStatus` / `streamStatus` badges.
   - `ProcessesComponent`: Correct query parameter dispatch on sort and limit changes; drawer opening on row selection.
   - `ServicesComponent`: Verification that `main_pid` is rendered as a link only when $\ge 1$.
   - `ContainersComponent`: Verification of intentional 503 `DOCKER_UNAVAILABLE` empty state rendering versus container table rendering.
   - `DrawerComponent`: Verification of `Escape` key handling and query parameter synchronization.

---

## 14. Explicit Non-Goals & Invariants (v1)

- ❌ **No Backend Code Changes**: Zero modifications to C++20 Drogon controllers, services, collectors, or CMake files.
- ❌ **No Invented Backend Endpoints**: Frontend exclusively uses endpoints documented in `docs/api/API.md`.
- ❌ **No Direct Host Mutation**: No buttons or actions to kill processes, restart services, reboot the host, or mutate containers.
- ❌ **No In-Browser Historical Database Queries**: The PostgreSQL repository in Phase 14 is an internal agent sink; the frontend does not attempt to query historical PostgreSQL tables.
- ❌ **No Raw Prometheus UI**: The `/metrics` endpoint is reserved for Prometheus/VictoriaMetrics scrapers, not human console inspection.
- ❌ **No In-Browser API Key Input / Storage**: Angular never prompts for, stores, or transmits API keys; authentication is strictly owned by the upstream reverse proxy.
- ❌ **No Third-Party Charting Kits**: Charting is implemented via lightweight, native SVG components bound to Signal rolling buffers.
- ❌ **No Multi-Host Switcher**: NodePulse v1 is purpose-built as an individual host operator console.
