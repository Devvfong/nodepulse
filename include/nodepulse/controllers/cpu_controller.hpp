#pragma once

#include <functional>
#include <memory>
#include <mutex>

#include <drogon/HttpController.h>

#include <nodepulse/services/cpu_service.hpp>

namespace nodepulse::controllers {

class CpuController : public drogon::HttpController<CpuController> {
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(CpuController::get_cpu, "/api/v1/cpu", drogon::Get);
    METHOD_LIST_END

    void get_cpu(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    static void set_cpu_service(std::shared_ptr<services::CpuService> service);

    [[nodiscard]] static std::shared_ptr<services::CpuService> get_cpu_service();

  private:
    static std::shared_ptr<services::CpuService> cpu_service_;
    static std::mutex mutex_;
};

}  // namespace nodepulse::controllers
