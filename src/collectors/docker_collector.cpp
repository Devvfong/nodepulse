#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <nodepulse/collectors/docker_collector.hpp>

#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace nodepulse::collectors {

namespace {

constexpr size_t kMaxHeaderSize = 32 * 1024;       // 32 KB limit on headers
constexpr size_t kMaxBodySize = 10 * 1024 * 1024;  // 10 MB limit on response body

struct SocketCloser {
    int fd{-1};
    ~SocketCloser() {
        if (fd >= 0) {
            ::close(fd);
        }
    }
};

bool decode_chunked_body(std::string_view raw_body, std::string& out_body, std::string& err_msg,
                         size_t max_body_size) {
    out_body.clear();
    size_t pos = 0;
    while (pos < raw_body.size()) {
        size_t line_end = raw_body.find("\r\n", pos);
        if (line_end == std::string_view::npos) {
            err_msg = "Malformed chunked encoding: missing chunk size delimiter";
            return false;
        }

        std::string_view size_str = raw_body.substr(pos, line_end - pos);
        size_t semi = size_str.find(';');
        if (semi != std::string_view::npos) {
            size_str = size_str.substr(0, semi);
        }
        while (!size_str.empty() && std::isspace(static_cast<unsigned char>(size_str.front()))) {
            size_str.remove_prefix(1);
        }
        while (!size_str.empty() && std::isspace(static_cast<unsigned char>(size_str.back()))) {
            size_str.remove_suffix(1);
        }

        if (size_str.empty()) {
            pos = line_end + 2;
            continue;
        }

        size_t chunk_size = 0;
        try {
            chunk_size = std::stoull(std::string(size_str), nullptr, 16);
        } catch (...) {
            err_msg = "Invalid chunk size: " + std::string(size_str);
            return false;
        }

        if (chunk_size == 0) {
            return true;
        }

        pos = line_end + 2;
        if (pos + chunk_size > raw_body.size()) {
            err_msg = "Truncated chunked body: chunk data exceeds stream length";
            return false;
        }

        if (out_body.size() + chunk_size > max_body_size) {
            err_msg = "Decoded chunked body exceeds maximum size limit";
            return false;
        }

        out_body.append(raw_body.data() + pos, chunk_size);
        pos += chunk_size;

        if (pos + 2 <= raw_body.size() && raw_body.substr(pos, 2) == "\r\n") {
            pos += 2;
        }
    }

    err_msg = "Chunked body terminated without terminal zero chunk";
    return false;
}

DockerHttpResponse parse_raw_http(std::string_view raw, size_t max_body_size) {
    DockerHttpResponse resp;
    size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string_view::npos) {
        resp.status = DockerTransportStatus::kMalformedResponse;
        resp.error_message = "Malformed HTTP response: headers not terminated with CRLF CRLF";
        return resp;
    }

    std::string_view headers = raw.substr(0, header_end);
    std::string_view body = raw.substr(header_end + 4);

    size_t status_line_end = headers.find("\r\n");
    std::string_view status_line =
        (status_line_end != std::string_view::npos) ? headers.substr(0, status_line_end) : headers;
    if (!status_line.starts_with("HTTP/1.0 ") && !status_line.starts_with("HTTP/1.1 ")) {
        resp.status = DockerTransportStatus::kMalformedResponse;
        resp.error_message = "Invalid HTTP status line: " + std::string(status_line);
        return resp;
    }

    size_t first_space = status_line.find(' ');
    size_t second_space = status_line.find(' ', first_space + 1);
    std::string_view code_str =
        (second_space != std::string_view::npos)
            ? status_line.substr(first_space + 1, second_space - first_space - 1)
            : status_line.substr(first_space + 1);

    int status_code = 0;
    auto [ptr, ec] =
        std::from_chars(code_str.data(), code_str.data() + code_str.size(), status_code);
    if (ec != std::errc{} || status_code < 100 || status_code > 599) {
        resp.status = DockerTransportStatus::kMalformedResponse;
        resp.error_message = "Invalid HTTP status code in status line: " + std::string(code_str);
        return resp;
    }
    resp.http_status = status_code;

