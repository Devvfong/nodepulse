#pragma once

#include <cstdint>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nodepulse/domain/process_info.hpp>

namespace nodepulse::collectors {

struct ProcessStatFields {
    int32_t pid{0};
    std::string comm;
    std::string state{"S"};
    int32_t ppid{0};
    uint64_t utime{0};
    uint64_t stime{0};
    uint32_t num_threads{1};
    uint64_t starttime{0};
    uint64_t vsize_bytes{0};
    uint64_t rss_pages{0};
};

struct ProcessStatusFields {
    uint32_t uid{0};
    uint64_t vm_rss_bytes{0};
    uint64_t vm_size_bytes{0};
    uint32_t threads{1};
};

class ProcessCollector {
  public:
    explicit ProcessCollector(std::string proc_dir = "/proc");
    virtual ~ProcessCollector() = default;

    [[nodiscard]] std::string proc_dir() const noexcept {
        return proc_dir_;
    }

    // Static parsers for deterministic unit testing
    static bool parse_stat_line(std::string_view line, ProcessStatFields& out) noexcept;
    static bool parse_status_stream(std::istream& stream, ProcessStatusFields& out) noexcept;
    static std::string sanitize_cmdline(std::string_view raw,
                                        std::string_view fallback_comm) noexcept;

    // Direct /proc inspection
    [[nodiscard]] virtual int64_t get_pid_max() const;
    [[nodiscard]] virtual std::vector<int32_t> get_process_ids() const;
    [[nodiscard]] virtual std::optional<domain::ProcessDetail> collect_process_detail(
        int32_t pid, double cpu_percent = 0.0, uint64_t expected_starttime = 0) const;
    [[nodiscard]] virtual std::vector<domain::ProcessInfo> collect_processes() const;

    // Helper for tick to epoch conversion
    [[nodiscard]] virtual uint64_t get_boot_time_epoch() const;

  protected:
    std::string proc_dir_;
    mutable uint64_t cached_boot_time_{0};
    mutable int64_t cached_pid_max_{0};

    [[nodiscard]] std::string resolve_username(uint32_t uid) const;
    [[nodiscard]] uint32_t count_open_fds(int32_t pid) const;
    [[nodiscard]] std::string read_cwd(int32_t pid) const;
    [[nodiscard]] std::string read_cmdline_file(int32_t pid, std::string_view fallback_comm) const;
};

}  // namespace nodepulse::collectors
