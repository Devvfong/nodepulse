# NodePulse — Current Project State

## Project Metadata
- **Project**: NodePulse
- **Description**: Lightweight Linux server monitoring and management agent written in C++20 using Drogon
- **Status**: System Collector implemented, hardened, and verified
- **Current Implementation Phase**: Phase 3 — System Collector
- **Next Approved Phase**: Phase 4 — CPU Collector
- **Production Ready**: No
- **Active Git Branch**: `master`
- **Current Implementation Exists**: Yes (build system, logger, config loader, server lifecycle, health endpoint, system collector & endpoint, error responses, unit & integration tests)

---

## Phase Status Summary

| Phase | Description | Status | Approved to Start |
|---|---|---|---|
| **Phase 0** | Documentation & Architectural Specification | **COMPLETED** | N/A |
| **Phase 1** | Repository Foundation (CMake, Clang-Tooling, GTest) | **COMPLETED** | N/A |
| **Phase 2** | HTTP Foundation (Drogon setup, Health Check, JSON handling) | **COMPLETED** | N/A |
| **Phase 3** | System Collector (`/proc/uptime`, `/etc/os-release`, uname) | **COMPLETED** | N/A |
| **Phase 4** | CPU Collector (`/proc/stat`, `/proc/loadavg`, `/proc/cpuinfo`) | PENDING | **YES (Next Approved)** |
| **Phase 5** | Memory Collector (`/proc/meminfo`, virtual memory & swap) | PENDING | NO |
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
  - `config/config.example.json`: Sample configuration file (binds to localhost by default)
  - Targets: `format-check`, `format-fix`, `tidy`
- **Continuous Integration**:
  - `.github/workflows/ci.yml`: Matrix build testing GCC and Clang, unit & integration tests, formatting, and clang-tidy
- **Source Code**:
  - `include/nodepulse/domain/system_info.hpp`: Domain model struct for system metadata
  - `include/nodepulse/collectors/system_collector.hpp` & `src/collectors/system_collector.cpp`: Collector parsing `/proc/uptime`, `/proc/stat` (`btime`), `/etc/os-release`, and invoking `gethostname()` / `uname()`
  - `include/nodepulse/services/system_service.hpp` & `src/services/system_service.cpp`: Service layer coordinating system collection
  - `include/nodepulse/controllers/system_controller.hpp` & `src/controllers/system_controller.cpp`: Drogon controller exposing `GET /api/v1/system`
  - `include/nodepulse/config/config.hpp` & `src/config/config.cpp`: Configuration domain structs, JSON loader, environment overrides, and schema validation
  - `include/nodepulse/utils/error_response.hpp` & `src/utils/error_response.cpp`: Standard JSON error envelope generator and error code taxonomy
  - `include/nodepulse/utils/logger.hpp` & `src/utils/logger.cpp`: Logger abstraction wrapping spdlog with custom and structured JSON patterns
  - `include/nodepulse/controllers/health_controller.hpp` & `src/controllers/health_controller.cpp`: Drogon controller for `GET /api/v1/health`
  - `include/nodepulse/server/server.hpp` & `src/server/server.cpp`: Drogon server lifecycle manager, loopback-only network binding enforcement, validated Request ID injector, access logger, and centralized 404/exception handlers
  - `apps/server/main.cpp`: Application entry point with CLI parsing (`--config`, `--validate-config`, `--version`, `--help`)
- **Test Fixtures**:
  - `tests/fixtures/proc/uptime`: Sample `/proc/uptime`
  - `tests/fixtures/proc/stat`: Sample `/proc/stat` containing `btime` line
  - `tests/fixtures/etc/os-release`: Sample `/etc/os-release`
- **Tests**:
  - `tests/unit/smoke_test.cpp`: Test harness and basic JSON serialization tests
  - `tests/unit/config_test.cpp`: Config defaults, JSON parsing, validation constraints, and env override tests
  - `tests/unit/error_response_test.cpp`: Standard error JSON structure, timestamping, and HTTP response content type tests
  - `tests/unit/system_collector_test.cpp`: Stream parsers, fallback behaviors, and fixture-driven system collector and service tests (21 unit tests)
  - `tests/integration/http_integration_test.cpp`: In-process Drogon HTTP tests verifying health probe, system endpoint contract, request ID validation/sanitization, 404 handler, collector failure handling, concurrency, single Content-Type emission regression, and loopback binding security enforcement (10 integration tests)
- **Documentation**:
  - Complete Phase 0 documentation suite in `docs/` and root specification files
  - `docs/api/API.md`: Updated with explicit error responses for `GET /api/v1/system` matching `docs/api/ERRORS.md`

---

## Phase 3 Verification and Gate Status
- **Exit Gate Criteria**:
  - [x] Stream-based parsers implemented for `/etc/os-release`, `/proc/uptime`, and `/proc/stat` (`btime`).
  - [x] Syscall helpers implemented for `gethostname()` and `uname()`.
  - [x] Fallback boot time calculation implemented when `btime` is missing from `/proc/stat`.
  - [x] Fallback OS identification implemented when `/etc/os-release` is absent or unreadable.
  - [x] Strict layering maintained: `SystemController` -> `SystemService` -> `SystemCollector` -> virtual filesystem / kernel interfaces.
  - [x] `GET /api/v1/system` returns HTTP 200 with JSON matching exact documented schema (`hostname`, `os_name`, `os_version`, `kernel_version`, `architecture`, `boot_time_utc`, `uptime_seconds`).
  - [x] `GET /api/v1/system` handles collector failures gracefully, returning HTTP 500 with standard `COLLECTOR_FAILURE` error envelope.
  - [x] Single `Content-Type: application/json; charset=utf-8` header emission verified across health, system, and error responses.
  - [x] Non-loopback network binding strictly rejected before Phase 9 authentication enforcement.
  - [x] Whole-archive linking backwards-compatible with minimum CMake version (>= 3.22).
  - [x] Request ID validation and sanitization prevents header/log injection.
  - [x] 100% test pass rate across unit and integration test suites (47/47 passed).
  - [x] Zero warnings or errors under `-Wall -Wextra -Wpedantic -Werror`.
  - [x] `format-check` passes with zero violations.
  - [x] `tidy` passes with zero errors and zero warnings across all source files.
  - [x] Live HTTP smoke test verified with `curl` for `/api/v1/health`, `/api/v1/system`, and error responses, verifying single `Content-Type` header emission.
- **Verification Commands Executed**:
  ```bash
  cmake -S . -B build
  cmake --build build -- -j
  ctest --test-dir build --output-on-failure
  cmake --build build --target format-check
  cmake --build build --target tidy
  ./build/apps/server/nodepulse_server &
  curl -s -i http://127.0.0.1:8080/api/v1/health
  curl -s -i http://127.0.0.1:8080/api/v1/system
  curl -s -i http://127.0.0.1:8080/api/v1/non_existent_route
  ```
- **Verification Results**:
  - Build: Succeeded cleanly (all targets built under `-Wall -Wextra -Wpedantic -Werror`).
  - Tests: 47/47 passed (100% pass rate).
  - Format check: Succeeded (0 violations).
  - Static analysis: Succeeded (0 errors, 0 warnings).
  - Live HTTP smoke test: Verified single `content-type` emission (count: 1 across all routes), valid system info JSON, request ID roundtrip, and graceful shutdown on SIGTERM.
  - Security check: Verified attempting to run `nodepulse_server` on non-loopback host `0.0.0.0` immediately exits with code 1 and descriptive security error message.
- **Confirmation**: Phase 4 (CPU Collector) has NOT been started.