    std::string lower_headers;
    lower_headers.reserve(headers.size());
    for (char c : headers) {
        lower_headers.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    bool is_chunked = (lower_headers.find("transfer-encoding: chunked") != std::string::npos);
    std::optional<size_t> content_length = std::nullopt;
    size_t cl_pos = lower_headers.find("content-length:");
    if (cl_pos != std::string::npos) {
        size_t val_start = cl_pos + 15;
        size_t val_end = lower_headers.find("\r\n", val_start);
        std::string_view cl_val =
            (val_end != std::string_view::npos)
                ? std::string_view(lower_headers).substr(val_start, val_end - val_start)
                : std::string_view(lower_headers).substr(val_start);
        while (!cl_val.empty() && std::isspace(static_cast<unsigned char>(cl_val.front()))) {
            cl_val.remove_prefix(1);
        }
        while (!cl_val.empty() && std::isspace(static_cast<unsigned char>(cl_val.back()))) {
            cl_val.remove_suffix(1);
        }
        size_t cl = 0;
        auto [cptr, cec] = std::from_chars(cl_val.data(), cl_val.data() + cl_val.size(), cl);
        if (cec == std::errc{}) {
            content_length = cl;
        }
    }

    if (is_chunked) {
        std::string decoded;
        std::string err;
        if (!decode_chunked_body(body, decoded, err, max_body_size)) {
            resp.status = (err.find("exceeds") != std::string::npos)
                              ? DockerTransportStatus::kResponseTooLarge
                              : DockerTransportStatus::kMalformedResponse;
            resp.error_message = err;
            return resp;
        }
        resp.body = std::move(decoded);
    } else if (content_length.has_value()) {
        if (*content_length > max_body_size) {
            resp.status = DockerTransportStatus::kResponseTooLarge;
            resp.error_message = "Content-Length exceeds maximum allowed body size";
            return resp;
        }
        if (body.size() < *content_length) {
            resp.status = DockerTransportStatus::kMalformedResponse;
            resp.error_message = "Body size smaller than Content-Length";
            return resp;
        }
        resp.body = std::string(body.substr(0, *content_length));
    } else {
        if (body.size() > max_body_size) {
            resp.status = DockerTransportStatus::kResponseTooLarge;
            resp.error_message = "HTTP body exceeds maximum allowed size";
            return resp;
        }
        resp.body = std::string(body);
    }

    resp.status = DockerTransportStatus::kOk;
    return resp;
}

}  // namespace

DockerUnixSocketTransport::DockerUnixSocketTransport(std::string socket_path, uint32_t timeout_ms,
                                                     bool enabled)
    : socket_path_(std::move(socket_path)), timeout_ms_(timeout_ms), enabled_(enabled) {}

