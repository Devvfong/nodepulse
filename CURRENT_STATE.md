# NodePulse — Current Project State

## Project Metadata
- **Project**: NodePulse
- **Description**: Lightweight Linux server monitoring and management agent written in C++20 using Drogon
- **Status**: Rate Limiting implemented and verified with in-memory token-bucket limiter (`nodepulse::middleware::RateLimiter`), Drogon `RateLimitFilter`, AOP post-routing advice pipeline integration (Auth -> Rate Limiting), per-client IP address tracking (`req->peerAddr().toIp()`), deterministic idle state eviction and bounded memory cap (10,000 max entries), Retry-After header calculation, standard 429 `RATE_LIMITED` envelope, and full health route rate-limiting policy enforcement
- **Current Implementation Phase**: Phase 10 — Rate Limiting (Token-bucket middleware & HTTP 429 enforcement)
- **Next Approved Phase**: Phase 11 — Docker Integration (`/var/run/docker.sock` client)
- **Production Ready**: No
- **Active Git Branch**: `master`
- **Current Implementation Exists**: Yes (build system, logger, config loader, server lifecycle, health endpoint, system collector, CPU collector & endpoint, memory collector & endpoint, disk collector & endpoint, network collector & endpoint, process collector & endpoints, service collector & endpoints, auth filter & constant-time security helper, rate limit filter & token-bucket limiter, error responses, unit & integration tests)

---

## Phase Status Summary

| Phase | Description | Status | Approved to Start |
|---|---|---|---|
| **Phase 0** | Documentation & Architectural Specification | **COMPLETED** | N/A |
| **Phase 1** | Repository Foundation (CMake, Clang-Tooling, GTest) | **COMPLETED** | N/A |
| **Phase 2** | HTTP Foundation (Drogon setup, Health Check, JSON handling) | **COMPLETED** | N/A |
| **Phase 3** | System Collector (`/proc/uptime`, `/etc/os-release`, uname) | **COMPLETED** | N/A |
| **Phase 4** | CPU Collector (`/proc/stat`, `/proc/loadavg`, `/proc/cpuinfo`) | **COMPLETED** | N/A |
| **Phase 5** | Memory Collector (`/proc/meminfo`, virtual memory & swap) | **COMPLETED** | N/A |
| **Phase 6** | Disk Collector (`/proc/mounts`, `statvfs`) | **COMPLETED** | N/A |
| **Phase 7** | Network Collector (`/proc/net/dev`, `/sys/class/net`) | **COMPLETED** | N/A |
| **Phase 8** | Process & Service Collector (`/proc/<pid>`, systemd service inspection) | **COMPLETED** | N/A |
| **Phase 9** | Authentication (`X-API-Key` middleware & validation) | **COMPLETED** | N/A |
| **Phase 10** | Rate Limiting (Token-bucket / Leaky-bucket middleware) | **COMPLETED** | N/A |
| **Phase 11** | Docker Integration (`/var/run/docker.sock` client) | PENDING | **YES (Next Approved)** |
| **Phase 12** | SSE Live Metrics (`/api/v1/events` streaming channel) | PENDING | NO |
| **Phase 13** | Prometheus Exposition (`/metrics` scrape endpoint) | PENDING | NO |
| **Phase 14** | PostgreSQL Metric History (libpqxx repository & storage) | PENDING | NO |
| **Phase 15** | Operations (systemd unit, packaging, config validator) | PENDING | NO |
| **Phase 16** | Production Readiness (Sanitizers, Benchmarks, Security Audit) | PENDING | NO |

---

## Current Artifact Inventory
- **Build Configuration**:
  - `CMakeLists.txt`: Root target-based CMake configuration (C++20, `-Wall -Wextra -Wpedantic -Werror`, quality targets, minimum CMake 3.22, local user library directories and build RPATH)
  - `src/CMakeLists.txt`: Static library `nodepulse_lib` definition linking `Drogon`, `spdlog`, `json`, and `${CMAKE_DL_LIBS}`
  - `apps/CMakeLists.txt`: Applications directory definition
  - `apps/server/CMakeLists.txt`: `nodepulse_server` executable target definition with backwards-compatible `WHOLE_ARCHIVE` linking (CMake 3.24+ `$<LINK_LIBRARY:WHOLE_ARCHIVE,...>` and CMake >= 3.22 `-Wl,--whole-archive`)
  - `tests/CMakeLists.txt`: `tests_unit` and `tests_integration` executables with GoogleTest discovery and test fixtures definitions
