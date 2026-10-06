#pragma once

#include <iosfwd>
#include <optional>
#include <string>

#include <nodepulse/domain/memory_info.hpp>

namespace nodepulse::collectors {

class MemoryCollector {
  public:
    explicit MemoryCollector(std::string meminfo_path = "/proc/meminfo");
    virtual ~MemoryCollector() = default;

    [[nodiscard]] virtual std::optional<domain::MemoryMetrics> collect() const;

    [[nodiscard]] static std::optional<domain::MemoryMetrics> parse_meminfo_stream(
        std::istream& stream);

  private:
    std::string meminfo_path_;
};

}  // namespace nodepulse::collectors