DockerHttpResponse DockerUnixSocketTransport::get(const std::string& path) {
    if (!enabled_) {
        return {DockerTransportStatus::kDisabled, 0, "",
                "Docker collector is disabled in configuration"};
    }

    if (socket_path_.empty()) {
        return {DockerTransportStatus::kSocketNotFound, 0, "", "Socket path is empty"};
    }

    struct stat st {};
    if (::stat(socket_path_.c_str(), &st) != 0) {
        int err = errno;
        if (err == ENOENT) {
            return {DockerTransportStatus::kSocketNotFound, 0, "",
                    "Socket file does not exist: " + socket_path_};
        }
        if (err == EACCES || err == EPERM) {
            return {DockerTransportStatus::kPermissionDenied, 0, "",
                    "Permission denied accessing socket: " + socket_path_};
        }
        return {DockerTransportStatus::kSocketNotFound, 0, "",
                "Failed to access socket: " + socket_path_};
    }
    if (!S_ISSOCK(st.st_mode)) {
        return {DockerTransportStatus::kSocketNotFound, 0, "",
                "Path is not a Unix domain socket: " + socket_path_};
    }

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return {DockerTransportStatus::kSocketNotFound, 0, "",
                "Failed to create UNIX domain socket"};
    }
    SocketCloser closer{fd};

    struct timeval tv {};
    tv.tv_sec = static_cast<time_t>(timeout_ms_ / 1000);
    tv.tv_usec = static_cast<suseconds_t>((timeout_ms_ % 1000) * 1000);
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    if (socket_path_.size() >= sizeof(addr.sun_path)) {
        return {DockerTransportStatus::kSocketNotFound, 0, "",
                "Socket path exceeds sockaddr_un buffer"};
    }
    std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        int err = errno;
        if (err == ENOENT) {
            return {DockerTransportStatus::kSocketNotFound, 0, "", "Socket file not found"};
        }
        if (err == EACCES || err == EPERM) {
            return {DockerTransportStatus::kPermissionDenied, 0, "",
                    "Permission denied connecting to socket"};
        }
        if (err == ECONNREFUSED) {
            return {DockerTransportStatus::kConnectionRefused, 0, "",
                    "Connection refused by Docker daemon"};
        }
        if (err == ETIMEDOUT) {
            return {DockerTransportStatus::kTimeout, 0, "", "Connection timed out"};
        }
        return {DockerTransportStatus::kConnectionRefused, 0, "",
                "Failed to connect to Docker socket"};
    }

    std::string req = "GET " + path +
                      " HTTP/1.1\r\n"
                      "Host: localhost\r\n"
                      "User-Agent: NodePulse/1.0\r\n"
                      "Accept: application/json\r\n"
                      "Connection: close\r\n\r\n";

    size_t written = 0;
    while (written < req.size()) {
        struct pollfd pfd {};
        pfd.fd = fd;
        pfd.events = POLLOUT;
        int pret = ::poll(&pfd, 1, static_cast<int>(timeout_ms_));
        if (pret < 0) {
            if (errno == EINTR) {
                continue;
            }
            return {DockerTransportStatus::kConnectionRefused, 0, "", "Poll error on socket send"};
        }
        if (pret == 0) {
            return {DockerTransportStatus::kTimeout, 0, "",
                    "Timeout sending request to Docker daemon"};
        }
        ssize_t n = ::write(fd, req.data() + written, req.size() - written);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return {DockerTransportStatus::kConnectionRefused, 0, "",
                    "Write error to Docker socket"};
        }
        if (n == 0) {
            break;
        }
        written += static_cast<size_t>(n);
    }

    std::string raw_response;
    std::vector<char> buffer(8192);
    bool headers_parsed = false;
    size_t header_end_pos = std::string::npos;
    bool is_chunked = false;
    std::optional<size_t> content_length = std::nullopt;

    while (true) {
        struct pollfd pfd {};
        pfd.fd = fd;
        pfd.events = POLLIN;
        int pret = ::poll(&pfd, 1, static_cast<int>(timeout_ms_));
        if (pret < 0) {
            if (errno == EINTR) {
                continue;
            }
            return {DockerTransportStatus::kConnectionRefused, 0, "", "Poll error on socket read"};
        }
        if (pret == 0) {
            return {DockerTransportStatus::kTimeout, 0, "",
                    "Timeout reading response from Docker daemon"};
        }

        ssize_t n = ::read(fd, buffer.data(), buffer.size());
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return {DockerTransportStatus::kTimeout, 0, "", "Timeout reading response"};
            }
            return {DockerTransportStatus::kConnectionRefused, 0, "",
                    "Read error from Docker socket"};
        }
        if (n == 0) {
            break;
        }

        raw_response.append(buffer.data(), static_cast<size_t>(n));

        if (!headers_parsed) {
            header_end_pos = raw_response.find("\r\n\r\n");
            if (header_end_pos != std::string::npos) {
                headers_parsed = true;
                std::string header_str = raw_response.substr(0, header_end_pos);
                std::string lower_headers = header_str;
                for (char& c : lower_headers) {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                if (lower_headers.find("transfer-encoding: chunked") != std::string::npos) {
                    is_chunked = true;
                }
                auto cl_pos = lower_headers.find("content-length:");
                if (cl_pos != std::string::npos) {
                    auto line_end = lower_headers.find("\r\n", cl_pos);
                    std::string cl_val =
                        lower_headers.substr(cl_pos + 15, line_end - (cl_pos + 15));
                    size_t start = cl_val.find_first_not_of(" \t");
                    size_t end = cl_val.find_last_not_of(" \t");
                    if (start != std::string::npos && end != std::string::npos) {
                        try {
                            content_length = std::stoull(cl_val.substr(start, end - start + 1));
                        } catch (...) {}
                    }
                }
            } else if (raw_response.size() > kMaxHeaderSize) {
                return {DockerTransportStatus::kResponseTooLarge, 0, "",
                        "HTTP headers exceeded 32KB limit"};
            }
        }

        if (headers_parsed) {
            size_t body_bytes = raw_response.size() - (header_end_pos + 4);
            if (content_length.has_value()) {
                if (*content_length > kMaxBodySize) {
                    return {DockerTransportStatus::kResponseTooLarge, 0, "",
                            "Content-Length exceeds 10MB limit"};
                }
                if (body_bytes >= *content_length) {
                    break;
                }
            } else if (is_chunked) {
                std::string_view body_part(raw_response.data() + header_end_pos + 4, body_bytes);
                if (body_part.ends_with("\r\n0\r\n\r\n") || body_part == "0\r\n\r\n") {
                    break;
                }
            }
            if (raw_response.size() > kMaxHeaderSize + kMaxBodySize) {
                return {DockerTransportStatus::kResponseTooLarge, 0, "",
                        "Response exceeds maximum size"};
            }
        }
    }

    return parse_raw_http(raw_response, kMaxBodySize);
}