- **Quality Tooling & Configuration**:
  - `.gitignore`: Ignore build artifacts, IDE configs, coverage, logs
  - `.clang-format`: Formatter rules conforming to project coding standards (Clang-Format 18)
  - `.clang-tidy`: Static analysis rules with warnings-as-errors (Clang-Tidy 18)
  - `config/config.example.json`: Sample configuration file (binds to localhost by default, `cpu.sample_interval_ms: 1000`, `disks.ignored_fstypes: [...]`)
  - Targets: `format-check`, `format-fix`, `tidy`
- **Continuous Integration**:
  - `.github/workflows/ci.yml`: Matrix build testing GCC and Clang, unit & integration tests, formatting, and clang-tidy
- **Source Code**:
  - `include/nodepulse/domain/system_info.hpp`: Domain model struct for system metadata
  - `include/nodepulse/domain/cpu_info.hpp`: Domain model structs for CPU metrics (`LoadAverage`, `CpuCoreMetrics`, `CpuMetrics`) with `std::optional<double> usage_percent` and `measurement_status`
  - `include/nodepulse/domain/memory_info.hpp`: Domain model struct for memory and swap metrics (`MemoryMetrics`)
  - `include/nodepulse/domain/disk_info.hpp`: Domain model struct for filesystem and partition metrics (`DiskPartitionMetrics`)
  - `include/nodepulse/domain/network_info.hpp`: Domain model struct for network interface metrics (`NetworkInterfaceMetrics`) with cumulative counters and delta bandwidth rates (`rx_bytes_per_sec`, `tx_bytes_per_sec`)
  - `include/nodepulse/domain/process_info.hpp`: Domain model structs for process metrics (`ProcessInfo`, `ProcessDetail`)
  - `include/nodepulse/domain/service_info.hpp`: Domain model structs for systemd service metrics (`ServiceInfo`, `ServiceDetail`)
  - `include/nodepulse/utils/security.hpp` & `src/utils/security.cpp`: Timing side-channel resistant string comparison utility `nodepulse::utils::constant_time_equals` evaluating differences in bounded execution time proportional only to the expected secret length
  - `include/nodepulse/middleware/auth_filter.hpp` & `src/middleware/auth_filter.cpp`: Drogon `HttpFilter` and AOP post-routing advice interceptor enforcing `X-API-Key` authentication with health probe exemption and structured 401 error envelope responses
  - `include/nodepulse/middleware/rate_limit_filter.hpp` & `src/middleware/rate_limit_filter.cpp`: Thread-safe in-memory token-bucket rate limiter (`RateLimiter`) and Drogon `RateLimitFilter` tracking client IP addresses (`req->peerAddr().toIp()`), with deterministic stale entry cleanup, bounded in-memory state (10,000 max entries), `Retry-After` header generation, and standard 429 `RATE_LIMITED` JSON error responses
  - `include/nodepulse/collectors/system_collector.hpp` & `src/collectors/system_collector.cpp`: Collector parsing `/proc/uptime`, `/proc/stat` (`btime`), `/etc/os-release`, and invoking `gethostname()` / `uname()`
  - `include/nodepulse/collectors/cpu_collector.hpp` & `src/collectors/cpu_collector.cpp`: Collector parsing `/proc/stat`, `/proc/loadavg`, and `/proc/cpuinfo`
  - `include/nodepulse/collectors/memory_collector.hpp` & `src/collectors/memory_collector.cpp`: Collector parsing `/proc/meminfo` with kibibytes-to-bytes conversion, `MemAvailable` fallback, zero-swap safeguards, and integer overflow checks
  - `include/nodepulse/collectors/disk_collector.hpp` & `src/collectors/disk_collector.cpp`: Collector parsing `/proc/mounts`, octal unescaping, pseudo-fs filtering, deduplication, and invoking `statvfs()` with distinct unprivileged available capacity
  - `include/nodepulse/collectors/network_collector.hpp` & `src/collectors/network_collector.cpp`: Collector parsing `/proc/net/dev` with token-based `std::from_chars` validation, whitespace handling, and `/sys/class/net` sysfs attribute enrichment (`address`, `operstate`, `speed`)
  - `include/nodepulse/collectors/process_collector.hpp` & `src/collectors/process_collector.cpp`: Collector inspecting numeric `/proc/[0-9]+` directories, parsing `/proc/<pid>/stat` (handling arbitrary spaces and nested parentheses in comm), `/proc/<pid>/status` (`Uid`, `VmRSS`, `VmSize`, `Threads`), sanitized cmdline reading (null-separated strings converted to spaces, capped at 4096 bytes), open file descriptor counting via `/proc/<pid>/fd`, and working directory inspection via `/proc/<pid>/cwd`
  - `include/nodepulse/collectors/service_collector.hpp` & `src/collectors/service_collector.cpp`: Collector communicating directly with systemd via D-Bus using dynamic runtime binding (`dlopen`/`dlsym`) of `libsystemd.so.0` (`ListUnits`, `GetUnit`, `sd_bus_get_property_*`), regex validation for unit names, automatic `.service` suffix normalization, and custom mock provider support for deterministic testing
  - `include/nodepulse/services/system_service.hpp` & `src/services/system_service.cpp`: Service layer coordinating system collection
  - `include/nodepulse/services/cpu_service.hpp` & `src/services/cpu_service.cpp`: Service layer coordinating CPU snapshot delta calculations, non-blocking background sampling thread, explicit `warming_up` state, counter wrap recovery, CPU hotplug handling, and zero event loop starvation
  - `include/nodepulse/services/memory_service.hpp` & `src/services/memory_service.cpp`: Service layer coordinating memory collection
  - `include/nodepulse/services/disk_service.hpp` & `src/services/disk_service.cpp`: Service layer coordinating disk collection with worker thread offload
  - `include/nodepulse/services/network_service.hpp` & `src/services/network_service.cpp`: Service layer coordinating network metrics caching, background delta rate calculation thread, initial `null` rates for warming up and hotplugged interfaces, counter wrap recovery, and thread-safe snapshot retrieval
  - `include/nodepulse/services/process_service.hpp` & `src/services/process_service.cpp`: Service layer coordinating background delta process CPU sampling thread (1000ms), sort (`cpu`, `memory`, `pid`) and limit handling, async worker queue offloading (`trantor::ConcurrentTaskQueue`, DEC-013), and PID bounds checking against `/proc/sys/kernel/pid_max`
  - `include/nodepulse/services/service_manager_service.hpp` & `src/services/service_manager_service.cpp`: Service layer coordinating service querying, state filtering (`active`, `inactive`, `failed`, `all`), limit capping, and async worker queue offloading
  - `include/nodepulse/controllers/system_controller.hpp` & `src/controllers/system_controller.cpp`: Drogon controller exposing `GET /api/v1/system`
  - `include/nodepulse/controllers/cpu_controller.hpp` & `src/controllers/cpu_controller.cpp`: Drogon controller exposing `GET /api/v1/cpu`
  - `include/nodepulse/controllers/memory_controller.hpp` & `src/controllers/memory_controller.cpp`: Drogon controller exposing `GET /api/v1/memory`
  - `include/nodepulse/controllers/disk_controller.hpp` & `src/controllers/disk_controller.cpp`: Drogon controller exposing `GET /api/v1/disks`
  - `include/nodepulse/controllers/network_controller.hpp` & `src/controllers/network_controller.cpp`: Drogon controller exposing `GET /api/v1/network`
  - `include/nodepulse/controllers/process_controller.hpp` & `src/controllers/process_controller.cpp`: Drogon controller exposing `GET /api/v1/processes` and `GET /api/v1/processes/{pid}` with async worker offloading, query parameter validation, and strictly typed JSON error responses
  - `include/nodepulse/controllers/service_controller.hpp` & `src/controllers/service_controller.cpp`: Drogon controller exposing `GET /api/v1/services` and `GET /api/v1/services/{name}` with async worker offloading, unit name regex validation, and error handling
  - `include/nodepulse/config/config.hpp` & `src/config/config.cpp`: Configuration domain structs, JSON loader, environment overrides, and schema validation
  - `include/nodepulse/utils/error_response.hpp` & `src/utils/error_response.cpp`: Standard JSON error envelope generator and error code taxonomy
  - `include/nodepulse/utils/logger.hpp` & `src/utils/logger.cpp`: Logger abstraction wrapping spdlog with custom and structured JSON patterns
  - `include/nodepulse/controllers/health_controller.hpp` & `src/controllers/health_controller.cpp`: Drogon controller for `GET /api/v1/health`
  - `include/nodepulse/server/server.hpp` & `src/server/server.cpp`: Drogon server lifecycle manager, loopback-only network binding enforcement, empty API key startup rejection, validated Request ID injector, access logger, post-routing auth advice registration, background CPU, Network, and Process sampling lifecycle orchestration, and centralized 404/exception handlers
  - `apps/server/main.cpp`: Application entry point with CLI parsing (`--config`, `--validate-config`, `--version`, `--help`) and empty API key startup validation
