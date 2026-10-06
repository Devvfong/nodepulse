#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nodepulse/collectors/service_collector.hpp>

#include <dlfcn.h>

namespace nodepulse::collectors {

namespace {

// Minimal C declarations for libsystemd sd-bus types
struct sd_bus;
struct sd_bus_message;

struct sd_bus_error {
    const char* name;
    const char* message;
    int _need_free;
};

using fn_sd_bus_open_system = int (*)(sd_bus**);
using fn_sd_bus_close_unref = sd_bus* (*)(sd_bus*);
using fn_sd_bus_unref = sd_bus* (*)(sd_bus*);
using fn_sd_bus_call_method = int (*)(sd_bus*, const char*, const char*, const char*, const char*,
                                      sd_bus_error*, sd_bus_message**, const char*, ...);
using fn_sd_bus_message_enter_container = int (*)(sd_bus_message*, char, const char*);
using fn_sd_bus_message_exit_container = int (*)(sd_bus_message*);
using fn_sd_bus_message_read = int (*)(sd_bus_message*, const char*, ...);
using fn_sd_bus_message_unref = sd_bus_message* (*)(sd_bus_message*);
using fn_sd_bus_get_property_string = int (*)(sd_bus*, const char*, const char*, const char*,
                                              const char*, sd_bus_error*, char**);
using fn_sd_bus_get_property_trivial = int (*)(sd_bus*, const char*, const char*, const char*,
                                               const char*, sd_bus_error*, char, void*);
using fn_sd_bus_error_free = void (*)(sd_bus_error*);

class SdBusLibrary {
  public:
    static const SdBusLibrary& instance() {
        static const SdBusLibrary s_instance;
        return s_instance;
    }

    [[nodiscard]] bool is_available() const noexcept {
        return open_system != nullptr && call_method != nullptr;
    }

    void close_and_unref_bus(sd_bus* bus) const noexcept {
        if (!bus) {
            return;
        }
        if (close_unref) {
            close_unref(bus);
        } else if (unref) {
            unref(bus);
        }
    }

    void* handle{nullptr};
    fn_sd_bus_open_system open_system{nullptr};
    fn_sd_bus_close_unref close_unref{nullptr};
    fn_sd_bus_unref unref{nullptr};
    fn_sd_bus_call_method call_method{nullptr};
    fn_sd_bus_message_enter_container enter_container{nullptr};
    fn_sd_bus_message_exit_container exit_container{nullptr};
    fn_sd_bus_message_read message_read{nullptr};
    fn_sd_bus_message_unref message_unref{nullptr};
    fn_sd_bus_get_property_string get_property_string{nullptr};
    fn_sd_bus_get_property_trivial get_property_trivial{nullptr};
    fn_sd_bus_error_free error_free{nullptr};

  private:
    SdBusLibrary() {
        handle = dlopen("libsystemd.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            handle = dlopen("libsystemd.so", RTLD_NOW | RTLD_LOCAL);
        }
        if (!handle) {
            return;
        }

        open_system = reinterpret_cast<fn_sd_bus_open_system>(dlsym(handle, "sd_bus_open_system"));
        close_unref = reinterpret_cast<fn_sd_bus_close_unref>(dlsym(handle, "sd_bus_close_unref"));
        unref = reinterpret_cast<fn_sd_bus_unref>(dlsym(handle, "sd_bus_unref"));
        call_method = reinterpret_cast<fn_sd_bus_call_method>(dlsym(handle, "sd_bus_call_method"));
        enter_container = reinterpret_cast<fn_sd_bus_message_enter_container>(
            dlsym(handle, "sd_bus_message_enter_container"));
        exit_container = reinterpret_cast<fn_sd_bus_message_exit_container>(
            dlsym(handle, "sd_bus_message_exit_container"));
        message_read =
            reinterpret_cast<fn_sd_bus_message_read>(dlsym(handle, "sd_bus_message_read"));
        message_unref =
            reinterpret_cast<fn_sd_bus_message_unref>(dlsym(handle, "sd_bus_message_unref"));
        get_property_string = reinterpret_cast<fn_sd_bus_get_property_string>(
            dlsym(handle, "sd_bus_get_property_string"));
        get_property_trivial = reinterpret_cast<fn_sd_bus_get_property_trivial>(
            dlsym(handle, "sd_bus_get_property_trivial"));
        error_free = reinterpret_cast<fn_sd_bus_error_free>(dlsym(handle, "sd_bus_error_free"));
    }

