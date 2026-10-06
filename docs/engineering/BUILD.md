# NodePulse — Build & Toolchain Guide

This document details prerequisites, toolchain requirements, CMake build targets, options, and dependency installations for building NodePulse.

---

## 1. Prerequisites & Toolchain Requirements

| Component | Minimum Version | Recommended | Notes |
|---|---|---|---|
| **OS** | Linux Kernel 4.18+ | Ubuntu 22.04 LTS / Debian 12 | glibc 2.31+ |
| **Compiler** | GCC 11+ or Clang 14+ | GCC 12 / Clang 16 | Requires full C++20 support |
| **Build Generator** | CMake 3.22+ | CMake 3.28+ | Modern target-based CMake |
| **Build Tool** | Ninja 1.10+ | Ninja | Preferred over GNU Make for fast incremental builds |
| **Static Analysis** | Clang-Tidy 14+ | Clang-Tidy 16+ | Zero warnings enforced |
| **Code Formatter** | Clang-Format 14+ | Clang-Format 16+ | `.clang-format` standard |

---

## 2. Installing System Dependencies

### Ubuntu 22.04 LTS / Debian 12
```bash
sudo apt update && sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    clang-14 \
    clang-tidy-14 \
    clang-format-14 \
    git \
    libjsoncpp-dev \
    uuid-dev \
    zlib1g-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    libspdlog-dev \
    nlohmann-json3-dev \
    libgtest-dev \
    libsystemd-dev
```

*Note: Drogon and Drogon dependencies (Trantor) can be installed via system packages or pulled automatically via CMake `FetchContent`.*

---

## 3. CMake Configuration Options

| Option | Default | Description |
|---|---|---|
| `CMAKE_BUILD_TYPE` | `Debug` | `Debug`, `Release`, or `RelWithDebInfo` |
| `BUILD_TESTING` | `ON` | Build GoogleTest unit and integration test targets |
| `ENABLE_SANITIZERS` | `OFF` | Enable AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan) |
| `ENABLE_TSAN` | `OFF` | Enable ThreadSanitizer (mutually exclusive with ASan) |
| `ENABLE_POSTGRES` | `OFF` | Compile PostgreSQL repository support via `libpqxx` (Phase 14) |
| `ENABLE_CLANG_TIDY` | `OFF` | Run `clang-tidy` during build compilation |
| `WARNINGS_AS_ERRORS` | `ON` | Enforce `-Werror` on compiler flags |

---

## 4. Standard Build Workflows

### 4.1 Debug Build (Default for Development)
```bash
# Configure build directory
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON

# Compile all targets
cmake --build build -- -j$(nproc)

# Run test suite
ctest --test-dir build --output-on-failure
```

### 4.2 Optimized Release Build
```bash
cmake -B build-release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON

cmake --build build-release -- -j$(nproc)
```

### 4.3 Sanitizer Build (ASan + UBSan)
```bash
cmake -B build-asan -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_SANITIZERS=ON \
    -DBUILD_TESTING=ON

cmake --build build-asan -- -j$(nproc)
ctest --test-dir build-asan --output-on-failure
```

---

## 5. CMake Target Hierarchy

- **`nodepulse_lib`**: Core static library containing domain models, utilities, collectors, services, and middleware.
- **`nodepulse_server`**: Executable binary (`apps/server/main.cpp`) linking `nodepulse_lib` and Drogon.
- **`tests_unit`**: GTest binary executing pure unit and parser fixture tests.
- **`tests_integration`**: GTest binary executing HTTP endpoint and Drogon lifecycle tests.