- **Test Fixtures**:
  - `tests/fixtures/proc/uptime`: Sample `/proc/uptime`
  - `tests/fixtures/proc/stat`: Sample `/proc/stat` containing `btime` line
  - `tests/fixtures/proc/stat_sample1`: Baseline sample with 10,000 total jiffies
  - `tests/fixtures/proc/stat_sample2`: Delta sample with 50% aggregate usage, 60% core0, 40% core1
  - `tests/fixtures/proc/stat_sample3`: 100% busy scenario
  - `tests/fixtures/proc/stat_sample_reset`: Smaller counters simulating counter wrap / reset
  - `tests/fixtures/proc/loadavg`: Sample `/proc/loadavg`
  - `tests/fixtures/proc/cpuinfo`: Multi-core sample `/proc/cpuinfo`
  - `tests/fixtures/proc/meminfo_valid`: Realistic `/proc/meminfo` matching API doc example
  - `tests/fixtures/proc/meminfo_zero_swap`: `/proc/meminfo` fixture with zero swap configured
  - `tests/fixtures/proc/meminfo_missing_available`: `/proc/meminfo` fixture missing `MemAvailable` (legacy kernel) with used swap
  - `tests/fixtures/proc/meminfo_malformed`: Malformed/corrupted `/proc/meminfo`
  - `tests/fixtures/proc/meminfo_overflow`: `/proc/meminfo` fixture with value exceeding `UINT64_MAX / 1024ULL`
  - `tests/fixtures/proc/mounts_valid`: Realistic `/proc/mounts` with root ext4, boot vfat, loop squashfs, tmpfs, proc, sysfs
  - `tests/fixtures/proc/mounts_escaped`: Sample `/proc/mounts` with octal-escaped mount points (`\040` spaces, `\011` tabs, `\134` backslashes)
  - `tests/fixtures/proc/mounts_duplicate`: Sample `/proc/mounts` with shadowed/duplicate mount points
  - `tests/fixtures/proc/mounts_malformed`: Malformed `/proc/mounts` lines (missing fields, blank lines)
  - `tests/fixtures/proc/net_dev_valid`: Sample `/proc/net/dev` with `lo`, `eth0`, `wlan0`
  - `tests/fixtures/proc/net_dev_spaces`: Sample `/proc/net/dev` with non-uniform whitespace column alignment
  - `tests/fixtures/proc/net_dev_malformed`: Sample `/proc/net/dev` with invalid rows (fewer columns, non-numeric tokens, invalid characters)
  - `tests/fixtures/proc/net_dev_overflow`: Sample `/proc/net/dev` with near-`UINT64_MAX` counters
  - `tests/fixtures/proc/net_dev_sample1`: Delta rate baseline fixture
  - `tests/fixtures/proc/net_dev_sample2`: Delta rate second sample fixture (+100k RX bytes, +200k TX bytes)
  - `tests/fixtures/proc/net_dev_sample_reset`: Delta rate fixture with smaller counter values simulating counter reset/reboot
  - `tests/fixtures/proc/1248/stat`, `status`, `cmdline`: Sample process fixtures for normal server process
  - `tests/fixtures/proc/2345/stat`, `status`, `cmdline`: Sample process fixtures with spaces and parentheses in comm `(proc (with) spaces)`
  - `tests/fixtures/proc/2/stat`, `status`, `cmdline`: Sample process fixtures for kernel thread with empty cmdline
  - `tests/fixtures/sys/class/net/eth0/*` & `lo/*`: Mock sysfs attributes (`address`, `operstate`, `speed`)
  - `tests/fixtures/etc/os-release`: Sample `/etc/os-release`
