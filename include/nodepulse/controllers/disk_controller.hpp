#pragma once

#include <functional>
#include <memory>
#include <mutex>

#include <drogon/HttpController.h>

#include <nodepulse/services/disk_service.hpp>

namespace nodepulse::controllers {

class DiskController : public drogon::HttpController<DiskController> {
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(DiskController::get_disks, "/api/v1/disks", drogon::Get);
    METHOD_LIST_END

    void get_disks(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    static void set_disk_service(std::shared_ptr<services::DiskService> service);

    [[nodiscard]] static std::shared_ptr<services::DiskService> get_disk_service();

  private:
    static std::shared_ptr<services::DiskService> disk_service_;
    static std::mutex mutex_;
};

}  // namespace nodepulse::controllers
