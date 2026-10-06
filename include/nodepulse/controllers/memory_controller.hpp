#pragma once

#include <functional>
#include <memory>

#include <drogon/HttpController.h>

#include <nodepulse/services/memory_service.hpp>

namespace nodepulse::controllers {

class MemoryController : public drogon::HttpController<MemoryController> {
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(MemoryController::get_memory, "/api/v1/memory", drogon::Get);
    METHOD_LIST_END

    void get_memory(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    static void set_memory_service(std::shared_ptr<services::MemoryService> service) {
        memory_service_ = std::move(service);
    }

    [[nodiscard]] static std::shared_ptr<services::MemoryService> get_memory_service();

  private:
    static std::shared_ptr<services::MemoryService> memory_service_;
};

}  // namespace nodepulse::controllers
