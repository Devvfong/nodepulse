#pragma once

#include <chrono>
#include <functional>

#include <drogon/HttpController.h>

namespace nodepulse::controllers {

class HealthController : public drogon::HttpController<HealthController> {
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HealthController::get_health, "/api/v1/health", drogon::Get);
    METHOD_LIST_END

    void get_health(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    static void set_start_time(std::chrono::steady_clock::time_point start_time) noexcept {
        start_time_ = start_time;
    }

    [[nodiscard]] static std::chrono::steady_clock::time_point get_start_time() noexcept {
        return start_time_;
    }

  private:
    static std::chrono::steady_clock::time_point start_time_;
};

}  // namespace nodepulse::controllers