DockerCollector::DockerCollector(std::shared_ptr<IDockerTransport> transport,
                                 std::string socket_path)
    : transport_(std::move(transport)), socket_path_(std::move(socket_path)) {
    if (!transport_) {
        transport_ = std::make_shared<DockerUnixSocketTransport>(socket_path_);
    }
}

bool DockerCollector::is_valid_container_id(std::string_view id) noexcept {
    if (id.empty() || id.size() > 128) {
        return false;
    }
    if (id == "." || id == ".." || id.find("..") != std::string_view::npos) {
        return false;
    }
    for (char c : id) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '_' || c == '-' || c == '.';
        if (!ok) {
            return false;
        }
    }
    return true;
}

uint64_t DockerCollector::parse_iso8601_to_epoch(std::string_view str) noexcept {
    if (str.size() < 19 || str[4] != '-' || str[7] != '-' || str[10] != 'T' || str[13] != ':' ||
        str[16] != ':') {
        return 0;
    }
    auto parse_int = [](std::string_view sv, int& out) noexcept -> bool {
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), out);
        return ec == std::errc{} && ptr == sv.data() + sv.size();
    };
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int min = 0;
    int sec = 0;
    if (!parse_int(str.substr(0, 4), year) || !parse_int(str.substr(5, 2), month) ||
        !parse_int(str.substr(8, 2), day) || !parse_int(str.substr(11, 2), hour) ||
        !parse_int(str.substr(14, 2), min) || !parse_int(str.substr(17, 2), sec)) {
        return 0;
    }
    struct tm tm_buf {};
    tm_buf.tm_year = year - 1900;
    tm_buf.tm_mon = month - 1;
    tm_buf.tm_mday = day;
    tm_buf.tm_hour = hour;
    tm_buf.tm_min = min;
    tm_buf.tm_sec = sec;
    tm_buf.tm_isdst = 0;

    time_t t = timegm(&tm_buf);
    return t > 0 ? static_cast<uint64_t>(t) : 0;
}

double DockerCollector::calculate_cpu_percent(uint64_t cpu_total, uint64_t prev_cpu_total,
                                              uint64_t sys_total, uint64_t prev_sys_total,
                                              uint32_t online_cpus) noexcept {
    if (online_cpus == 0) {
        online_cpus = 1;
    }
    if (cpu_total <= prev_cpu_total || sys_total <= prev_sys_total) {
        return 0.0;
    }
    uint64_t cpu_delta = cpu_total - prev_cpu_total;
    uint64_t sys_delta = sys_total - prev_sys_total;
    if (sys_delta == 0) {
        return 0.0;
    }
    double percent = (static_cast<double>(cpu_delta) / static_cast<double>(sys_delta)) *
                     static_cast<double>(online_cpus) * 100.0;
    if (percent < 0.0) {
        return 0.0;
    }
    return percent;
}