- **Tests**:
  - `tests/unit/smoke_test.cpp`: Test harness and basic JSON serialization tests (3 tests)
  - `tests/unit/config_test.cpp`: Config defaults, JSON parsing, validation constraints, and env override tests (10 tests)
  - `tests/unit/error_response_test.cpp`: Standard error JSON structure, timestamping, and HTTP response content type tests (3 tests)
  - `tests/unit/security_test.cpp`: Timing side-channel resistant string comparison tests covering exact matches, single-byte variances at prefix/middle/suffix, length mismatches (shorter/longer), empty inputs, oversized payloads, case sensitivity, and binary content with null bytes (11 tests)
  - `tests/unit/auth_filter_test.cpp`: Path exemption rule checks, null request handling, unconfigured key rejection, valid key validation (including case insensitivity), invalid and empty key rejection, and mock request interception with 401 error envelope generation (9 tests)
  - `tests/unit/system_collector_test.cpp`: Stream parsers, fallback behaviors, and fixture-driven system collector and service tests (21 tests)
  - `tests/unit/cpu_collector_test.cpp`: Stream parsers, warming up baseline handling (DEC-014), delta calculations, wrap/reset recovery, zero elapsed time caching, hotplug core count detection, steal time accounting, background sampling thread validation, and missing file error handling (21 tests)
  - `tests/unit/memory_collector_test.cpp`: Stream parser verification, exact byte conversions, zero swap resilience, MemAvailable fallback, malformed input rejection, integer overflow prevention, arithmetic underflow prevention, fixture files, live host sanity, and service delegation tests (20 tests)
  - `tests/unit/disk_collector_test.cpp`: Octal unescaping, mount stream parser, duplicate mount point deduplication, malformed line handling, pseudo-filesystem filtering, normal metric calculations, zero-capacity handling, clamping and safety, integer overflow protection, mock statvfs fixture testing, inaccessible/disappearing mount handling, nonexistent mounts file error handling, live host disk sanity, service delegation, and async worker thread offload tests (17 tests)
  - `tests/unit/network_collector_test.cpp`: Stream parser validation, whitespace variance, malformed/non-numeric row rejection, uint64 maximum boundary, sysfs enrichment (`address`, `operstate`, `speed`), fallback on missing sysfs files, rate delta calculations, warming up baseline handling, counter wrap recovery, hotplug and interface removal handling, background sampling thread lifecycle, and live Linux host sanity tests (16 tests)
  - `tests/unit/process_collector_test.cpp`: Stat line stream parser (including complex names with spaces and nested parentheses), status stream parser (`Uid`, `VmRSS`, `VmSize`, `Threads`), cmdline sanitization and truncation, PID validation (`pid >= 1`, upper bound `/proc/sys/kernel/pid_max`), process sorting (`cpu`, `memory`, `pid`), limit truncation, and fixture-driven collector tests (8 tests)
  - `tests/unit/service_collector_test.cpp`: Unit name validation regex (`^[a-zA-Z0-9_\-\.\@]+$`), unit name normalization (auto `.service` append), mock D-Bus listing by state (`active`, `inactive`, `failed`, `all`), mock service detail query, and error handling (5 tests)
  - `tests/unit/rate_limit_filter_test.cpp`: Requests below limit, exact boundary behavior, immediate 429 rejection on burst exhaustion, window recovery, full refill capacity restoration, independent client IP isolation, stale entry cleanup, reset clearing, concurrent thread safety (16 threads), disabled filter pass-through, HTTP 429 envelope format, and simulated clock time advance recovery (12 tests)
  - `tests/integration/http_integration_test.cpp`: In-process Drogon HTTP tests verifying health probe exemption, authenticated access across all 9 protected routes, missing API key rejection (401), invalid API key rejection (401), empty and oversized key header rejection (401), case-insensitive header lookup (`x-api-key`), unknown route 404 preservation (including routes resembling /health), concurrent authenticated and unauthenticated queries, server startup empty key rejection, rate limit requests below limit, burst exhaustion returning HTTP 429 with `Retry-After` and standard JSON envelope, health endpoint rate-limiting policy compliance, auth evaluation prior to rate limiting, simulated time recovery, and disabled rate limiting pass-through (42 integration tests)

