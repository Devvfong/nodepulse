#include <algorithm>
#include <cctype>
#include <charconv>
#include <climits>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nodepulse/collectors/process_collector.hpp>

#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

namespace nodepulse::collectors {

namespace {

std::string_view trim_view(std::string_view s) noexcept {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0) {
        s.remove_suffix(1);
    }
    return s;
}

template <typename T>
bool parse_int_token(std::string_view token, T& out) noexcept {
    if (token.empty()) {
        return false;
    }
    auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), out);
    return ec == std::errc{} && ptr == token.data() + token.size();
}

}  // namespace

ProcessCollector::ProcessCollector(std::string proc_dir) : proc_dir_(std::move(proc_dir)) {}

bool ProcessCollector::parse_stat_line(std::string_view line, ProcessStatFields& out) noexcept {
    auto open_paren = line.find('(');
    auto close_paren = line.rfind(')');

    if (open_paren == std::string_view::npos || close_paren == std::string_view::npos ||
        close_paren <= open_paren) {
        return false;
    }

    std::string_view pid_part = trim_view(line.substr(0, open_paren));
    if (!parse_int_token(pid_part, out.pid) || out.pid < 1) {
        return false;
    }

    out.comm = std::string(line.substr(open_paren + 1, close_paren - (open_paren + 1)));

    std::string_view rest = line.substr(close_paren + 1);
    std::vector<std::string_view> tokens;
    size_t i = 0;
    while (i < rest.size()) {
        while (i < rest.size() && std::isspace(static_cast<unsigned char>(rest[i])) != 0) {
            ++i;
        }
        if (i >= rest.size()) {
            break;
        }
        size_t start = i;
        while (i < rest.size() && std::isspace(static_cast<unsigned char>(rest[i])) == 0) {
            ++i;
        }
        tokens.push_back(rest.substr(start, i - start));
    }

    // Need at least 22 fields after comm:
    // 0:state, 1:ppid, 2:pgrp, 3:session, 4:tty_nr, 5:tpgid, 6:flags, 7:minflt,
    // 8:cminflt, 9:majflt, 10:cmajflt, 11:utime, 12:stime, 13:cutime, 14:cstime,
    // 15:priority, 16:nice, 17:num_threads, 18:itrealvalue, 19:starttime, 20:vsize, 21:rss
    if (tokens.size() < 22) {
        return false;
    }

    out.state = std::string(tokens[0]);
    if (!parse_int_token(tokens[1], out.ppid)) {
        return false;
    }
    if (!parse_int_token(tokens[11], out.utime)) {
        return false;
    }
    if (!parse_int_token(tokens[12], out.stime)) {
        return false;
    }
    if (!parse_int_token(tokens[17], out.num_threads)) {
        out.num_threads = 1;
    }
    if (!parse_int_token(tokens[19], out.starttime)) {
        return false;
    }
    if (!parse_int_token(tokens[20], out.vsize_bytes)) {
        out.vsize_bytes = 0;
    }
    if (!parse_int_token(tokens[21], out.rss_pages)) {
        out.rss_pages = 0;
    }

    return true;
}

bool ProcessCollector::parse_status_stream(std::istream& stream,
                                           ProcessStatusFields& out) noexcept {
    std::string line;
    bool found_uid = false;

    while (std::getline(stream, line)) {
        std::string_view sv = line;
        if (sv.starts_with("Uid:")) {
            std::string_view val = trim_view(sv.substr(4));
            auto space_pos = val.find_first_of(" \t");
            std::string_view real_uid =
                space_pos != std::string_view::npos ? val.substr(0, space_pos) : val;
            if (parse_int_token(real_uid, out.uid)) {
                found_uid = true;
            }
        } else if (sv.starts_with("VmRSS:")) {
            std::string_view val = trim_view(sv.substr(6));
            auto space_pos = val.find_first_of(" \t");
            std::string_view num_str =
                space_pos != std::string_view::npos ? val.substr(0, space_pos) : val;
            uint64_t kb = 0;
            if (parse_int_token(num_str, kb)) {
                out.vm_rss_bytes = kb * 1024ULL;
            }
        } else if (sv.starts_with("VmSize:")) {
            std::string_view val = trim_view(sv.substr(7));
            auto space_pos = val.find_first_of(" \t");
            std::string_view num_str =
                space_pos != std::string_view::npos ? val.substr(0, space_pos) : val;
            uint64_t kb = 0;
            if (parse_int_token(num_str, kb)) {
                out.vm_size_bytes = kb * 1024ULL;
            }
        } else if (sv.starts_with("Threads:")) {
            std::string_view val = trim_view(sv.substr(8));
            parse_int_token(val, out.threads);
        }
    }

    return found_uid;
}

