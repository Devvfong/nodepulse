# NodePulse — Current Project State

## Project Metadata
- **Project**: NodePulse
- **Description**: Lightweight Linux server monitoring and management agent written in C++20 using Drogon
- **Status**: CPU Collector implemented, hardened, and verified with non-blocking background sampling and explicit warming-up state
- **Current Implementation Phase**: Phase 4 — CPU Collector
- **Next Approved Phase**: Phase 5 — Memory Collector
- **Production Ready**: No
- **Active Git Branch**: `master`
- **Current Implementation Exists**: Yes (build system, logger, config loader, server lifecycle, health endpoint, system collector, CPU collector & endpoint, error responses, unit & integration tests)

---

## Phase Status Summary

| Phase | Description | Status | Approved to Start |
|---|---|---|---|
| **Phase 0** | Documentation & Architectural Specification | **COMPLETED** | N/A |
| **Phase 1** | Repository Foundation (CMake, Clang-Tooling, GTest) | **COMPLETED** | N/A |
| **Phase 2** | HTTP Foundation (Drogon setup, Health Check, JSON handling) | **COMPLETED** | N/A |
| **Phase 3** | System Collector (`/proc/uptime`, `/etc/os-release`, uname) | **COMPLETED** | N/A |
| **Phase 4** | CPU Collector (`/proc/stat`, `/proc/loadavg`, `/proc/cpuinfo`) | **COMPLETED** | N/A |
| **Phase 5** | Memory Collector (`/proc/meminfo`, virtual memory & swap) | PENDING | **YES (Next Approved)** |
| **Phase 6** | Disk Collector (`/proc/mounts`, `statvfs`) | PENDING | NO |
| **Phase 7** | Network Collector (`/proc/net/dev`, `/sys/class/net`) | PENDING | NO |
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
  - `config/config.example.json`: Sample configuration file (binds to localhost by default, `cpu.sample_interval_ms: 1000`)
  - Targets: `format-check`, `format-fix`, `tidy`
- **Continuous Integration**:
  - `.github/workflows/ci.yml`: Matrix build testing GCC and Clang, unit & integration tests, formatting, and clang-tidy
- **Source Code**:
  - `include/nodepulse/domain/system_info.hpp`: Domain model struct for system metadata
  - `include/nodepulse/domain/cpu_info.hpp`: Domain model structs for CPU metrics (`LoadAverage`, `CpuCoreMetrics`, `CpuMetrics`) with `std::optional<double> usage_percent` and `measurement_status`
  - `include/nodepulse/collectors/system_collector.hpp` & `src/collectors/system_collector.cpp`: Collector parsing `/proc/uptime`, `/proc/stat` (`btime`), `/etc/os-release`, and invoking `gethostname()` / `uname()`
  - `include/nodepulse/collectors/cpu_collector.hpp` & `src/collectors/cpu_collector.cpp`: Collector parsing `/proc/stat`, `/proc/loadavg`, and `/proc/cpuinfo`
  - `include/nodepulse/services/system_service.hpp` & `src/services/system_service.cpp`: Service layer coordinating system collection
  - `include/nodepulse/services/cpu_service.hpp` & `src/services/cpu_service.cpp`: Service layer coordinating CPU snapshot delta calculations, non-blocking background sampling thread, explicit `warming_up` state, counter wrap recovery, CPU hotplug handling, and zero event loop starvation
  - `include/nodepulse/controllers/system_controller.hpp` & `src/controllers/system_controller.cpp`: Drogon controller exposing `GET /api/v1/system`
  - `include/nodepulse/controllers/cpu_controller.hpp` & `src/controllers/cpu_controller.cpp`: Drogon controller exposing `GET /api/v1/cpu`
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
  - `tests/fixtures/etc/os-release`: Sample `/etc/os-release`
- **Tests**:
  - `tests/unit/smoke_test.cpp`: Test harness and basic JSON serialization tests (3 tests)
  - `tests/unit/config_test.cpp`: Config defaults, JSON parsing, validation constraints, and env override tests (10 tests)
  - `tests/unit/error_response_test.cpp`: Standard error JSON structure, timestamping, and HTTP response content type tests (3 tests)
  - `tests/unit/system_collector_test.cpp`: Stream parsers, fallback behaviors, and fixture-driven system collector and service tests (21 tests)
  - `tests/unit/cpu_collector_test.cpp`: Stream parsers, warming up baseline handling (DEC-014), delta calculations, wrap/reset recovery, zero elapsed time caching, hotplug core count detection, steal time accounting, background sampling thread validation, and missing file error handling (21 tests)
  - `tests/integration/http_integration_test.cpp`: In-process Drogon HTTP tests verifying health probe, system endpoint contract, cpu endpoint contract, request ID validation/sanitization, 404 handler, collector failure handling, concurrency across endpoints, single Content-Type emission regression, and loopback binding security enforcement (13 integration tests)