    ~SdBusLibrary() {
        if (handle) {
            dlclose(handle);
            handle = nullptr;
        }
    }
};

struct SdBusErrorGuard {
    sd_bus_error error{nullptr, nullptr, 0};
    const SdBusLibrary& lib;

    explicit SdBusErrorGuard(const SdBusLibrary& l) : lib(l) {}
    ~SdBusErrorGuard() {
        if (lib.error_free) {
            lib.error_free(&error);
        }
    }
};

}  // namespace

bool ServiceCollector::is_valid_unit_name(std::string_view name) noexcept {
    if (name.empty() || name.size() > 256) {
        return false;
    }
    size_t normalized_len = name.ends_with(".service") ? name.size() : name.size() + 8;
    if (normalized_len > 256) {
        return false;
    }
    for (char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == '.' || c == '@')) {
            return false;
        }
    }
    // Disallow path traversal patterns like ".."
    if (name.find("..") != std::string_view::npos) {
        return false;
    }
    return true;
}

std::string ServiceCollector::normalize_service_name(std::string_view name) {
    if (name.ends_with(".service")) {
        return std::string(name);
    }
    return std::string(name) + ".service";
}

std::optional<std::vector<domain::ServiceInfo>> ServiceCollector::list_services(
    const std::string& state_filter) const {
    if (list_provider_) {
        return list_provider_(state_filter);
    }

    const auto& lib = SdBusLibrary::instance();
    if (!lib.is_available()) {
        return std::nullopt;
    }

    sd_bus* bus = nullptr;
    int ret = lib.open_system(&bus);
    if (ret < 0 || !bus) {
        return std::nullopt;
    }

    SdBusErrorGuard err_guard(lib);
    sd_bus_message* reply = nullptr;

    ret = lib.call_method(bus, "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager", "ListUnits", &err_guard.error, &reply,
                          "");

    if (ret < 0 || !reply) {
        lib.close_and_unref_bus(bus);
        return std::nullopt;
    }

    std::vector<domain::ServiceInfo> services;

    ret = lib.enter_container(reply, 'a', "(ssssssouso)");
    if (ret >= 0) {
        const char* name = nullptr;
        const char* desc = nullptr;
        const char* load = nullptr;
        const char* active = nullptr;
        const char* sub = nullptr;
        const char* followed = nullptr;
        const char* unit_path = nullptr;
        uint32_t job_id = 0;
        const char* job_type = nullptr;
        const char* job_path = nullptr;

        while (lib.enter_container(reply, 'r', "ssssssouso") > 0) {
            lib.message_read(reply, "ssssssouso", &name, &desc, &load, &active, &sub, &followed,
                             &unit_path, &job_id, &job_type, &job_path);

            if (name != nullptr) {
                std::string s_name(name);
                if (s_name.ends_with(".service")) {
                    std::string s_active = active ? active : "unknown";
                    bool matches = false;
                    if (state_filter == "all" || state_filter.empty()) {
                        matches = true;
                    } else if (state_filter == "active" && s_active == "active") {
                        matches = true;
                    } else if (state_filter == "inactive" && s_active == "inactive") {
                        matches = true;
                    } else if (state_filter == "failed" && s_active == "failed") {
                        matches = true;
                    }

                    if (matches) {
                        domain::ServiceInfo info;
                        info.name = s_name;
                        info.description = desc ? desc : "";
                        info.load_state = load ? load : "unknown";
                        info.active_state = s_active;
                        info.sub_state = sub ? sub : "unknown";
                        info.unit_file_state = "unknown";
                        services.push_back(std::move(info));
                    }
                }
            }

            lib.exit_container(reply);
        }
        lib.exit_container(reply);
    }

    lib.message_unref(reply);
    lib.close_and_unref_bus(bus);

    return services;
}

