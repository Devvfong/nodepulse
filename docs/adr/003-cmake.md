# ADR 003: Modern Target-Centric CMake Build System

## Status
Accepted

## Date
2026-10-06

## Context
A robust, portable, and reproducible build system is required for NodePulse. The build system must:
- Support GCC 11+ and Clang 14+ with strict warnings (`-Wall -Wextra -Wpedantic -Werror`).
- Provide seamless integration with modern IDEs (VS Code, CLion, Neovim LSP).
- Orchestrate GoogleTest unit and integration test execution via CTest.
- Support AddressSanitizer (ASan), UndefinedBehaviorSanitizer (UBSan), and ThreadSanitizer (TSan).
- Enforce modularity through target-scoped properties (`target_include_directories`, `target_link_libraries`).

## Decision
We standardize on **Modern Target-Based CMake (version >= 3.22)** utilizing the **Ninja** build generator for local builds and CI pipelines.

## Consequences

### Positive
- **Industry Standard**: CMake is the de facto standard build orchestration tool for C++ across enterprise Linux, open-source communities, and CI/CD vendors.
- **Target Encapsulation**: Strict scoping of includes and compile definitions prevents leaky compilation flags across library boundaries.
- **Sanitizer and Tooling Integration**: First-class support for CTest, `FetchContent`, Clang-Tidy, and compiler sanitizer flags.
- **Fast Incremental Builds**: Pairing CMake with Ninja ensures sub-second incremental builds during test-driven development.

### Negative / Trade-Offs
- **DSL Complexity**: CMake language syntax has historical quirks and requires strict discipline to prevent global flag pollution (`set(CMAKE_CXX_FLAGS ...)`).

## Alternatives Considered
- **Meson**: Very fast and clean syntax, but requires Python runtime and has less ubiquitous IDE tooling support compared to CMake.
- **Bazel**: Powerful for large monorepos, but introduces high operational overhead and complex toolchain definitions for a single-repo agent.
- **Plain Makefiles**: Fragile, non-portable, and lacks automated dependency tracking and target scoping.