- **Documentation**:
  - Complete Phase 0 documentation suite in `docs/` and root specification files
  - `docs/api/API.md`: Updated with `measurement_status` values (`warming_up`, `ready`, `cached`) and nullable `usage_percent` specification
  - `docs/domain/DATA_MODEL.md` & `docs/domain/DOMAIN_MODEL.md`: Updated CPU domain models with `std::optional<double> usage_percent` and `measurement_status`
  - `docs/domain/BUSINESS_RULES.md`: Added rule BR-013 defining CPU sampling state and truthful reporting contract

---

## Phase 4 Verification and Gate Status
- **Exit Gate Criteria**:
  - [x] Stream-based parsers implemented for `/proc/stat`, `/proc/loadavg`, and `/proc/cpuinfo`.
  - [x] Native Linux interfaces utilized exclusively; zero shell/command execution.
  - [x] Accurate CPU delta calculation using two `/proc/stat` snapshot samples.
  - [x] Double-counting guest time prevented (`guest` and `guest_nice` accounted for within `user` and `nice`).
  - [x] Steal time accurately accounted for in total and busy time.
  - [x] DEC-014 recorded and enforced: first sample establishes baseline and reports `usage_percent: null` with `measurement_status: "warming_up"`.
  - [x] Non-blocking execution: dedicated background sampling thread takes samples periodically; Drogon event-loop threads service `GET /api/v1/cpu` in sub-microseconds without performing blocking file I/O.
  - [x] Thread-safe snapshot state managed via `std::mutex` and `std::condition_variable` in `CpuService`.
  - [x] Counter wrap / reboot recovery: detects counter regression, resets baseline, and reports `null` / `"warming_up"`.
  - [x] CPU core changes (hotplug) detected dynamically: core count changes reset aggregate baseline to `"warming_up"`, newly onlined cores report `null`, and `logical_cores` reflects active online cores.
  - [x] Zero elapsed time handled gracefully: returns last computed usage percentage explicitly marked with `measurement_status: "cached"`.
  - [x] Core-level breakdowns computed per logical CPU core with valid `core_id` and nullable `usage_percent`.
  - [x] `GET /api/v1/cpu` returns HTTP 200 with JSON matching exact documented schema (`usage_percent`, `measurement_status`, `model_name`, `physical_cores`, `logical_cores`, `load_average`, `cores`).
  - [x] `GET /api/v1/cpu` handles collector failure gracefully, returning HTTP 500 with standard `COLLECTOR_FAILURE` envelope (`target_file: /proc/stat`).
  - [x] Single `Content-Type: application/json; charset=utf-8` header emission verified on all CPU responses (success and error).
  - [x] 100% test pass rate across unit and integration test suites (71/71 passed).
  - [x] Zero warnings or errors under `-Wall -Wextra -Wpedantic -Werror`.
  - [x] `format-check` passes with zero violations.
  - [x] `tidy` passes with zero errors and zero warnings across all 12 source files.
  - [x] Live HTTP smoke test verified with `curl` for baseline call (`warming_up` with `usage_percent: null`), subsequent delta sample call (`ready` with computed usage), and graceful SIGINT server shutdown.
- **Verification Commands Executed**:
  ```bash
  cmake -S . -B build
  cmake --build build -- -j
  ctest --test-dir build --output-on-failure
  cmake --build build --target format-check
  cmake --build build --target tidy
  ./build/apps/server/nodepulse_server --config config/config.example.json &
  curl -s -i http://127.0.0.1:8080/api/v1/cpu
  curl -s -i http://127.0.0.1:8080/api/v1/cpu
  ```
- **Verification Results**:
  - Build: Succeeded cleanly (all targets built under `-Wall -Wextra -Wpedantic -Werror`).
  - Tests: 71/71 passed (100% pass rate).
  - Format check: Succeeded (0 violations).
  - Static analysis: Succeeded (0 errors, 0 warnings across all 12 source files).
  - Live HTTP smoke test: Verified single `content-type` emission, baseline reporting `usage_percent: null` with `measurement_status: "warming_up"` in 0.11ms, subsequent call reporting accurate delta CPU usage with `measurement_status: "ready"` in 0.15ms, and graceful server shutdown.
- **Confirmation**: Phase 5 (Memory Collector) has NOT been started.
