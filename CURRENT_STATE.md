# NodePulse — Current Project State

## Project Metadata
- **Project**: NodePulse
- **Description**: Lightweight Linux server monitoring and management agent written in C++20 using Drogon
- **Status**: Disk Collector implemented, hardened, and verified with native /proc/mounts and statvfs() integration, unprivileged available capacity accounting, octal unescaping, duplicate mount deduplication, zero-capacity handling, and integer overflow prevention
- **Current Implementation Phase**: Phase 6 — Disk Collector
- **Next Approved Phase**: Phase 7 — Network Collector
- **Production Ready**: No
- **Active Git Branch**: `master`
- **Current Implementation Exists**: Yes (build system, logger, config loader, server lifecycle, health endpoint, system collector, CPU collector & endpoint, memory collector & endpoint, disk collector & endpoint, error responses, unit & integration tests)

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
| **Phase 7** | Network Collector (`/proc/net/dev`, `/sys/class/net`) | PENDING | **YES (Next Approved)** |
| **Phase 8** | Process & Service Collector (`/proc/<pid>`, systemd service inspection) | PENDING | NO |
| **Phase 9** | Authentication (`X-API-Key` middleware & validation) | PENDING | NO |
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
  - `CMakeLists.txt`: Root target-based CMake configuration (C++20, `-Wall -Wextra -Wpedantic -Werror`, quality targets, minimum CMake 3.22)
  - `src/CMakeLists.txt`: Static library `nodepulse_lib` definition
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
  - `include/nodepulse/collectors/system_collector.hpp` & `src/collectors/system_collector.cpp`: Collector parsing `/proc/uptime`, `/proc/stat` (`btime`), `/etc/os-release`, and invoking `gethostname()` / `uname()`
  - `include/nodepulse/collectors/cpu_collector.hpp` & `src/collectors/cpu_collector.cpp`: Collector parsing `/proc/stat`, `/proc/loadavg`, and `/proc/cpuinfo`
  - `include/nodepulse/collectors/memory_collector.hpp` & `src/collectors/memory_collector.cpp`: Collector parsing `/proc/meminfo` with kibibytes-to-bytes conversion, `MemAvailable` fallback, zero-swap safeguards, and integer overflow checks
  - `include/nodepulse/collectors/disk_collector.hpp` & `src/collectors/disk_collector.cpp`: Collector parsing `/proc/mounts`, octal unescaping, pseudo-fs filtering, deduplication, and invoking `statvfs()` with distinct unprivileged available capacity
  - `include/nodepulse/services/system_service.hpp` & `src/services/system_service.cpp`: Service layer coordinating system collection
  - `include/nodepulse/services/cpu_service.hpp` & `src/services/cpu_service.cpp`: Service layer coordinating CPU snapshot delta calculations, non-blocking background sampling thread, explicit `warming_up` state, counter wrap recovery, CPU hotplug handling, and zero event loop starvation
  - `include/nodepulse/services/memory_service.hpp` & `src/services/memory_service.cpp`: Service layer coordinating memory collection
  - `include/nodepulse/services/disk_service.hpp` & `src/services/disk_service.cpp`: Service layer coordinating disk collection
  - `include/nodepulse/controllers/system_controller.hpp` & `src/controllers/system_controller.cpp`: Drogon controller exposing `GET /api/v1/system`
  - `include/nodepulse/controllers/cpu_controller.hpp` & `src/controllers/cpu_controller.cpp`: Drogon controller exposing `GET /api/v1/cpu`
  - `include/nodepulse/controllers/memory_controller.hpp` & `src/controllers/memory_controller.cpp`: Drogon controller exposing `GET /api/v1/memory`
  - `include/nodepulse/controllers/disk_controller.hpp` & `src/controllers/disk_controller.cpp`: Drogon controller exposing `GET /api/v1/disks`
  - `include/nodepulse/config/config.hpp` & `src/config/config.cpp`: Configuration domain structs, JSON loader, environment overrides, and schema validation
  - `include/nodepulse/utils/error_response.hpp` & `src/utils/error_response.cpp`: Standard JSON error envelope generator and error code taxonomy
  - `include/nodepulse/utils/logger.hpp` & `src/utils/logger.cpp`: Logger abstraction wrapping spdlog with custom and structured JSON patterns
  - `include/nodepulse/controllers/health_controller.hpp` & `src/controllers/health_controller.cpp`: Drogon controller for `GET /api/v1/health`
  - `include/nodepulse/server/server.hpp` & `src/server/server.cpp`: Drogon server lifecycle manager, loopback-only network binding enforcement, validated Request ID injector, access logger, background CPU sampling lifecycle orchestration, and centralized 404/exception handlers
  - `apps/server/main.cpp`: Application entry point with CLI parsing (`--config`, `--validate-config`, `--version`, `--help`)
