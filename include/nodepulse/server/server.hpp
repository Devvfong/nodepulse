#pragma once

#include <chrono>
#include <string>

#include <nodepulse/config/config.hpp>

namespace nodepulse::services {
class HistoryService;
}

namespace nodepulse::server {

class Server {
  public:
    explicit Server(config::Config config);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(Server&&) = delete;

    void setup();
    void run();
    void stop();

    [[nodiscard]] const config::Config& get_config() const noexcept {
        return config_;
    }

    [[nodiscard]] static std::chrono::steady_clock::time_point get_start_time() noexcept {
        return start_time_;
    }

  private:
    config::Config config_;
    static std::chrono::steady_clock::time_point start_time_;
    std::shared_ptr<services::HistoryService> history_service_;
};

}  // namespace nodepulse::server
