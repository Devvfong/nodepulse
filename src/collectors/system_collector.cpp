#include <chrono>
#include <cstdint>
#include <fstream>
#include <istream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include <nodepulse/collectors/system_collector.hpp>

#include <sys/utsname.h>
#include <unistd.h>

namespace nodepulse::collectors {

namespace {

std::string strip_quotes(std::string_view val) {
    if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') ||
                            (val.front() == '\'' && val.back() == '\''))) {
        return std::string(val.substr(1, val.size() - 2));
    }
    return std::string(val);
}

}  // namespace

SystemCollector::SystemCollector(std::string os_release_path, std::string uptime_path,
                                 std::string stat_path)
    : os_release_path_(std::move(os_release_path)),
      uptime_path_(std::move(uptime_path)),
      stat_path_(std::move(stat_path)) {}

std::string SystemCollector::collect_hostname() {
    char hostname_buf[256];
    if (gethostname(hostname_buf, sizeof(hostname_buf)) == 0) {
        hostname_buf[sizeof(hostname_buf) - 1] = '\0';
        return std::string(hostname_buf);
    }
    return "unknown";
}

std::pair<std::string, std::string> SystemCollector::collect_uname() {
    struct utsname uts {};
    if (uname(&uts) == 0) {
        return {std::string(uts.release), std::string(uts.machine)};
    }
    return {"unknown", "unknown"};
}

std::pair<std::string, std::string> SystemCollector::parse_os_release_stream(std::istream& stream) {
    std::string line;
    std::string pretty_name;
    std::string name;
    std::string version_id;
    std::string version;

    while (std::getline(stream, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }

        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        std::string_view key = std::string_view(line).substr(0, eq_pos);
        std::string_view raw_val = std::string_view(line).substr(eq_pos + 1);

        if (key == "PRETTY_NAME") {
            pretty_name = strip_quotes(raw_val);
        } else if (key == "NAME" && name.empty()) {
            name = strip_quotes(raw_val);
        } else if (key == "VERSION_ID") {
            version_id = strip_quotes(raw_val);
        } else if (key == "VERSION" && version.empty()) {
            version = strip_quotes(raw_val);
        }
    }

    std::string final_os_name =
        !pretty_name.empty() ? pretty_name : (!name.empty() ? name : "Linux");
    std::string final_os_version = !version_id.empty() ? version_id : version;

    return {final_os_name, final_os_version};
}

std::optional<double> SystemCollector::parse_uptime_stream(std::istream& stream) {
    double uptime_val{0.0};
    if (stream >> uptime_val && uptime_val >= 0.0) {
        return uptime_val;
    }
    return std::nullopt;
}

std::optional<uint64_t> SystemCollector::parse_btime_stream(std::istream& stream) {
    std::string line;
    while (std::getline(stream, line)) {
        if (line.starts_with("btime ")) {
            std::istringstream iss(line.substr(6));
            uint64_t btime_val{0};
            if (iss >> btime_val) {
                return btime_val;
            }
        }
    }
    return std::nullopt;
}

std::optional<domain::SystemInfo> SystemCollector::collect() const {
    domain::SystemInfo info;

    info.hostname = collect_hostname();
    auto [kernel_ver, arch] = collect_uname();
    info.kernel_version = kernel_ver;
    info.architecture = arch;

    // Parse OS release
    std::ifstream os_file(os_release_path_);
    if (!os_file.is_open() && os_release_path_ == "/etc/os-release") {
        os_file.open("/usr/lib/os-release");
    }

    if (os_file.is_open()) {
        auto [os_name, os_version] = parse_os_release_stream(os_file);
        info.os_name = os_name;
        info.os_version = os_version;
    } else {
        info.os_name = "Linux";
        info.os_version = "unknown";
    }

    // Parse uptime (required)
    std::ifstream uptime_file(uptime_path_);
    if (!uptime_file.is_open()) {
        return std::nullopt;
    }

    auto uptime_opt = parse_uptime_stream(uptime_file);
    if (!uptime_opt.has_value()) {
        return std::nullopt;
    }
    info.uptime_seconds = *uptime_opt;

    // Parse boot time
    std::ifstream stat_file(stat_path_);
    if (stat_file.is_open()) {
        auto btime_opt = parse_btime_stream(stat_file);
        if (btime_opt.has_value()) {
            info.boot_time_utc = *btime_opt;
        }
    }

    // Fallback boot time if /proc/stat had no btime
    if (info.boot_time_utc == 0) {
        auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
        if (now_epoch > static_cast<int64_t>(info.uptime_seconds)) {
            info.boot_time_utc =
                static_cast<uint64_t>(now_epoch - static_cast<int64_t>(info.uptime_seconds));
        }
    }

    return info;
}

}  // namespace nodepulse::collectors