- **Architectural Decision Records**:
  - `DECISIONS.md`: Added DEC-014 (CPU Utilization Sampling, Warming-Up State & Non-Blocking Architecture)
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
  - `tests/fixtures/etc/os-release`: Sample `/etc/os-release`
- **Tests**:
  - `tests/unit/smoke_test.cpp`: Test harness and basic JSON serialization tests (3 tests)
  - `tests/unit/config_test.cpp`: Config defaults, JSON parsing, validation constraints, and env override tests (10 tests)
  - `tests/unit/error_response_test.cpp`: Standard error JSON structure, timestamping, and HTTP response content type tests (3 tests)
  - `tests/unit/system_collector_test.cpp`: Stream parsers, fallback behaviors, and fixture-driven system collector and service tests (21 tests)
  - `tests/unit/cpu_collector_test.cpp`: Stream parsers, warming up baseline handling (DEC-014), delta calculations, wrap/reset recovery, zero elapsed time caching, hotplug core count detection, steal time accounting, background sampling thread validation, and missing file error handling (21 tests)
  - `tests/unit/memory_collector_test.cpp`: Stream parser verification, exact byte conversions, zero swap resilience, MemAvailable fallback, malformed input rejection, integer overflow prevention, arithmetic underflow prevention, fixture files, live host sanity, and service delegation tests (20 tests)
  - `tests/unit/disk_collector_test.cpp`: Octal unescaping, mount stream parser, duplicate mount point deduplication, malformed line handling, pseudo-filesystem filtering, normal metric calculations, zero-capacity handling, clamping and safety, integer overflow protection, mock statvfs fixture testing, inaccessible/disappearing mount handling, nonexistent mounts file error handling, live host disk sanity, service delegation, and async worker thread offload tests (17 tests)
  - `tests/integration/http_integration_test.cpp`: In-process Drogon HTTP tests verifying health probe, system endpoint contract, cpu endpoint contract, memory endpoint contract, disks endpoint contract, request ID validation/sanitization, 404 handler, collector failure handling across all endpoints (including disks), concurrency across 5 endpoints, single Content-Type emission regression, and loopback binding security enforcement (17 integration tests)
- **Documentation**:
  - Complete Phase 0 documentation suite in `docs/` and root specification files
  - `docs/api/API.md`: Updated with `available_bytes` in disks section, corrected authentication applicability note for pre-Phase 9 endpoints, and error responses
  - `docs/domain/DATA_MODEL.md` & `docs/domain/DOMAIN_MODEL.md`: Updated `DiskPartitionMetrics` domain models with `available_bytes`
  - `docs/domain/BUSINESS_RULES.md`: Added rule BR-013 defining CPU sampling state and truthful reporting contract

---