ServiceDetailResult ServiceCollector::get_service_detail(const std::string& unit_name) const {
    if (detail_provider_) {
        return detail_provider_(unit_name);
    }

    const auto& lib = SdBusLibrary::instance();
    if (!lib.is_available()) {
        return {ServiceStatusResult::kCollectorFailure, std::nullopt};
    }

    sd_bus* bus = nullptr;
    int ret = lib.open_system(&bus);
    if (ret < 0 || !bus) {
        return {ServiceStatusResult::kCollectorFailure, std::nullopt};
    }

    SdBusErrorGuard err_guard(lib);
    sd_bus_message* reply = nullptr;

    ret = lib.call_method(bus, "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager", "GetUnit", &err_guard.error, &reply,
                          "s", unit_name.c_str());

    if (ret < 0) {
        ServiceStatusResult status = ServiceStatusResult::kCollectorFailure;
        if (err_guard.error.name != nullptr &&
            std::strstr(err_guard.error.name, "NoSuchUnit") != nullptr) {
            status = ServiceStatusResult::kNotFound;
        } else if (ret == -ENOENT) {
            status = ServiceStatusResult::kNotFound;
        }
        lib.close_and_unref_bus(bus);
        return {status, std::nullopt};
    }

    const char* unit_path = nullptr;
    ret = lib.message_read(reply, "o", &unit_path);
    if (ret < 0 || !unit_path) {
        lib.message_unref(reply);
        lib.close_and_unref_bus(bus);
        return {ServiceStatusResult::kCollectorFailure, std::nullopt};
    }

    std::string path(unit_path);
    lib.message_unref(reply);

    domain::ServiceDetail detail;
    detail.name = unit_name;

    char* str_val = nullptr;

    // 1. Description
    if (lib.get_property_string(bus, "org.freedesktop.systemd1", path.c_str(),
                                "org.freedesktop.systemd1.Unit", "Description", nullptr,
                                &str_val) >= 0 &&
        str_val) {
        detail.description = str_val;
        free(str_val);
        str_val = nullptr;
    }

    // 2. LoadState
    if (lib.get_property_string(bus, "org.freedesktop.systemd1", path.c_str(),
                                "org.freedesktop.systemd1.Unit", "LoadState", nullptr,
                                &str_val) >= 0 &&
        str_val) {
        detail.load_state = str_val;
        free(str_val);
        str_val = nullptr;
    }

    // 3. ActiveState
    if (lib.get_property_string(bus, "org.freedesktop.systemd1", path.c_str(),
                                "org.freedesktop.systemd1.Unit", "ActiveState", nullptr,
                                &str_val) >= 0 &&
        str_val) {
        detail.active_state = str_val;
        free(str_val);
        str_val = nullptr;
    }

    // 4. SubState
    if (lib.get_property_string(bus, "org.freedesktop.systemd1", path.c_str(),
                                "org.freedesktop.systemd1.Unit", "SubState", nullptr,
                                &str_val) >= 0 &&
        str_val) {
        detail.sub_state = str_val;
        free(str_val);
        str_val = nullptr;
    }

    // 5. UnitFileState
    if (lib.get_property_string(bus, "org.freedesktop.systemd1", path.c_str(),
                                "org.freedesktop.systemd1.Unit", "UnitFileState", nullptr,
                                &str_val) >= 0 &&
        str_val) {
        detail.unit_file_state = str_val;
        free(str_val);
        str_val = nullptr;
    }

    // 6. MainPID
    uint32_t pid_val = 0;
    if (lib.get_property_trivial(bus, "org.freedesktop.systemd1", path.c_str(),
                                 "org.freedesktop.systemd1.Service", "MainPID", nullptr, 'u',
                                 &pid_val) >= 0) {
        detail.main_pid = static_cast<int32_t>(pid_val);
    }

    // 7. RestartCount (NRestarts)
    uint32_t restarts = 0;
    if (lib.get_property_trivial(bus, "org.freedesktop.systemd1", path.c_str(),
                                 "org.freedesktop.systemd1.Service", "NRestarts", nullptr, 'u',
                                 &restarts) >= 0) {
        detail.restart_count = restarts;
    }

    // 8. ActiveEnterTimestamp (usec -> sec)
    uint64_t ts_usec = 0;
    if (lib.get_property_trivial(bus, "org.freedesktop.systemd1", path.c_str(),
                                 "org.freedesktop.systemd1.Unit", "ActiveEnterTimestamp", nullptr,
                                 't', &ts_usec) >= 0) {
        detail.active_enter_timestamp_utc = ts_usec / 1000000ULL;
    }

    // 9. MemoryCurrent
    uint64_t mem_bytes = 0;
    if (lib.get_property_trivial(bus, "org.freedesktop.systemd1", path.c_str(),
                                 "org.freedesktop.systemd1.Service", "MemoryCurrent", nullptr, 't',
                                 &mem_bytes) >= 0) {
        detail.memory_current_bytes = mem_bytes == UINT64_MAX ? 0 : mem_bytes;
    }

    lib.close_and_unref_bus(bus);
    return {ServiceStatusResult::kOk, detail};
}

}  // namespace nodepulse::collectors
