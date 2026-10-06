#include <cctype>
#include <fstream>
#include <istream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nodepulse/collectors/cpu_collector.hpp>

#include <unistd.h>

namespace nodepulse::collectors {

namespace {

std::string trim(std::string_view str) {
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) {
        str.remove_prefix(1);
    }
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) {
        str.remove_suffix(1);
    }
    return std::string(str);
}

RawCpuTime parse_cpu_times(std::istream& iss) {
    RawCpuTime t;
    iss >> t.user >> t.nice >> t.system >> t.idle;
    if (iss >> t.iowait) {
        if (iss >> t.irq) {
            if (iss >> t.softirq) {
                if (iss >> t.steal) {
                    if (iss >> t.guest) {
                        iss >> t.guest_nice;
                    }
                }
            }
        }
    }
    return t;
}

}  // namespace

CpuCollector::CpuCollector(std::string stat_path, std::string loadavg_path,
                           std::string cpuinfo_path)
    : stat_path_(std::move(stat_path)),
      loadavg_path_(std::move(loadavg_path)),
      cpuinfo_path_(std::move(cpuinfo_path)) {}

std::optional<CpuSnapshot> CpuCollector::parse_stat_stream(std::istream& stream) {
    std::string line;
    CpuSnapshot snapshot;
    bool found_aggregate = false;

    while (std::getline(stream, line)) {
        if (line.starts_with("cpu ")) {
            std::istringstream iss(line.substr(4));
            snapshot.aggregate = parse_cpu_times(iss);
            found_aggregate = true;
        } else if (line.starts_with("cpu")) {
            // Check if followed by digits
            size_t idx = 3;
            while (idx < line.size() && std::isdigit(static_cast<unsigned char>(line[idx]))) {
                ++idx;
            }
            if (idx > 3 && idx < line.size() &&
                std::isspace(static_cast<unsigned char>(line[idx]))) {
                uint32_t core_id = 0;
                try {
                    core_id = static_cast<uint32_t>(std::stoul(line.substr(3, idx - 3)));
                } catch (const std::exception&) {
                    continue;
                }
                std::istringstream iss(line.substr(idx));
                RawCpuTime t = parse_cpu_times(iss);
                snapshot.cores.push_back({core_id, t});
            }
        }
    }

    if (!found_aggregate) {
        return std::nullopt;
    }

    return snapshot;
}

std::optional<domain::LoadAverage> CpuCollector::parse_loadavg_stream(std::istream& stream) {
    domain::LoadAverage load;
    if (stream >> load.one_minute >> load.five_minute >> load.fifteen_minute) {
        if (load.one_minute >= 0.0 && load.five_minute >= 0.0 && load.fifteen_minute >= 0.0) {
            return load;
        }
    }
    return std::nullopt;
}

CpuStaticInfo CpuCollector::parse_cpuinfo_stream(std::istream& stream) {
    CpuStaticInfo info;
    std::string line;
    std::set<std::pair<int, int>> unique_physical_cores;
    uint32_t processor_count = 0;
    int current_physical_id = -1;
    int current_core_id = -1;
    int cpu_cores_declared = 0;

    while (std::getline(stream, line)) {
        auto colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            if (line.empty() && current_physical_id >= 0 && current_core_id >= 0) {
                unique_physical_cores.insert({current_physical_id, current_core_id});
                current_physical_id = -1;
                current_core_id = -1;
            }
            continue;
        }

        std::string key = trim(line.substr(0, colon_pos));
        std::string val = trim(line.substr(colon_pos + 1));

        if (key == "model name" && info.model_name.empty()) {
            info.model_name = val;
        } else if (key == "processor") {
            ++processor_count;
        } else if (key == "physical id") {
            try {
                current_physical_id = std::stoi(val);
            } catch (const std::exception&) {
                current_physical_id = -1;
            }
        } else if (key == "core id") {
            try {
                current_core_id = std::stoi(val);
            } catch (const std::exception&) {
                current_core_id = -1;
            }
        } else if (key == "cpu cores" && cpu_cores_declared == 0) {
            try {
                cpu_cores_declared = std::stoi(val);
            } catch (const std::exception&) {
                cpu_cores_declared = 0;
            }
        }
    }

    if (current_physical_id >= 0 && current_core_id >= 0) {
        unique_physical_cores.insert({current_physical_id, current_core_id});
    }

    info.logical_cores = processor_count;
    if (info.logical_cores == 0) {
        long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
        info.logical_cores = (nprocs > 0) ? static_cast<uint32_t>(nprocs) : 1;
    }

    if (!unique_physical_cores.empty()) {
        info.physical_cores = static_cast<uint32_t>(unique_physical_cores.size());
    } else if (cpu_cores_declared > 0) {
        info.physical_cores = static_cast<uint32_t>(cpu_cores_declared);
    } else {
        info.physical_cores = info.logical_cores;
    }

    if (info.model_name.empty()) {
        info.model_name = "Unknown CPU";
    }

    return info;
}

std::optional<CpuSnapshot> CpuCollector::read_stat() const {
    std::ifstream file(stat_path_);
    if (!file.is_open()) {
        return std::nullopt;
    }
    return parse_stat_stream(file);
}

std::optional<domain::LoadAverage> CpuCollector::read_loadavg() const {
    std::ifstream file(loadavg_path_);
    if (!file.is_open()) {
        return std::nullopt;
    }
    return parse_loadavg_stream(file);
}

CpuStaticInfo CpuCollector::read_cpuinfo() const {
    std::ifstream file(cpuinfo_path_);
    if (!file.is_open()) {
        CpuStaticInfo fallback;
        fallback.model_name = "Unknown CPU";
        long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
        fallback.logical_cores = (nprocs > 0) ? static_cast<uint32_t>(nprocs) : 1;
        fallback.physical_cores = fallback.logical_cores;
        return fallback;
    }
    return parse_cpuinfo_stream(file);
}

}  // namespace nodepulse::collectors