## Phase 6 Verification and Gate Status
- **Exit Gate Criteria**:
  - [x] Stream-based parser implemented for `/proc/mounts` with octal unescape support (`\040` -> space, `\011` -> tab, `\012` -> newline, `\134` -> backslash).
  - [x] Native Linux interfaces utilized exclusively (`/proc/mounts`, `statvfs()`); zero shell/command execution.
  - [x] Distinct unprivileged available capacity accounted for (`available_bytes = stat.f_bavail * block_size`, clamped to `free_bytes`), avoiding conflation with root-reserved space (`free_bytes = stat.f_bfree * block_size`).
  - [x] Fundamental filesystem block size correctly determined (`stat.f_frsize > 0 ? stat.f_frsize : stat.f_bsize`).
  - [x] Used bytes computed accurately as `(stat.f_blocks - stat.f_bfree) * block_size` (or `total_bytes - free_bytes`).
  - [x] Usage percentage calculated safely as `total_bytes > 0 ? (used_bytes / total_bytes) * 100.0 : 0.0`, clamped to `[0.0, 100.0]`, and rounded to 2 decimal places.
  - [x] Zero-capacity filesystems handled gracefully without division by zero (`total_bytes == 0` -> `usage_percent = 0.0`).
  - [x] Integer overflow protected on multiplications (`blocks * block_size` checked against `UINT64_MAX / block_size`).
  - [x] Inode metrics collected safely (`inodes_total = stat.f_files`, `inodes_free = min(stat.f_ffree, stat.f_files)`).
  - [x] Duplicate and shadowed mount points deduplicated in-place, preserving order while tracking the latest active mount.
  - [x] Configurable pseudo-filesystem filtering applied via `disks.ignored_fstypes` (e.g., `proc`, `sysfs`, `tmpfs`, `devtmpfs`, etc.).
  - [x] Inaccessible or disappearing mount points (`statvfs` error such as `EACCES`, `ENOENT`) handled gracefully by skipping the entry with a debug log rather than failing the entire collection.
  - [x] Missing or unreadable `/proc/mounts` returns empty metrics and triggers a standard 500 `COLLECTOR_FAILURE` error envelope.
  - [x] Disk collection and `statvfs()` calls offloaded from Drogon's event-loop threads to a bounded worker task queue (`trantor::ConcurrentTaskQueue`), preventing slow or unresponsive mounts from stalling HTTP event loops.
  - [x] `GET /api/v1/disks` returns HTTP 200 with JSON array matching exact documented schema (`mount_point`, `filesystem`, `fstype`, `total_bytes`, `used_bytes`, `free_bytes`, `available_bytes`, `usage_percent`, `inodes_total`, `inodes_free`).
  - [x] `GET /api/v1/disks` handles collector failure gracefully, returning HTTP 500 with standard `COLLECTOR_FAILURE` envelope (`target_file: /proc/mounts`).
  - [x] Single `Content-Type: application/json; charset=utf-8` header emission verified on all disk responses (success and error).
  - [x] 100% test pass rate across unit and integration test suites (112/112 passed).
  - [x] Zero warnings or errors under `-Wall -Wextra -Wpedantic -Werror`.
  - [x] `format-check` passes with zero violations.
  - [x] `tidy` passes with zero errors and zero warnings across all 18 source files.
  - [x] Live HTTP smoke test verified with `curl` for `GET /api/v1/disks`, alongside health, system, cpu, and memory endpoints, and graceful server shutdown.
- **Verification Commands Executed**:
  ```bash
  cmake -S . -B build
  cmake --build build -- -j
  ctest --test-dir build --output-on-failure
  cmake --build build --target format-check
  cmake --build build --target tidy
  ./build/apps/server/nodepulse_server --config config/config.example.json &
  curl -s -i http://127.0.0.1:8080/api/v1/disks
  ```
- **Verification Results**:
  - Build: Succeeded cleanly (all targets built under `-Wall -Wextra -Wpedantic -Werror`).
  - Tests: 112/112 passed (100% pass rate).
  - Format check: Succeeded (0 violations).
  - Static analysis: Succeeded (0 errors, 0 warnings across all 18 source files).
  - Live HTTP smoke test: Verified single `content-type` emission, HTTP 200 OK with mounted filesystems array in live environment, and graceful server shutdown.
- **Confirmation**: Phase 7 (Network Collector) has NOT been started.