std::string ProcessCollector::sanitize_cmdline(std::string_view raw,
                                               std::string_view fallback_comm) noexcept {
    constexpr size_t kMaxCmdlineLength = 4096;
    if (raw.empty()) {
        return std::string(fallback_comm);
    }

    size_t len = std::min(raw.size(), kMaxCmdlineLength);
    std::string result;
    result.reserve(len);

    for (size_t i = 0; i < len; ++i) {
        char c = raw[i];
        if (c == '\0' || static_cast<unsigned char>(c) < 32 ||
            static_cast<unsigned char>(c) > 126) {
            if (!result.empty() && result.back() != ' ') {
                result.push_back(' ');
            }
        } else {
            result.push_back(c);
        }
    }

    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }

    if (result.empty()) {
        return std::string(fallback_comm);
    }
    return result;
}

int64_t ProcessCollector::get_pid_max() const {
    if (cached_pid_max_ > 0) {
        return cached_pid_max_;
    }

    std::ifstream file(proc_dir_ + "/sys/kernel/pid_max");
    if (file.is_open()) {
        std::string line;
        if (std::getline(file, line)) {
            int64_t val = 0;
            if (parse_int_token(trim_view(line), val) && val > 0) {
                cached_pid_max_ = val;
                return cached_pid_max_;
            }
        }
    }

    cached_pid_max_ = 4194304;  // Linux 64-bit default
    return cached_pid_max_;
}

uint64_t ProcessCollector::get_boot_time_epoch() const {
    if (cached_boot_time_ > 0) {
        return cached_boot_time_;
    }

    std::ifstream file(proc_dir_ + "/stat");
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.starts_with("btime ")) {
                uint64_t btime = 0;
                if (parse_int_token(trim_view(line.substr(6)), btime)) {
                    cached_boot_time_ = btime;
                    return cached_boot_time_;
                }
            }
        }
    }

    cached_boot_time_ = 0;
    return cached_boot_time_;
}

std::string ProcessCollector::resolve_username(uint32_t uid) const {
    struct passwd pwd {};
    struct passwd* result = nullptr;
    char buf[1024];

    if (getpwuid_r(uid, &pwd, buf, sizeof(buf), &result) == 0 && result != nullptr &&
        result->pw_name != nullptr) {
        return std::string(result->pw_name);
    }

    return std::to_string(uid);
}

uint32_t ProcessCollector::count_open_fds(int32_t pid) const {
    std::string fd_dir = proc_dir_ + "/" + std::to_string(pid) + "/fd";
    DIR* dir = opendir(fd_dir.c_str());
    if (!dir) {
        return 0;
    }

    uint32_t count = 0;
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] != '.') {
            ++count;
        }
    }
    closedir(dir);
    return count;
}

std::string ProcessCollector::read_cwd(int32_t pid) const {
    std::string cwd_path = proc_dir_ + "/" + std::to_string(pid) + "/cwd";
    char buf[PATH_MAX];
    ssize_t len = readlink(cwd_path.c_str(), buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return std::string(buf);
    }
    return "";
}

std::string ProcessCollector::read_cmdline_file(int32_t pid, std::string_view fallback_comm) const {
    std::string path = proc_dir_ + "/" + std::to_string(pid) + "/cmdline";
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return std::string(fallback_comm);
    }

    char buffer[4096];
    file.read(buffer, sizeof(buffer));
    std::streamsize bytes_read = file.gcount();
    if (bytes_read <= 0) {
        return std::string(fallback_comm);
    }

    return sanitize_cmdline(std::string_view(buffer, static_cast<size_t>(bytes_read)),
                            fallback_comm);
}

std::vector<int32_t> ProcessCollector::get_process_ids() const {
    std::vector<int32_t> pids;
    DIR* dir = opendir(proc_dir_.c_str());
    if (!dir) {
        return pids;
    }

    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN) {
            std::string_view name = entry->d_name;
            if (!name.empty() && std::isdigit(static_cast<unsigned char>(name[0])) != 0) {
                int32_t pid = 0;
                if (parse_int_token(name, pid) && pid >= 1) {
                    pids.push_back(pid);
                }
            }
        }
    }
    closedir(dir);

    std::sort(pids.begin(), pids.end());
    return pids;
}

