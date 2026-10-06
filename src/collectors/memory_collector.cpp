#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <istream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <nodepulse/collectors/memory_collector.hpp>

namespace nodepulse::collectors {

namespace {

constexpr uint64_t kMaxKbForBytes = std::numeric_limits<uint64_t>::max() / 1024ULL;

std::string_view trim_whitespace(std::string_view str) {
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) {
        str.remove_prefix(1);
    }
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) {
        str.remove_suffix(1);
    }
    return str;
}

}  // namespace

MemoryCollector::MemoryCollector(std::string meminfo_path)
    : meminfo_path_(std::move(meminfo_path)) {}

std::optional<domain::MemoryMetrics> MemoryCollector::collect() const {
    std::ifstream stream(meminfo_path_);
    if (!stream.is_open()) {
        return std::nullopt;
    }
    return parse_meminfo_stream(stream);
}

std::optional<domain::MemoryMetrics> MemoryCollector::parse_meminfo_stream(std::istream& stream) {
    std::string line;
    std::optional<uint64_t> mem_total_kb;
    std::optional<uint64_t> mem_free_kb;
    std::optional<uint64_t> mem_available_kb;
    uint64_t buffers_kb = 0;
    uint64_t cached_kb = 0;
    uint64_t swap_total_kb = 0;
    uint64_t swap_free_kb = 0;

    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }

        auto colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }

        std::string_view key = trim_whitespace(std::string_view(line).substr(0, colon_pos));
        std::string_view val_view = trim_whitespace(std::string_view(line).substr(colon_pos + 1));

        bool is_target_key =
            (key == "MemTotal" || key == "MemFree" || key == "MemAvailable" || key == "Buffers" ||
             key == "Cached" || key == "SwapTotal" || key == "SwapFree");

        if (val_view.empty()) {
            if (is_target_key) {
                return std::nullopt;
            }
            continue;
        }

        uint64_t num_kb = 0;
        const char* first = val_view.data();
        const char* last = val_view.data() + val_view.size();
        auto [ptr, ec] = std::from_chars(first, last, num_kb);
        if (ec != std::errc{}) {
            if (is_target_key) {
                return std::nullopt;
            }
            continue;
        }

        std::string_view rest =
            trim_whitespace(std::string_view(ptr, static_cast<size_t>(last - ptr)));
        if (!rest.empty() && rest != "kB" && rest != "kb" && rest != "KB") {
            if (is_target_key) {
                return std::nullopt;
            }
            continue;
        }

        if (num_kb > kMaxKbForBytes) {
            if (is_target_key) {
                return std::nullopt;
            }
            continue;
        }

        if (key == "MemTotal") {
            mem_total_kb = num_kb;
        } else if (key == "MemFree") {
            mem_free_kb = num_kb;
        } else if (key == "MemAvailable") {
            mem_available_kb = num_kb;
        } else if (key == "Buffers") {
            buffers_kb = num_kb;
        } else if (key == "Cached") {
            cached_kb = num_kb;
        } else if (key == "SwapTotal") {
            swap_total_kb = num_kb;
        } else if (key == "SwapFree") {
            swap_free_kb = num_kb;
        }
    }

    if (!mem_total_kb.has_value() || *mem_total_kb == 0 || !mem_free_kb.has_value()) {
        return std::nullopt;
    }

    if (*mem_free_kb > *mem_total_kb) {
        mem_free_kb = *mem_total_kb;
    }

    uint64_t avail_kb = 0;
    if (mem_available_kb.has_value()) {
        avail_kb = *mem_available_kb;
    } else {
        // Fallback for older Linux kernels (< 3.14): MemFree + Buffers + Cached
        avail_kb = *mem_free_kb;
        if (avail_kb > kMaxKbForBytes - buffers_kb) {
            return std::nullopt;
        }
        avail_kb += buffers_kb;
        if (avail_kb > kMaxKbForBytes - cached_kb) {
            return std::nullopt;
        }
        avail_kb += cached_kb;
    }

    if (avail_kb > *mem_total_kb) {
        avail_kb = *mem_total_kb;
    }

    domain::MemoryMetrics metrics;
    metrics.total_bytes = *mem_total_kb * 1024ULL;
    metrics.free_bytes = *mem_free_kb * 1024ULL;
    metrics.available_bytes = avail_kb * 1024ULL;
    metrics.buffers_bytes = buffers_kb * 1024ULL;
    metrics.cached_bytes = cached_kb * 1024ULL;

    metrics.used_bytes = (metrics.total_bytes >= metrics.available_bytes)
                             ? (metrics.total_bytes - metrics.available_bytes)
                             : 0ULL;

    if (metrics.total_bytes > 0) {
        double pct =
            (static_cast<double>(metrics.used_bytes) / static_cast<double>(metrics.total_bytes)) *
            100.0;
        pct = std::clamp(pct, 0.0, 100.0);
        metrics.usage_percent = std::round(pct * 100.0) / 100.0;
    } else {
        metrics.usage_percent = 0.0;
    }

    metrics.swap_total_bytes = swap_total_kb * 1024ULL;
    metrics.swap_free_bytes = swap_free_kb * 1024ULL;

    if (metrics.swap_total_bytes > 0) {
        if (metrics.swap_free_bytes > metrics.swap_total_bytes) {
            metrics.swap_free_bytes = metrics.swap_total_bytes;
        }
        metrics.swap_used_bytes = metrics.swap_total_bytes - metrics.swap_free_bytes;
        double swap_pct = (static_cast<double>(metrics.swap_used_bytes) /
                           static_cast<double>(metrics.swap_total_bytes)) *
                          100.0;
        swap_pct = std::clamp(swap_pct, 0.0, 100.0);
        metrics.swap_usage_percent = std::round(swap_pct * 100.0) / 100.0;
    } else {
        metrics.swap_total_bytes = 0ULL;
        metrics.swap_free_bytes = 0ULL;
        metrics.swap_used_bytes = 0ULL;
        metrics.swap_usage_percent = 0.0;
    }

    return metrics;
}

}  // namespace nodepulse::collectors
