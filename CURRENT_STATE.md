# NodePulse — Current Project State

## Project Metadata
- **Project**: NodePulse
- **Description**: Lightweight Linux server monitoring and management agent written in C++20 using Drogon
- **Status**: Process & Service Collector implemented, hardened, and verified with native `/proc/<pid>` parsing (stat with comm parentheses/space handling, status, sanitized cmdline), systemd D-Bus inspection via dynamic `sd-bus` runtime loading, async worker thread pool offloading (DEC-013), background delta process CPU sampling, query filtering and limit validation, and standard Drogon endpoint integration
- **Current Implementation Phase**: Phase 8 — Process & Service Collector
- **Next Approved Phase**: Phase 9 — Authentication (`X-API-Key` middleware & validation)
- **Production Ready**: No
- **Active Git Branch**: `master`
- **Current Implementation Exists**: Yes (build system, logger, config loader, server lifecycle, health endpoint, system collector, CPU collector & endpoint, memory collector & endpoint, disk collector & endpoint, network collector & endpoint, process collector & endpoints, service collector & endpoints, error responses, unit & integration tests)

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
| **Phase 9** | Authentication (`X-API-Key` middleware & validation) | PENDING | **YES (Next Approved)** |
| **Phase 10** | Rate Limiting (Token-bucket / Leaky-bucket middleware) | PENDING | NO |
| **Phase 11** | Docker Integration (`/var/run/docker.sock` client) | PENDING | NO |
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
  - `include/nodepulse/server/server.hpp` & `src/server/server.cpp`: Drogon server lifecycle manager, loopback-only network binding enforcement, validated Request ID injector, access logger, background CPU, Network, and Process sampling lifecycle orchestration, and centralized 404/exception handlers
  - `apps/server/main.cpp`: Application entry point with CLI parsing (`--config`, `--validate-config`, `--version`, `--help`)
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
  - `tests/unit/system_collector_test.cpp`: Stream parsers, fallback behaviors, and fixture-driven system collector and service tests (21 tests)
  - `tests/unit/cpu_collector_test.cpp`: Stream parsers, warming up baseline handling (DEC-014), delta calculations, wrap/reset recovery, zero elapsed time caching, hotplug core count detection, steal time accounting, background sampling thread validation, and missing file error handling (21 tests)
  - `tests/unit/memory_collector_test.cpp`: Stream parser verification, exact byte conversions, zero swap resilience, MemAvailable fallback, malformed input rejection, integer overflow prevention, arithmetic underflow prevention, fixture files, live host sanity, and service delegation tests (20 tests)
  - `tests/unit/disk_collector_test.cpp`: Octal unescaping, mount stream parser, duplicate mount point deduplication, malformed line handling, pseudo-filesystem filtering, normal metric calculations, zero-capacity handling, clamping and safety, integer overflow protection, mock statvfs fixture testing, inaccessible/disappearing mount handling, nonexistent mounts file error handling, live host disk sanity, service delegation, and async worker thread offload tests (17 tests)
  - `tests/unit/network_collector_test.cpp`: Stream parser validation, whitespace variance, malformed/non-numeric row rejection, uint64 maximum boundary, sysfs enrichment (`address`, `operstate`, `speed`), fallback on missing sysfs files, rate delta calculations, warming up baseline handling, counter wrap recovery, hotplug and interface removal handling, background sampling thread lifecycle, and live Linux host sanity tests (16 tests)
  - `tests/unit/process_collector_test.cpp`: Stat line stream parser (including complex names with spaces and nested parentheses), status stream parser (`Uid`, `VmRSS`, `VmSize`, `Threads`), cmdline sanitization and truncation, PID validation (`pid >= 1`, upper bound `/proc/sys/kernel/pid_max`), process sorting (`cpu`, `memory`, `pid`), limit truncation, and fixture-driven collector tests (8 tests)
  - `tests/unit/service_collector_test.cpp`: Unit name validation regex (`^[a-zA-Z0-9_\-\.\@]+$`), unit name normalization (auto `.service` append), mock D-Bus listing by state (`active`, `inactive`, `failed`, `all`), mock service detail query, and error handling (5 tests)
  - `tests/integration/http_integration_test.cpp`: In-process Drogon HTTP tests verifying health probe, system endpoint contract, cpu endpoint contract, memory endpoint contract, disks endpoint contract, network endpoint contract, processes endpoint contract (`sort`, `limit`, 400 validation), process detail endpoint contract (`pid >= 1`, 400 validation, 404 not found), services endpoint contract (`state`, `limit`, 400 validation, 500 failure), service detail endpoint contract (regex validation, 400, 404, 500), concurrency across all 8 endpoints (24 concurrent threads), single Content-Type emission regression across all endpoints and error paths, and loopback binding security enforcement (27 integration tests)

