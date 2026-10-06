# NodePulse — Current Project State

## Project Metadata
- **Project**: NodePulse
- **Description**: Lightweight Linux server monitoring and management agent written in C++20 using Drogon
- **Status**: Repository foundation implemented and verified
- **Current Implementation Phase**: Phase 1 — Repository Foundation
- **Next Approved Phase**: Phase 2 — HTTP Foundation
- **Production Ready**: No
- **Active Git Branch**: `master`
- **Current Implementation Exists**: Yes (build system, logger utility, minimal server entry point, smoke tests)

---

## Phase Status Summary

| Phase | Description | Status | Approved to Start |
|---|---|---|---|
| **Phase 0** | Documentation & Architectural Specification | **COMPLETED** | N/A |
| **Phase 1** | Repository Foundation (CMake, Clang-Tooling, GTest) | **COMPLETED** | N/A |
| **Phase 2** | HTTP Foundation (Drogon setup, Health Check, JSON handling) | PENDING | **YES (Next Approved)** |
| **Phase 3** | System Collector (`/proc/uptime`, `/etc/os-release`, uname) | PENDING | NO |
| **Phase 4** | CPU Collector (`/proc/stat`, `/proc/loadavg`, `/proc/cpuinfo`) | PENDING | NO |
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
  - `CMakeLists.txt`: Root target-based CMake configuration (C++20, `-Wall -Wextra -Wpedantic -Werror`, quality targets)
  - `src/CMakeLists.txt`: Static library `nodepulse_lib` definition
  - `apps/CMakeLists.txt`: Applications directory definition
  - `apps/server/CMakeLists.txt`: `nodepulse_server` executable target definition
  - `tests/CMakeLists.txt`: `tests_unit` executable with GoogleTest discovery
- **Quality Tooling**:
  - `.gitignore`: Ignore build artifacts, IDE configs, coverage, logs
  - `.clang-format`: Formatter rules conforming to project coding standards
  - `.clang-tidy`: Static analysis rules with warnings-as-errors
  - Targets: `format-check`, `format-fix`, `tidy`
- **Continuous Integration**:
  - `.github/workflows/ci.yml`: Matrix build testing GCC and Clang, unit tests, formatting, and clang-tidy
- **Source Code**:
  - `include/nodepulse/utils/logger.hpp`: Logger abstraction wrapping spdlog
  - `src/utils/logger.cpp`: Logger implementation supporting standard and JSON formatting
  - `apps/server/main.cpp`: Minimal entry point verifying linkage of Drogon, spdlog, and nlohmann/json
- **Tests**:
  - `tests/unit/smoke_test.cpp`: GoogleTest test suite verifying test harness, logging, and JSON parsing
- **Documentation**:
  - Complete Phase 0 documentation suite in `docs/` and root specification files

---

## Phase 1 Verification and Gate Status
- **Exit Gate Criteria**:
  - [x] Modern target-based CMake configuration requiring C++20.
  - [x] Zero compiler warnings under `-Wall -Wextra -Wpedantic -Werror`.
  - [x] Out-of-source build support (`build/` directory).
  - [x] GoogleTest integration operational; minimal smoke tests pass 100%.
  - [x] `clang-format` checking target (`format-check`) passes clean.
  - [x] `clang-tidy` static analysis target (`tidy`) passes clean with zero errors.
  - [x] GitHub Actions CI workflow authored and configured for GCC & Clang.
  - [x] Minimal application entry point verifies library linkage without implementing HTTP/collector logic.
- **Verification Commands Executed**:
  ```bash
  cmake -B build -S .
  cmake --build build
  ctest --test-dir build --output-on-failure
  cmake --build build --target format-check
  cmake --build build --target tidy
  ./build/apps/server/nodepulse_server --version
  ```
- **Verification Results**:
  - Build: Succeeded cleanly (all targets built).
  - Tests: 3/3 passed (100% pass rate).
  - Format check: Succeeded (0 violations).
  - Static analysis: Succeeded (0 errors).
  - Server executable: Ran successfully and printed version string.
- **Known Issues & Environment Decisions**:
  - The repository contains zero machine-specific paths or hardcoded linker RPATH flags (`-Wl,--disable-new-dtags` removed). In non-root development environments with libraries installed in custom user prefixes, standard CMake `CMAKE_PREFIX_PATH` and linker `LIBRARY_PATH` / `LD_LIBRARY_PATH` environment variables are utilized during invocation.
  - In `.clang-tidy`, `-misc-include-cleaner` is disabled to allow standard C++ library usage of umbrella headers (`<spdlog/spdlog.h>`, `<nlohmann/json.hpp>`, `<drogon/drogon.h>`). Clang-Tidy 18 is standardized matching CI.
- **Confirmation**: Phase 2 (HTTP Foundation / REST controllers) has NOT been started.
