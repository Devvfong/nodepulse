#pragma once

#include <functional>
#include <memory>
#include <string>

#include <drogon/HttpController.h>

#include <nodepulse/services/process_service.hpp>

namespace nodepulse::controllers {

class ProcessController : public drogon::HttpController<ProcessController> {
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ProcessController::get_processes, "/api/v1/processes", drogon::Get);
    ADD_METHOD_TO(ProcessController::get_process_by_pid, "/api/v1/processes/{1}", drogon::Get);
    METHOD_LIST_END

    void get_processes(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void get_process_by_pid(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                            std::string pid_param);

    static void set_process_service(std::shared_ptr<services::ProcessService> service) {
        process_service_ = std::move(service);
    }

    [[nodiscard]] static std::shared_ptr<services::ProcessService> get_process_service();

  private:
    static std::shared_ptr<services::ProcessService> process_service_;
};

}  // namespace nodepulse::controllers