double DockerCollector::calculate_memory_percent(uint64_t usage, uint64_t limit,
                                                 uint64_t cache) noexcept {
    if (limit == 0) {
        return 0.0;
    }
    uint64_t effective_usage = (usage > cache) ? (usage - cache) : 0;
    double percent = (static_cast<double>(effective_usage) / static_cast<double>(limit)) * 100.0;
    if (percent < 0.0) {
        return 0.0;
    }
    if (percent > 100.0) {
        return 100.0;
    }
    return percent;
}

DockerContainersResult DockerCollector::list_containers() {
    if (!transport_) {
        return {DockerStatusResult::kUnavailable, std::nullopt, "Transport not configured"};
    }

    auto resp = transport_->get("/containers/json?all=1");
    if (resp.status == DockerTransportStatus::kDisabled ||
        resp.status == DockerTransportStatus::kSocketNotFound ||
        resp.status == DockerTransportStatus::kPermissionDenied ||
        resp.status == DockerTransportStatus::kConnectionRefused ||
        resp.status == DockerTransportStatus::kTimeout) {
        return {DockerStatusResult::kUnavailable, std::nullopt, resp.error_message};
    }

    if (resp.status != DockerTransportStatus::kOk || resp.http_status != 200) {
        return {DockerStatusResult::kCollectorFailure, std::nullopt,
                resp.error_message.empty()
                    ? ("Docker returned HTTP " + std::to_string(resp.http_status))
                    : resp.error_message};
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(resp.body);
    } catch (const std::exception& ex) {
        return {DockerStatusResult::kCollectorFailure, std::nullopt,
                "Malformed Docker JSON: " + std::string(ex.what())};
    }

    if (!root.is_array()) {
        return {DockerStatusResult::kCollectorFailure, std::nullopt,
                "Docker response is not a JSON array"};
    }

    std::vector<domain::ContainerSummary> containers;
    for (const auto& item : root) {
        if (!item.is_object()) {
            continue;
        }

        domain::ContainerSummary summary;
        summary.id = item.value("Id", item.value("id", ""));

        if (item.contains("Names") && item["Names"].is_array()) {
            summary.names = item["Names"].get<std::vector<std::string>>();
        } else if (item.contains("names") && item["names"].is_array()) {
            summary.names = item["names"].get<std::vector<std::string>>();
        }

        summary.image = item.value("Image", item.value("image", ""));
        summary.status = item.value("Status", item.value("status", ""));
        summary.state = item.value("State", item.value("state", ""));

        if (item.contains("Created") && item["Created"].is_number()) {
            summary.created = item["Created"].get<uint64_t>();
        } else if (item.contains("created") && item["created"].is_number()) {
            summary.created = item["created"].get<uint64_t>();
        }

        containers.push_back(std::move(summary));
    }

    return {DockerStatusResult::kOk, std::move(containers), ""};
}