---

## Phase 10 Verification and Gate Status
- **Exit Gate Criteria**:
  - [x] Thread-safe in-memory token-bucket rate limiter implemented (`nodepulse::middleware::RateLimiter`) with configurable requests-per-minute and burst capacity.
  - [x] Rate limiting middleware implemented (`nodepulse::middleware::RateLimitFilter`) inheriting from `drogon::HttpFilter<RateLimitFilter>` and centrally chained in Drogon AOP post-routing advice pipeline.
  - [x] Middleware ordering strictly enforced: Authentication is evaluated prior to rate limiting (`DATA_FLOW.md`); unauthenticated or invalid API key requests fail with 401 `UNAUTHORIZED` without consuming rate limit quota.
  - [x] Client identity strictly grounded in client IP address (`req->peerAddr().toIp()`) without trusting untrusted `X-Forwarded-For` headers.
  - [x] Loopback-only network binding restriction (`127.0.0.1`, `::1`, `localhost`) strictly preserved.
  - [x] Bounded in-memory state with strict 10,000 maximum entries cap and deterministic stale entry cleanup based on elapsed refill duration.
  - [x] Injectable / mockable clock function supported (`RateLimiter::ClockFunc`) enabling 100% deterministic, non-flaky sub-millisecond testing without sleeps.
  - [x] Health probe `GET /api/v1/health` subjected to rate limiting per documented policy without unauthorized exemption.
  - [x] HTTP 429 `RATE_LIMITED` emitted on burst exhaustion with standard JSON error envelope (`error.code: "RATE_LIMITED"`, non-empty timestamp, `X-Request-ID`, `details[0].retry_after_seconds`).
  - [x] Response headers on 429: exactly one `Content-Type: application/json; charset=utf-8`, standard `X-Request-ID`, and `Retry-After: <seconds>` indicating backoff duration.
  - [x] Zero secret exposure: API keys and internal limiter state are never logged, never emitted in JSON error payloads, and never printed.
  - [x] 100% test pass rate across unit and integration test suites (202/202 passed).
  - [x] Zero compiler warnings or errors under `-Wall -Wextra -Wpedantic -Werror`.
  - [x] `format-check` passes with zero violations.
  - [x] `tidy` passes with zero warnings or errors across all 30 source files.
  - [x] Live HTTP smoke test verified with `curl` for requests below limit (200), burst exhaustion (429), `Retry-After: 1` header, standard error envelope, token replenishment recovery, health endpoint rate limiting, and auth-before-rate-limit 401 priority.
