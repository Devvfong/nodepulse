#include <memory>
#include <string_view>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <nodepulse/utils/logger.hpp>

namespace nodepulse::utils {

std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;

void Logger::init(std::string_view level, bool json_format) {
    if (logger_) {
        return;
    }

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    if (json_format) {
        console_sink->set_pattern(
            "{\"timestamp\":\"%Y-%m-%dT%H:%M:%S.%eZ\",\"level\":\"%l\",\"logger\":\"%n\","
            "\"message\":\"%v\"}");
    } else {
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
    }

    logger_ = std::make_shared<spdlog::logger>("nodepulse", console_sink);

    auto log_level = spdlog::level::info;
    if (level == "trace") {
        log_level = spdlog::level::trace;
    } else if (level == "debug") {
        log_level = spdlog::level::debug;
    } else if (level == "warn") {
        log_level = spdlog::level::warn;
    } else if (level == "error") {
        log_level = spdlog::level::err;
    } else if (level == "critical") {
        log_level = spdlog::level::critical;
    }

    logger_->set_level(log_level);
    spdlog::set_default_logger(logger_);
}

void Logger::shutdown() {
    spdlog::shutdown();
    logger_ = nullptr;
}

std::shared_ptr<spdlog::logger> Logger::get() {
    if (!logger_) {
        init();
    }
    return logger_;
}

}  // namespace nodepulse::utils