---

## Phase 8 Verification and Gate Status
- **Exit Gate Criteria**:
  - [x] Stream parser implemented for `/proc/<pid>/stat` safely extracting comm between first `(` and last `)`, properly handling processes with spaces and nested parentheses in process names.
  - [x] Stream parser implemented for `/proc/<pid>/status` safely extracting `Uid`, `VmRSS`, `VmSize`, and `Threads`.
  - [x] Cmdline sanitizer implemented converting null bytes to spaces and truncating at 4096 bytes, with fallback to `comm` for kernel threads.
  - [x] Native Linux interfaces utilized directly (`/proc/<pid>`, `/proc/sys/kernel/pid_max`, `/run/dbus/system_bus_socket`); zero shell commands (`ps`, `top`, `pgrep`, `awk`, `grep`, `systemctl`, `journalctl`).
  - [x] Systemd D-Bus communication implemented via dynamic runtime resolution (`dlopen`/`dlsym`) of `libsystemd.so.0`, providing zero-dependency compilation and graceful fallback when systemd is unavailable.
  - [x] Process and service requests offloaded asynchronously to `trantor::ConcurrentTaskQueue` (DEC-013), eliminating blocking I/O on Drogon's reactive event loop.
  - [x] Dedicated background sampling thread implemented for `ProcessService` (1000ms interval) calculating per-process delta CPU utilization without request stalls.
  - [x] Query parameters `sort` (`cpu`, `memory`, `pid`) and `limit` (`1..200`) validated with standard 400 `INVALID_REQUEST` responses on invalid inputs.
  - [x] Single process lookup (`GET /api/v1/processes/{pid}`) strictly validates positive integer (`pid >= 1`) against `/proc/sys/kernel/pid_max`, returning 400 on invalid, 404 `RESOURCE_NOT_FOUND` when not found, and 200 with all 13 documented fields.
  - [x] Single service lookup (`GET /api/v1/services/{name}`) validates name against `^[a-zA-Z0-9_\-\.\@]+$`, auto-appends `.service` suffix, returns 400 on invalid format, 404 on `NoSuchUnit`, and 500 on D-Bus communication failure.
  - [x] Single `Content-Type: application/json; charset=utf-8` header emission verified on all process and service endpoints (both success and error responses).
  - [x] 100% test pass rate across unit and integration test suites (151/151 passed).
  - [x] Zero warnings or errors under `-Wall -Wextra -Wpedantic -Werror`.
  - [x] `format-check` passes with zero violations.
  - [x] `tidy` passes with zero errors and zero warnings across all 27 source files.
  - [x] Live HTTP smoke test verified with `curl` for all 4 new endpoints (`/api/v1/processes`, `/api/v1/processes/{pid}`, `/api/v1/services`, `/api/v1/services/{name}`) alongside prior endpoints, 400 validation, and 404 not found handling.
- **Verification Commands Executed**:
  ```bash
  cmake --build build -- -j
  ctest --test-dir build --output-on-failure
  cmake --build build --target format-check
  cmake --build build --target tidy
  ./build/apps/server/nodepulse_server -c config/nodepulse_smoke.json &
  curl -s -i "http://127.0.0.1:8089/api/v1/processes?sort=cpu&limit=2"
  curl -s -i "http://127.0.0.1:8089/api/v1/processes/$PID"
  curl -s -i "http://127.0.0.1:8089/api/v1/services?state=active&limit=2"
  curl -s -i "http://127.0.0.1:8089/api/v1/services/cron.service"
  ```
- **Verification Results**:
  - Build: Succeeded cleanly (all targets built under `-Wall -Wextra -Wpedantic -Werror`).
  - Tests: 151/151 passed (100% pass rate).
  - Format check: Succeeded (0 violations).
  - Static analysis: Succeeded (0 errors, 0 warnings across all 27 source files).
  - Live HTTP smoke test: Verified single `content-type` emission, HTTP 200 OK with process array and service array, 400 invalid parameter handling, 404 resource not found handling, and graceful server shutdown.
- **Confirmation**: Phase 9 (Authentication) has NOT been started.
