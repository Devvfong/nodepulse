# NodePulse — C++20 Coding Standards & Conventions

This document specifies the mandatory coding standards, style guidelines, and language paradigms for all C++20 code in NodePulse.

---

## 1. Modern C++20 Standard Guidelines

1. **Standard Compliance**: Target ISO C++20 (`-std=c++20`).
2. **View Types Over Copies**:
   - Use `std::string_view` for read-only string function parameters.
   - Use `std::span<const T>` for contiguous sequential collections.
3. **Concepts & Constraints**:
   - Constrain template parameters using C++20 concepts rather than raw `typename T` with SFINAE.
4. **Structured Bindings**:
   - Prefer structured bindings (`auto [key, value] = ...`) for tuple and map traversals.
5. **No Owning Raw Pointers**:
   - Never use `new` or `delete` directly.
   - Use `std::make_unique<T>()` and `std::make_shared<T>()`.
   - Pass non-owning references (`const T&`) or raw non-owning observation pointers (`T*`).
6. **Strict RAII**:
   - All files, sockets, database transactions, and mutex locks must be bound to object lifetimes using RAII wrappers (`std::ifstream`, `std::lock_guard`, `std::unique_lock`).

---

## 2. Naming Conventions

| Entity | Convention | Example |
|---|---|---|
| **Namespaces** | Lowercase snake_case | `nodepulse::collectors` |
| **Classes & Structs** | PascalCase | `CpuCollector`, `SystemInfo` |
| **Methods & Functions** | snake_case | `collect_metrics()`, `to_json()` |
| **Local Variables** | snake_case | `cpu_usage`, `bytes_received` |
| **Member Variables** | snake_case with trailing `_` | `socket_path_`, `api_key_` |
| **Constants / Enums** | UPPER_SNAKE_CASE | `MAX_PID_LIMIT`, `DEFAULT_PORT` |
| **File Names** | snake_case (`.hpp`, `.cpp`) | `cpu_collector.hpp`, `auth_filter.cpp` |

---

## 3. Header Hygiene & Structure

- Always start headers with `#pragma once`.
- Include files in three distinct groups separated by a blank line:
  1. Standard Library headers (`<string>`, `<vector>`, `<memory>`).
  2. Third-party library headers (`<drogon/HttpController.h>`, `<spdlog/spdlog.h>`).
  3. NodePulse internal headers (`<nodepulse/domain/cpu_info.hpp>`).
- Avoid `#include` in headers when a forward declaration suffices.

Example header:
```cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include <nodepulse/domain/cpu_info.hpp>

namespace nodepulse::services {

class CpuService {
public:
    explicit CpuService();
    ~CpuService() = default;

    [[nodiscard]] domain::CpuMetrics get_current_metrics();

private:
    double previous_total_jiffies_{0.0};
};

} // namespace nodepulse::services
```

---

## 4. Error Handling Guidelines

1. **Expected Failures**:
   - Use `std::optional<T>` or `std::expected<T, ErrorCode>` for predictable domain failures (e.g. process not found, key not matched).
2. **Exceptional Invariants**:
   - C++ exceptions (`std::runtime_error`, `std::invalid_argument`) may be thrown during startup or fatal invariant violations.
   - Never allow an exception to escape uncaught from a Drogon controller handler. Wrap controller logic in top-level try/catch blocks that return uniform JSON error responses.

---

## 5. Formatting & Static Analysis

- **Clang-Format**: All code must strictly conform to `.clang-format` (4 spaces, 100 columns, sorted includes).
  ```bash
  clang-format -i src/**/*.cpp include/nodepulse/**/*.hpp tests/**/*.cpp
  ```
- **Clang-Tidy**: Code must pass `clang-tidy` checks clean without warnings.
- **Compiler Flags**: `-Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow`.
  - Disabling warnings via `#pragma GCC diagnostic ignored` is strictly prohibited.

