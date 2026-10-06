#pragma once

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nodepulse/domain/cpu_info.hpp>

namespace nodepulse::collectors {

struct RawCpuTime {
    uint64_t user{0};
    uint64_t nice{0};
    uint64_t system{0};
    uint64_t idle{0};
    uint64_t iowait{0};
    uint64_t irq{0};
    uint64_t softirq{0};
    uint64_t steal{0};
    uint64_t guest{0};
    uint64_t guest_nice{0};

    [[nodiscard]] constexpr uint64_t total() const noexcept {
        // In Linux kernels, guest and guest_nice are already accounted for in user and nice.
        // Therefore, we must NOT add them again to prevent double-counting.
        return user + nice + system + idle + iowait + irq + softirq + steal;
    }

    [[nodiscard]] constexpr uint64_t idle_all() const noexcept {
        return idle + iowait;
    }

    [[nodiscard]] constexpr uint64_t busy() const noexcept {
        uint64_t tot = total();
        uint64_t idl = idle_all();
        return (tot >= idl) ? (tot - idl) : 0;
    }
};

struct CpuCoreSnapshot {
    uint32_t core_id{0};
    RawCpuTime time;
};

struct CpuSnapshot {
    RawCpuTime aggregate;
    std::vector<CpuCoreSnapshot> cores;
};

struct CpuStaticInfo {
    std::string model_name;
    uint32_t physical_cores{0};
    uint32_t logical_cores{0};
};

class CpuCollector {
  public:
    explicit CpuCollector(std::string stat_path = "/proc/stat",
                          std::string loadavg_path = "/proc/loadavg",
                          std::string cpuinfo_path = "/proc/cpuinfo");
    virtual ~CpuCollector() = default;

    [[nodiscard]] virtual std::optional<CpuSnapshot> read_stat() const;
    [[nodiscard]] virtual std::optional<domain::LoadAverage> read_loadavg() const;
    [[nodiscard]] virtual CpuStaticInfo read_cpuinfo() const;

    [[nodiscard]] static std::optional<CpuSnapshot> parse_stat_stream(std::istream& stream);
    [[nodiscard]] static std::optional<domain::LoadAverage> parse_loadavg_stream(
        std::istream& stream);
    [[nodiscard]] static CpuStaticInfo parse_cpuinfo_stream(std::istream& stream);

  private:
    std::string stat_path_;
    std::string loadavg_path_;
    std::string cpuinfo_path_;
};

}  // namespace nodepulse::collectors