DockerContainerDetailResult DockerCollector::get_container(const std::string& id) {
    if (!transport_) {
        return {DockerStatusResult::kUnavailable, std::nullopt, "Transport not configured"};
    }

    if (!is_valid_container_id(id)) {
        return {DockerStatusResult::kNotFound, std::nullopt, "Invalid container identifier"};
    }

    auto resp = transport_->get("/containers/" + id + "/json");
    if (resp.status == DockerTransportStatus::kDisabled ||
        resp.status == DockerTransportStatus::kSocketNotFound ||
        resp.status == DockerTransportStatus::kPermissionDenied ||
        resp.status == DockerTransportStatus::kConnectionRefused ||
        resp.status == DockerTransportStatus::kTimeout) {
        return {DockerStatusResult::kUnavailable, std::nullopt, resp.error_message};
    }

    if (resp.http_status == 404) {
        return {DockerStatusResult::kNotFound, std::nullopt, "Container not found"};
    }

    if (resp.status != DockerTransportStatus::kOk || resp.http_status != 200) {
        return {DockerStatusResult::kCollectorFailure, std::nullopt,
                resp.error_message.empty()
                    ? ("Docker returned HTTP " + std::to_string(resp.http_status))
                    : resp.error_message};
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(resp.body);
    } catch (const std::exception& ex) {
        return {DockerStatusResult::kCollectorFailure, std::nullopt,
                "Malformed Docker JSON: " + std::string(ex.what())};
    }

    if (!root.is_object()) {
        return {DockerStatusResult::kCollectorFailure, std::nullopt,
                "Docker inspect response is not a JSON object"};
    }

    domain::ContainerDetail detail;
    detail.id = root.value("Id", root.value("id", id));
    detail.name = root.value("Name", root.value("name", ""));

    if (root.contains("Config") && root["Config"].is_object() && root["Config"].contains("Image") &&
        root["Config"]["Image"].is_string()) {
        detail.image = root["Config"]["Image"].get<std::string>();
    }
    if (detail.image.empty()) {
        detail.image = root.value("Image", root.value("image", ""));
    }

    if (root.contains("State") && root["State"].is_object()) {
        const auto& s = root["State"];
        detail.state = s.value("Status", s.value("status", ""));
        detail.running = s.value("Running", s.value("running", false));
        detail.exit_code = s.value("ExitCode", s.value("exit_code", int32_t{0}));
    } else {
        detail.state = root.value("state", root.value("State", ""));
        detail.running = root.value("running", root.value("Running", false));
        detail.exit_code = root.value("exit_code", root.value("ExitCode", int32_t{0}));
    }

    if (root.contains("Status") && root["Status"].is_string()) {
        detail.status = root["Status"].get<std::string>();
    } else if (root.contains("status") && root["status"].is_string()) {
        detail.status = root["status"].get<std::string>();
    } else {
        detail.status = detail.state;
    }

    if (root.contains("port_mappings") && root["port_mappings"].is_array()) {
        detail.port_mappings = root["port_mappings"].get<std::vector<std::string>>();
    } else {
        const nlohmann::json* ports_obj = nullptr;
        if (root.contains("NetworkSettings") && root["NetworkSettings"].is_object() &&
            root["NetworkSettings"].contains("Ports") &&
            root["NetworkSettings"]["Ports"].is_object()) {
            ports_obj = &root["NetworkSettings"]["Ports"];
        } else if (root.contains("HostConfig") && root["HostConfig"].is_object() &&
                   root["HostConfig"].contains("PortBindings") &&
                   root["HostConfig"]["PortBindings"].is_object()) {
            ports_obj = &root["HostConfig"]["PortBindings"];
        }

        if (ports_obj) {
            for (auto it = ports_obj->begin(); it != ports_obj->end(); ++it) {
                const std::string& container_port = it.key();
                if (it.value().is_array()) {
                    for (const auto& binding : it.value()) {
                        if (binding.is_object()) {
                            std::string host_ip = binding.value("HostIp", "0.0.0.0");
                            std::string host_port = binding.value("HostPort", "");
                            if (host_ip.empty()) {
                                host_ip = "0.0.0.0";
                            }
                            if (!host_port.empty()) {
                                detail.port_mappings.push_back(host_ip + ":" + host_port + "->" +
                                                               container_port);
                            }
                        }
                    }
                }
            }
            std::sort(detail.port_mappings.begin(), detail.port_mappings.end());
        }
    }

    if (root.contains("mount_sources") && root["mount_sources"].is_array()) {
        detail.mount_sources = root["mount_sources"].get<std::vector<std::string>>();
    } else if (root.contains("Mounts") && root["Mounts"].is_array()) {
        for (const auto& m : root["Mounts"]) {
            if (m.is_object() && m.contains("Source") && m["Source"].is_string()) {
                detail.mount_sources.push_back(m["Source"].get<std::string>());
            }
        }
        std::sort(detail.mount_sources.begin(), detail.mount_sources.end());
    }

    if (root.contains("created") && root["created"].is_number()) {
        detail.created = root["created"].get<uint64_t>();
    } else if (root.contains("Created") && root["Created"].is_number()) {
        detail.created = root["Created"].get<uint64_t>();
    } else if (root.contains("Created") && root["Created"].is_string()) {
        detail.created = parse_iso8601_to_epoch(root["Created"].get<std::string>());
    } else if (root.contains("created") && root["created"].is_string()) {
        detail.created = parse_iso8601_to_epoch(root["created"].get<std::string>());
    }

    return {DockerStatusResult::kOk, std::move(detail), ""};
}

}  // namespace nodepulse::collectors