std::optional<domain::ProcessDetail> ProcessCollector::collect_process_detail(
    int32_t pid, double cpu_percent, uint64_t expected_starttime) const {
    std::string stat_path = proc_dir_ + "/" + std::to_string(pid) + "/stat";
    std::ifstream stat_file(stat_path);
    if (!stat_file.is_open()) {
        return std::nullopt;
    }

    std::string stat_line;
    if (!std::getline(stat_file, stat_line)) {
        return std::nullopt;
    }

    ProcessStatFields stat_fields;
    if (!parse_stat_line(stat_line, stat_fields)) {
        return std::nullopt;
    }

    std::string status_path = proc_dir_ + "/" + std::to_string(pid) + "/status";
    std::ifstream status_file(status_path);
    ProcessStatusFields status_fields;
    if (status_file.is_open()) {
        parse_status_stream(status_file, status_fields);
    }

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        page_size = 4096;
    }

    uint64_t rss_bytes = status_fields.vm_rss_bytes > 0
                             ? status_fields.vm_rss_bytes
                             : stat_fields.rss_pages * static_cast<uint64_t>(page_size);

    uint64_t vms_bytes =
        status_fields.vm_size_bytes > 0 ? status_fields.vm_size_bytes : stat_fields.vsize_bytes;

    uint32_t thread_count =
        status_fields.threads > 0 ? status_fields.threads : stat_fields.num_threads;

    long clk_tck = sysconf(_SC_CLK_TCK);
    if (clk_tck <= 0) {
        clk_tck = 100;
    }

    uint64_t boot_time = get_boot_time_epoch();
    uint64_t start_time_epoch =
        boot_time + (stat_fields.starttime / static_cast<uint64_t>(clk_tck));

    double effective_cpu_percent = cpu_percent;
    if (expected_starttime != 0 && stat_fields.starttime != expected_starttime) {
        effective_cpu_percent = 0.0;
    }

    domain::ProcessDetail detail;
    detail.pid = pid;
    detail.ppid = stat_fields.ppid;
    detail.name = stat_fields.comm;
    detail.user = resolve_username(status_fields.uid);
    detail.state = stat_fields.state;
    detail.cpu_percent = effective_cpu_percent;
    detail.memory_rss_bytes = rss_bytes;
    detail.memory_vms_bytes = vms_bytes;
    detail.thread_count = thread_count;
    detail.open_fd_count = count_open_fds(pid);
    detail.start_time_epoch = start_time_epoch;
    detail.cmdline = read_cmdline_file(pid, stat_fields.comm);
    detail.working_directory = read_cwd(pid);

    return detail;
}

std::vector<domain::ProcessInfo> ProcessCollector::collect_processes() const {
    auto pids = get_process_ids();
    std::vector<domain::ProcessInfo> processes;
    processes.reserve(pids.size());

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        page_size = 4096;
    }

    for (int32_t pid : pids) {
        std::string stat_path = proc_dir_ + "/" + std::to_string(pid) + "/stat";
        std::ifstream stat_file(stat_path);
        if (!stat_file.is_open()) {
            continue;
        }

        std::string stat_line;
        if (!std::getline(stat_file, stat_line)) {
            continue;
        }

        ProcessStatFields stat_fields;
        if (!parse_stat_line(stat_line, stat_fields)) {
            continue;
        }

        std::string status_path = proc_dir_ + "/" + std::to_string(pid) + "/status";
        std::ifstream status_file(status_path);
        ProcessStatusFields status_fields;
        if (status_file.is_open()) {
            parse_status_stream(status_file, status_fields);
        }

        uint64_t rss_bytes = status_fields.vm_rss_bytes > 0
                                 ? status_fields.vm_rss_bytes
                                 : stat_fields.rss_pages * static_cast<uint64_t>(page_size);

        domain::ProcessInfo info;
        info.pid = pid;
        info.name = stat_fields.comm;
        info.user = resolve_username(status_fields.uid);
        info.state = stat_fields.state;
        info.cpu_percent = 0.0;
        info.memory_rss_bytes = rss_bytes;
        info.cmdline = read_cmdline_file(pid, stat_fields.comm);

        processes.push_back(std::move(info));
    }

    return processes;
}

}  // namespace nodepulse::collectors