- **Verification Commands Executed**:
  ```bash
  cmake --build build -- -j
  ctest --test-dir build --output-on-failure
  cmake --build build --target format-check
  cmake --build build --target tidy
  ./build/apps/server/nodepulse_server -c /tmp/nodepulse_smoke_ratelimit.json &
  curl -s -i -H "X-API-Key: <valid>" "http://127.0.0.1:18099/api/v1/system" # Returns 200 OK
  # Burst exhaustion:
  curl -s -i -H "X-API-Key: <valid>" "http://127.0.0.1:18099/api/v1/system" # Returns 429 Too Many Requests (Retry-After: 1)
  # Auth priority:
  curl -s -i -H "X-API-Key: wrong" "http://127.0.0.1:18099/api/v1/system" # Returns 401 Unauthorized
  # Health rate limiting:
  curl -s -i "http://127.0.0.1:18099/api/v1/health" # Returns 429 Too Many Requests
  ```
- **Verification Results**:
  - Build: Succeeded cleanly (all targets built under `-Wall -Wextra -Wpedantic -Werror`).
  - Tests: 202/202 passed (100% pass rate).
  - Format check: Succeeded (0 violations).
  - Static analysis: Succeeded (0 errors, 0 warnings across all 30 source files).
  - Live HTTP smoke test: Verified burst handling, HTTP 429 status, `Retry-After` header, standard error envelope, token replenishment, and auth evaluation priority.
- **Confirmation**: Phase 11 (Docker Integration) has NOT been started.
