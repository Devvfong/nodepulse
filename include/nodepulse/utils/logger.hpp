#pragma once

#include <memory>
#include <string_view>

#include <spdlog/spdlog.h>

namespace nodepulse::utils {

class Logger {
  public:
    static void init(std::string_view level = "info", bool json_format = false);
    static void shutdown();
    [[nodiscard]] static std::shared_ptr<spdlog::logger> get();

  private:
    static std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace nodepulse::utils
